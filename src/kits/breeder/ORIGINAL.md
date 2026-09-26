# Breeder's Kit: the 1996 original

How the original `Breeder's Kit.exe` behaves, from its disassembly and from
running it in the lab. Addresses are in that binary. This build keeps its
protocol and shop file but replaces its pictures with labelled readings
(below); numbered bugs are marked "Fix (bug N)" in the source. It leaves out
the cover page and all sound.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `Sex.OLE`, CLSID `{B4A467E1-AF33-11CF-BBF2-0020AF71E433}` |
| Tool slot | 5 (`Sex.OLE|Breeder's Kit|Pregnancy monitoring|5`: strings 116, 117) |
| Window title | "Breeder's Kit - " + the creature's name (string 108); paused, "Breeder's Kit..." + " Paused - " (107, 109) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures 1\Breeder's Kit\1.0`: `Location`, `On Top`, `Page` (the cover is 0) |

This build adds `Size`.

## Pages (all 216 x 212 dialog units)

- **Cover** (dialog 140). Not in this build.
- **Fertility page** (CFertilityPage, class CBreedersKitPage8A, dialog 138):
  `dde: putv gend`, then every second
  `inst,dde: putv chem a,...,endm` for five chemicals
  (RefreshSelectedCreatureBreedingStatus @ 0x00409140): for a female
  oestrogen (63), sex drive (13), progesterone (66), gonadotrophin (65) and
  glycogen (59); for a male testosterone (64) in place of oestrogen and
  glycogen in place of progesterone. It drew three unnumbered gauges ("Sex
  Drive", "Health", "Fertility"), a pregnancy silhouette (`Pregnancy.spr`;
  crossed out for a male), the female or male icon (187, 188) and a trace
  of oestrogen or testosterone (`Fertility.spr`).
  *This build:* the same chemicals as labelled bars with their levels, the
  pregnancy, age and life force from `getb ovvd`, and a graph of all of
  them over the last minutes.
- **Aphrodisiac page** (CAddObjectPage, CBreedersKitPage8E, dialog 142): the
  Health Kit's shop page on the stock file `Aphro` (Tomato, Ugly Tomato).
  *This build:* the shop page the Health Kit shares (every item in a list,
  the selected one drawn large, "Put one in the world").

A third page, CPregnancyPage (CBreedersKitPage8D), is constructed but never
added to the sheet. Its start-up (StartBreedingCycle @ 0x00402820) would
rewrite `Preg_1.spr` and `Pregnancy.spr` in the game's folder.

## The shop file

`Aphro` in the game's installation folder: the same format as the Health
Kit's `Health` (`c1kitlib/include/c1kit/health_files.hpp`); this build writes
it back byte for byte.

## Sound

A sound player like the other kits'. **Not in this build.**

## Bugs in the original

1. The shop's counts are written back only when the Aphrodisiac page saves
   its state as the kit shuts down (CBreedersKitPage8E::SaveShopState
   @ 0x0040ccc0). When the game terminates the kit on `app: quit` (as
   LibreCreatures does under Wine) that never happens, and the items taken
   are not counted. (Follows from the save point and the game's quit
   handling.)
2. With no creature selected, "There is no subject" loops as a modal
   retry/cancel box (CBreedersKitMainFrame::SelectDdeOwner @ 0x004063a0).
3. "Always on top" can only be set in the registry; nothing loads the kit's
   menu for it (menu 143).
