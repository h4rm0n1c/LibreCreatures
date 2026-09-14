# Picked-up objects could never be dropped -- pending_right_button() was hardcoded false

Date: 2026-09-14

## The bug

`Document::update_world` (`application/document.cpp`) polls this every
tick while an object is being carried:

```cpp
if (host.has_edit_object()) {
    host.place_edit_object_at_pointer();
    if (host.pending_right_button()) {
        host.clear_pending_input();
        host.finalize_edit_object();
        host.clear_edit_object();
    }
}
```

`C1WindowsDocument::pending_right_button()` was:

```cpp
bool C1WindowsDocument::pending_right_button() const { return false; }
```

A stub that always returns false. Since the drop condition can never be
satisfied, any object picked up with shift+left-click (the `edit_object`
mechanism -- confirmed against native's real `ProcessPendingInput`
disassembly earlier this session, `0x00428860`, which only enters this
branch on `LEFT_WITH_SHIFT`) followed the mouse forever and could never
be released again. Reported by the user: picked up a coffee pot and
could not put it down.

## Fix

`pending_right_button()` now reads the same `SfcViewPendingInputFlag::
right_button` bit that `on_right_button_down` (`ui/views.cpp`) already
correctly sets, via the exact pattern already used by the adjacent,
already-correct `mouse_client_x()`/`mouse_client_y()` (same file,
`active_main_frame()` -> `active_c1_view()` -> `view->view_state()`):

```cpp
bool C1WindowsDocument::pending_right_button() const {
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    if (view == nullptr) {
        return false;
    }
    constexpr std::uint32_t kRightButton = static_cast<std::uint32_t>(
        creatures1::ui::SfcViewPendingInputFlag::right_button);
    return (view->view_state().pending_input_flags & kRightButton) != 0;
}
```

No new state needed -- the flag it should have been reading already
existed and was already being set correctly; it just was never read.

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir
  /tmp/bm-cache` -- 139/139 compile, resource compiles, link succeeds
  (only this one file recompiled).
- Live UI testing to directly reproduce pick-up/drop was attempted but
  inconclusive this pass: the main viewport kept auto-scrolling
  (follow-selected-creature mode, apparently uncontrollable via CAOS
  `CMRA` -- see the runtime-behavior handoff doc's own open lead on
  that) mid-test, making pixel-precise clicks land on the wrong content
  between screenshots. Not re-attempted given that instability; this
  fix's correctness rests on matching an established, already-working
  sibling function's exact pattern, not on a live repro this pass.
