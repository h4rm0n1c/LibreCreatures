# Object Injector: the original (Injector Kit 2.0)

How the original `Injector.exe` behaves, from its disassembly and from
running it in the lab. It is the "Injector Kit 2.0" rebuild of the 1996
Object Injector (its PDB path: `...\Tools\New Injector Kit\Release`).
Addresses are in that binary. This build keeps its pages, protocol, files
and settings, lays the pages out in code so they grow with the window, and
fixes the numbered bugs below (marked "Fix (bug N)" in the source). No
cover page and no sound -- except in the classic skin (the original beside
it as `Injector.old`, and `jigsaw.bmp` in the game's folder).  That is a
separate set of pages, the 2.0 kit's own on its templates (dialogs 150 and
151; `injector_classic.cpp`), sharing only the data and actions with this
build's pages, in the 2.0 kit's window: the `jigsaw.bmp` cover, menu 130
(File, Window, Advanced, Help, with "Mute Ambient Sound" added to Window and
saved as `Mute Ambient`), the Keep on top, Hide (here it minimises) and
Close row, and the looping `kits` sound at -10 dB.  COBs: the plain COB list
(one column as wide as the list, so it never scrolls sideways as the
original's did), the picture on `Alima.bmp` as CDibView drew it, the
description with its warnings, Inject, Remove, Refresh, the quantity (its
label moved clear of Refresh, which covered it), Browse and the folder.
Analysis: the 2.0 tree with the original's own pictures (bitmap 200) --
General Information, Scripts ("<2 10 12> Activate 1"), Chemicals Affected
(the harmful ones in red) and Warnings (DANGER in bold).  Both skins say
"No description available." for a COB without one, as the original did.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `ObjectInjector.OLE`, CLSID `{B07C9809-2CE6-11D0-AB39-0020AF71E433}` |
| Tool slot | 7 ("Injector Kit", "Inject and remove agents": strings 504, 505) |
| Window title | "Injector Kit - " + the creature's name, or "No Subject" (500, 502) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures\Injector Kit\2.0`: `CobFolder`, `Keep on top`, `Hide`, `IgnoreAmount`, `AllowWithoutSubject`, `Location`, `Page` |

This build adds `Size` and leaves out `Hide`.

## COB files

`*.cob` in the COB folder (the game's folder until one is chosen), read by
LoadInjectorCobFile @ 0x00408370; the format is in
`c1kitlib/include/c1kit/cob.hpp`. Each has scripts to install (`scrp f g s
e`), scripts that make the object (`inst,new: ...`), a picture, a name, a
description, how many are left (255: unlimited) and an optional expiry date.
`<name>.rcb` beside a COB is its removal: a COB whose second list removes it.

## Pages

- **COBs** (CAgentsPage, dialog 150): Find, the list, the picture, the
  description, Inject, Remove, Refresh, the quantity left, Browse (the COB
  folder). Menu 130 had Set COB Folder, Refresh, Keep on Top, Hide,
  Ignore Amount Remaining and Allow Without Active Creature. *This build:*
  the same controls, with the two Advanced items as ticks on the page, Keep
  on Top on the system menu, and the warnings under the description.
- **Analysis** (CAnalysisPage, 151): Find, the list, and a tree: General
  Information, Scripts (each by its event: InjectorEventCodeToName
  @ 0x00408b90), Chemicals Affected, Warnings. *This build:* the same, with
  classifiers named from the game's `ClassifierNames.txt`, each script's
  commands under it, and how it would be removed.

## Injecting (InjectSelectedCob @ 0x00405e60)

Not when expired (CCobObject::IsExpired @ 0x00408100: good through its
expiry day). If an inject script names `norn`, a creature must be selected
(`dde: putv norn`) unless Allow Without Active Creature is on. Then every
install script, and one inject script (mode 0: from the last back, one per
injection) or all of them, each on a new scheduler holder with ExecuteMacro;
the count goes down unless Ignore Amount Remaining is on.

## Removing (RemoveSelectedCob @ 0x00406220)

Its `.rcb` if there is one; otherwise, after a warning, a removal generated
from its own scripts (BuildCobInjectionScript @ 0x0040a4c0): kill every
object it makes and `scrx` every script it installs. For the Albian Carrot
Beetle that is the same commands as its own `.rcb`.

## Bugs in the original

1. A COB with none left can still be injected: the count test accepts 0,
   although the page shows it as used up.

The counts are kept only while the kit runs, and are read afresh from the
files on Refresh; this build does the same (it does not write your COB
files).
