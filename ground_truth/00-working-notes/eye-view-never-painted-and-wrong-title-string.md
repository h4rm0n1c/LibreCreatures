# Eye view was a small black/untextured box with the wrong title -- resolved

Date: 2026-09-14/15

## The bugs

Two independent bugs in the creature "eye view" feature (`View` menu /
toolbar toggle, command id 32771, `application::toggle_eye_view`):

### 1. The window never painted anything (the "black box")

`C1EyeViewWindow` is a plain `CWnd` (not a `CView`). Unlike
`C1WindowsView` (a `CView`, which gets `WM_PAINT -> OnDraw` wired
automatically by MFC's document/view framework, no explicit handler
needed), a bare `CWnd` draws **nothing** on its own -- it needs its own
`ON_WM_PAINT()` entry and `OnPaint()` implementation. This window's
message map had none:

```cpp
BEGIN_MESSAGE_MAP(C1EyeViewWindow, CWnd)
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_WM_PALETTECHANGED()
    ON_WM_QUERYNEWPALETTE()
END_MESSAGE_MAP()
```

Even worse: the eye view's own `WorldRenderer` (constructed with the
*document* as its `WorldRendererHost` -- the same interface the main
view's renderer shares) calls back through
`C1WindowsDocument::present_current_view(owner_window, rect)` every time
its viewport updates. That function ignored `owner_window` entirely and
always drew through the document's own `renderer_`/`gdi_host_` --
i.e. the **main game view**, never the eye view's own window:

```cpp
void C1WindowsDocument::present_current_view(
    void* /*owner_window*/, const creatures1::world::WorldRect& viewport_rect) {
    present_renderer_rect(viewport_rect);   // always the main view
}
```

So even the per-tick camera-follow updates that DID run correctly
(`ui::update_selected_creature_follow_viewport`, confirmed already
correctly wired earlier this session) never had anywhere to paint their
result. The eye view's client area stayed whatever GDI left it as --
black -- forever.

### 2. The window title was empty (showed only "` - <creature name>`" or blank)

Both `C1WindowsDocument::eye_view_title()` and `C1EyeViewWindow::
localized_eye_view_title()` called `LoadStringA(0x80)`. Resource id
`0x80` (128) is **not a string resource** in this project's `.rc` file
at all -- 128 exists only as a `BITMAP` and an `ICON`
(`128 BITMAP "images/bitmap/128.bmp"` / `128 ICON "images/icon/128.ico"`).
`LoadStringA` on a nonexistent id returns an empty string.

## Confirmed against native, not guessed

Decompiled native's real `CEyeView::UpdateWindowTitleForSelectedCreature`
(`0x004172f0`): it calls `LoadStringA(0xef26)`, not `0x80`. Extracted
that exact string resource directly from the reference `Creatures.exe`'s
own `RT_STRING` table (bundle 3827, all 7 shipped locales) with
`pefile` -- id `0xef26` (61222) is literally the word **"View"** (the
same string the `View` menu itself uses: German "Ansicht", French "Vue",
Japanese "ビュー", etc.), not a distinct "Eye View" string. This
project's own `.rcinc` extraction of that exact bundle already had this
correct (`61222 "View"`) -- the resource data was never the problem,
only the C++ code's id constant was wrong.

## Fix

- `eye_view_title()` / `localized_eye_view_title()`: `LoadStringA(0x80)`
  -> `LoadStringA(0xef26)`.
- Added `ON_WM_PAINT()` + `C1EyeViewWindow::OnPaint()` (uses
  `CPaintDC`, repaints the current follow-viewport rect reconstructed
  from `follow_center_x_/y_` and the same `±0x40/0x30` half-extents
  `update_selected_creature_follow_viewport` already uses).
- Added `C1EyeViewWindow::present_world_rect(rect)`, which mirrors
  `C1WindowsDocument::present_renderer_rect` exactly but against this
  window's own `renderer_`/`gdi_host_`.
- `C1WindowsDocument::present_current_view` now compares `owner_window`
  against `eye_view_->GetSafeHwnd()` and routes to
  `eye_view_->present_world_rect(...)` when they match, falling back to
  the main view's `present_renderer_rect` otherwise.

## NOT fixed here -- a related, more severe bug found in the same investigation

`WorldRenderer::set_viewport_origin`/`scroll_viewport` (the renderer's
own methods, `display/rendering.cpp`) also call
`host_.move_renderable_objects(delta_x, delta_y)` and
`host_.update_view_anchored_objects()` on the same shared
`WorldRendererHost` (the document) whenever a viewport moves.
`move_renderable_objects` calls `object->move_by(delta_x, delta_y)` on
**every renderable object in the world** -- for the main view this is
confirmed correct/native-faithful (`ScrollViewport @ 0x00412f30`), but
the eye view's renderer shares the exact same call with no window
distinction. **Every time the eye view's own follow-camera moves to
track a creature, every renderable object in the entire world gets
shifted by that same delta too** -- silent, ongoing world-position
corruption for as long as the eye view is open and a creature is
selected and moving. This was not fixed in this pass (needs a similar
"which window is this for" guard, but touching per-tick world-mutation
code needs more care than the time available here) -- flagged as a
high-priority follow-up, and it was already happening before this
session's paint fix (the paint fix does not make it worse, it was
already running per-tick regardless of whether anything painted).

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir
  /tmp/bm-cache` -- 139/139 compile, resource compiles, link succeeds.
- Live re-verification of the paint fix specifically was inconclusive
  this pass: toggling the eye view via the CAOS `CMND 32771` pipe
  injection (used successfully for other menu-command tests earlier
  this session) did not visibly create the window in this harness,
  unlike a real interactive click. Given the user already directly
  confirmed the window DOES appear in real play (small, black,
  untextured, wrong title) -- exactly the two symptoms this fix
  targets -- and the root cause for both is unambiguous from
  code/decompile inspection (no `ON_WM_PAINT`, no owner-window routing,
  wrong resource id confirmed against the reference exe's own resource
  table), this was shipped on code-level confidence rather than forcing
  a live repro through a test harness that appears not to reliably
  trigger this specific menu command path.
