# Score Kit ("Performance Kit"): the 1996 original

How the original `Score Kit.exe` behaves, from its disassembly and from
running it in the lab. Addresses are in that binary. This build follows it
except where a numbered bug below is fixed (marked "Fix (bug N)" in the
source), and it leaves out the cover page and all sound.

## Identity

| Item | Value |
| --- | --- |
| OLE server ProgID | `Score.OLE`, CLSID `{3DC4BDA1-B95B-11CF-BBF2-0020AF71E433}` |
| Tool slot | 8 (`Tool8` = `Score.OLE|Performance Kit|Scoring data|8`) |
| Window title | "Performance Kit" (string 101), " - Paused" appended while paused (108) |
| Settings | HKCU `SOFTWARE\Gameware Development\Creatures 1\Score Kit\1.0`: `Location` (8-byte {left, top}, default 256,128), `On Top`, `Page` |

## Pages

- Cover (dialog 140): `Score.bmp`. **Not in this build.**
- Score (dialog 138, "Performance page"): four counters (Hatchery Eggs,
  Natural Eggs, Previous Norns, Current Norns), a picture panel with a
  scrollbar, Breeders Score, Elapsed Time and Close.

## Game traffic

One query holder for the kit's lifetime. A refresh (`RefreshDisplay
@ 0x004084e0`) sends:

1. `inst,dde: putv scor 0,dde: putv scor 1,dde: putv scor 2,dde: putv scor 3,dde: putv scor 4,endm`
   (reply `h|n|d|l|p|`: hatchery eggs used, natural eggs laid, dead norns,
   living norns, the game's running score);
2. `dde: putv hour,endm`, then 3. `dde: putv mins,endm`.

Quitting sends `inst,app: quit 8,endm` on a fresh holder.

When it connects (`InitializeDdeConnection @ 0x00401fa0`), and on control
states 6 and 7, the shared kit framework also asks for the selected
creature's owner and name (`dde: putv ownr`, `dde: getb cnam`), showing
"There is no subject" (retry/cancel) when none is selected. The Score Kit
never uses either. At start-up a flag suppresses it, and control states 6
and 7 were not seen in the lab.

Game to kit: kind 1 code 4 (sent to slot 8 by `NotifyDDEScoreChanged`,
`SFCDoc::UpdateWorld`, and the creature load/removal paths; the payload is
the address of a string, meaningless) forces a refresh
(`CScoreSheet::ForceScorePageRefresh @ 0x004031c0`). Control state 8
closes, 9 toggles pause.

## Timing

A 1-second page timer (`CScorePage::OnTimer @ 0x00407e90`) blinks the colon
in the elapsed-time box (`Time.spr` frames 0/1) and refreshes every 50th
tick, resetting the panel's scroll position each time. Paused, neither
happens.

## Art (read from the game's Main Directory)

| File | Use |
| --- | --- |
| `AllNumbers.spr` | digits 0-9 and a blank, 13x20, drawn 10 px apart |
| `Score.spr` | panel icons: hatchery egg, natural egg, crossed-out norn, norn |
| `Time.spr` | blank and colon |
| `Scorebgd.bmp` | counter backdrop |
| `Brdscore.bmp` | Breeders Score and Elapsed Time backdrop |
| `palette.dta` (Palette Directory) | the game palette the sprites index |

The `.spr` files are the kit framework's own format, not the game's:
an optional repeated header {u16 frame count, u32 data size}, then per frame
{u32 row stride, u32 height, u16 width} and bottom-up rows of palette
indices (`CPhasedSprite::SerializeArchive @ 0x00404610`).

The panel is a 700-pixel-wide surface filled with palette index 0xd6. Each
of the four counters draws that many icons of its kind: rows 50 px apart,
icons 30 px apart, at most 20 per row.

Breeders Score is `natural eggs * 256 + running score`, capped at 99999
(`ClampScoreForDisplay @ 0x00408a60`), drawn as eight digits. It is not the
game's own "Score"; the weighting of eggs looks deliberate, and is kept.
Elapsed Time is five hour digits, the colon, and two minute digits.

## Sound

A sound player (`ScoreSoundManager`, DirectSound, `Sounds\####.wav`) starts
a looping `kitp` sound at launch. **Not in this build.**

## Bugs in the original

1. The label statics are too narrow and word-wrap, so "Hatchery Eggs" and
   "Previous Norns" show as "Hatchery" and "Previous". Seen in the lab.
2. Every refresh scrolls the panel back to the start, and the scroll range
   is the whole 700-pixel surface, so it scrolls a box-width past the last
   icon.
3. At most 20 icons per row; the rest are silently not drawn.
4. The panel is cleared only when the number of living norns falls, so
   other rows keep stale icons when their counts drop.
5. Counters show only the last four digits of larger values (and Breeders
   Score the last eight).
6. Elapsed Time draws five hour digits, the fifth partly off the left edge
   of its box.
7. The figures refresh every 50 seconds (plus on the game's change
   notices), so Elapsed Time lags by up to 50 seconds.
8. Settings are never kept. The registry handler needs an HKLM
   `Score Kit` key that installs do not create, and saving happens only when
   the window is destroyed, which does not happen when the game terminates
   the kit on `app: quit` (as LibreCreatures does under Wine). Seen in the
   lab: closing on the Performance page left `Page` at 0.
9. A missing art file gives "Can not create data file." (for files it only
   reads), naming no file, and leaves the page half-built.
10. The framework pauses on keyboard activation and can fall out of step
    after a minimise (as the Observation Kit's, bugs 5 and 16 there).
11. The palette buffer is allocated two bytes short of what the loader
    writes, and setup failures leak it.
12. "Always on top" can only be set in the registry. The kit has a menu for
    it (menu 143, "Keep window always on top"), but nothing loads it.
