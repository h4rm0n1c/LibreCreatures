# Kits

Rebuilt Creatures 1 kits: the helper programs the game opens from its Tools
menu and talks to over OLE automation.

| Directory | What it is |
| --- | --- |
| `c1kitlib/` | `c1kitlib.dll`, the data side every kit shares: the game connection (the SFC.OLE macro calls), the kit's own OLE server and `Communicate` entry point, launch arguments, reply parsing and settings. No windows, drawing or sound. |
| `shell/` | The kit shell, compiled into every kit: the MFC application, the connected property-sheet window (resizable), bitmaps, and control layout for pages. |
| `observation/` | The Observation Kit (Tools slot 6), `observation.exe`. |
| `owner/` | The Owner's Kit (Tools slot 2), `Owner's Kit.exe`: register a birth, a photo album, the birth certificate. Keeps `The Register` and `<moniker>.Photo Album` beside the world, in the 1996 format, so the original kits read them too. |
| `score/` | The Score Kit, titled "Performance Kit" (Tools slot 8), `Score Kit.exe`. Reads its art (`AllNumbers.spr`, `Score.spr`, `Time.spr`, `Scorebgd.bmp`, `Brdscore.bmp`) from the game's main directory. |

Each kit keeps the original's OLE ProgID, CLSID and Tools slot, so the game
opens it in the original's place. Run a kit once on its own (no arguments)
to register it; it writes its server registration and its `Tool<N>` menu
entry, then exits.

A kit's `ORIGINAL.md` describes how the 1996 program behaved and numbers its
bugs. The rebuild keeps the original's protocol traffic, registry settings
and look, and fixes those bugs; each fix is marked `Fix (bug N)` in the
source.

Two things are left out of every kit on purpose: cover pages (the kits open
on their first working page) and sound.  Several originals played WAV
effects and looping ambience, which players found maddening; the rebuilt
kits are silent.

## Building

CMake builds `c1kitlib.dll` and each kit with the game (see
[`docs/building.md`](../../docs/building.md)).  Each kit installs under the
original's file name, so copying the package into an original installation
replaces the 1996 kits' files (keep copies if you want them).

## Tests

The protocol, reply parsing, macro sequencing and the Observation Kit's alert
decisions are plain C++ and build with any compiler:

```sh
g++ -std=c++17 -D'__declspec(x)=' -Ic1kitlib/include \
    c1kitlib/tests/c1kit_core_test.cpp -o c1kit_core_test && ./c1kit_core_test
g++ -std=c++17 -D'__declspec(x)=' -Iobservation/src -Ic1kitlib/include \
    observation/tests/creature_monitor_test.cpp -o monitor_test && ./monitor_test
```
