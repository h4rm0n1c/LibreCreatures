# Hatchery: the 1996 original

How the original `Hatchery.exe` behaves, from its disassembly and from
running it in the lab. Addresses are in that binary. This build keeps its
script and nest state but shows the nest with labels instead of the animated
machine; numbered bugs are marked "Fix (bug N)" in the source. It has no
sound.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `Hatchery.OLE`, CLSID `{F10D5CA1-A8B7-11CF-BBF2-0020AF71E433}` |
| Tool slot | 0 (`Hatchery.OLE|The Hatchery|Hatch a new norn|0`; the name and help are literals in the code, not resources) |
| Window | an MFC document window (CMainFrame, CHatcheryDoc, CHatcheryView) showing `Hatchery\hatchery.bmp`: the incubator machine with six eggs in its nest, fans, lights and a scanner (`Hatchery\*.bmp`) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures 1\Hatchery\1.0`: `Location`, `Eggstra` (the nest) |

This build adds `Size` and `On Top`.

## Eggs

Egg n (1-6) is always a child of the stock genomes `mum<n>.gen` and
`dad<n>.gen` in the Genetics folder, crossed afresh by the game. `Eggstra`
holds six characters, one per egg: `0` male, `1` female, `x` taken.
Pointing at an egg shows its sex (the mouse-move handler at 0x00404b90).
A **double-click** hatches it (WM_LBUTTONDBLCLK: HandleEggSlotClick
@ 0x00403c30), sending on a scheduler holder with ExecuteMacro, for egg 1
(female):

```
inst,sys: wtop,sys: cmra 2223 724,new: simp eggs 8 0 2000 0,pose 3,
setv clas 33882624,setv attr 67,new: gene tokn mum1 tokn dad1 obv0,
setv obv1 2,tick 2400,dde: hatc,mvto 2408 870
```

(`simp eggs 8 <8(n-1)>`, `mum<n>`/`dad<n>`, `obv1` 1 male or 2 female, and
`mvto <2408 + 40(n-1)> 870`.) The egg's character becomes `x`
(ConsumeEggstraSlot @ 0x00404780). During `dde: hatc` the game sends the kit
control state 8 (close), and the kit then sends `inst,app: quit <id>,endm`.

With no eggs left it asks for an "Egg Disk" in drive A: `a:\header.dat`,
then copies new genomes from `a:\eggx\` into the Genetics folder
(ActivateEggSlot @ 0x004048e0, CopyEggSlotGeneticsToUserDirectory
@ 0x004042d0), with the errors "Egg Disk has been write protected", "You
have no more eggs!" and "The egg could not be moved from the disk!".

*This build:* the six eggs drawn from the same pictures, each with its sex
and parents; click to pick, double-click or "Hatch this egg" to hatch; the
same script and `Eggstra` value.

## Bugs in the original

1. Once the six eggs are gone it wants an "Egg Disk" floppy in drive A,
   which nobody has; the Hatchery never gives another egg. This build
   refills the nest with six new eggs (random sexes; the same stock
   parents) when it is empty.
2. It closes after every egg, so each one means reopening it from the Tools
   menu. This build stays open (and ignores the game's close request during
   `dde: hatc`).
