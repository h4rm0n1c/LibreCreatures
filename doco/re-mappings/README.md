# Reverse-engineering mappings

These files record where the clean-room C1 source in `src/c1` came from.
Every function the port translates is keyed to its address in the original
executable, so any piece of the port can be traced back to the native code it
reproduces, and any native function can be looked up to see where the port
implements it (or why it deliberately does not).

**Program:** `Creatures.exe`, the Creatures 1 Community Edition build.
Addresses are virtual addresses in that executable as mapped by its Ghidra
project.

**Evidence snapshot:** most files carry `snapshot_sha256`
`ce33b958a77ecae12ddf5b29f1156a5e02138ec6036a8639b5f5b74bffee70ae`, the hash of
the Ghidra-derived evidence export they were generated from.  Files from the
same snapshot are mutually consistent.

The files contain addresses, symbol names, file paths, counts and review
statuses.  They contain no decompiled code.

## Start here

| file | what it answers |
|---|---|
| [`c1-clean-source-admissions-v1.json`](c1-clean-source-admissions-v1.json) | **The mapping itself.** Each entry pairs a native `address` with the `source_file` and `source_symbol` in `src/c1` that translates it.  739 entries.  `support_sources` lists port files that exist to serve an admitted function without translating a native function of their own. |
| [`c1-port-completeness-v1.md`](c1-port-completeness-v1.md) | A one-page summary of how much of the executable is accounted for, by dimension. |
| [`c1-vtable-wiring-20260919.json`](c1-vtable-wiring-20260919.json) and [`.md`](c1-vtable-wiring-20260919.md) | The native vtables for `Object` and ten derived classes: every slot's function address and Ghidra name.  The companion note records how the port routes those slots. |

Reading an admission entry:

```json
{
  "address": "0043c800",
  "source_file": "creatures/skeleton.cpp",
  "source_symbol": "creatures1::creatures::Skeleton::load_genome"
}
```

means the native function at `0x0043c800` is translated by
`Skeleton::load_genome` in `src/c1/creatures/skeleton.cpp`.

## The rest of the set

Where a JSON has a Markdown companion of the same name, the Markdown renders
it for reading.

| file | purpose |
|---|---|
| `clean-source-emission-manifest-v1` | Address-keyed inventory of every function in the executable, derived from the Ghidra project, with the disposition decided for each (translate, framework/runtime, absorbed helper, and so on). |
| `c1-source-translation-queue-v1` | The work list derived from the manifest: every game-owned function that needs a semantic translation, grouped by destination source file. |
| `c1-source-translation-triage-v1` | Batching and risk profile of that queue. |
| `c1-clean-source-admission-audit-v1` | The gate between the queue and `src/c1`: which queued addresses are admitted, and which remain pending. |
| `c1-translation-checkpoint-coverage-audit-v1` | Cross-checks admissions against the per-batch translation checkpoints. |
| `c1-boundary-contract-audit-v1` | Separates code the game authored from code that arrived through MSVC, STL, CRT, MFC, ATL or Windows imports, so library code is not mistaken for game logic. |
| `c1-library-edge-audit-v1` | Every call from a game function into runtime/SDK code, recorded as a dependency rather than as something to port. |
| `c1-composition-liveness-audit-v1` | Reachability of the port's host implementations: compiling proves types line up, this proves each one is actually constructed. |
| `c1-interface-granularity-audit-v1` | The port's abstract interfaces and how many implementors each has. |
| `c1-entry-composition-reconciliation-v1` | Reconciliation of the executable's entry unit with the port's. |
| `c1-ui-surface-audit-v1` | Menu commands and message-map IDs from the original resources, and whether the port routes each one. |
| `executeinterpreter-coverage-v1.json` | The CAOS interpreter, `Macro::ExecuteInterpreter` @ `0x0041dc40`: all 128 command-token sites, each with its native evidence and the compile-checked port slice that reproduces it, plus the command and prefix route ledgers. |
| `transformer-manifest.json` | The checks and advisories recorded by the source-transformation pipeline for the same snapshot. |

## Known staleness

These are accurate to the snapshot above, with these known exceptions:

- **The admission map has been corrected by hand after the snapshot** for six
  entries whose port functions were renamed or moved:
  `00406d40` -> `Skeleton::get_bounds`;
  `00408080` -> `Skeleton::body_sprites_are_stale`;
  `0042c0b0`, `0042c170`, `0042c230` -> `Vehicle::handle_queued_event_0/1/2`;
  `0042c2c0` -> `Vehicle::complete_floor_arrival`.
  The last four belong to `Vehicle`, not `Lift`: the three event handlers sit
  in Vehicle's vtable (Lift's own vtable replaces all three), and
  `complete_floor_arrival` is a Vehicle method that Lift's tick reuses.
- **The generated reports predate that correction.**  The translation queue
  still groups those four Vehicle functions under `objects/lift.cpp` with
  their old Ghidra names (`Lift::HandleQueuedEventSlot22` and so on), and the
  admission audit still lists the pre-correction symbol names.  They will line
  up when the queue is regenerated from a fresh evidence export of the
  corrected Ghidra project.
- The admission validator also reports the port's Windows/MFC adapter files
  (`src/c1/platform/*`, parts of `src/c1/world/`) as having no address-keyed
  admission.  That is expected: they are the port's own platform layer, not
  translations of native functions.
