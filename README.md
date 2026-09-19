# LibreCreatures

LibreCreatures is an independent, GPL-licensed reimplementation of the
**Creatures 1** engine. It targets the 32-bit Windows edition of the game and
aims to make its simulation, tools, and file formats usable on a current
system.

The project contains the engine and the small resources needed to build it. It
does not include the original game's galleries, sounds, genomes, body data, or
other content. To run a world, supply data from a legally obtained copy of
Creatures 1. The executable and the data are separate parts of the installation.

The [game systems guide](docs/README.md) explains the simulation in practical
terms: world ticks, creatures, brains, chemistry, objects, CAOS and kits,
persistence, rendering, and sound. The source is organised around the same
boundaries, so the guide is a useful starting point before changing code.

## Status

This is a work in progress. The main simulation, creature systems, CAOS
interface, persistence, rendering, and sound paths are implemented, while some
edge cases and tooling remain under active development. Check the issue tracker
and the source before treating an unfinished area as production-ready.

## Performance

The reimplementation has substantially more headroom than the original CE
engine. A measured world tick took about 12.3 ms here versus 90.2 ms for the
original CE executable on the same machine. The default gameplay timer remains
at the original pace, so the extra headroom does not change creature ageing or
other tick-based rules.

## Building

Builds require Windows, Visual Studio 2022 with the **Desktop development with
C++** workload and **MFC and ATL support (x86)**, plus CMake 3.20 or newer. The
engine targets the 32-bit x86 ABI and does not currently support another
compiler or architecture.

For a local release build, run the same script used by continuous integration:

```powershell
pwsh -File .\scripts\build-release.ps1
```

The script configures CMake, builds `Release`, installs the executable and
public documentation into a staging directory, and creates a ZIP package. See
[`docs/building.md`](docs/building.md) for the directory layout and for the
manual CMake commands.

The executable uses its working directory as the fallback resource directory.
For the simplest setup, unpack the release beside the original game's resource
directories and launch it there. A saved `World.sfc` may also use `Images` and
`Genetics` directories beside the save. No original game data is redistributed
by this project.

## Layout

```text
src/c1/
  application/    Document/app-state model, startup and shutdown sequencing
  archive/        The .sfc/.gen/.exp binary archive format
  biochemistry/   Chemical reaction simulation
  brain/          Lobes, neurons, and connection rules
  creatures/      Creature, genome, and life-stage logic
  display/        Rendering and galleries
  objects/        The base world-object model
  platform/       Win32/MFC/COM/DirectSound boundary
  resources/      The .rc resource script and bundled UI resources
  scripting/      CAOS and the external automation transport
  sound/          Sound-cache and mixer policy
  ui/             Views, toolbars, and dialogs
  world/          Map, rooms, objects, and world ticks
docs/             Human-readable game systems guide
include/          Shared cross-cutting declarations
```

`platform/` is the only layer that talks to real Win32/MFC/COM/DirectSound
APIs. The simulation and protocol code uses host interfaces, which keeps the
game rules separate from the Windows boundary.

## Contributing

Start with the guide page for the system you are changing. Keep resource
loading, world ownership, tick ordering, and the platform boundary explicit.
Small focused changes are easier to test against saved worlds and external
kits.

## License

GPLv3. See [LICENSE](LICENSE).
