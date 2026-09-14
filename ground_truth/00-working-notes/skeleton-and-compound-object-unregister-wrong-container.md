# Destroying a Creature/Vehicle/Lift/Blackboard/CompoundObject leaked a dangling pointer into totl/enum forever -- resolved (partially)

Date: 2026-09-14

## The bug

`WorldRuntime` keeps two *separate* object lists:

- `objects_` -- the registry `totl`/`enum`/every CAOS object-enumeration
  command actually iterates (`non_scenery_object_count()`/
  `non_scenery_object_at()` -> `WorldRuntime::object_count()`/`object_at()`).
- `world_objects_` -- a different registry (`world_object_count()`/
  `world_object_at()`), unrelated to CAOS enumeration.

Two lifetime-cleanup adapters -- `WindowsSkeletonLifetimeHost::
unregister_from_object_registry` (runs from `Skeleton`'s destructor, i.e.
every `Creature`) and `WindowsNewObjectHost::unregister_from_object_
registry` (runs from `CompoundObject`'s destructor, i.e. every `Vehicle`/
`Lift`/`Blackboard`/plain `CompoundObject`) -- both had the same double
bug:

```cpp
const std::size_t count = runtime->world_object_count();
for (std::size_t index = 0; index < count; ++index) {
    if (runtime->world_object_at(index) == &skeleton) {
        runtime->remove_at(index);   // erases from creatures_, not
                                      // world_objects_ or objects_!
        return;
    }
}
```

This searches `world_objects_` to find the index, then calls
`WorldRuntime::remove_at(index)`, which erases from `creatures_` (the
creature-selection registry) at that same index -- a third, unrelated
container. Net effect: destroying any of these five object kinds never
removed its `Object*` from **either** `objects_` or `world_objects_`, and
could additionally delete the wrong entry from `creatures_`.

Every `objects_` entry left behind this way is a dangling pointer into
freed memory. `enabled_object_count_matching` (backing `totl`) and the
`enum`/`next` family then dereference it on every subsequent query,
reading whatever the allocator has since put there -- explaining wildly
unstable, drifting `totl 4 0 0`/`totl 2 0 0` counts (real values in the
hundreds range, alternating between values call to call) against a save
with only 2 real Creature objects.

## How it was found

Cross-checked `totl`/`enum` output against the *real* classifier data in
the user's crashed save, extracted directly from the archive via
`parse_sfc.py` (the project's own byte-exact SFC reader): the save has
exactly 2 `Creature`-kind objects (classifier family 4), 23 `Vehicle`/
`Lift`/`Blackboard`/`CompoundObject`-kind objects (family 3), and 165
`SimpleObject`/`CallButton`/`PointerTool`-kind objects (family 2) --
confirmed ground truth, not a guess. `totl 4 0 0` returning 15-236
(non-deterministic, growing over a live session) against a real count of
2 for family 4 could not be reconciled with the enum-exclusion mechanism
documented earlier (`IsSoundSourceBelowWorldY`) or with the just-fixed
xvec/yvec classifier corruption -- neither touches object *lifetime*.
Traced every call site of `WorldRuntime`'s object-registry removal, found
the two `unregister_from_object_registry` overrides above searching one
container and deleting from another, and confirmed the already-correct
sibling function (`Object::unregister_from_non_scenery_object_registry`,
used by plain `SimpleObject`/`Scenery`) does it the right way: find by
pointer in `objects_`, remove by index from `objects_`.

## Fix

Rewrote both broken overrides to mirror
`Object::unregister_from_non_scenery_object_registry`'s pattern for
`objects_` (search `object_count()`/`object_at()`, remove via
`remove_object_at()`), then separately clean up `world_objects_` via its
own real removal API (`WorldRuntime::remove_world_object`), instead of
the previous mismatched search-one-delete-another logic.

## Verification

- 139/139 compile (2 files recompiled: `windows_creature_hosts.cpp`,
  `windows_macro_host.cpp`).
- Live-tested against the user's crashed save: `totl 3 0 0` (Vehicle/
  Lift/Blackboard/CompoundObject family) now returns **23**, an *exact*
  match for the real count extracted from the save via `parse_sfc.py`
  (11 Vehicle + 9 Lift + 1 Blackboard + 2 CompoundObject). Before the fix
  this family, too, drifted with no stable value.
- `totl 4 0 0` (Creature family) is now stable call-to-call (no more
  wild oscillation) but still reads 15-17 against a real count of 2 --
  see "Not fixed" below. This confirms the destroy-time double-container
  bug was real and is now fixed for the object kinds it actually covers,
  but a **second, distinct** bug affects Creature specifically.

## NOT fixed here: 15-17 phantom Creature-family objects, present even on a freshly-loaded save

Immediately after a completely fresh load (no creature has died yet, so
the just-fixed destroy-time bug cannot have fired), `enum 4 0 0` already
returns ~15 matches instead of the real 2. The ~13 extra entries all
share the exact same classifier pattern: `genus=1, species=0, posl=0`
(the two real creatures have `species=1`/`species=2` and real, non-zero
positions). `species=0` on a genus-1 (Norn) object is not a value either
real creature has, and the phantom count grows slowly over a live
session (16 -> 17 across ~8 queries a few seconds apart), consistent
with something periodically constructing full `Creature` objects (via
`WindowsGeneratedCreatureHost::create_generated_creature` /
`WorldRuntime::adopt_creature`, which registers correctly and is not
itself buggy) whose species never gets finalized from the placeholder
state. Not yet root-caused: the actual call site creating these
(hatchery/incubator script in this specific save re-triggering generation
repeatedly? a construction path that never assigns `species` at all?)
still needs to be found and checked against native disassembly before
fixing, per this project's standing discipline. Left open as the next
thread to pull.
