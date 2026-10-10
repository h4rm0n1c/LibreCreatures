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

**Lab trace.** `C1_LAB_TRACE=<file>` appends one line per event:
`<world tick> TAB <ms> TAB <category> TAB <text>`. `C1_LAB_TRACE_CATEGORIES`
picks categories, comma separated (default all):

| category | events |
| --- | --- |
| `kits` | kit launch, shutdown and its cause (menu, `KITQUIT`, shutdown message), every broadcast with the slots it reached, every message sent to a kit |
| `kitpipe` | every kit request on the pipe and its answer |
| `selection` | selected creature changes |
| `camera` | every camera move: from, to, and what caused it (`by=script <family> <genus> <species> <event>`, `turn owner=...`, `injected pipe/dde/start`); navigation mode changes; placement on load |
| `scheduler` | object scripts started, held back and ended |
| `sound` | the sound manager's trace lines (as `C1_SOUND_LOG`) |
| `lifecycle` | world opened and saved, deaths, creatures removed |

The trace only observes. The c1-lab `launch(trace=...)` sets it, and its
`trace` and `trace_wait` tools read it.

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

### Art that ships only as `.s32` (phase 1b)

The game needs an 8-bit `.spr` behind every gallery. The world file records
frames by their place in it, and frame sizes, hit tests, pigment remapping,
the 8-bit renderer and snapshots all read its pixels. So when a `NAME.spr`
is asked for and there is none in any image directory, the file layer
(`application/s32_sprite_files`) answers with one made from `NAME.s32` in
the same directory (`display/s32_to_spr`):

- Alpha under 128 becomes index 0 (transparent).
- Every other pixel becomes the nearest of the game's own colours, palette
  indices 10..245 from the install's `PALETTE.DTA`. That file is read on its
  own, because a world's sprite indexes are read before its palette is set
  up.
- A real `NAME.spr` anywhere always wins, so game logic uses a `.spr`'s
  frames whenever there is one.

The made `.spr` lives in memory, one per file per world. The SDL renderer
still draws the `.s32` itself, in true colour; the made `.spr` is what the
rules and the 8-bit path see. `tiers.txt` lists each `.spr` made, or the
`.s32` refused and why (tests: `tests/c1_s32_to_spr_test.cpp`).

Checked:
- `back.spr` converted to `.s32` and back through the real palette gives a
  file of the same size and index table. Of 10 million pixels, 7,241 have a
  different index, all with the same colour (the palette repeats colours).
- In the lab, with the install's `back.spr` set aside and only `back.s32` in
  the world, the world loads and plays. The 8-bit frame from the made `.spr`
  and the SDL frame from the `.s32` differ by 0 pixels.

Such a world will not load in the original game, which needs the `.spr`.

### In the kits

The kits read game art through `c1kitshell/game_art` (kit shell). It
follows the same rules:
- The world's `Images` first, then the installation's.
- `.s32` in true colour where it fits the `.spr`'s frames, or on its own.
- `@2x.s32` when the kit draws at double size or more.

It uses the game's own S32 reader and PNG decoder
(`shell/src/game_art_decoders.cpp` compiles `src/c1/display/s32.cpp` and
`png_decode.cpp` into each kit). The Hatchery's eggs are the kits' only
game art (the kits' own `.spr` pictures are a different, kit-only format).

Tests: `src/kits/shell/tests/game_art_test.cpp`.

Checked in the lab:
- Outlined `eggs.s32` and `eggs@2x.s32` in the world's `Images`: the
  Hatchery (eggs at 2x) draws the `@2x` art.
- With the installation's `eggs.spr` set aside and only `eggs.s32` present,
  it draws the `.s32`.

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
- Tiers: at 1x or smaller, `.s32` is drawn where it exists and `.spr`
  otherwise; `@2x` only when the view is magnified (phase 3).
- `C1_RENDERER`, or the `Renderer` string under the game's user registry
  key, picks the SDL driver (`direct3d11`, `opengl`, `software`, ...); `gdi`
  keeps the old GDI path, and so does any failure to start SDL.

Checked in the lab (Wine, `direct3d` driver), with no `.s32` files: the SDL
frame equals the 8-bit frame, 0 pixels differing, in the kitchen with two
norns and at four other camera positions, including across the wrap seam.
With a `back.s32` whose frames are outlined (`spr_to_s32.py --mark`), the
only differing pixels are the outlines. Clicks, pick-up and carrying, menus
and the tip dialog behave as under GDI.

## Zoom and detail (phase 3)

View menu, all seven languages, command IDs 32944-32951 (clear of the kit
tools' range):

- **Zoom**: 1x, 2x, or *Scale world height to window* (window height / 1200,
  so the whole world height fits, smaller or larger than 1x).
- **Maximum detail**: SPR, S32 or @2x S32. Drawing takes the best tier at or
  under this; @2x is used only while magnified.
- **Creature's view size**: Normal (128x96) or Double (256x192).

Settings `Zoom`, `MaxImageDetail` and `EyeViewZoom` sit beside the game's
other view settings in the registry. All three need SDL; with the GDI
renderer the items are greyed and the view stays 1x.

The renderer, the scroll bars, follow and every game rule stay in world
pixels. The view divides each mouse point by the zoom before the game sees
it, and the renderer's viewport is the client size divided by the zoom
(rounded up). SDL scales on the way to the screen: nearest-neighbour when
a tier's pixels are magnified a whole number of times, linear otherwise,
with every edge rounded to a whole screen pixel so background tiles meet
exactly. Changing the zoom keeps the middle of the view where it was.

The eye view is drawn by SDL too, with SDL's software renderer (the view is
at most 256x192, and under Wine a second Direct3D renderer beside the main
view's failed an assertion in `vkAllocateDescriptorSets`). Its back buffer
always stays the 128x96 follow viewport. Changing its size closes and
reopens it; resizing the open window left it short by a caption's height
under Wine.

Checked in the lab: 2x draws the `@2x` tier (an outlined `back@2x.s32`),
and the hand sits under the mouse. Scale-to-height shows the whole world
height with the hand under the mouse. The SPR cap at 1x differs from the
8-bit frame by 0 pixels. The eye view is right at both sizes, both when
opened and when switched. The settings survive a restart.

## World clock and a smooth hand (phase 4)

The world no longer ticks on `WM_TIMER` 1. Every place that set or killed
that timer now calls the main frame's world clock (`set_world_clock`,
`stop_world_clock`), and those keep its meaning: setting restarts the
period, stopping pauses the world. That covers pause/resume, speed changes,
file open/save (the world stops while their dialogs are up), macro version
errors, and kit pause and resume.

A 10 ms pulse timer drives a `QueryPerformanceCounter` accumulator. It runs
a tick whenever one is due, up to three in a row to catch up after a stall,
and drops any further backlog as `WM_TIMER` did. It is still a window timer,
so the world keeps ticking inside menus and modal loops as before. Measured
in the lab: ticks 90.00 ms apart on average, 5th to 95th percentile within
0.1 ms, the same as before.

Between ticks, the pulse draws a new SDL frame whenever the mouse has moved:
about 100 frames a second while it moves, against 11 before. In that frame
the hand, and anything that follows the mouse with it (a carried object,
creature or compound object, or an object being placed), is drawn as far
from its world position as the mouse has moved since the last tick. That is
drawing only: the world moves the pointer on its next tick exactly as
before, so `pntr` and the kits see the same values. While paused, the hand
stays where the world has it, as it always has.

`compare.log` lines now end with `frames=`, the number of SDL frames drawn.

### World speed

View > World speed (1x, 2x, 5x or 10x; setting `WorldSpeed`, commands
32953-32956, all seven languages) runs the world that many steps for each
normal one: the clock's step is the world's interval over the speed. The
interval itself is untouched, so CAOS, the save and the kits see the same
value. Everything in the world counts steps, so the status bar's world
clock, ages, timers and life stages all run that much faster; the clock
shows world time, not time on the wall.

- The catch-up limit scales with the speed (three steps per normal step), and
  one pulse spends at most 50 ms stepping, so a world too busy to keep up
  runs as fast as it can and the window stays responsive.
- Smooth motion glides over the shorter step.
- Measured in the lab (8 creatures): 22.2, 55.4 and 111.8 steps a second at
  2x, 5x and 10x, against 11.1 at 1x.
- Neorender only: main still steps on `WM_TIMER`, which cannot run much
  faster than 5x.

### Smooth motion

View > Smooth motion (on by default; setting `SmoothMotion`, command 32952,
all seven languages) glides the view and every sprite between ticks:

- At the start of each tick the view's origin and every entity's position
  are recorded. Each SDL frame then draws them `progress` of the way from
  there to where the tick left them, where `progress` is the time since the
  tick over the tick interval. The world is therefore drawn one tick
  behind; the hand is not, as it is drawn at the mouse over the view as
  drawn.
- The camera alone is not enough: a followed creature still steps once a
  tick, so a gliding view would make it jitter. Moving both keeps it still
  on screen.
- A walking creature's step against its heading (east or west) is not
  glided along x. Its skeleton hangs from the planted foot, so as its legs
  change pose it rocks back on that foot, about 9 px once a stride. Gliding
  that slid the whole norn, planted foot and all, back and forward again: it
  rubber-banded. That step is now shown at once, as the native shows it.
  Measured over 60 s of a walking norn: 260 frames slid backwards before,
  none after (`C1_GLIDE_TRACE`, below).
- Not glided, shown as they are:
  - a sprite new since the tick;
  - anything that moved more than 64 px in one tick (a teleport, the
    world's wrap);
  - a view that moved more than half its size (a favourite place, a camera
    command).
  - Image frames change per tick as before.
- Paused, or with the setting off, frames show the world exactly as it is.
  The frame-dump comparison always draws unglided.
- While anything glides, the pulse draws a frame every 10 ms.
- A click still lands on the world as the tick left it, up to one tick's
  movement from what is drawn.

Checked in the lab:
- With the camera moved 4 px every other tick by a CAOS script, the view
  moved in 1 px steps across the tick; with the setting off, in 4 px jumps.
- The hand, and what it carried, stayed on the same screen pixels while
  the view glided under it.
- A followed walking norn stayed still on screen.
- The frame dump still differs by 0 pixels.
- Ticks still average 90.00 ms. Their spread widened, as each pulse now
  draws a frame: 5th to 95th percentile within 1 ms, worst about 107 ms.

`C1_GLIDE_TRACE=<Windows path>` (for example `Z:\tmp\glide.txt`) writes
one line per drawn frame for the selected creature: time, tick, progress,
the body's world position and frame, where it was drawn, the drawn view,
the down foot, the skeleton's bounds, the head's end and the facing.

### Developer view: Show rooms

View > Developer view > Show rooms (off by default; setting `ShowRooms`,
command 32957, tooltip format string 32958, all seven languages) draws
each room in the map's room table as a 1-pixel outline over the main view:

- Yellow outlines; the rooms whose edge is under the mouse are white.
- With the mouse within about 3 screen pixels of an edge, a tooltip gives
  each such room's number (its place in the room table, from 0), its type
  (1 takes the outdoor temperature) and its bounds. Rooms that share an
  edge are all listed.
- While the tooltip shows, a 100 ms timer checks the mouse again, so the
  tooltip follows the view as it scrolls under a still mouse and goes when
  the mouse leaves the window. (Under Wine a mouse that jumps out of the
  window sends no `WM_MOUSELEAVE`.)
- SDL view only: the item is greyed out with GDI. Drawing only: the 8-bit
  back buffer, snapshots and the frame dump do not show the outlines.
- `world/room_edges.hpp` holds the edge test; `tests/c1_room_edges_test.cpp`
  checks it.

## Fixes since the 34f556b test build

- The egg in the sky: `new:` no longer ends a script's turn, so a norn's
  egg-laying script sets the egg up in the same turn it lays it, and
  nothing can catch the egg half made at the world origin. *(original)*
  Eggs already stuck at the origin stay; remove them from the CAOS
  console (View > Developer view) with
  `enum 2 0 0,doif clas eq 33554432,doif posl eq 0,doif post eq 0,kill targ,endi,endi,endi,next`.
  Also on `main`.

## Fixes since the last test build (bbc0930)

These are game and kit fixes, not renderer work. Every one is also on
`main`: each was cherry-picked there as it was made, so the normal
LibreCreatures build has them too. A fix marked *(original)* corrects a bug
the original game or kit has as well; those are listed in
`docs/original-bugs.md`. The rest were mistakes in this port.

Creatures, import and export:
- An export that cannot be written keeps the creature in the world, and
  the genome file is checked before it is read as well as written.
  *(original)*
- An imported pregnant norn keeps the child genome saved for her; a
  renamed import breeds with its own genome. *(original, the gamete)*
- Importing a creature rebuilds its body only and keeps its saved
  chemistry and brain.
- Norns sense the world's light again.
- A loaded creature that dies or is killed before its next life stage
  frees its limbs and body gallery; a creature's looping sound stops when
  it goes.

Biochemistry and disease:
- A reaction's full yield reaches its products (a yield over 255 wrapped).
  *(original)*
- A reaction with the same chemical in both reactant slots no longer
  counts its supply twice. *(original)*
- Shed bacteria move through the pool whole, sneezing spreads them again,
  and Infect Current Norn infects the norn from the pool.

CAOS and scripts:
- `enum .. kill targ .. next` visits every creature; an `enum` with no
  match skips its whole body, nested `enum`s included. *(original)*
- `doif`/`enum` scans and `gsub` ignore control words inside bracketed
  text. *(original)*
- `ltcy` with an empty or whole-range span, and `divv`/`modv` of
  -2147483648 by -1, no longer stop the game. *(original)*
- A script that ends no longer costs the next script its turn that tick.
  *(original)*
- Pronunciation substitutions keep the rest of the word ("right" now
  becomes "wight", not "wght").

Vehicles and objects:
- A vehicle circling the world stays on it, a vehicle a script moves stays
  where it was moved, and a moving vehicle shows lowercase animation
  frames as a standing one does. *(original)*
- Loaded compound objects, call buttons, simple objects and scenery
  release their galleries and stop their looping sounds when deleted.
- Speech bubbles on screen at a save keep their words after loading.

Sound:
- Looping sounds come back after unmute, and after the window gets focus
  again under `sndf fore`. *(original)*

Kits:
- Science Kit: dendrite ranges where a gene's maximum is below its minimum
  read as the game reads them, and dendrite totals use the brain's own
  lobes (by sex, life stage and build order).
- Injector: a failed inject script reports the failure and does not use
  the COB up. *(original kit)*
- All kits send a script longer than 4 KB whole.
- A brain activity report is sized to the brain, so a large brain no
  longer writes past the kit's 4 KB reply buffer. *(original)*
