# Creature locomotion: where travel actually comes from — 2026-09-17

## Why this note exists

Three creature passes in a row treated "poses advance" as evidence of walking.
The native code says they are unrelated. This records the single comparison
that produces travel, so the next newborn test measures the right thing.

## Native evidence

`Creature::Update` @00408fe0 contains no translation at all. Neither does
`Creature::AdvancePoseAnimation` @0043bb20, which only selects a target pose
from motion-link direction/render-plane and steps each pose component one
character toward it.

All travel comes from `Skeleton::UpdateAnchorAndBounds` @0043b950:

    RecomputeBodyPartLayout(this);
    if (movement_bounds.max_y < limb_chain_end_y[(down_foot == LEFT) + 1]) {
        down_foot_x = limb_chain_end_x[(down_foot == LEFT) + 1];
        down_foot_y = movement_bounds.max_y;
        down_foot   = RIGHT - down_foot;
        RecomputeBodyPartLayout(this);
        PlaySoundEffect('step');
    }

When the swinging leg's endpoint passes below the floor, the down foot
switches to that leg and the body re-anchors at its X. That foot swap *is*
the step. `movement_bounds.max_y` is the floor.

The port's `Skeleton::update_anchor_and_bounds` reproduces this exactly,
including the `RIGHT - down_foot` toggle, the halved boundary correction and
the `step` sound. `Creature::handle_drop_event` and the Creature constructor
are likewise faithful to @00409ae0 and @0040d580 (`unbounded_2`, update
bounds, down foot (1000, 2000), pose `211111111111111`).

## Consequence for the frozen newborn

If `movement_bounds.max_y` is the world bottom (0x4b0) or the unbounded
sentinel (0x7fff) rather than the room floor, the comparison can never hold
and the creature cycles poses on the spot forever — the reported symptom.

`Object::UpdateMovementBounds` @00426470 consults the map room only when
`bounds_flags & 0x40` is set (`TEST byte ptr [ESI+0x9], 0x40`). No
instruction anywhere in Creatures.exe ORs 0x40 into that byte, so for a
creature it arrives from the save archive or from CAOS. That is a candidate
explanation for saved Norns moving while newborns do not; it is NOT yet
established, and must be measured rather than assumed.

## Bounds-flag defect fixed in this pass

`Creature::initialize_runtime_state` did `object_bounds_flags_ |= 0x01` on a
Creature-side member that nothing ever read. Native `InitializeRuntimeState`
@00408520 opens with `OR byte ptr [EDI+0x9], 0x2` — the Object bounds-flag
byte, bit **0x02**, not 0x01. Two faults in one line: wrong bit, and a write
lost to a shadow field because the port composes the Object into Skeleton
instead of inheriting it.

Bit semantics are pinned by `SimpleObject::HandleQueuedEvent4` @00427cc0,
which tests both low bits: 0x01 is the creature explicit-rect permission and
0x02 is pointer-tool unbounded placement (hand pickup). Both are now named
constants on `Object`, and the write goes through `Object::merge_bounds_flags`
on the Skeleton. This is the same shadow-field class of bug as the
`tick_enabled_` duplicate fixed earlier in this working set.

## Pose-index arithmetic: re-verified, both correct

Independently re-derived from the disassembly, because the two fixes appeared
to contradict each other in the earlier write-up.

`Update` @00408fe0 computes `16 * (10 * (row - 0x33) + column)` with the
column keeping its ASCII bias. Expanding, that is `10*row - 510 + column`,
while the plain decimal index is `10*row + column - 528`; the difference is
exactly +18 entries = 0x120 bytes, the pose table's own base. So the single
`-0x33` constant folds both digits' `'0'` bias and the table base, and a
source-level array wants ordinary decimal decoding.

`SetTargetPoseFromTableIndex` @0043bf10 does `(index + 0x12) << 4 + this` —
the same 0x120 base with a plain index. The two fixes agree. The function has
only two DATA xrefs, both vtable entries; the Creature vtable begins at
0x4547fc, making it slot 37 (POSE), with 35 = ANIM and 36 = OVER.

## Instrumentation

`log_creature_step_probe` in `platform/windows_document_host.cpp` logs both
sides of the step comparison plus the bounds state that produces the floor:

    C1_TRACE_CREATURE=<genome moniker hex>   ->  Creatures.motor.log

Rows carry mode, flags, movement bounds, down foot, both leg endpoints, the
opposite-leg index, the evaluated predicate, current/target pose, animation
cursor and motion link. Bounded at 600 rows and silent unless the variable is
set. All other ad-hoc trace scaffolding was removed from both trees in this
pass (creature action/build/entity logs, classifier dispatch log, GDI BitBlt
probe, image blit trace, renderer DIB probe).

## Root cause found: missing `OR bounds_flags, 0x44`

`Skeleton::InitializePoseAndMotionState` @0043ac70, at **0043ad5c**:

    OR byte ptr [ESI + 0x9], 0x44

0x44 is Wallbound (0x40) | Activatable (0x04) per the CAOS Attributes table.
`Creature::InitializeRuntimeState` @00408555 then ORs Mouseable (0x02), and
the Creature constructor calls the Skeleton initializer at 0040d63b. Together
they produce **0x46**, which is byte-for-byte what retail-written saves carry
in every creature's Object record (verified by hexdump of knowngood/World.sfc:
classifier `00 01 01 04`, mode `00`, flags `46`, then movement_bounds
`{2242,757,6335,927}`).

The port's `initialize_pose_and_motion_state` reset every pose/motion field
but never wrote the bounds-flag byte, so port-born creatures had no Wallbound.
Without it `UpdateMovementBounds` returns the whole world `{0,0,0x20a0,0x4b0}`,
the floor sits at y=1200 while the feet are near y=900, and the foot-swap
comparison in `UpdateAnchorAndBounds` can never fire. Poses advance, the body
never moves.

Now fixed in both trees. Verified live: a creature constructed after the fix
reports `flags=46`; before it reported `flags=02`.

Search note for future work: the write was missed twice because the sweep
looked for `OR [reg+9], 0x40` specifically. The constant is 0x44. Sweep for
the opcode/ModRM pair (`80 4? 09`) without pinning the immediate.

## Not established by this pass

Newborn walking is still open. Nothing here measures brain decisions,
stimulus chemistry, or the hatchery placement path. The next run should read
one `Creatures.motor.log` and answer: is the floor wrong, or is the leg
endpoint wrong?

## Drop resting-Y restored to the native contract — 2026-09-17

`SimpleObject::EndInteractionWithSource` @00428bb0 computes the resting Y in
one line, read AFTER `DispatchScriptEvent(EVENT_5)` so a stateful drop script
(the carrot changes pose, hence sprite height, in its own drop script) is
reflected:

    MoveToAndRedraw(target_x, movement_bounds.max_y - current_image.height)

This plants the object's bottom edge exactly on the room floor. It is the
family-2 snap, and the creature analogue is `down_foot_y =
movement_bounds.max_y` in both branches of `Creature::HandleDropEvent`
@00409ae0 and in `HandlePickupEvent` @004098e0.

The port had replaced that single formula with three branches: a "release
room" re-derived via `find_nearest_room_bounds_at_point` from the object's
PRE-move position, a no-room sentinel fallback that kept the current Y, and
the native path. Now reverted to native exactly.

**Two prior workarounds were removed with it.** If either symptom returns,
fix it without reintroducing a second resting-Y formula:
- a room-bound object released over a gap in the room table used to be kept
  at its visible Y; native sends it to `max_y - height` with the 9999
  no-room sentinel, which looks like the object vanished.
- the release-room lookup was added while chasing hand-drop placement; the
  native ordering (EVENT_5 before reading the image height) already covers
  the stateful-pose case it was meant to fix.

Why it matters here: the hatch script places a newborn with
`mvto <egg centre X> <egg posb>` then `slim`, and the placement probe shows
the move happens while the creature is still UNBOUNDED_2 (floor 32767). The
creature does not learn its room until `slim` runs on the next statement, and
nothing revisits the foot afterwards — `UpdateAnchorAndBounds` can only pull a
creature DOWN through a floor, never up onto one. So the egg's resting Y is
inherited directly as the newborn's down foot and has to be exact.

## Diagnostics are on by default

`Creatures.motor.log` and `Creatures.place.log` are written without any
environment variable (setting one on Windows is impractical). Disable with
`C1_TRACE_CREATURE=0` or `off`; set it to a moniker in hex to narrow the motor
trace to one creature. Caps: 4000 rows per creature, 400 placement rows.

`POSL/POSR/POST/POSB` are implemented and correct (sprite min X, +width,
min Y, +height) but are still MISNAMED in `macro.cpp` as
`kRvalueSoundLeft/SoundRight/SoundSourceY/SoundRightFromOrigin`. They are
`AgentBounds` edges per the CAOS manual, not sound values. Rename before the
naming misleads someone again.
