# Eye-view composition and overlay

## Symptom

The creature eye-view window could open as a small black box. Its renderer
discarded the native creation parameters, started at `(0, 0)`, did not draw a
full frame before the first paint, and used the main-world viewport setter.
The latter shifts document-owned renderable objects, which is invalid for the
secondary eye view. The overlay gallery was also left disabled.

## Native evidence

Ghidra/decompiled native `Creatures.exe` confirms:

- `CApplication::ToggleEyeView` at `0x00432140` constructs a `128x96`
  `CWorldRenderer`, initially at the selected creature's sound-source X/Y,
  with smooth scrolling disabled and overlay gallery `0x62627562`.
- `CEyeView::UpdateSelectedCreatureFollowViewport` at `0x004176e0` publishes
  the eye renderer viewport directly as center minus `(0x40, 0x30)` and queues
  a dirty rectangle; it does not shift main-world renderables.
- `CWorldRenderer::RenderWorldRectToDIB` draws the world entities first, then
  blits overlay gallery `0x62627562`, image 0, at the viewport origin. This is
  the centred eye-frame sprite and must remain the final composition layer.
- `CEyeView::CreateWindow` at `0x00417440` uses the normal `128x96` default
  rectangle and the persisted `EyePosn` window placement.

## Port fix

`C1EyeViewWindow` now retains the native construction parameters, uses the
native viewport and overlay, and has a secondary-only viewport-origin setter.
`C1WindowsDocument::create_eye_view` runs the selected-creature follow update
and an explicit full redraw before the first paint. Eye-view paints redraw a
full frame if no back-buffer redraw has happened yet.

## Verification

Both `byte_match` and `LibreCreatures` independently compile and link the
139-file closure. The public-lane executable is staged at
`/home/harri/creatures/builds/Creatures.clean.exe`.

## Remaining live checks

Run the Windows build and verify: non-black world contents, the overlay sprite
centred over the viewport, eye-view toggle close/reopen, selected-creature
follow, and that scrolling the eye view does not move main-world objects.
The port still intentionally reports motion-target and sleep-indicator state
as unavailable, so exact native attention-target follow remains a separate
follow-up.
