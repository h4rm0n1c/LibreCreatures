# Health Kit: the 1996 original

How the original `Health Kit.exe` behaves, from its disassembly and from
running it in the lab. Addresses are in that binary. This build keeps its
protocol and shop file but replaces its pictures with labelled readings
(below); numbered bugs are marked "Fix (bug N)" in the source. It leaves out
the cover page and all sound.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `Health.OLE`, CLSID `{7CCFFEC1-A43D-11CF-BBF2-0020AF71E433}` |
| Tool slot | 3 (`Health.OLE|Health Kit|Initial monitoring|3`: strings 114, 115) |
| Window title | "Health Kit -  " + the creature's name (string 102); paused, "Health Kit... " + " Paused - " (101, 103) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures 1\Health Kit\1.0`: `Location`, `On Top`, `Page` (the cover is 0) |

This build adds `Size`.

## Pages (all 255 x 191 dialog units)

- **Cover** (dialog 140). Not in this build.
- **Fitness page** (CFitnessPage, 138): `Skeleton.bmp` with a thermometer
  and a heart monitor (`Therm.bmp`, `Hearts.spr`, `Blink.spr`), from
  `inst,dde: putv chem 62,dde: putv chem 59,dde: putv chem 4,dde: putv chem
  5,endm` once a second: carbon dioxide and glycogen set the heartbeat
  (ReadHealthScannerDdeResponse @ 0x0040d9b0), coldness against hotness the
  thermometer. No numbers.
  *This build:* labelled bars: life force, health, age and pregnancy (from
  `getb ovvd`), a cold/hot scale with coldness and hotness, carbon dioxide,
  and the energy stores (glycogen, glucose, starch).
- **Drives and needs** (CStatePage, 139): gauges (`Gauge.spr`) for pain,
  hunger, tiredness ("Exhaustion"), sleepiness and boredom
  (InitializeChemicalControls @ 0x00403bf0); the query asks for all sixteen
  drive chemicals.
  *This build:* a gauge for every drive with its level, and the strongest
  named.
- **Brain activity** (CScannerPage, 134): a cartoon brain (`Lobes.bmp`) whose
  busiest part lights up (`Lobe_<n>.spr`), from the brain report (a mode 2
  holder, `inst,setv var0 1,endm`, which asks for activation but gets
  firing strength: nothing runs a report holder's script) and `dde: lobe`.
  *This build:* a row per lobe: its name, what it does, and the share of its
  neurons firing (every neuron the report lists, level 0 included).
- **Doctor's page** (CAddObjectPage, 142): one shop item at a time in a
  frame (`Shop.bmp`), with arrows, its name, how many are left and what it
  does; the middle button runs the item's CAOS and takes one off
  (SubmitSelectedHealthValue @ 0x00405c80).
  *This build:* every item in a list, the selected one's picture drawn large
  with its details, and "Put one in the world".

## The shop file

`Health` in the game's installation folder (shared by every world): a uint16
count, then per item a uint16 count left, the CAOS that makes it (`inst,new:
simp herb ...,edit`: made and put on the pointer), its picture in the album
photographs' format, a name and a description. The stock is Feverfew,
Morning Glory and Cheese. The format is in
`c1kitlib/include/c1kit/health_files.hpp`; this build writes it back byte for
byte. The Breeder's Kit's `Aphro` has the same format.

## Sound

A sound player like the other kits'. **Not in this build.**

## Bugs in the original

1. The shop's counts are written back only when the Doctor's page releases
   its items as the kit shuts down (InitializeItemResources @ 0x00404ca0).
   When the game terminates the kit on `app: quit` (as LibreCreatures does
   under Wine) that never happens, and the items taken are not counted.
   (Follows from the save point and the game's quit handling.)
2. With no creature selected, "There is no subject" loops as a modal
   retry/cancel box (CHealthSheet::SelectDdeOwner @ 0x00407760).
3. "Always on top" can only be set in the registry; nothing loads the kit's
   menu for it (menu 143).
