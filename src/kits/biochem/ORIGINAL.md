# Biochemistry Kit: the original (v1.2)

How the original `BiochemKit.exe` (v1.2) behaves, from its disassembly and
from running it in the lab. Addresses are in that binary. This build keeps
its pages, protocol, files and settings, lays the pages out in code so they
grow with the window, and fixes the numbered bugs below (marked "Fix (bug
N)" in the source). No cover page, no syringe animation and no sound.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `BiochemKit.OLE`, CLSID `{A3F2B711-4E82-4D1A-9C63-7B8E1D5F3A20}` |
| Tool slot | 1 ("Biochemistry Kit", "Biochemistry tools": strings 503, 504) |
| Window title | "Biochemistry Kit - " + the creature (500) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures\Biochemistry Kit\1.2` |

## Pages

- **Biochemistry** (CMonitorPage, dialog 150): Filter, Chemical ("Name (n)"),
  Add, Remove, Clear; Saved sets with Load, Save and Delete
  (`biochem_saved.txt` beside the kit: `name=1,2,3` lines,
  LoadSavedChemicalGroups @ 0x004045b0, SaveChemicalGroups @ 0x00405d90);
  the chemicals followed with their levels; a graph sampled every second
  (`inst,dde: putv chem n,...,endm`) with a tooltip of the levels under the
  pointer. *This build:* the same, drawn by the graph the Science Kit uses.
- **Injections** (CInjectPage, 151): Chemical and Filter, the dosage (a
  slider and an amount), Inject (`inst,chem n amount,endm`), and a repeat
  every n seconds, n times (0: until stopped), stopped by changing the
  chemical; a syringe animation. *This build:* the same without the
  animation.
- **Chemical Names** (CChemicalsPage, 152): every chemical by number with
  Search; Rename, and Save, which writes `allchemicals.str` (every kit's
  names; SaveChemicalNames @ 0x00409580). *This build:* the same, asking
  before it writes.

## Bugs in the original

1. The graph has 32 channels (CMonitorChannelRecord[32]) and nothing stops a
   33rd being added; every sample then writes past them
   (UpdateGraphSamples @ 0x00406070).
2. Its history holds 2048 samples (about 34 minutes) and then stops
   recording.
