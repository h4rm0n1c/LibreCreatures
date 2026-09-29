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
compared against (`display/frame_dump`).

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
