# Kits missing from BOTH the Tools menu and the toolbar: a real port bug -- found

Date: 2026-09-15

## Retracting the earlier conclusion

An earlier pass this session wrote
`kit-registration-is-an-installer-gap-not-a-port-bug.md`, concluding this
was purely installer-level registry state, not a port bug. **That
conclusion was wrong.** The user's decisive test was swapping an exe onto
a machine where the game had been *legitimately installed* -- the
`Tool0..N` / `NumTools` registry values were already correctly populated
by the real installer -- and kits still did not appear, including their
**toolbar buttons on the tray**, not just their Tools menu entries. That
rules out registry population as the cause entirely: the bug is in how
this port's own `C1MainFrame::create_main_toolbar()` builds the toolbar
after reading an already-correct registry.

## The real bug

`C1MainFrame::create_main_toolbar()` reserves 0x23 (35) toolbar button
slots up front, fills the first 14 (indices 0-0xd) with the fixed,
built-in buttons, then calls `populate_embedded_kit_menu_and_toolbar()`,
which fills slots starting at index 0x0f (15) with one button per
registered kit (via `set_embedded_kit_toolbar_button`). Immediately
after that call, the following ran:

```cpp
populate_embedded_kit_menu_and_toolbar();
for (int button_index = 0x22; button_index >= 0xe; --button_index) {
    main_toolbar_.GetToolBarCtrl().DeleteButton(button_index);
}
```

This unconditionally deletes **every** button from index 0x22 down to
0xe -- which is exactly the range the kit populate step just filled in.
Every kit toolbar button (and the still-unused reserved separators past
it) gets deleted a few lines after being created, every single time,
regardless of the registry's content. This is why kits were missing
their toolbar presence even on a legitimately-installed registry: the
menu item loss (from the separate, since-fixed `read_tool_registry_value`
REG_SZ/REG_BINARY type-check bug) was a real bug, but even with correct
menu entries, no toolbar button ever survived this trim loop.

## Confirmed against native (`src/decompiled/MyToolBar.cpp`)

Native's real trim step (right after
`PopulateEmbeddedKitMenuAndToolbarFromRegistry()`, same function) is not
an unconditional delete -- it's a **scan-then-trim**:

```
toolbar_button_index = 0x22;
last_nonempty_button_index = 0;
do {
    GetButtonInfo(toolbar_button_index, ...);
    if (button_command_id != 0) {
        last_nonempty_button_index = toolbar_button_index;
        if (0x21 < toolbar_button_index) goto LAB_00421d66;   // all slots used, skip trim
        break;
    }
    toolbar_button_index -= 1;
} while (-1 < toolbar_button_index);

button_removal_index = 0x22;
do {
    SendMessageA(hwnd, TB_DELETEBUTTON, button_removal_index, 0);
    button_removal_index -= 1;
} while (last_nonempty_button_index < button_removal_index);
```

It scans backward from slot 0x22 for the **highest-index button that
actually got a real command id** during the kit populate step, then
deletes only the trailing, still-empty separator placeholders above that
point -- never touching any button a kit actually claimed. If a button
at 0x22 itself is populated (all slots used), it skips the trim
entirely.

## Fix

`create_main_toolbar()` now mirrors that scan-then-trim exactly: find
`last_populated_button_index` by scanning 0x22 downto 0 via
`GetToolBarCtrl().GetButton(index, &TBBUTTON)` for the first nonzero
`idCommand`, then delete only from 0x22 down to (but not including)
that index -- skipping the trim entirely if the highest slot (0x22) is
itself populated.

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir
  /tmp/bm-cache` -- 139/139 compile, resource compiles, link succeeds.
- Not yet re-verified with a live click-through against a registered kit
  set (the harness's own registry auto-registration from the earlier,
  now-retracted pass is still valid test-environment hygiene and remains
  in place). The root cause is unambiguous from decompiled native
  comparison: the previous code deleted a fixed range that includes every
  slot the kit populate step could ever fill, with no dependency on
  registry content at all -- this fully explains a legitimately-installed
  registry still producing zero kit toolbar buttons.
