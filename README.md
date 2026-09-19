# LibreCreatures

A clean-room reimplementation of **Creatures 1** (originally Gameware Development /
Mindscape, 1996), reconstructed from a Ghidra disassembly of the **Community
Edition (CE)** build of `Creatures.exe` and rebuilt from that semantic
understanding, not by copying its machine code. The CE build is a community-
maintained rebuild of the original engine, not the 1996 shipped binary itself;
this project's recovered behavior is measured against CE specifically, and may
differ from the original 1996 release wherever CE itself does.

This repository holds the buildable implementation only. It is not a research
archive: it does not contain the reverse-engineering notes, the Ghidra project,
diffing/build-verification tooling, or any of the process used to recover this
source. What's here is meant to compile into a working `Creatures.exe` and to be
read as an implementation in its own right.

The [game systems guide](docs/README.md) explains how the simulation, creatures,
brains, chemistry, world objects, CAOS and kits, persistence, rendering, and
sound fit together. It describes the recovered mechanisms for people who want
to understand or extend the game without starting from the source tree.

## What "clean room" means here

This was not a formal two-team clean-room process where one side never saw the
original code. The disassembly and decompiled output of the CE binary were
read and studied extensively -- that reading is how the behavior described
below was recovered in the first place, and the source and commit history in
the private research repository this was curated from show that work directly.

What "clean room" means here instead: the C++ in `src/` was not produced by
copying machine code or pasting decompiler pseudo-C into this tree. It's an
independent reimplementation, written from the *understanding* that reading
gained -- what each function does, what data it reads and writes, what
protocol it speaks -- not a transcription of it. Where a type, a struct
layout, or a numeric constant could be pinned down from evidence (an
instruction operand, an on-disk file format, a wire protocol observed live),
it's implemented to match; where it couldn't, this project says so rather
than guessing.

This work was carried out in Australia, where analysing a computer program by
disassembly or decompilation -- including reproducing parts of it in the
process -- for research, study, or interoperability purposes is lawful under
the Copyright Act 1968 (Cth). That's a statement of the general legal
landscape this project relies on, not legal advice about this repository
specifically; if you're relying on the same basis elsewhere, get your own
advice for your own jurisdiction.

A handful of small resource images under `src/resources/images/` (an app icon,
a cursor, and two toolbar strip bitmaps) are shipped as part of the build. No
other original game asset -- sounds, genetics, sprites, palettes, body data --
is included anywhere in this repository. **You need your own legally obtained
copy of Creatures 1 to get game content to point this build at**; this project
gives you an executable, not the game's data.

## Status

This is a work in progress, not a finished, drop-in replacement. Large parts of
the simulation (creature biochemistry, the brain/lobe network, world scripting,
the embedded-kit protocol, sound) are implemented and have been run against the
original engine's behavior; other corners are incomplete or approximate. Expect
rough edges, and expect this README to undersell or oversell status at any
given moment -- check the source and the issue tracker before relying on a
claim here.

## Performance

This isn't a byte-for-byte recompile with the same runtime cost as the
original -- it's a genuine reimplementation, and it measurably shows.
Running the same world tick loop side by side with the original
CE `Creatures.exe`, on the same machine, gave:

| | measured pace |
|---|---|
| Original CE `Creatures.exe` | ~90.2 ms/tick (~11.1 ticks/sec) |
| This project's engine, unthrottled | ~12.3 ms/tick (~81 ticks/sec) |

That's roughly **7x lighter per tick** -- and it isn't a benchmark
artifact. The original engine's own code requests an unthrottled timer
by default (an unset update interval floors to 1ms in both the
original and this project alike); the original never actually reaches
that rate because a single tick's own workload -- period-appropriate
rendering, GDI, MFC overhead -- is itself the bottleneck. This engine
computes the same tick fast enough that it isn't. The simulation is no
longer coupled to hardware that stopped shipping decades ago: on a
modern machine, this project's world tick is no longer CPU-bound the
way the original always was.

Gameplay pace defaults to the original's authentic ~11 ticks/sec
regardless -- age, incubation, and conception are all gated by tick
count, so running unthrottled changes game balance, not just
performance. The headroom is real and measured; the default keeps
gameplay matching what the original ever actually delivered to a
player.

## Building

You need:

- **Windows**, and **Visual Studio 2022** (or a compatible MSVC toolset) with:
  - the "Desktop development with C++" workload, and
  - the "MFC and ATL support (x86)" individual component -- this project links
    MFC dynamically (`/D_AFXDLL`) and will not build without it.
- **CMake 3.20+**.

This is an MSVC/MFC/Win32 project specifically, not a portable one: MFC and ATL
are MSVC-only, and the recovered layouts and calling conventions assume the
32-bit x86 ABI throughout. There is no other supported toolchain or target
architecture, and CMake will refuse to configure with anything else.

```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

The resulting `Creatures.exe` lands under `build/Release/` (or `build/Debug/`
for a debug configuration). Run it beside your own copy of the original game's
data files (or a fresh `World.sfc`) the way you would the original executable.

## Layout

```
src/c1/
  application/    Document/app-state model, startup and shutdown sequencing
  archive/        The .sfc/.gen/.exp binary archive format
  biochemistry/   Chemical reaction simulation
  brain/          Lobes, neurons, connection rules -- the neural network
  common/         Shared small utilities
  creatures/      Creature, genome, and life-stage/instinct logic
  display/        Rendering
  objects/        The base object model (Entity/Object/SimpleObject/...)
  platform/       The Win32/MFC/COM/DirectSound concrete boundary
  resources/      The .rc resource script and its dialogs/menus/strings/images
  scripting/      CAOS macro interpreter, the DDE/pipe transport
  sound/          The sound-cache/mixer policy layer
  ui/             Views, toolbars, dialogs
  world/          Map, room, and world-tick runtime
include/          A small number of cross-cutting declarations more than one
                  subsystem depends on (a couple of shared data tables, and
                  the module-global variables genuinely referenced from more
                  than one translation unit)

The `c1/` nesting under `src/` is deliberate, not an accident of how this repo
was assembled: it leaves room for a `src/c2/` or `src/c3/` clean-room lane
later without reshuffling this one.
```

`platform/` is the only layer that talks to real Win32/MFC/COM/DirectSound
APIs. Everything else is written against abstract host interfaces, so the
simulation and protocol logic has no hard dependency on the concrete platform
code beyond that boundary.

## License

GPLv3. See [LICENSE](LICENSE).
