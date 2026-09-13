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

## What "clean room" means here

The source in `src/` was written from the *recovered behavior* of the CE
binary -- what each function does, what data it reads and writes, what protocol
it speaks -- not transcribed from its disassembly or decompiler output. Where a
type, a struct layout, or a numeric constant could be pinned down from evidence
(an instruction operand, an on-disk file format, a wire protocol observed live),
it's implemented to match; where it couldn't, this project says so rather than
guessing.

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
include/          Small build-support headers the source depends on (scalar
                  type aliases matching the recovered naming convention,
                  cross-cutting declarations)

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
