# Observation Kit: the 1996 original

How the original `observation.exe` (1996, MFC 4.0, "Observation Kit -
version 1.2.0") behaves, from its disassembly and from running it side by
side with a faithful rebuild. Addresses below are in that binary. This
build follows it except where a numbered bug below is fixed; the fixes are
marked "Fix (bug N)" in the source. Class names in the disassembly are
partly scrambled between the sheet, the app and the pages, so this is
organised by behaviour; the address is the evidence.

## Identity and launch

| Item | Value | Evidence |
| --- | --- | --- |
| OLE server ProgID | `OVERVIEW.OLE` (dispatch map, string `0x0040a0a4`) | `RegisterOverviewToolOleClass 0x004019f0` |
| Tool slot | 6 | `InitializeOverviewOleRegistration 0x00401f10` |
| `Tool6` value | `wsprintf("%s|%s|%s|%d", "Overview.OLE", str 0x75, str 0x76, 6)` = `Overview.OLE|Observation Kit|Monitor all creatures|6`, REG_SZ, HKCU | same |
| Game automation object | `CreateDispatch("SFC.OLE")` | `ConnectToApplicationOle 0x00403320`, string `0x0040a238` |
| Connect failure | `CException::ReportError`, else `AfxMessageBox("Can not communicate with application")` | same |

`InitInstance 0x00401cd0`:
1. `AfxOleInit` (failure: string 100). `Enable3dControls`.
2. Not `/Embedding` and not `/Automation`: `COleObjectFactory::UpdateRegistryAll`,
   write `Tool6` (above), exit (returns FALSE).
3. Otherwise `COleObjectFactory::RegisterAll` (failure: "Could not register OLE
   factories"), create the 12/6 MS Sans Serif font (`CreateOverviewDefaultFont
   0x00402140`: `CreateFontA(12,6,0,0,400,0,0,0,0,4,0x20,2,0x26,...)`), build the
   sheet (caption string 0x66/0x67), add the Cover page, `Create(NULL,
   0x90CA0000 = WS_POPUP|WS_VISIBLE|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
   WS_EX_DLGMODALFRAME)`.
4. The app constructor sets the current directory to `Main Directory`
   (HKLM `SOFTWARE\Gameware Development\Creatures 1\1.0`, default `C:\`);
   failure: "Unable to locate Creatures' directory. Using current directory
   instead."

## Sheet start-up (`LoadPreferences 0x00403ae0`, sheet `OnTimer 0x00403c80`)

- `OnInitDialog`, then preferences (below), `SetActivePage(Page)`,
  topmost or not per `On Top`, position = `Location` clamped to the screen
  (default 256,128), `SetTimer(3, 30 ms)`.
- Timer 3 fires once: `InitializeSheetPages 0x00403660`: connect to SFC.OLE;
  allocate the command BSTR `SysAllocStringByteLen(NULL, 0x400)` (1024 bytes);
  add the list page ("Details", dialog 130) and the Options page (dialog 143).

## Preferences (`CRegistryHandler`, key `SOFTWARE\Gameware Development\Creatures 1\Overview Kit\1.0`)

Opened by `OpenProductRegistryKey 0x004046e0` from the sheet's `OnCreate`
(`InitializeInstance 0x00403910`, which fails window creation if it fails): it
creates **HKCU** only (`RegCreateKeyExA(0x80000001, ..., KEY_ALL_ACCESS)`).

The game's own key (`...\Creatures 1\1.0`, for `Main Directory` and `Tool6`)
goes through `OpenRegistryKeyFromComponents 0x00404460` instead, which also
opens **HKLM** read-only (`RegOpenKeyExA(0x80000002)`) and counts as open only
if both succeed. The Ghidra comment on that function has the hives reversed.

| Value | Type | Field |
| --- | --- | --- |
| `Location` | 8-byte binary {left, top} | window position, clamped on save and load |
| `On Top` | DWORD | always on top |
| `Alert near Death` | DWORD | |
| `Alert on Pregnancy` | DWORD | |
| `Alert on Birth` | DWORD | |
| `Message Box` | DWORD | alerts show a dialog |
| `Warn Level` | DWORD | life-force percentage for the near-death alert |
| `Page` | DWORD | active sheet page |

Saved on close (`SavePreferences 0x00403980`).

## Kit -> game (SFC.OLE, DISPIDs from the pushed constants)

| Wrapper | DISPID | Method |
| --- | --- | --- |
| `0x004065e0` | 1 | `RequestMacro` |
| `0x00406610` | 2 | `ExecuteMacro` |
| `0x00406640` | 3 | `CreateMacro` |
| `0x00406670` | 4 | `DestroyMacro` |
| `0x004066a0` | 5 | `LoadMacro` |

All: `InvokeHelper(id, DISPATCH_METHOD, VT_BOOL, &ok, "\x4c\x4c", &req, &resp)`.
The VARIANT tags are 2 (`VT_I2`, mode), 3 (`VT_I4`, handle), 8 (`VT_BSTR`,
command buffer).

- **Poll** (`LoadOverviewData 0x004055f0`): `DestroyMacro(old handle)` then
  `CreateMacro(mode 1)` (a new holder every poll); unless quitting,
  `LoadMacro(handle, "inst,dde: getb ovvd,endm")`, then `RequestMacro(handle)`,
  and the reply BSTR replaces the buffer pointer.
- **Quit** (`RequestGameQuit 0x004038a0`): set "quitting"; if connected,
  `CreateMacro(mode 0)` + `ExecuteMacro("inst,app: quit <tool id>,endm")`
  (the holder is kept because quitting is set); if never connected, post
  `WM_COMMAND 0xE141`. Then `CPropertySheet::OnClose`.

## Game -> kit (`Communicate`, handlers at sheet vtable +0xC8 / +0xCC)

- Kind 1 (`0x004037a0`): code 3 (YOUR_ID_IS) stores the tool id byte (used by
  quit); code 4 calls a virtual that does nothing here.
- Kind 2, control state (`HandleHotkeyCommand 0x004037f0`):
  - state 8: post `WM_CLOSE`;
  - state 9: toggle paused: title becomes `"Observation Kit - Paused"`
    (strings 0x67 + 0x74), polling stops; toggled back, the title is restored.
    Ignored while a keyboard-activation pause (below) is active.

## Overview list page (dialog 130, one list control)

- Columns (`InitializeOverviewDisplay 0x00405920`): Name 85, Moniker **0**
  (hidden row key), Sex 30, Age 40, Pregnant 55, Life Force 60, Medical 55 (px;
  strings 0x68, 0x6d, 0x6e, 0x69, 0x6a, 0x6b, 0x6c). Image list from bitmap 142,
  16 px wide. Confirmed against the running original.
- Initial load and list update, then `SetTimer(5, 3000 ms)`. Each tick
  (`OnTimer 0x00405590`), unless paused: poll, update the list, and re-arm any
  near-death alert whose per-creature timer this is.
- Parse: records split on `&`, then 8 `|` fields each (the engine sends 10),
  **at most 13 creatures**. Fields: name, moniker (row key), sex (`1` shows
  "M", else "F"), age, pregnancy (`N/A`, `No`, stage), life force (`NN%`,
  `Dead`), medical, room.
- List update (`UpdateOverviewList 0x00405b00`): update rows found by moniker,
  insert new ones, delete rows no longer reported.
- Alerts (dialog 144 "Alert", modal, only when `Message Box` is on; text is
  name + string):
  - pregnancy stage `1` (and `Alert on Pregnancy`): " has just become
    pregnant." (111), once per creature;
  - stage `7` or `8` (and `Alert on Birth`): " is about to give birth." (112),
    once;
  - stage `2` re-arms both of the above;
  - life force below `Warn Level` (and `Alert near Death`): " has an alarmingly
    low life force." (113), then a per-creature timer (ids from 10, 30000 ms)
    re-arms it.

## Options page (dialog 143)

`DoDataExchange 0x00401250` against dialog 143: 1006 (0x3EE) Always On Top;
1007 (0x3EF) near-death ("Warn if a Norn's Life Force falls below (%):"),
1008 pregnancy, 1009 birth, 1010 message box; 1011 (0x3F3) the spin control;
1012 (0x3F4) the Warn Level edit. The edit accepts digits and backspace only
(`OnChar 0x004067e0`), limit 2, spin range 0..99. Changes are pushed to the
sheet on kill-active; "Always On Top" applies immediately. 1013 About
(dialog 100); 1014 Close pushes the settings and quits the kit
(`SendSelectedCreatureCommandAndQuit 0x00401300`).

## Window behaviour (`WindowProc 0x00403da0`)

Activated by keyboard (`WM_ACTIVATE`, `WA_ACTIVE`): enter the paused title
state, unless the transition target (`+0xf0`, constructor sets 1) is set, in
which case only clear it -- so the first activation after creation or a restore
does not pause. Any click, mouse move, context menu, system command, mouse activation or
click-activation: leave it. System-menu About (`OnSysCommand 0x004024c0`).
Cover page (dialog 129): bitmap 0x88 blitted at (7,7) with its palette.

## Bugs in the original

1. `Main Directory` and the `Tool6` entry go through the both-keys handler
   (`0x00404460`): without the HKLM `Creatures 1\1.0` key the kit neither finds
   the game directory nor registers itself.
2. At most 13 creatures are listed; the rest are silently dropped.
3. Pages are 244x106 dialog units and the window does not resize.
4. A new macro holder is created and destroyed on every 3-second poll.
5. Activating the window from the keyboard pauses polling until the mouse is
   used.
6. Alerts are modal dialogs raised from inside the timer handler, so the list
   stops updating while one is open.
7. The pregnancy alert only fires if a poll lands on stage `1`.
8. A dead creature's life force reads "Dead", which parses as 0, so dead
   creatures raise the near-death alert.
9. Alert state (pregnancy/birth alerted, near-death armed, re-arm timer) is
   indexed by list row, not by creature, so it moves between creatures when
   rows are inserted or deleted.
10. With "Use a Message Box" off, alerts do nothing at all, and it is off by
    default.
11. The row icons (pregnant, birth, near death) only appear when the matching
    alert option is on.
12. The saved `Page` is applied before the list and Options pages exist (they
    are added 30 ms later), so it never restores them.
13. After a `RequestMacro` the reply BSTR becomes the command buffer, and the
    next script is copied into it without a size check.
14. Deleting stale rows skips the row that slides into a deleted row's place,
    so several departing creatures take several polls to disappear.
15. The list page reads its alert settings from the Options page's members
    (`page+0x148`, getters `0x00401520..0x00401550`). Until the Options tab is
    first opened those are its constructor defaults (message box off), so the
    saved alert settings have no effect until then. Confirmed on the running
    original.
16. Restoring a minimised window delivers `WM_ACTIVATE` before the restoring
    `WM_SIZE`; the target was cleared by the minimise, so the activation takes
    the keyboard-pause path (toggling the display back on, title " - Paused")
    and the `WM_SIZE` then finds nothing to undo. Afterwards the title reads
    Paused while polling runs, and because the activation guard (`+0xec`) is
    now set, the game's pause (control state 9) is ignored until the kit's
    own window gets a click or mouse move. Confirmed on the running original (ShowWindow
    minimise/restore); our build matches it step for step.
17. Settings are saved only when the window is destroyed. When the game
    launched the kit itself (as LibreCreatures does under Wine) it
    terminates the kit while handling `app: quit`, so closing the kit from
    its Close button or the game loses every change made in that session.
    (Seen with the faithful rebuild, which saves at the same point; it
    follows for the original from its SavePreferences call site.) This
    build saves before sending the quit.

