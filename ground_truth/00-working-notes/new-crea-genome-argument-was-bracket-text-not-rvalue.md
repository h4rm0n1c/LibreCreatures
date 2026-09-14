# NEW: CREA's genome-name argument used the wrong parser -- resolved

Date: 2026-09-14

## Correcting an earlier wrong conclusion

The immediately-prior ground_truth note in this file's sibling
(`skeleton-and-compound-object-unregister-wrong-container.md`) closed out
a "runaway Creature population" investigation by concluding the cause was
a logic bug in the *save file's own CAOS script* (a "restock the
population if extinct" machine checking for surviving genus=2 creatures
while only ever spawning genus=1). **That conclusion was wrong**, and the
user caught it: the object being investigated is a Grendel Mother, and
checking for surviving genus=2 (Grendel) creatures is exactly correct,
intended behavior for that machine -- not a save-content mismatch. The
real bug was in this port's own `NEW: CREA` argument parsing, which
silently discarded the genome id that machine's script correctly
generated and used, spawning a broken placeholder creature instead of a
real Grendel every time.

## The bug

`Macro`'s `crea` handler (`scripting/macro.cpp`, inside
`execute_new_command`) read its genome-name operand via
`host.parse_rvalue_text(*this)`, which called
`Macro::read_bracketed_text_argument()` -- a parser that requires the
current script byte to be a literal `[`. The Grendel Mother's real script
(`new: gene tokn gren 0 var0,new: crea var0 1,...`) passes the genome id
as `var0`, a plain variable -- never bracketed. `read_bracketed_text_argument`
sees a non-`[` byte, sets `execution_terminated = true`, and returns an
empty string immediately.

The empty string then fed `WindowsNewObjectHost::create_creature`'s
`std::memcpy(&genome_id, request.genome_source_filename.data(), min(4,
size))` -- with `size() == 0`, this copies zero bytes, leaving
`genome_id` at its default-initialized **0**. `Creature::Creature` is
then constructed with genome id 0, which loads as an empty genome (no
genes at all, matching this project's own already-documented "id 0 ==
no file" behavior). Every gene-dependent field on the resulting creature
silently falls back to its default: `creature_genus_selector` stays 0
(`Skeleton::load_genus_identity`'s search loop never runs), giving
`genus = 0 + 1 = 1` (Norn) regardless of what was actually requested, and
`species` (from `genome.sex()`) stays 0 since no sex gene is found
either.

The genome file the Grendel Mother's script actually generates
(`Gren.GEN` cloned via `new: gene tokn gren 0 var0`, verified byte-for-
byte against the real `Gren.GEN` on disk) is **completely correct** --
its first gene's `genus_selector` byte is `1`, giving genus=2 (Grendel)
exactly as intended. It just never reached the creature constructor,
because the id holding its filename (`var0`) was never actually read.

## Confirmed against native, not guessed

Disassembled native's real `Macro::ExecuteNewCommand` (`0x0041d130`) and
found its `'crea'` case (`0x61657263`):

```c
if (new_command_subtype == 0x61657263) {       // 'crea'
    token_or_string_cursor = (char *)ParseRValue(this);   // plain rvalue
    construction_sex = ParseRValue(this);
    ...
    Creature::Creature(creature, token_or_string_cursor, construction_sex);
}
```

Both operands are ordinary `ParseRValue` calls -- the same mechanism
every other `new:` argument uses (see the same function's `cbtn`, `vhcl`,
`simp`, `lift`, `part` cases, all `ParseRValue`-based with no bracket
handling at all). Native never reads a bracketed literal for this
command; a variable, `TOKN <name>`, or any other integer-valued
expression all work identically because they're all just rvalues. The
"bracketed text operand" behavior in this port's prior implementation was
an invented assumption, not something ever confirmed against native.

## Fix

- `NewCreatureRequest::genome_source_filename` (`scripting/macro.hpp`)
  changed from `std::string` to a plain `std::uint32_t` packed id.
- `execute_new_command`'s `crea` case (`scripting/macro.cpp`) now reads
  it with `parse_rvalue(runtime, host)`, matching native's `ParseRValue`
  call exactly.
- Removed `MacroNewObjectHost::parse_rvalue_text` (the interface method)
  and its sole implementation (`WindowsNewObjectHost::parse_rvalue_text`,
  a thin wrapper over `read_bracketed_text_argument()`) -- it had no
  other caller.
- `WindowsNewObjectHost::create_creature` now passes
  `request.genome_source_filename` straight through to `Creature`'s
  constructor; the `memcpy`-from-string extraction is gone since the
  value is already the right type.

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir
  /tmp/bm-cache` -- 139/139 compile, resource compiles, link succeeds.
- Live-tested against the user's crashed save:
  - Before the fix: population grew ~1 broken (genus=1, species=0)
    creature every 2-4 real seconds indefinitely, confirmed over a
    continuous 30-second, zero-input observation window.
  - After the fix: `totl 4 0 0`/`totl 4 2 0` held perfectly stable
    (2 and 0 respectively) across a continuous ~110-second observation
    window with the game running live and ticking -- the runaway growth
    is completely gone.
  - Directly invoked the Grendel Mother's exact script pattern via the
    CAOS pipe (`new: gene tokn gren 0 var0,new: crea var0 <sex>`) twice
    (male and female): both newly-created creatures immediately read
    `gnus == 2` (Grendel) as their own target, with no `targ` needed --
    `NEW: CREA` sets `object_context.target_object` to the freshly
    created creature itself, matching native's
    `(this->object_context).target_object = constructed_creature;`.
    `totl 4 1 0` (Norn) stayed at the real count of 2 throughout;
    `totl 4 2 0` (Grendel) correctly incremented by exactly one per
    spawn.

## Why this matters beyond this one save

This isn't specific to Grendel Mothers or this save file: `NEW: CREA
<variable-or-expression> <sex>` is the standard, general CAOS pattern for
spawning a creature from a programmatically-generated or previously-
stored genome id (as opposed to a hardcoded literal via `TOKN`). Every
COB, agent, or world script using that pattern -- population-restocking
machines, breeding/incubation objects, debug/testing tools -- was
silently producing broken, geneless, mis-classified creatures instead of
real ones. This was very likely a meaningful contributor to the user's
original crash report, independent from (and probably more consequential
than) the earlier, already-fixed registry dangling-pointer leak.
