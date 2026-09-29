# neorender

Work on the `neorender` branch: true-colour and double-resolution art, zoom,
an SDL3 renderer inside the MFC shell, and a fixed-step world loop. None of it
changes what a world file stores or how the simulation behaves; a world made
here still loads in the original game.

## Goals

- **Art tiers.** Galleries load from `.spr`, `.s32` and `@2x.s32` files
  (`BACK.spr`, `BACK.s32`, `BACK@2x.s32`). S32 is the format the Creatures 3
  and Docking Station community builds use: a small header, then one PNG per
  frame with real alpha. A View menu setting caps the highest tier drawn.
- **Zoom.** View menu: 1x, 2x, and "Scale world height to window" (the world
  is 8352 x 1200). The creature's-eye view gets 1x and 2x.
- **SDL3 rendering** in the world view and eye view, with menus, dialogs and
  the kit protocol left as they are.
- **A fixed-step loop.** The world keeps its 90 ms tick, but drawing happens
  every display frame, so the hand moves smoothly.

## Rules that keep it compatible

- Game logic always uses 1x frame sizes: bounds, collisions, placement and
  pixel hit tests come from the `.spr` (or the `.s32` when there is no
  `.spr`). The tier setting changes drawing only.
- A `.s32` must match the 1x frame count and sizes; an `@2x.s32` must be
  exactly twice the size. A tier that doesn't match is ignored.
- Images the game draws into while it runs (blackboards, speech, creature
  pigment) are always drawn from their 1x pixels.
- With no `.s32` files, at 1x, the picture is pixel-identical to the
  8-bit renderer.
- The snapshot command still writes an 8-bit `temp.spr`.

## Measuring

**Frame dumps.** With `C1_FRAME_DUMP_DIR` set, `sys: cmnd 32940` writes the
current view as `frame-<tick>-gdi.bmp` in that directory, expanded through
the game palette. It is the reference every new renderer back-end is
compared against (`display/frame_dump`). When the view is drawn with SDL the
same command also reads that frame back as `frame-<tick>-sdl.bmp` and
appends a line to `compare.log`: the driver, both sizes, how many pixels
differ over the area they share, and the texture memory in use.

**Tick timing.** `C1_TRACE_TICK=1`, or a `trace_tick` file in the world
folder, appends a summary line to `Creatures.tick.log` every 110 ticks.

Baseline before any neorender change (lab, two norns, 612 x 358 view,
2026-09-29), per 110-tick window:

| | mean | p95 | max |
| --- | --- | --- | --- |
| interval between ticks | 90.0 ms | 90.1 ms | 90.9 ms |
| whole tick | 0.66-0.85 ms | 1.0-2.4 ms | 1.5-5.0 ms |
| drawing inside the tick | 0.09-0.22 ms | 0.12-0.31 ms | 0.17-3.4 ms |
| simulation inside the tick | 0.58-0.68 ms | | |

The world is busy under 1% of the time. The simulation itself is cheap; what
grows with resolution is drawing, which is why it moves to the GPU and out of
the tick.

## Art tiers (phase 1)

`display/s32` reads S32 files: the header, every offset, each PNG's
signature and size, all checked against the real file length before
anything is decoded (tests: `tests/c1_s32_test.cpp`). PNGs decode through the
vendored `stb_image` (`third_party/`). Frames over 16M pixels are refused:
the 1 GB a 16384-square frame needs would not fit a 32-bit process.

`display/image_tiers` finds `NAME.s32` and `NAME@2x.s32` in the same places
as `NAME.spr` (the world's `Images`, then the install's), keeps a tier only
if it has a frame for each of a gallery's images at exactly the 1x size (or
twice it), and picks the best tier under a cap (tests:
`tests/c1_image_tiers_test.cpp`). An image the game draws into while running
(pigment, speech, blackboards, fills) is marked by `Image::mark_pixels_changed`
and always drawn from its own pixels.

With `C1_FRAME_DUMP_DIR` set, `sys: cmnd 32941` writes `tiers.txt` there:
each loaded gallery and its tiers, then any tier file refused and why.

`scripts/spr_to_s32.py` converts a `.spr` to `.s32` (or `@2x.s32` by
nearest-neighbour doubling) through the game palette, to test with.

Not yet: an `.s32` with no `.spr` beside it. The world file records each
frame's size and its offset in the `.spr`, and game logic, hit tests and the
8-bit renderer all use the `.spr`'s pixels, so that case needs the `.s32`
quantised to the palette for them. It also would not load in the original
game, which needs the `.spr`.

## SDL world view (phase 2)

The world view is drawn with SDL3 (`third_party/SDL3`, `SDL3.dll` beside the
exe) inside the MFC view window: SDL wraps the view's existing window and
only draws. MFC keeps the message loop, the mouse, menus, dialogs and the
kit protocol.

`WorldRenderer::collect_scene` walks the world exactly as the 8-bit
renderer always has (background tiles, sprites in plane order, the overlay)
into a list of `SceneItem`s. The 8-bit renderer blits that list into its
back buffer, which still backs snapshots (`temp.spr` is unchanged) and every
native dirty-rectangle call. `platform/sdl_world_view` draws the same list
as textures, one whole frame at a time, whenever the game would have copied
dirty rectangles to the screen.

- Palette images upload through the current game palette; index 0 is
  transparent on sprites and black on background tiles, as the blitter
  treats it. A texture is rebuilt when the game draws into its image, and
  every texture is dropped on a new world or palette. Textures past 256 MB
  go least recently drawn first.
- Tiers: at 1x, `.s32` is drawn where it exists and `.spr` otherwise; `@2x`
  waits for zoom (phase 3).
- `C1_RENDERER`, or the `Renderer` string under the game's user registry
  key, picks the SDL driver (`direct3d11`, `opengl`, `software`, ...); `gdi`
  keeps the old GDI path, and so does any failure to start SDL.

Checked in the lab (Wine, `direct3d` driver), with no `.s32` files: the SDL
frame equals the 8-bit frame, 0 pixels differing, in the kitchen with two
norns and at four other camera positions, including across the wrap seam.
With a `back.s32` whose frames are outlined (`spr_to_s32.py --mark`), the
only differing pixels are the outlines. Clicks, pick-up and carrying, menus
and the tip dialog behave as under GDI.
