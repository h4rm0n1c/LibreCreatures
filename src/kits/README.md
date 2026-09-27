# Kits

Rebuilt Creatures 1 kits: the helper programs the game opens from its Tools
menu and talks to over OLE automation.

| Directory | What it is |
| --- | --- |
| `c1kitlib/` | `c1kitlib.dll`, the data side every kit shares: the game connection (the SFC.OLE macro calls), the kit's own OLE server and `Communicate` entry point, launch arguments, reply parsing and settings. No windows, drawing or sound. |
| `shell/` | The kit shell, compiled into every kit: the MFC application, the connected property-sheet window (resizable), bitmaps, and control layout for pages. |
| `hatchery/` | The Hatchery (Tools slot 0), `Hatchery.exe`: the nest's six eggs with their sex and parents; double-click or "Hatch this egg" sends the 1996 script; refills the nest when it is empty instead of asking for an Egg Disk. |
| `health/` | The Health Kit (Tools slot 3), `Health Kit.exe`: labelled vital signs (life force, temperature, breathing, energy stores), every drive, each lobe's activity, and the Doctor's page shop, whose stock it keeps in `Health` (the 1996 format). |
| `injector/` | The Object Injector (Injector Kit 2.0, Tools slot 7), `Injector.exe`: the COBs in a folder, to inject or remove (by their `.rcb`, or a generated removal), and an analysis of each: its scripts by event, the chemicals it affects and its warnings. |
| `observation/` | The Observation Kit (Tools slot 6), `observation.exe`. |
| `biochem/` | The Biochemistry Kit (v1.2, Tools slot 1), `BiochemKit.exe`: up to 32 chemicals graphed with saved sets (`biochem_saved.txt`), injections once or repeated, and the chemical names (renaming writes `allchemicals.str`). |
| `breeder/` | The Breeder's Kit (Tools slot 5), `Breeder's Kit.exe`: the sex hormones for the creature's sex as labelled bars and a graph, pregnancy from the game's overview, and the Aphrodisiac shop, whose stock it keeps in `Aphro` (the 1996 format). |
| `funeral/` | The Funeral Kit (Tools slot 9), `Funeral Kit.exe`: a memorial page for each creature whose death the game reports, with its photographs from the Owner's Kit album and an epitaph, and a graveyard of the headstones made for them. Reads `The Register` and the albums; keeps its graves in `Funeral Kit Graves` beside the world. |
| `owner/` | The Owner's Kit (Tools slot 2), `Owner's Kit.exe`: register a birth, a photo album, the birth certificate. Keeps `The Register` and `<moniker>.Photo Album` beside the world, in the 1996 format, so the original kits read them too. |
| `science/` | The Science Kit (Tools slot 4), `Science Kit.exe`: chemical levels over time (up to 16 at once, with themes), the genome as a property list read from the genome file, a live brain map with every lobe outlined and each neuron named, the decision lobe, and medicine injections. Reads `allchemicals.str`, `decision.str`, `injections.str` and `themes.str`; saves themes to `Science Kit Themes`. |
| `score/` | The Score Kit, titled "Performance Kit" (Tools slot 8), `Score Kit.exe`. Reads its art (`AllNumbers.spr`, `Score.spr`, `Time.spr`, `Scorebgd.bmp`, `Brdscore.bmp`) from the game's main directory. |

Each kit keeps the original's OLE ProgID, CLSID and Tools slot, so the game
opens it in the original's place. Run a kit once on its own (no arguments)
to register it; it writes its server registration and its `Tool<N>` menu
entry, then exits.

A kit's `ORIGINAL.md` describes how the 1996 program behaved and numbers its
bugs. The rebuild keeps the original's protocol traffic, registry settings
and look, and fixes those bugs; each fix is marked `Fix (bug N)` in the
source.

Two things are left out of the kits' own interface on purpose: cover pages
(the kits open on their first working page) and sound.  Several originals
played WAV effects and looping ambience, which players found maddening.

## Art, and the classic look

The kits carry none of the original kits' pictures.  They do carry the
originals' icons, as the Community Edition does -- small, stock interface
pieces like the game's own toolbar and cursor.  Every other picture a kit
shows is either drawn by the kit itself (button faces, symbols and marks are
drawn in code) or read, while it runs, from the player's own copy of the
game: the pictures the 1996 kits kept as files in the game's folder (the
album wallpaper, the headstones, the score counters, the egg sprites) are
loaded from there and never copied into this repository.

A player who owns the original kits can have their 1996 look back.  Rename
each original kit beside the new one to `<name>.old` (for example
`Science Kit.old` next to `Science Kit.exe`) -- the 1996 release, the later
one and GOG's are all accepted.  A kit that finds its `.old`, and finds in
it the pictures its classic pages use, opens that file as data (nothing in
it runs), reads the 1996 art from it and wears the classic look: the same
working kit, with the 1996 pictures and page layouts, cover pages, and
sound (with a setting to mute the continuous ambience).  Without the `.old`,
or if it lacks what the kit needs, the kit keeps its own interface.  There
is no other switch.

So the line the kits hold is: none of the original kits' pictures is copied
into them or into this repository; those are only ever loaded at run time
from files the player already has.  The 1996 dialog layouts (the positions and sizes of a
page's controls) are carried in the kits, as the structure a classic page is
built on; they hold no artwork.

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
