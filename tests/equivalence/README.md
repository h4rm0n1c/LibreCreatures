# Function-level equivalence tests

These tests call a function of the original `Creatures.exe` and the
LibreCreatures function that replaces it with the same inputs, and compare
every byte of the result.

- The original exe is only read. Its sections are copied into the test
  process at 0x400000; the test is linked at that base with an empty first
  section for them (`image_hole.ld`, `native_image.cpp`). No entry point,
  C runtime start-up or TLS callback of the original runs, and every
  import it has is a trap that names the import and exits.
- LC is built twice. Native (`LC_NATIVE_MODE`, every fix in
  `docs/original-bugs.md` off) must match the original on every case. As
  shipped, every differing case must be explained by the rows whose
  switches are in the code under test.
- Inputs the original would crash on are not generated; those are the
  ledger's guards.

## Biochemistry: `CBiochemistry::Update` @0042ee10 (CE 10.3)

    tests/equivalence/build.sh [build-dir]
    WINEPREFIX=<throwaway prefix> tests/equivalence/run_biochemistry.sh \
        <CE 10.3 Creatures.exe> [cases] [build-dir]

Each case is random chemistry: 256 concentrations and half-lives, up to 128
emitters, receptors and reactions, the loci they read and write, and 1 to 8
ticks. `EQUIVALENCE_ONLY=decay|emitters|receptors|reactions` narrows the
cases to one kind of record.

First run (2026-10-16): 200,000 cases, 899,974 updates.
- LC native: 0 differing cases.
- LC as shipped: 55,452 differing cases; row 50 alone explains 24,256, row
  51 alone 41,608, the two together 3.

Generated values stay in what the game can reach: reaction amounts 1-16 (the
genome load makes them so; LC's world load maps other bytes into that
range), loci in the brain and creature domains, emission periods 1-255.

## Brain: `CBrain::Update` @00404e90 (CE 10.3)

The whole brain tick: the early phase of lobes 0-5, the perception merge,
and the late phase of lobes 6 on (SVRules, susceptibility, reinforcement,
relaxation, dendrite growth and decay, loose dendrite re-attachment,
migration).

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_brain.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

Each case grows a brain from one of the genomes with LC's genome loader,
saves it in the world format and perturbs it: activations, firing,
dendrite sources, weights and strengths (loose ones too), each lobe's
chemical inputs and the winner-take-all exclusions. The native CBrain is
built from the same saved state. Both run 1 to 6 ticks; every neuron and
dendrite field and each lobe's active count, active fraction and loose
dendrite counts must match.

- `rand`: the original's import is bound to the MSVC formula and LC's
  `rand` is wrapped at link time (`--wrap=rand`), so both sides draw from
  one controlled stream. `EQUIVALENCE_CASE=<n>` prints a case tick by tick
  with every rand call's caller on each side.
- `DebugLog` (@00418000) returns at once in the copy.
- Native's migration duplicate test packs neuron pointers into 16 bits
  (row 5), so with fewer than four dendrites its answer depends on where a
  lobe's neurons sit. Each native neuron array is placed at the same
  address modulo 0x10000 as LC's.

First run (2026-10-16), the 12 stock genomes (mum1-6, dad1-6): 20,000
cases, 70,008 brain updates.
- LC native: 0 differing cases.
- LC as shipped: 6,526 differing cases. A single row explains 5,803
  (row 4), 575 (row 6) and 353 (row 5); row 2 alone changes nothing with
  the stock genomes; 175 need several rows together. Rows 2, 4, 5 and 6 are
  the only switches in the brain code, and the native build matches on
  every case, so every difference comes from them.

## Stimuli: `Creature::ApplyStimulus` @0040b8a0 (CE 10.3)

What a creature does with a stimulus: magnitude scaling or the sense-neuron
offset, sleep gating and halving, the attention record of the source's kind
(target, position, sense neuron and activation, visible and seen flags),
the attention (2) and
general sensory (5) lobe inputs, the four chemicals, and the goal-direction
matrix's learning from drive reducers.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_stimulus.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

Each case grows an LC creature's brain from one of the genomes and gives the
native side a Creature-shaped block holding the same state at the offsets
ApplyStimulus reads (from its disassembly): sleep, motion link, the 40
attention records, the goal matrix, lobes 2 and 5 and all 256 chemicals,
all random. Six sources of random kind and position, and the creature
itself, then send 1 to 4 random stimuli (descriptor flags, chemical ids
weighted to the drive reducers, magnitudes below 0 and above 255); after
each one every record, both lobes, the chemicals and the matrix must match.
A source sometimes stands at y 4000, where `kill` parks objects (row 9).

- creature.cpp reaches skeleton, sound, display and script code this
  function never calls; those symbols are trap stubs (`trap_stubs.py`) that
  name themselves and exit if reached.
- Not generated: genera with no attention record (row 10's guard) and dead
  creatures (both sides return at once). Goal weights are 0-255, as the
  game makes them: native divides the update unsigned and LC signed, which
  would differ only for a negative weight.

Run of 2026-10-17, the 12 stock genomes: 200,000 cases, 500,153 stimuli.
- LC native: 0 differing cases.
- LC as shipped: 43,866 differing cases, the same cases as with row 9
  alone fixed; row 9 is the only switch in apply_stimulus.
- The first run did not compare the seen flag. The perception test found
  that LC's apply_stimulus set it and ApplyStimulus does not; the test now
  compares it, and LC is fixed.

## Action selection: `Creature::UpdateActionSelection` @0040c630 with `Creature::SetAction` @0040b680 (CE 10.3)

Involuntary actions (cooldowns, trigger rolls, their scripts), the choice
of action from the decision lobe and the action-target table, clearing the
old action's neuron, the sleep indicator's teardown, SetAction's event to
the bounds reference, its target-override, own and quiescent script
lookups, the speech roll and the chosen neuron's boost.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_action.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

Each case grows an LC creature's brain from one of the genomes and gives the
native side a Creature-shaped block with the same state at the offsets the
two functions read (dream countdown, the eight involuntary actions, the
active involuntary action, the selected action and boost, sleep and its
indicator object, motion link, bounds reference and mode, the decision
lobe). Scripts exist for a random set of classifiers: native
`ExecuteScriptForClassifier` (@00419e00, owner in ECX, source in EDX) is
redirected to a recorder that answers from that set, and LC's host answers
from the same set. Both sides draw rand from the MSVC formula with equal
seeds. Up to six updates run per case; after each, the state, every script
lookup in order, the sleep indicator's calls, the events in the immediate
event ring and whether a phrase was chosen must match. A phrase ends the
case: native's phrase functions are recorders, so the rand streams part.
LC's action-target table (`action_target_requirement_for`) is also checked
against native's at @00454450.

- Not generated: a creature with no quiescent script (native SetAction then
  retries action 0 for ever), negative selected actions, decision lobes of
  other than 16 neurons.

First run (2026-10-17), the 12 stock genomes: 200,000 cases, 1,068,397
updates (37,037 cases ended by speech); coverage: 307,894 updates looked
up scripts, 239,083 looked up involuntary actions, 32,260 queued events,
24,886 tore down the sleep indicator.
- LC native: 0 differing cases.
- LC as shipped: 10,357 differing cases, the same cases as with row 11
  alone fixed; row 11 is the only switch in the two functions.

## Dreaming: `Creature::ProcessDreaming` @00409360 with `CInstinct::RunDreamStep` @00407370 (CE 10.3)

Instinct replay while asleep: the reset step, replay steps (a biochemistry
update, the first eight lobes cleared, the instinct's three neurons and
decision neuron driven, a brain update), the dream chemical restored on
steps 0x23-0x27, and the finishing step (every dendrite's long-term weight
raised to its short-term weight, the brain reset, the instinct dropped, the
dream counted down).

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_dream.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The native side is real: a CBrain built as in the brain test (a stock
genome's brain, saved and perturbed) and a CBiochemistry built as in the
biochemistry test (`brain_native.hpp`, `biochemistry_native.hpp`), native
CInstinct blocks (deleted through a recording vtable) and a Creature-shaped
block at the offsets ProcessDreaming reads. Each case queues 1 to 4 random
instincts, some part-way through, and runs up to 90 steps; after each, the
whole brain, every concentration and locus, the instinct queue, the
biochemistry tick and the instincts deleted must match. rand is shared as
in the brain test.

- The dreaming code has no switch of its own; LC as shipped differs through
  the brain (2, 4, 5, 6) and biochemistry (50, 51) rows it calls, so the
  `rows` build fixes just those six and must differ on exactly the same
  cases.
- Not generated: negative instinct neuron indices (genome bytes are 0-255).

First run (2026-10-17), the 12 stock genomes: 20,000 cases, 767,905 dream
steps, 15,121 instincts finished.
- LC native: 0 differing cases.
- LC as shipped: 8,250 differing cases, the same cases as the build with
  only rows 2, 4, 5, 6, 50 and 51 fixed.

## Attention: `Creature::UpdateAttention` @0040bbc0 with `Creature::SetAction` (CE 10.3)

The choice of attended object from the attention lobe (7), and on a change
of target: the motion link and its part centre, the decision (6), sensory
(5) and stimulus-source (2) lobes cleared and fed, the sleep indicator's
teardown, SetAction(quiescent), the decision lobe's exclusions from the
action-target table, and a pending goal-direction verb into the verb lobe
(3). Every record's sense neuron is then cleared.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_attention.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The native side is a Creature-shaped block and a CBrain-shaped block with
the five lobes, at the offsets the function reads. Scripts and events are
recorded as in the action test. A new target's part centre comes from the
LC object paired with the native one, so both sides see the same point. Half
the cases aim the pending verb at an object that then wins attention. One
update per case; then the motion link and target point, every record's
sense neuron, all neurons of the five lobes (firing, activation,
exclusion), the goal-direction state, the sleep indicator calls, the script
lookups and the events must match.

- Not generated: genera with no attention record (row 10), negative verb
  neurons, attention lobes of other than 40 and decision lobes of other than
  16 neurons.

First run (2026-10-17), the 12 stock genomes: 200,000 cases, 177,038
attention shifts, 43,593 verb deliveries.
- LC native: 0 differing cases.
- LC as shipped: 40,943 differing cases, the same cases as the build with
  only rows 8 and 40 fixed; they are the only switches in the code under
  test.

## Perception: `Creature::UpdatePerception` @0040bf10 with `Creature::CanPerceiveObject` @00409750 (CE 10.3)

What a creature notices in one update: moving sound sources (stimuli 5 and
6), objects seen first or again (stimulus 8, with the first-seen and
return-to-view magnitudes), records that go out of view, the motion link's
kin neurons in the general sensory lobe (5), and the wall and vehicle
neurons. CanPerceiveObject decides each sighting from facing, the
movement bounds or the nearest map room, and distance. The stimuli go
through TriggerBuiltInStimulus @0040b820 and ApplyStimulus.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_perception.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The native side is a Creature-shaped block with the 40 records, the
built-in stimulus contexts, the goal matrix, lobes 2, 5 and 7, the
chemicals, sleep, facing, foot point, limb end, movement bounds, bounds
reference, motion link, monikers and sex. Eight objects of random kind
stand near it; two are creatures with random monikers and sex. The native
object registry (0x4704a8, 0x4704ac) and LC's host list the same objects,
the creature itself sometimes among them. Bounds, sound positions and
flags come from one table on both sides. The four rectangle imports are
bound to plain functions, and FindNearestMapRoomBoundsAtPoint @00422d70 is
replaced on both sides by one function, because world geometry is not
under test. LC uses the kin and map-room rules that the game's host uses
(`motion_link_facts_for`, `is_map_room_marker`).

Some record targets have the classifier of another record, as after
`setv clas`. One update per case; then every record (target, position,
sense neuron and activation, visible, seen), lobes 2, 5 and 7, the
chemicals and the goal matrix must match.

- Not generated: genera with no attention record (row 10).
- Not covered: objects that `new:` has just made at the world origin
  (row 73's first part); the host always answers "placed".

First run (2026-10-17), the 12 stock genomes: 200,000 cases, 104,009
records seen first, 33,348 cases with a creature as motion link.
- The run found two translation defects in LC, now fixed:
  - apply_stimulus set the seen flag of the record. ApplyStimulus does
    not. After a stimulus from an object, LC gave the smaller sighting
    magnitude where C1 gives the first-seen magnitude.
  - When a record came back into view, LC used the seen flag of that
    record. UpdatePerception uses the flag of the slot for the target's
    classifier now.
- LC native: 0 differing cases.
- LC as shipped: 82,231 differing cases, the same cases as the build with
  only rows 9, 38 and 73 fixed; they are the only switches in the code
  under test.

## Goal direction: `Creature::UpdateGoalDirection` @0040ccc0 with `Creature::ApplyGoalDirection` @0040abc0 (CE 10.3)

How a creature with strong drives looks for a remembered object. The drives
are scaled and set the push. Each attention record is scored from the goal
matrix, and the best record with a known position wins. Then one of three
things occurs:
- The memory is cleared when the creature is in the same room, faces away
  from the remembered position and is less than 500 pixels from it.
- The stimulus-source lobe (2) is fed when the record has a target.
- Otherwise a verb is chosen from the candidate tables: a known lift, the
  call button's interaction event, left or right, above or below, and the
  room type. The verb goes
  into the verb lobe (3) or the pending goal-direction state.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_goal.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The native side is a Creature-shaped block with the 16 drive levels, the 40
records, the goal matrix, the goal-direction state, lobes 2 and 3, death,
sleep, the dream countdown, the bounds reference, the motion link, facing,
foot point, movement bounds and sound position. Six objects of random kind
are record targets and the motion link. Their interaction event byte
(+0x24) is random. The room table (0x468d64) holds 0 to 6 random rooms of
random type near the creature. LC's host answers from the same table, as
the game's host does: the first room that contains the point. PtInRect is
bound to a plain implementation.

One update per case. Then every attention record, the goal-direction state
and both lobes (firing and activation) must match.

- Not generated: genera with no attention record (row 10) and goal weights
  below 0.

First run (2026-10-17), the 12 stock genomes: 200,000 cases, 6,641
memories cleared, 112,363 updates fed lobe 2, 74,895 gave a verb.
- The run found one translation defect in LC, now fixed: the third row of
  the interaction-context scores (0045426c, the call button's interaction
  event set) was copied two places to the left. C1 then favours the call
  button (record 2, verb 4) and LC favoured the lift (record 27, verb 2).
- LC native: 0 differing cases.
- LC as shipped: 0 differing cases. The code under test has no switch.

## Map geometry: the room functions and `Object::UpdateMovementBounds` (CE 10.3)

- `FindNearestMapRoomBoundsAtPoint` @00422d70: the room whose x span holds
  the point and whose floor is nearest the point's y. It changes the
  caller's rectangle in place.
- `MapData::FindRoomContainingPoint` @00422df0: the first room that contains
  the point (PtInRect).
- `GetAmbientTemperatureAtPoint` @00422e40: the map's temperature, outside
  every room or in a room of type 1.
- `Object::UpdateMovementBounds` @00426470: the movement bounds for each of
  the five bounds modes, and the whole world for the edit object.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_geometry.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

Each case makes a room table of 0 to 40 rooms in the native MapData layout,
at the MapData pointer 0x468d64, and the same table for LC. Most rooms
share edges in a grid, as the game's rooms do; some have gaps, overlaps, no
area or inverted corners. Then 32 points, many on room edges, go to the
three room functions, with a random starting rectangle for the nearest-room
function. Then 8 objects of random bounds mode, flags, bounds, reference
and vehicle data go to UpdateMovementBounds. LC's host for the nearest room
is the game's (`C1WindowsDocument`).

First run (2026-10-18): 200,000 cases, 6,400,000 points (1,327,665 with no
room over the x), 1,600,000 objects.
- The test found one translation defect in LC, now fixed: the game's host
  started the nearest-room function from an empty rectangle. The original
  keeps the caller's left, top and right when no room spans the x, so an
  object kept its last room's sides. In the native build LC gave
  `{0, 0, 0, 9999}`. Before the fix, 931 of 5,000 cases differed.
- LC native: 0 differing cases.
- LC as shipped: 167,929 differing cases, the same cases as the build with
  only row 22 fixed. Row 22 is the only switch in the code under test.

## Creature fan-out: `MapData::QueueEvent...` and `Queue...Stimulus` (CE 10.3)

The seven functions that send an event or a stimulus to the creatures near
an object:

| Function | Address | Creatures chosen |
| --- | --- | --- |
| `QueueEventForCreaturesInSpeechRange` | 00422f90 | in the speech window |
| `QueueEventForCreaturesThatCanPerceiveObject` | 00423030 | that can perceive the object |
| `QueueTactEventForOverlappingCreatures` | 00423090 | that touch the object, or ride it |
| `QueueForPerceivingCreatures` | 00423150 | that can perceive the object |
| `QueueForOverlappingCreatures` | 004231f0 | that touch the object, or ride it |
| `QueueSignStimulusForPerceivingCreatures` | 00423390 | that can perceive the object |
| `QueueTactStimulusForOverlappingCreatures` | 00423470 | every creature, when one creature touches the object (row 17) |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_fanout.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

Each case makes 0 to 8 creatures (the native creature list at 0x4704bc,
0x4704c0) and 4 other objects near each other. Each has a random
classifier, flags (vehicle, invisible), facing, foot point, movement
bounds, bounds and bounds reference. Each creature has random built-in
stimulus contexts. Some bounds have no area or inverted corners. Then the
case makes 6 calls of random function and arguments. The source is any of
the objects or creatures. CanPerceiveObject, the nearest-room function and
QueueObjectEvent run as native code, on a random room table. LC's host is
the game's (`WindowsCreatureFanoutHost`).

After each call, the queued object events (immediate ring at 0x466470,
delayed table at 0x46e1e0), the queued stimuli (ring at 0x466468) and the
passed stimulus context must match.

- Not compared: the native sign and tact helpers also write source and
  target into the creature's own built-in context. LC's host copies the
  context instead, and every reader writes both fields before use.
- Not generated: negative stimulus indices (native reads before the
  contexts).

First run (2026-10-18): 100,000 cases, 600,000 calls, 123,503 events and
147,822 stimuli queued.
- The test found one translation defect in LC, now fixed: the overlap test
  counted a rectangle with no area, or with inverted corners, as touching a
  rectangle around it. The original uses IntersectRect, which needs an
  intersection with area. Before the fix, 15 of 3,000 cases differed.
- LC native: 0 differing cases.
- LC as shipped: 19,406 differing cases, the same cases as the build with
  only rows 17, 22 and 73 fixed. They are the only switches in the code
  under test.

## Skeleton: body layout, stepping, poses and movement (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `RecomputeBodyPartLayout` | 0043b5a0 | `Skeleton::recompute_body_part_layout` |
| `UpdateLimbFramesForPose` | 0043b8f0 | `update_limb_frames_for_pose` |
| `UpdateAnchorAndBounds` | 0043b950 | `update_anchor_and_bounds` |
| `ApplyPoseString` | 0043ba20 | `apply_pose_string` |
| `AdvancePoseAnimation` | 0043bb20 | `Creature::advance_pose_animation` |
| `SetTargetPoseString` | 0043be00 | `set_target_pose_string` |
| `IsPoseAtTarget` | 0043be70 | `is_pose_at_target` |
| `SetTargetPoseFromTableIndex` | 0043bf10 | `set_target_pose_from_table_index` |
| `SelectTargetPoseForMotion` | 0043bf40 | `select_target_pose_for_motion` |
| `SetDownFootPositionAndRecomputeLayout` | 0043c070 | `set_down_foot_position_and_recompute_layout` |
| `TranslateBy` | 0043c0b0 | `translate_by` |
| `SetDownFootPositionAndInvalidateBounds` | 0043c2f0 | `set_down_foot_position_and_invalidate_bounds` |
| `UpdateAndInvalidateBounds` | 0043c380 | `update_and_invalidate_bounds` |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_skeleton.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

Each case builds one creature body twice: an LC Skeleton with a Body and
six limb chains (head 1, legs 3, arms 2, tail 2 parts), and native
Creature, Body and Limb blocks with the same state. These are random:
- the attachment tables and the image sizes (a 256-image gallery, so every
  byte image index is in range);
- the image bases, poses, views and render planes;
- facing, down foot, eyes, expression and movement bounds;
- the current and target poses and the 100-entry pose table;
- the motion link (none, an object, or an object the creature carries)
  and its target point;
- a room table.

Then 16 random operations run on both sides. After each operation the whole
skeleton must match: each part's image, position, pose, view and plane; the
down foot; the chain ends; the sprite and previous bounds; the boundary
flag; facing; the poses; the animation sequence and cursor; the transition
count; the return values; and the queued dirty rectangles.

The native vtable has the Skeleton's own entries for the slots these
functions call. The renderer's dirty queue (00412610) and PlaySoundEffect
(0042ce20) are recorders. UpdateMovementBounds and the nearest-room
function run as native code. LC's hosts are the game's. Sounds are muted
on the LC side, so the step sound is not compared; the step is seen in the
down foot.

Set `EQUIVALENCE_CASE=<n>` to print the poses after each operation of
case n.

First run (2026-10-18): 200,000 cases, 3,200,000 operations, 235,065 steps,
1,402,030 operations after a wall correction.
- The test found two translation defects in LC, now fixed:
  - Rows 5 to 8 of the motion pose table (00454690) held the native rows
    4 to 7. When its target was a little below it, a creature took a
    higher pose digit than C1 gives.
  - `select_target_pose_for_motion` tested `motion_link == this`.
    SelectTargetPoseForMotion tests whether the link's bounds reference is
    the creature, as for a carried object, and then returns 1 at once.
    Before the fix, 247 of 2,000 cases differed.
- LC native: 0 differing cases.
- LC as shipped: 16,358 differing cases, the same cases as the build with
  only row 22 fixed. Row 22 (the nearest room, through the down-foot
  setter) is the only switch the code under test reaches.

## Tables copied from the original

    tests/equivalence/check_tables.py <CE 10.3 Creatures.exe>

Compares 29 LC tables with the bytes at their addresses in the exe. They
include numeric tables, enum tables, string pointer tables, the default
stimulus contexts (through their pointer table) and the vocabulary banks.
The script reads each table from the LC source. Exit status 1 when a
table differs.

The sweep of 2026-10-18 searched the exe for every constant table in
`src/c1`. It found two more copy defects, now fixed:
- `kActionPhraseWordIndices` entry 11 (walk east) was 0xff. C1 has 11, so
  a creature says the word for walking east.
- `kAmbientEnvironmentRecords` record 9 byte 2 was 255. C1 has 0. LC does
  not read that byte.

Three earlier copy defects were found by the function tests (the goal
interaction scores and the motion pose table, see above).

## Vehicles, lifts and call buttons (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `Vehicle::Tick` | 0042bd90 | `Vehicle::tick` |
| Vehicle queued events 0, 1, 2 | 0042c0b0, 0042c170, 0042c230 | `handle_queued_event_0` to `_2` |
| `Lift::InitializeState` | 0042c320 | `Lift::initialize_state` |
| `Lift::Tick` | 0042c4f0 | `Lift::tick` |
| `Lift::RequestMoveUp`, `RequestMoveDown` | 0042c5c0, 0042c680 | `request_move_up`, `request_move_down` |
| `Lift::SelectNearestCallButtonAndStartMove` | 0042c590 | `select_nearest_call_button_and_start_move` |
| `CallButton::UpdateLiftStateAndQueueRedraw` | 00429a70 | `update_lift_state_and_queue_redraw` |
| `CallButton::RequestLiftCall` | 00429c10 | `request_lift_call` |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_vehicle.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

The native objects are real native objects. Each block has the native
vtable of its class, so every virtual call runs native code. There are
three exceptions:
- slot 22 of the vehicle and lift (MoveByAndRedraw) is the native MoveBy
  without the redraw;
- the redraw at 0042b2b0 returns at once;
- script dispatch (00419e00) is a recorder.
LC's hosts are the game's.

A lift case has these parts:
- a lift of 1 to 3 parts;
- 2 to 6 floors, as rooms one above the other;
- one call button on each floor, registered through UpdateLiftState;
- 0 to 3 passengers (simple objects whose bounds reference is the lift);
- a creature and an object that ask for the lift.

When a request starts the lift, the test sets a speed toward the floor on
both sides, as the game's lift scripts do. A vehicle case has movement-bounds
collision (flag 0x40) or terrain collision (flag 0x80, on random ground
heights), with passengers and the same two sources. A lift case runs 200
random operations and a vehicle case 80. After each operation these must
match:
- the vehicle: part positions, velocity, fixed-point position, collision
  sides, interaction, floors, call buttons, selection, cooldown;
- the passengers: position and movement bounds;
- the call buttons: interaction and floor;
- the scripts run, the queued object events and the queued stimuli.

- Not generated: part animation sequences, and vehicles beyond one world
  width (row 52).
- Not compared: dirty rectangles.

First run (2026-10-18): 50,000 cases, 8,045,880 operations, 21,120 lift
arrivals, 106,229 operations with a collision side, 3,441,616 ticks with
passengers.
- LC native: 0 differing cases. The test found no defect.
- LC as shipped: 36,625 differing cases, the same cases as the build with
  only rows 23, 41, 45 and 52 fixed. They are the only switches in the code
  under test.

## Simple objects: pick-up, carrying and putting down (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `SimpleObject::HandleQueuedEvent4` (pick up) | 00427cc0 | `handle_queued_event_4` |
| `SimpleObject` queued event 5 (put down) with `EndInteractionWithSource` | 00427f40, 00428bb0 | `handle_queued_event_5` |
| `SimpleObject::Tick` carried (mode 4), `UpdateEntityForExplicitRectBoundsAndRedraw` | 00427a70, 00428d30 | `tick` |
| `SimpleObject::FinalizeObjectEdit` | 00426ee0 | `finalize_object_edit` |

`UpdateMovementBounds`, the nearest-room function and
`FindTopmostOverlappingObject` @00425c30 run as native code.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_simple_object.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

The native objects use the native vtable of their class, with these
changes:
- slots 22 and 23 are the native MoveBy and MoveTo without the redraw;
- `SetBoundsMode` (004257f0) only stores the mode, so its renderable-set
  upkeep is not compared;
- the carried tick returns once the object is placed (a jump at 00428ea7
  skips the redraw);
- script dispatch (00419e00) is a recorder on both sides.

Each case has:
- a room table;
- the object under test, with random carry flags (hand 0x02, creature
  0x01, vehicle-proof 0x20, room-bound 0x40), plane and position;
- two other simple objects, one of them sometimes carried by creature A;
- two creatures with random body plane, sprite bounds, movement bounds,
  right-hand chain end and facing;
- two vehicles (lift-like genus 3 2, or plain), with random parts, planes
  and local bounds;
- the hand.

Then 40 operations: pick up by the hand, a creature or an object; put
down by the hand or a creature; a carried tick; the edit drop at a random
privilege; and moves of the creatures and vehicles. After each, every
simple object's mode, reference, position, plane and movement bounds, the
scripts run, and the queued events and stimuli must match.

- Not generated: a machine as carrier (native reads a pointer byte for the
  plane), the hand's own tick (row 29, mouse input), image sequences.

First run (2026-10-18): 50,000 cases, 2,000,000 operations, 483,463 with
the object carried by a creature, 32,982 in a vehicle, 85,234 carried
ticks.
- LC native: 0 differing cases. The test found no defect.
- LC as shipped: 5,265 differing cases, the same cases as the build with
  only rows 22 and 27 fixed. Row 28 is also in the code, on the branch
  for the hand's own search, which this test does not generate.

## Learned words and phrases (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `InitializeLearnedWordRecords` | 00409e80 | `initialize_learned_word_records` |
| `InitializeDefaultVocabulary` | 0040a490 | `initialize_default_vocabulary` |
| `UpdateLearnedWordRecord` | 0040a0c0 | `update_learned_word_record` |
| `RegenerateLearnedWordResponse` | 0040a1f0 | `regenerate_learned_word_response` |
| `NormalizeLearnedWordResponse` | 0040a380 | `normalize_learned_word_response` |
| `SpeakDominantDrivePhrase` | 0040b160 | `speak_dominant_drive_phrase` |
| `SpeakActionPhrase` | 0040afa0 | `speak_action_phrase` |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_words.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

The native creature is a Creature-shaped block. It holds:
- the 80 learned-word records at +0xb64, 56 bytes each: recognized word,
  response word, and reinforcement at +0x34;
- the classifier, the sleep flag, the drive levels, the selected action
  and the motion link.

The CRT functions rand, _mbsstr, _mbsinc, _mbschr, _mbsicmp, strcpy_s and
strcat_s are bound to plain single-byte implementations, and LC's text API
is the same. Speak, SpeakTextWithVoice and the speech-range event queue
are recorders on both sides.

Each case has random records (words of 1 to 12 letters, real and made up,
any reinforcement) and a random classifier (either sex partition). Then 24
random operations run:
- initialize the records;
- initialize the default vocabulary;
- hear a word: a record's own word, another record's word or a new word,
  from an object or a creature, awake or asleep;
- regenerate a response;
- normalize a response;
- say the dominant drive's phrase;
- say the action phrase, with or without a motion link.

After each operation, all 80 records, the return value, everything said
and queued, and the rand state must match.

- Not generated: words too long for the 25-byte record (native strcpy_s
  stops the program), and the response of a word of vowels only (row 80).

First run (2026-10-18): 50,000 cases, 1,200,000 operations, 644,539
things said, 88,981 words relearned.
- The test found one translation defect in LC, now fixed. The baby-talk
  response is repeated when it is shorter than three letters, and also
  when it is shorter than five letters and the heard word is longer than
  five. LC repeated only in the first case: "friendly" gave "nri" where
  C1 gives "nrinri". Before the fix, 2,238 of 5,000 cases differed.
- The test also found a native defect that LC already guarded against,
  now row 80. For a word of vowels only, native scans past the word and
  writes over the record.
- LC native: 0 differing cases.
- LC as shipped: 12,663 differing cases, the same cases as the build with
  only row 14 fixed (the space in "come Norn").

## Heard words: `Creature::ProcessHeardWords` @0040a630 (CE 10.3)

What a creature does with a sentence it hears:
- it matches each word to its name, a noun (the 39 object categories) or
  a verb;
- it learns a word it does not know as the name of what it attends to;
- it takes a stimulus: nothing understood, or words from the hand or from
  another creature;
- it obeys yes, no, look and what;
- it gives attention and a verb through ApplyGoalDirection.

The "look" search (0040ac90), UpdateLearnedWordRecord, TriggerBuiltInStimulus,
ApplyStimulus, SpeakActionPhrase and ApplyGoalDirection run as native code.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_heard.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The native creature is a Creature-shaped block with:
- the learned-word and attention records;
- the built-in stimulus contexts;
- the goal matrix and goal-direction state;
- lobes 2, 3, 5 and 7, and all chemicals;
- sleep, the motion link, the movement bounds, the bounds reference (a
  vehicle, one case in three), the selected action and the classifier.

Eight objects of random kind and the hand stand near it. The native object
registry and LC's host list them alike, with bounds and sound positions from
one table. The CRT text functions, rand and the speech outputs are as in
the learned-word test. LC's hosts are the game's.

Each case speaks 8 sentences of 1 to 4 words. A word is the creature's
name, a noun, a verb, yes/no/look/what, or a word it does not know, in
either case. Some sentences have extra spaces. The speaker is the hand,
another creature or an object, and sometimes the creature is asleep.
After each sentence these must match:
- all learned-word and attention records;
- the four lobes, the chemicals and the goal matrix;
- the goal-direction state;
- everything said and queued, and the rand state.

- Not generated: words of vowels only (row 80), genera with no attention
  record (row 10), words too long for a record.

First run (2026-10-18): 50,000 cases, 400,000 sentences, 27,416 with
"look".
- The test found three translation defects in LC, now fixed:
  - LC ignored what a sleeping creature heard. ProcessHeardWords has no
    sleep test; only what it calls checks sleep.
  - "look" set a verb (0xff). Native sets only the attention magnitude,
    so "look" gives attention and no verb.
  - The game's host gave "look" the creature's own movement bounds. Native
    takes the bounds reference's (a vehicle's) movement bounds when there
    is one. Before the fix, 142 of 5,000 cases differed.
- LC native: 0 differing cases.
- LC as shipped: 28,016 differing cases, the same cases as the build with
  only rows 9, 37 and 39 fixed.
