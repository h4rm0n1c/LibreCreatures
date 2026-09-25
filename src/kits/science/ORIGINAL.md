# Science Kit: the 1996 original

How the original `Science Kit.exe` behaves, from its disassembly and from
running it in the lab. Addresses are in that binary. This build keeps its
protocol and data files but redesigns every page (below); numbered bugs are
marked "Fix (bug N)" in the source. It leaves out the cover page and all
sound.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `Science.OLE`, CLSID `{D7885C00-9F7D-11CF-BBF2-0020AF71E433}` |
| Tool slot | 4 (`Science.OLE|Science Kit|Advanced monitoring|4`: strings 404, 405) |
| Window title | "Science Kit - " + the creature's name (string 109); paused, "Science Kit...  " + " Paused - " (107, 108) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures 1\Science Kit\1.0`: `Location`, `On Top`, `Page` (the cover is 0), `Theme`, `Chemical` |

This build adds `Size`, `Chemicals` (the chemicals on the graph), `Scanner`
and `Scanner Rule` (the brain view's measure) and `Dose`.

## The subject

As the other kits: `dde: putv ownr` makes the creature selected in the game
the subject (0: none), then `getb monk`, `getb cnam`, `putv gend`, `getb
ctim`. Control state 6 is a new selection.

## Pages (all 252 x 188 dialog units)

- **Cover** (dialog 140). Not in this build.
- **Biochemistry** (CMonitorPage, 133): a graph of four chemicals, each picked
  in a combo box, polled with `inst,dde: putv chem a,...,endm`
  (SubmitSelectedChemicalValues @ 0x004057d0), and themes of four
  (`themes.str`). The chemical names come from `allchemicals.str`, filtered
  into `chemicals.str` (BuildFilteredChemicalIndex @ 0x00404030).
  *This build:* the graph fills the page; every named chemical is in a
  checklist beside it with its level now; up to sixteen are followed at
  once; themes hold any number and are saved to `Science Kit Themes`
  (`themes.str` is read when that file does not exist, and never written).
- **Genetics** (CChromosonePage, 143): gender, species, moniker,
  "fingerprint", the brain's neuron, lobe and dendrite counts, a spinning
  DNA animation, and a "Genetic Breakdown" of one gene type at a time
  (Next), from `dde: gene` (the game's count of the genes switched on for
  the creature's sex and age), with "nucleotides" the count times eight
  (RebuildGeneDisplayMetrics @ 0x0040f8e0).
  *This build:* one list of properties and values, as a file's Details
  are: the creature (with its parents from the genus gene, named from the
  Register), its brain (lobes, neurons and each lobe's size from
  `dde: lobe`; the dendrites its genes allow), and its genome read from
  `<Genetics Directory>\<moniker>.gen`: size, genes, sex-limited and
  mutable genes, and every gene type's count and bytes beside the game's
  count of those switched on now. No animation.
- **Brain scanner** (CScannerPage, 134): the brain report (a mode 2 holder,
  `inst,setv var0 1,endm`: three bytes per active neuron, '0' + grid x,
  grid y and value / 16; ParseScannerDdeResponse @ 0x00401880) painted as
  dots over a bitmap of a human brain.
  *This build:* the brain on its own 64 x 64 grid, zoomed to the lobes,
  each lobe outlined, coloured and named; every neuron shaded by the report,
  which can measure any of its five quantities (firing strength,
  activation, strongest weight, average target weight, average dendrite
  state); pointing at a neuron names its lobe and what it stands for
  (drives, actions, verbs and kinds of object from the game's own tables
  and files); clicking it follows it with `dde: cell` (exact firing
  strength, activation and both dendrite rules' weights).
- **Decisions** (CDecisionPage, 135): a bar per action in `decision.str`
  and for the reward and punishment echo chemicals (54, 55; icons 181, 182),
  from `inst,dde: putv _it_,dde: putv chem 54,dde: putv chem 55,setv var0
  0,reps 16,dde: cell 6 var0 0,addv var0 1,repe,endm`
  (RunScienceExperimentCaosScript @ 0x00407840).
  *This build:* the same query, with each bar's value, the strongest action
  marked, and a choice of which of `cell`'s seven values to show.
- **Injections** (CInjectPage, 144): a syringe animation, a slider, and the
  medicines in `injections.str`, which are chemicals 100 onwards; Go sends
  `inst,chem <100 + n> <slider / 8>,endm`
  (SendChemicalInjectionStep @ 0x0040aaf0).
  *This build:* the medicines, a dose of 1 to 255, Inject, and how much of
  the medicine the creature has now. No animation.

## Files (in the game's installation folder)

`allchemicals.str` (uint16 count, then CStrings), `chemicals.str` and
`injections.str` (CStrings to the end), `decision.str` (uint16 count, then
CStrings), `themes.str` (uint16 count, then a CString name and a uint32 of
four chemical numbers per theme). The formats are in
`c1kitlib/include/c1kit/science_files.hpp`; the genome format is in
`genome.hpp`, and the brain replies in `brain_map.hpp`.

## Sound

A sound player like the other kits'. **Not in this build.**

## Bugs in the original

1. Almost every query creates a new macro holder, and the genetics page's
   (RefreshSelectedCreatureGeneticsContext @ 0x00410130) is never
   destroyed. Seen in the traffic.
2. With no creature selected, "There is no subject" loops as a modal
   retry/cancel box (CScienceKitSheet::SelectDdeOwner @ 0x0040b930).
3. "Always on top" can only be set in the registry; nothing loads the kit's
   menu for it (menu 143).
4. The Genetics page's Species always says "NORN": control 1161 keeps its
   template text, and is neither data-bound nor set.

Not a kit bug: under LibreCreatures, opening the Genetics page crashed the
game. Every CREATEMACRO replaced the game's one macro host, freeing the host
the kit's earlier holders used, and this page creates three holders and then
uses an older one. Fixed in the game.
