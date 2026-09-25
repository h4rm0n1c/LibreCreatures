# Funeral Kit: the 1996 original

How the original `Funeral Kit.exe` behaves, from its disassembly and from
running it in the lab. Addresses are in that binary. This build follows it
except where a numbered bug below is fixed (marked "Fix (bug N)" in the
source) and where noted, and it leaves out the cover page and all sound.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `Funeral.OLE`, CLSID `{D3BCF121-A4D8-11CF-BBF2-0020AF71E433}` |
| Tool slot | 9 ("Creature Graveyard", help "Pay your respects": strings 105 and 107) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures 1\Funeral Kit\1.0`: `Location`, `On Top`, `Page` |

## Deaths

The game reports a death as an integer message (Communicate kind 1, code 4)
whose payload is the creature's moniker id; the moniker string, as the
Register and the album file names spell it, is that id in lower-case hex. It
sends one when a dead creature's pane on the event bar is clicked (launching
the kit first if it is not running), and queues one for each corpse that
leaves the event bar, sent when the kit next connects.

For each death the kit reads `The Register` afresh
(LoadRegisterPhotoCollection @ 0x00409b20) and adds a page whose tab is the
creature's name. A creature the Register does not know gets an unmarked
grave instead, tabbed "No Record" (string 104).

## Pages

- Cover (dialog 140). **Not in this build.**
- A dead creature (CFuneralSheet, dialog 137): `GRAVE.bmp` with the
  creature's photograph from `<moniker>.Photo Album` in its frame at
  (59, 28) (RenderSelectedPhotoItem @ 0x0040a2b0), or `UNKNOWN.bmp`, a
  silhouette, when it has none (RefreshPhotoAlbumView @ 0x00409660); the
  life span on the plinth (FormatPhotoDateRange @ 0x00409e10:
  "21:16 Sep 15 to 00:56 Sep 26 2026", the birth time with its year cut off,
  "to", and the time the kit was told of the death); PREV/NEXT; Make
  Headstone; Close.
- GraveYard (CGravePage, dialog 182): `funeral.bmp`, a headstone, with a
  name, life span and epitaph; PREVA/NEXTA walk the graves.
- UnMarked Grave (CUnMarkedPage, dialog 183): `UNKNOWN.bmp` and Close.

The original's epitaph entry was a multi-line edit (id 32772) it created
over the picture. This build puts an epitaph box on the plinth of each dead
creature's page (the template's 1067) instead.

## Files (in the world's folder: the per-user "Main Directory")

- Reads the Owner's Kit's `The Register` and `<moniker>.Photo Album` (see
  `../owner/ORIGINAL.md`).
- `The Graveyard`: the dead creatures' Register records (uint16 count, ten
  CStrings each).
- `Album`: a uint32 count, that many uint32s, then that many CStrings (the
  graves' epitaphs and whether they had photographs, as far as the code
  shows).

**This build leaves both alone** and keeps `Funeral Kit Graves`: a uint16
count, then ten CStrings per grave: moniker, name, father's and mother's
monikers, birth time and birthplace (copied from the Register), death time
(`%H:%M %b %d %Y`), epitaph, `1` once a headstone has been made, and one
unused. Its format is in `c1kitlib/include/c1kit/funeral_files.hpp`.
Graves with no headstone yet get their page back when the kit starts; the
graveyard shows those with one. Unmarked graves are not kept.

## Sound

A sound player like the other kits'. **Not in this build.**

## Bugs in the original

1. MFC's default stacked tabs: with several dead creatures the tabs stack in
   rows that swap places when clicked.
2. Its files are written only when it shuts down. When the game terminates
   the kit on `app: quit` (as LibreCreatures does under Wine) that never
   happens. Seen in the lab: `The Graveyard` was written with a count of 0,
   and `Album` not at all.
3. Make Headstone has no visible effect. Seen in the lab.
4. A missing or unreadable album is reported as a nameless
   " was not found.". Seen in the lab.
5. The life span always drops the birth year, even when the creature was
   born in an earlier year than it died.
6. "Always on top" can only be set in the registry; nothing loads the kit's
   menu for it (menu 143).
