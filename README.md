<p align="center">
  <img src="docs/logo.png" width="160" alt="The LibreCreatures logo: a cracked egg in grass">
</p>

# LibreCreatures

LibreCreatures is an independent, GPL-licensed reimplementation of the
**Creatures 1** engine (`Creatures.exe`) and its ten Tools-menu kits, plus
a new Ecology Kit, for Windows. It builds as a 32-bit (x86) program, as the original is, or as a
64-bit (x64) program.

The project contains the engine, the kits and the small resources needed to
build them. It does not include the original game's galleries, sounds,
genomes, body data or other content. To run a world, supply data from a
legally obtained copy of Creatures 1 (the GOG and Steam editions work).

## Documentation

- [Game systems guide](docs/README.md): world ticks, creatures, brains,
  chemistry, objects, CAOS and kits, persistence, rendering and sound. The
  source is organised around the same boundaries.
- [Changes from Creatures 1 Community Edition](docs/changes-from-ce.md):
  every way LibreCreatures behaves differently from CE and the 1996 kits.
- [Bugs in the original game that LibreCreatures fixes](docs/original-bugs.md),
  one numbered row each, and how to switch each fix off.
- [Comparison with the original game](docs/verification.md): the tests that
  compare LibreCreatures with the original executable, and their results.
- [64-bit builds](docs/64-bit.md), [building and packaging](docs/building.md),
  and the [kits](src/kits/README.md).

## Status

The simulation, creature systems, CAOS, the kit protocols (DDE, SFC.OLE and
the kit pipe), persistence, rendering and sound are implemented. The x86
build is the reference: function-level tests run the original executable's
code beside LibreCreatures' and compare the results (`tests/equivalence/`),
and with every switchable fix off, a world that LibreCreatures loads and saves
is the same, byte for byte, as the original's save of it. Some edge
cases and tooling are still being worked on; check the issue tracker before
treating an area as finished.

## Performance

The reimplementation has substantially more headroom than the original CE
engine. A measured world tick took about 12.3 ms here versus 90.2 ms for the
original CE executable on the same machine. The default gameplay timer remains
at the original pace, so the extra headroom does not change creature ageing or
other tick-based rules.

## Fixes, and running as the original

Where the original game has a defect, LibreCreatures fixes it, and almost
every fix can be switched off to get the original behaviour back:

- **Options > LibreFixes** in the game lists the switchable fixes, with
  `librefixes.ini` beside `Creatures.exe` keeping the choices. Some are on
  by default, some off, and some (crash guards, data repairs) always on.
- The environment variable `C1_NATIVE_ROWS` overrides the file for one run
  (`all`, or a comma list of row numbers), which the test tooling uses.
- A build with `-DLC_NATIVE_MODE=ON` compiles every switchable fix out.

The row numbers are those of [docs/original-bugs.md](docs/original-bugs.md).

## Branches

- **`main`**: the engine drawing the world through GDI, as the original does.
- **`neorender`**: the same engine with an SDL3 world view inside the MFC
  shell: zoom, true-colour `.s32` and double-resolution `@2x.s32` art, and a
  fixed-step loop that keeps the 90 ms world tick ([docs/neorender.md](docs/neorender.md)
  on that branch). It needs `SDL3.dll` beside `Creatures.exe` (included in its
  builds, from `third_party/SDL3`). It does not change what a world file
  stores or how the simulation behaves; a world made with it still loads in
  the original game.

Changes are made on `neorender` and copied to `main` when they apply to both.

## Downloads

Tagged releases (`v*`) are built by
[GitHub Actions](.github/workflows/windows-release.yml) as four zips, one for
each branch and architecture:

| Zip | Branch | Architecture |
| --- | --- | --- |
| `LibreCreatures-windows-x86.zip` | `main` | 32-bit |
| `LibreCreatures-windows-x64.zip` | `main` | 64-bit |
| `LibreCreatures-neorender-windows-x86.zip` | `neorender` | 32-bit |
| `LibreCreatures-neorender-windows-x64.zip` | `neorender` | 64-bit |

Each zip has `Creatures.exe`, the kits and `c1kitlib.dll`, all built for that
architecture.
Each one needs the matching (x86 or x64) Microsoft Visual C++ 2015-2022
runtime with MFC. A push to a branch only runs a quick x86 compile check; it
does not publish anything. The first release, v1.0.0-gm, was made before
64-bit support and has the two x86 zips only.

Under Wine, a 64-bit game cannot start kits yet: that needs a 64-bit
`OLEKitProxy.dll` ([docs/64-bit.md](docs/64-bit.md)). Use an x86 zip there.

To install: copy the zip's files into a Creatures 1 installation (keep copies
of the files you replace), then run each kit once on its own so that it
registers itself in the game's Tools menu.

## Building

Builds need Windows, Visual Studio 2022 with the **Desktop development with
C++** workload and **MFC and ATL support** for the architecture you build
(x86, x64 or both), and CMake 3.20 or newer.

```powershell
pwsh -File .\scripts\build-release.ps1              # 32-bit (the default)
pwsh -File .\scripts\build-release.ps1 -Arch x64    # 64-bit
```

The script configures CMake, builds `Release` (the game and every kit),
installs them with the public documentation into a staging directory, and
creates the zip. A 64-bit configure needs `-DLC_ALLOW_X64=ON`, which the
script passes for `-Arch x64`; without it CMake refuses to build anything
but 32-bit. See [docs/building.md](docs/building.md) for the manual CMake
commands and [docs/64-bit.md](docs/64-bit.md) for what changes in a 64-bit
build.

`Creatures.exe` reads the resource directories the original game records in
the registry, and falls back to its working directory. A saved `World.sfc` may
also use the `Images` and `Genetics` directories beside it.

## Layout

```text
src/c1/
  application/    Document/app-state model, startup and shutdown sequencing
  archive/        The .sfc/.gen/.exp binary archive format
  biochemistry/   Chemical reaction simulation
  brain/          Lobes, neurons, and connection rules
  common/         Shared pieces: the fix switches (compat.hpp, fix_catalog),
                  logging, the lab trace, file and time helpers
  creatures/      Creature, genome, skeleton, body, and life-stage logic
  display/        Rendering and galleries
  objects/        The base world-object model
  platform/       Win32/MFC/COM/DirectSound boundary
  resources/      The .rc resource script and bundled UI resources
  scripting/      CAOS, DDE, SFC.OLE and the kit pipe
  sound/          Sound-cache and mixer policy
  ui/             Views, toolbars, and dialogs
  world/          Map, rooms, objects, and world ticks
src/kits/         The kits: c1kitlib.dll, the shared kit shell, and one
                  directory per kit (see src/kits/README.md)
tests/            Portable unit tests
tests/equivalence/  Tests that run the original executable's functions
                  beside LibreCreatures' (MinGW, under Wine)
docs/             The game systems guide and the reference documents above
doco/re-mappings/ Audit records from the reverse-engineering work
include/          Shared data-table declarations
scripts/          The release build script, the .spr to .s32 converter
                  (neorender), and brain and genome study tools
third_party/      Bundled third-party code (SDL3 on neorender, stb)
```

`platform/` is the only layer that talks to real Win32/MFC/COM/DirectSound
APIs. The simulation and protocol code uses host interfaces, which keeps the
game rules separate from the Windows boundary.

## Contributing

Start with the guide page for the system you are changing. Keep resource
loading, world ownership, tick ordering, and the platform boundary explicit.
A change that fixes a defect of the original game gets a row in
[docs/original-bugs.md](docs/original-bugs.md) and, unless it guards against
a crash, a switch (`LC_FIX(<row>)`) that keeps the original behaviour
available. Small focused changes are easier to test against saved worlds and
external kits.

## Attribution

**Creatures** and everything in it — the design, the artwork, the sounds, the
genomes, the name — belong to **Gameware Development Ltd**. The series was
created by Cyberlife Technology, which became Creature Labs; when that company
closed in 2003 its assets and intellectual property passed to Gameware Europe,
Gameware Development's parent. Gameware are the rights holders today.

This project is not affiliated with, endorsed by, or supported by Gameware
Development. It reimplements the engine and kits only. It ships no original
game data, and it needs a legally obtained copy of Creatures 1 to run.

The rebuilt kits follow the same line: apart from small interface pieces
(icons, little button faces and markers), they carry none of the original
kits' pictures, and their optional 1996 look loads those at run time from the
player's own original kits (renamed to `.old`) or the game's folder, never
from copies in this repository. See
[src/kits/README.md](src/kits/README.md#art-and-the-classic-look).

The binary this work was derived from is the **Creatures 1 Community Edition**
maintained by **[lisdude](https://lisdude.com/Creatures/Community/C1/)** — a
series of patches over the last official Creatures 1 release, adding stability
fixes and quality-of-life improvements while staying compatible with the Steam
and GOG editions. The build studied here reports itself as Community Release
10.3, which is the version LibreCreatures matches its behaviour against; a few
later CE fixes are ported too, and credited where they are. Where this
project talks about "native" behaviour, it means that executable.

Without lisdude's work there would have been nothing maintained to study, and
without Gameware's there would have been no Creatures at all.

## License

GPLv3. See [LICENSE](LICENSE).

The GPL covers this project's own source. It makes no claim over Creatures
itself, over Gameware's intellectual property, or over the Community Edition
patches.
