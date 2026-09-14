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
state.

## Root-caused (2026-09-14, follow-up session): NOT a port bug -- the save's own script

Pulled this thread further. Watched the family-4 count over a real,
uninterrupted 30-second window with **zero** clicks/input sent from the
test harness the entire time: it climbed steadily, roughly +1 every
3-4 real seconds (197 -> 206 over 30s), proving conclusively this is
**not** input-driven (ruling out an earlier theory that `PointerTool::
process_pending_input` might leave `pending_input_flags` stuck --
disassembly of native's real `ProcessPendingInput` @ 0x00428860 confirms
it unconditionally clears `pending_input_flags` on every exit path via a
shared tail, and this port's `PointerTool::process_pending_input` already
calls `runtime.finish_pending_input()` unconditionally the same way, so
that mechanism is correctly wired here -- worth remembering as a
correctly-implemented area, not a lead).

Loading `Eden.sfc` (a real, creature-free vanilla save shipped with the
install) instead of the crash save gives `totl 4 0 0 == 0`, stable, and
every other family's count is an *exact* match to that file's own real
object counts (`totl 3 0 0 == 23`, `totl 2 0 0 == 166`,
`totl 0 0 0 == 189`) -- proof the registry fix above is completely
correct and the growth is specific to the crash save's own content, not
a general runtime defect.

Extracting the crashed save's own object scripts via `parse_sfc.py`
found the actual source: non-scenery object index 22 (a `SimpleObject`,
classifier family=2/genus=5/species=6, `timer_period=6000`) carries two
scripts:

```
Event 1 (0x02050601):
  new: gene tokn gren 0 var0, new: crea var0 1, mvto 0 2000, drea 1, ...

Event 9 / Timer (0x02050609):
  doif obv0 eq 0
    setv var0 0, enum 4 0 0, addv var0 1, next
    doif var0 gt 0, setv obv0 1, endi
    stop
  endi
  setv var0 0, enum 4 2 0, addv var0 1, next
  doif var0 eq 0, mesg writ targ 0, endi
```

This is a "restock the population if it goes extinct" machine: once any
creature has ever existed (`obv0` latches to 1), every timer tick (every
6000 game ticks) it checks whether any **genus=2** creature currently
exists; if none do, it sends itself a message that ultimately re-fires
event 1, which spawns another creature via `new: crea` from a `gren`
(generic/template) genome.

The bug is entirely within this script: the creatures it actually
spawns are **genus=1** (Norn -- confirmed by every phantom entry's
`gnus` reading 1, matching `load_genus_identity`'s real classifier
derivation), but the extinction check queries **genus=2**. Since the
script never spawns anything of the genus it's checking for, `enum 4 2 0`
always comes back empty and the "restock" branch fires unconditionally,
forever, once per timer period -- regardless of how many genus=1
creatures already exist. `species` stays 0 on every spawned creature
because the `gren` template genome it always uses has no sex gene for
`genome.sex()` to read (matching `Skeleton::load_genus_identity`'s real,
correct, already-checked-against-native derivation
`species = genome.sex()`).

This is a logic bug in the *save file's own CAOS script*, not in this
port: native Creatures 1 executing this exact script against this exact
save would spawn creatures at exactly the same unconditional rate, since
every primitive involved (`enum`, `new: crea`, `mesg writ`, timers) is
otherwise faithful. It is very plausibly the real mechanism behind the
user's original crash report (unbounded object creation over a long play
session eventually exhausting something), but it is not something to fix
in the port -- flagged back to the user as a property of this specific
save/world script, not a bug to chase further here.
