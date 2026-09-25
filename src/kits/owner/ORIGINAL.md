# Owner's Kit: the 1996 original

How the original `Owner's Kit.exe` behaves, from its disassembly and from
running it in the lab. Addresses are in that binary. This build follows it
except where a numbered bug below is fixed (marked "Fix (bug N)" in the
source), and it leaves out the cover page and all sound.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `Owner.OLE`, CLSID `{4388EF01-A35C-11CF-BBF2-0020AF71E433}` |
| Tool slot | 2 (`Tool2` = `Owner.OLE|Owner's kit|Norn details|2`) |
| Window title | "Owner's Kit - " + the creature's name (string 108) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures 1\Owners Kit\1.0`: `Location`, `On Top`, `Page` (the cover is 0), `Photo` (selected photo) |

## Pages

- Cover (dialog 140). **Not in this build.**
- Register the birth (136): the creature's name (editable, at most 10
  characters, string 105), sex, age; the owner's name, address, phone and
  email; Register Birth.
- Photo Album (137): `photograph.bmp` with the selected photo (or
  `Blank.bmp`) in its frame, when it was taken, a caption; Save as, Delete,
  Take photo, Back, Next (bitmap buttons, `<CAPTION>U/D/F`).
- Certificate (141): `birth.bmp` with the name, mother's and father's names
  (from the register, else "Unknown"), and the birth date and time. Present
  only for registered creatures.

## The subject

The kit looks after the creature selected in the game. `dde: putv ownr`
makes it the subject of the queries that follow (0 means none selected),
then `getb cnam`, `getb monk`, `getb data`, `putv gend` (1 male) and
`getb ctim` (" h:mm"). When the selection changes the game sends control
state 6 and the kit takes the new creature. Renaming it with `putb data`
makes the game send 7.

## The creature's record

The game keeps ten strings on each creature and fills in the first six at
birth: moniker, name, father's moniker, mother's moniker, birth time
(`%H:%M %b %d %Y`), birthplace; then the owner's name, address, phone and
email. `dde: getb data` returns them each followed by `|`. Register Birth
sends them back with `dde: putb [f0|f1|...|f9|] data,endm`, which also
renames the creature and teaches it the name.

## Files (in the world's folder: the per-user "Main Directory")

- `The Register`: uint16 count, then the ten strings per registered
  creature (MFC CStrings). Loaded at start-up, written back at shutdown.
- `<moniker>.Photo Album`: uint16 count, then per photo a CString timestamp
  (`%H:%M %d %B %Y`), the picture ({uint32 stride, uint32 height, uint16
  width} and bottom-up palette indices), a caption CString and a second,
  unused CString.

The Funeral Kit reads both.

## Photographs

Take photo sends `inst,dde: panc,dde: pict x|<0x8c>,endm`: `panc` pans the
camera to the creature, and `pict` takes the width (120) and height (140)
as raw bytes, writes the picture to `temp.spr` in the game's directory in
the album's picture format, and replies with its path. The kit loads it into
a new album entry and deletes the file. Save as writes a BMP.

Under LibreCreatures, Take photo failed for this kit (" was not found.")
because the game's `pict` reply lost its last character (`...temp.sp`). That
was a bug in the game, since fixed; the 1996 game counted the path's
terminator.

## Sound

A sound player like the other kits'. **Not in this build.**

## Bugs in the original

1. The tabs do not fit and stack in two rows, which swap places when
   clicked. Seen in the lab.
2. The register is written only when the kit shuts down, and an album only
   when its page is destroyed or the subject changes. When the game
   terminates the kit on `app: quit` (as LibreCreatures does under Wine)
   neither happens, and registrations and photos are lost. (Follows from
   the save points and the game's quit handling.)
3. Errors never name the file (" was not found.").
4. (Not a kit bug: see Photographs above.)
5. With no creature selected, "There is no subject" loops as a modal
   retry/cancel box.
6. Almost every query creates a new macro holder, and few are destroyed.
   Seen in the traffic.
7. The framework pauses on keyboard activation (as the Observation Kit's).
8. The Certificate page is removed and re-added as the subject changes
   between registered and unregistered creatures, so the tabs jump.
9. "Always on top" can only be set in the registry; the kit's menu for it
   (menu 143) is never loaded.
10. A selection change re-reads the moniker three times, each on a new
    holder. Seen in the traffic.
