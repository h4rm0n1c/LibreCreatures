# Newborn creature registration order

## Symptom

A Hatchery egg could mature and create a live object, but the newborn was not
consistently available through the normal creature-selection and UI paths. This
matched the in-game report that newly hatched creatures were alive but frozen,
silent, or invisible to the creature controls.

## Evidence

The native `Creature::Creature` routine at `Creatures.exe+0x0040d580` appends
the new creature to the global creature registry and then calls
`RebuildCreatureSelectionMenu`. Native `RebuildCreatureSelectionMenu` at
`Creatures.exe+0x00422250` walks that registry and adds every tick-enabled
creature to the selection array.

The port's `Creature` constructor makes the same two host calls in that order,
but `WindowsCreatureConstructionHost::append_to_creature_registry` had been a
deliberate no-op. `WorldRuntime::adopt_creature` only appended the creature
after construction returned. Therefore the constructor's menu rebuild could
never see a creature created by `NEW: CREA` or the generated-creature command.

A bounded live Hatchery stream confirmed the partial failure:

* `TOTL 4 1 0` was 2 before hatching and 3 after the egg expired.
* The egg advanced through its expected pose/timer path and created a ticking
  family-4 object.
* The brain activity report was populated, proving the brain path was not
  globally absent.
* The newborn was not reliably present in the external selection report,
  because the automation launch does not expose the same UI selection array as
  a normal interactive Windows session.

## Fix

After `WorldRuntime::adopt_creature` returns, both Windows creature creation
adapters now call `C1WindowsDocument::rebuild_creature_selection_menu()` once
more. This preserves ownership and exception safety in the two-phase port
construction while making the externally observable order equivalent to the
native registry-then-menu sequence.

The paths covered are the Hatchery/CAOS `NEW: CREA` path and the Testing-menu
generated-creature path. Archive loading already rebuilds after deserialization
and is not changed here.

## Verification

* Byte-match clean-source build: 138 cached objects reused, 1 source compiled,
  resource compilation passed, link passed.
* Managed GUI launch succeeded.
* Hatchery-style CAOS stream returned `OK`, and `TOTL 4 1 0` again changed from
  2 to 3.
* No crash or orphaned CAOS client remained after testing.

The full interactive Windows selection-menu behavior still needs a manual
Windows run because `/Automation`/`/Embedding` does not provide a trustworthy
`GETB OVVD` view of the native UI selection array.
