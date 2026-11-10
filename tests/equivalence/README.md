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

## All tests in one run

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_all.sh \
        <CE 10.3 Creatures.exe> <install-dir> [build-dir] [--full] \
        [--no-build] [--only name,name,...]

The script builds every test (`build.sh`), runs each run script and the
table check, and gives one line for each. It exits 1 if a test fails.
`<install-dir>` is the game folder: the genomes are all `*.gen` and `*.GEN`
files in its Genetics folder, and the skeleton-gene test takes copies of
its Images, Body Data and Palettes folders. Nothing in it is written.

- Case counts: about one twentieth of the first runs below by default;
  `--full` uses the first-run counts.
- `--no-build` uses the programs already in the build directory.
- `--only` runs the named tests; the names are those of the run scripts
  (`brain`, `skelgenes`, ...) and `tables`.
- The SHA-256 of the exe is taken before the runs and checked after them.
- Each test's output is in `<build-dir>/run_all.<name>.log`.
- `--coverage` records which of the original's functions the tests run
  and writes `<build-dir>/function-coverage.md` (see "Function coverage"
  below).

## Function coverage

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_all.sh \
        <CE 10.3 Creatures.exe> <install-dir> [build-dir] --coverage

`function_inventory.tsv` lists the original's 2,332 functions (from the
byte_match recovery): 924 the game's own, 955 MSVC/STL runtime and 453
MFC, Win32 and other import-library code, each with its category and the
LibreCreatures file that owns it. With `EQUIVALENCE_COVERAGE` set to a file
of function addresses, `native_image.cpp` puts an int3 at each function's
start; the first time a test's native code enters one, the handler appends
the address and the test program to `EQUIVALENCE_COVERAGE_OUT`, puts the
byte back and lets the function run. The tests' results do not change.
`function_coverage.py` then reports the share of the game's functions that
ran, by category, by LibreCreatures source file and by test, and lists the
ones no test runs. A function that ran is one whose effects a test compared:
it ran inside a call to the original that the test checks against
LibreCreatures. A function a test replaces with a recorder or a stub does
not run and does not count.

To add the world test's load and save, run `run_world.sh` afterwards with
`EQUIVALENCE_COVERAGE` and `EQUIVALENCE_COVERAGE_OUT` still set, then
`function_coverage.py` again.

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
| `CompoundObject::GetPartCenter` | 0042b800 | `get_part_center` |
| `CompoundObject` click event (part bounds and knobs) | 0042b750 | `click_event_id_at_world_position` |
| `Object::ClearReferencesTo` | 00423820 | `Object::clear_references_to` |

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

Two more operations (2026-11-09) prove the offsets of compound-object and
Object fields for the load test:
- part bounds: six random part rectangles (+0xcc, 16 bytes each) and six
  knob hotspots (+0x12c), then `GetPartCenter` for event 0 to 2 and the
  click event at a point near part 0;
- clear references: the bounds reference (+0x1c) and the CAOS object
  pointer (+0x20) set to nothing, the creature or another object, then
  `ClearReferencesTo` with one of them.

Rerun with these (20,000 cases): LC native 0 differing cases; LC as
shipped, every differing case is explained by rows 23, 41, 45 and 52.
Controls: part bounds at 0xd0, knobs at 0x130 or the CAOS pointer at 0x28
on the original's side make 1,997, 2,000 and 1,816 of 2,000 cases differ.

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

## The Object base and SimpleObject's click selectors (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `Object::Object` | 00425750 | `Object::Object` |
| `Object::UpdateSound` | 00426190 | `Object::update_sound` |
| `Object::~Object` | 00425a70 | `Object::~Object` |
| `SimpleObject` click event (vftable slot 28) | 00427fc0 | `SimpleObject::click_event_id_at_world_position` |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_object.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

Each case runs one of four operations:
- The constructor, on a block filled with 0xaa, against a new LC `Object`.
  Every field LC's Object keeps must have the value the constructor writes.
- The continuous sound's update. The sound id, the persist flag and the
  channel handle are random. Both paths are run: with no channel the sound
  starts again, and with an idle channel (0 to 31) the channel is stopped.
  The calls to the mixer (start: id and persist flag; stop: handle) and the
  three fields after the update must match.
- The destructor, with a channel handle and a gallery: the channel stopped
  and the gallery released must match.
- SimpleObject's click event for interaction states 0 to 2. The original
  reads the selector at +0x58 + state with no bounds check. A state of 3 or
  more is not generated.

The original's audibility test, attenuation and pan, mixer start and stop,
registry calls and destructor helper are recording stubs. LC's side has a
sound system with no device (`FakeSoundSystem`) and its real `SoundManager`.
LC's sound fields are set through `ObjectFieldDump`.

First run (2026-11-09): 20,000 cases.
- LC native and LC as shipped: 0 differing cases. The code under test has
  no switch of `docs/original-bugs.md`.
- Controls: moving one offset on the original's side makes the test fail.
  Tick enabled 0x4c to 0x4d: every constructor case differs. Handle 0x28
  to 0x24: constructor, sound and destructor cases differ. Sound id and
  persist flag swapped: 822 of 961 sound cases differ. Gallery 0x40 to
  0x3c: 674 of 984 destructor cases differ. Selectors 0x58 to 0x59: 955 of
  1,040 click cases differ.

## The blackboard (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `Blackboard::InitializeState` | 0042c910 | `initialize_state` |
| `Blackboard::RedrawDisplay` | 0042c9a0 | `redraw_display` |
| `Blackboard::SetEditMode` | 0042ca90 | `set_edit_mode` |
| `Blackboard::SetCurrentWordText` | 0042cb70 | `set_current_word_text` |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_blackboard.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

Both sides draw into a 400 x 300 image of their own, the original through
its own fill (004235f0) and text (00423660) code, LC through `Entity`'s.
The image's pixels come from a stub (00442ac0). Both use the same random
glyphs: the original's charset tables (the pointer 0x468db4, the advance
widths at 0x468dc0) and LC's raster host hold the same data.

Each case starts from a random board: 16 words of 0 to 10 letters, 16
values, the five display bytes, the selected word and edit mode. Then 12
random operations follow. After each, these must match: the words, the
values, the display bytes, the edit flag, the image, and the text input's
target, maximum length and allowed characters.

First run (2026-11-09): 20,000 cases.
- Found L37: when the board left edit mode, LC allowed the characters
  0x1f instead of 0x2f (see the fix below).
- After the fix: 0 differing cases, LC native and as shipped.
- Controls: moving the words, values, display bytes or edit flag by one on
  the original's side makes 1,485 to 2,000 of 2,000 cases differ. The
  values are proven only by InitializeState, which clears them: nothing
  else in LC's core reads them.

## The pointer tool's bubbles and shift-click (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `PointerTool` transient bubble text | 004299b0 | `set_transient_bubble_text` |
| `PointerTool::SetPersistentBubbleText` | 00429960 | `set_persistent_bubble_text` |
| `ProcessPendingInput`, shift-click (edit) path | 00428860 | `process_pending_input` |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_pointer_tool.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

The original's bubble creation (00426580), bubble text (0042a1a0), bubble
dismiss (bubble vtable +0x40) and speech-range event (00422f90) are
recorders. Each case starts with or without a persistent bubble and a
random text, then runs 8 random transient or persistent texts of 0 to 24
letters. After each, the calls, the bubble held and the text (up to its
NUL) must match. The bytes after the NUL are not compared: LC clears them,
the original's strcpy_s leaves them.

First run (2026-11-09): 10,000 cases.
- Found L38: 846 cases differed, all the same. With no bubble up, an
  empty persistent text makes an empty bubble in the original; LC made one
  and put it away at once.
- After the fix: 0 differing cases in 20,000, LC native and as shipped.
- Controls: the bubble at 0x60 or the text at 0x69 on the original's side
  makes 2,996 and 2,983 of 3,000 cases differ.

Shift-click (added 2026-11-09): each case also runs 4 shift-clicks. The
pointer's position and hotspot are random. The registry holds 0 to 6
objects and the pointer, and the scenery list 0 to 4 objects, each with
random bounds near the hotspot and a random render plane. The original's
UpdateMovementBounds runs as native code. The object chosen for editing and
the pending input flags must match.
- Found L39: LC chose the object with the highest render plane. With the
  old code, 310 of 3,000 cases differ.
- After the fix (20,000 cases, 80,000 shift-clicks: 22,381 chose a registry
  object, 11,430 a scenery object): 0 differing cases, LC native and as
  shipped.
- Controls: the hotspot at 0x58 or 0x64 on the original's side makes 1,772
  and 1,777 of 3,000 cases differ.

## Genome loci: `Creature::ResolveGenomeLocus` @00408d30 (CE 10.3)

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_locus.sh \
        <CE 10.3 Creatures.exe> 1 <build-dir>

Genes reach a creature's state only through this function (vtable slot 0
of the sub-object at Creature +0x804). The test resolves every (kind,
tissue, locus) a gene can name on both sides: receptor and emitter,
tissues 0 to 7, loci 0 to 255. The original gives an address in its
Creature block, LC a field of its Creature. The pairs must be one to one,
"no locus" must agree, and every field of LC's `ControlState` must be at
+0x2c24 plus its place in the struct.

First run (2026-11-09): 84 loci resolve to 60 fields (the control state,
the floating and gait loci, the drives, the pose transitions and the death
state); 4,012 resolve to nothing on both sides. 0 failures, LC native and
as shipped. Control: with the control state at 0x2c25, 82 failures.

## The voice: `Creature::SpeakTextWithVoice` @0040b560 (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `Voice::PrepareSpeechText` | 004452b0 | `Voice::prepare_speech_text` |
| `Voice::GetNextSoundPair` | 004453c0 | `Voice::get_next_sound_pair` |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_voice.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <install-dir>

The original's Voice (Creature +0x29c0) is built by its own constructor
(00445040). Its CString work runs in the install's real MFC (mfc140.dll,
only read). LC's side is the loop of
`WindowsCreatureSpeechPhraseHost::speak_text_with_voice` (platform code,
which the test cannot link), copied into the test, over LC's real `Voice`
reached through `Creature::voice()`. The sound queue (0042ce20) and the
bubble (00426580) are recorders. Each case has random voice tables and 4
random phrases. The sounds queued, the delay and the bubble's lifetime must
match.

First run (2026-11-09): 2,000 cases, 8,000 phrases, 51,693 sounds, 0
differing (native and as shipped). Controls: the masks, sound ids,
durations or delay moved by 4 bytes make 301 to 500 of 500 cases differ.

## The bacterium: `Creature::UpdateBacteriumAndEnvironment` @00409190 (CE 10.3)

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_bacterium.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

Both creatures are dead, so the function stops after the bacterium. The
original's AddChemicalMoles (00401890) runs as native code on a native
biochemistry block. LC's bacterium is set through its own archive load.
Each case has a random bacterium and random concentrations; after one
update, the bacterium's state and all 256 concentrations must match.

First run (2026-11-09): 100,000 cases (66,577 with a live bacterium, 33,250
killed, 11,128 activated), 0 differing (native and as shipped). Controls:
the thresholds swapped, or the state, input or outputs moved, make 1,091 to
3,303 of 5,000 cases differ.

## The status for kits: `Creature::FormatStatusForExternalQuery` @0040e520 (CE 10.3)

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_status.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <install-dir>

The original's history names are CStrings, built and assigned by the
install's real MFC (only read). Its string resources come from a stub of
`CString::LoadString` that gives "<id>"; LC's `StatusStrings` hold the same
texts for the ids its comments name. The map's rooms are the same random
rooms on both sides. Each case has a random display name and genome
moniker, age, sex, child, death state, down foot, the gestation, life-force
and sickness chemicals, and 0 to 6 rooms. The two ten-field texts must be
the same.

First run (2026-11-09):
- Found L40: at gestation stage 0 (row 15), LC native printed the whole age
  ("54:49"); the original prints the minutes alone ("49"), the last number
  it formatted. 297 of 3,000 cases.
- After the fix (20,000 cases): LC native 0 differing. LC as shipped 1,535,
  the same cases as the build with only rows 15 and 43 fixed.
- Controls: the display name and genome moniker swapped make every case
  differ; the age at 0x83c makes 1,560 of 2,000 differ.

## The runtime reset: `Creature::InitializeRuntimeState` @00408520 (CE 10.3)

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_initialize.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <install-dir>

The test compares field by field through the load test's own table
(`load_dump_fields.inc`), with the formatters both dumps use
(`load_dump_native.hpp`, `src/c1/creatures/load_dump_lc.hpp`). The original
runs twice on a creature block, filled with 0x00 and then with 0xff; its
ten history CStrings are built by the install's real MFC (only read), and
its string resource comes from a `LoadString` stub. A table field that
comes out the same both times is one the reset writes, and it must match
LC's after LC's reset. LC's host gives what the game's does: the default
stimulus contexts, the resource text and the "%lx" monikers. Random
genome, mother and father monikers go in; the reset writes them into the
history as text.

First run (2026-11-09): of the table's 1,666 lines, the reset writes 835.
- Found L41: the reset sets the classifier to 0x04000000 (family 4); LC's
  left it.
- After the fix: 500 cases, 0 differing, LC native and as shipped.
- Controls: the father and mother rows swapped, or the birthplace at 0x820,
  make every case differ.
- New rows it proves: the father's (0x818) and mother's (0x81c) monikers
  and the birthplace (0x824).

## Object variables: the CAOS r-values `obv0` to `obv2` (CE 10.3)

| Function | Address | LC |
| --- | --- | --- |
| `Macro::ParseRValue` (`obvN`, 0041aae2) | 0041a9d0 | `Macro::parse_rvalue` |

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_rvalue.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

The original's macro is a block with the script text "obvN " at +0xc (the
cursor at +0x10, the length at +0x14) and TARG at +0xa8. Its `obvN` path
returns the dword at TARG + 0x34 + 4 * N, with no other call. LC runs its
own interpreter on the same text with an `Object` whose three variables
hold the same random values.

First run (2026-11-09): 20,000 cases, 0 differing (native and as shipped).
Control: with the original's variables at 0x38, every case differs.

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

## Environment and life stage: `Creature::UpdateEnvironmentAndLifeStage` @0040c8d0 (CE 10.3)

What a creature does each tick with its environment and age:
- it sets the heat, cold and light loci from the room type and the season;
- it sets the nearness locus when a creature of its own kind is near;
- it sets the fertile, pregnant and ovulation loci from the gamete and
  child genomes;
- it goes to the next life stage when that ageing locus is set, and the
  genome is expressed again;
- it dies when the death locus is set.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_life.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir>

The native creature is a Creature-shaped block with the control-state loci,
the life stage, the gamete and child monikers, the genome moniker, the
classifier, the death state and the attention records. The room table and
the season's ambient index are at the MapData pointer. The genome
re-expression (0040d930) and Die (0040dc30) are recorders on both sides.

Each case: random loci, a stage 0 to 7, gamete and child monikers, a room
table, a season, and possibly a creature of the same kind near it. Then 6
updates, with random changes to the ovulation, ageing and death loci
between them. After each update these must match:
- the control-state loci;
- the life stage and the gamete moniker;
- the calls to re-express and to die.

First run (2026-10-18): 20,000 cases, 120,000 updates, 11,688 stage
changes, 17,650 calls to die.
- LC native: 0 differing cases.
- LC as shipped: 0 differing cases. The function has no switch.

## Gene expression: `Creature::LoadGenome` @00408960 (CE 10.3)

The creature genes of a genome at one life stage:
- the 36 built-in stimulus contexts;
- the 100 poses;
- the 8 gaits;
- the instincts (20 at most).

The genome re-expression (0040d930) calls this function at each stage
change.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_express.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The native genome reader is a CGenome in memory (vtable 00456094), with the
genome bytes, the cursor, the size, the moniker, the sex and the life
stage. A zero pad and a `gend` follow the genome bytes, so a read past
their end (row 81) is the same on each run. The native creature is a
Creature-shaped block with the stimulus
contexts, the pose and gait tables, the instinct list and a brain block
with the lobe sizes. The lobe sizes come from LC's brain, grown from stage
0 of the unchanged genome. The MFC operator new is bound to malloc.

Each case: one genome, a sex, a life stage 0 to 6, and random earlier
contents in each table. In half the cases, up to 40 bytes of the genome
change (never in a gene or end marker). One change in four is 0, 100 or
200, the values that end a gait. After the load these must match:
- the 36 stimulus contexts;
- the 100 poses;
- the 8 gaits, all 18 bytes;
- the instinct list.

First run (2026-10-18): the 12 mum and dad genomes of the CE 10.3
Genetics folder, 20,000 cases, 51,939 instincts loaded. Second run, with
the pad and all 14 genomes (`*.gen` and `*.GEN`): 20,000 cases, 49,462
instincts loaded.
- The test found two translation defects in LC, now fixed:
  - LC tested for the zero byte that ends a gait before it reduced the
    byte modulo 100. Native reduces first, so 100 and 200 also end the
    gait. LC gave a longer gait than the original.
  - LC cleared a gait before it wrote the gait. Native keeps the bytes
    after the R terminator. No reader and no save looks past the
    terminator, so only the test sees this difference.
- LC native: 0 differing cases.
- LC as shipped: 0 differing cases. The function has no switch.

## Biochemistry genes: `CBiochemistry::LoadGenome` @0042e940 (CE 10.3)

The biochemistry genes of a genome at one life stage: receptors, emitters,
reactions, half-lives and initial concentrations. The genome re-expression
(0040d930) calls this function at each stage change.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_biogenes.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The genome reader is as in the gene expression test. The native
CBiochemistry has its owner creature at +4. A receptor or emitter locus
resolves through the owner: the brain's resolver at [creature+0x2c18]+4,
the creature's at creature+0x804, through vtable slot 0 as
thiscall(kind, tissue, locus). Both sides resolve through one function: a
hash of the domain, kind, tissue and locus gives a byte in a pool of 48, or
no locus. Different genes share loci, so the de-duplication of receptors
and emitters runs.

Each case: one genome, a sex and a last life stage 0 to 6. Half the cases
change up to 40 bytes of the genome, as in the gene expression test. Both
sides load stage 0, then each later stage up to the last, on the same
biochemistry, as the game does. After each load these must match:
- all 256 chemicals (concentration and half-life);
- the receptor, emitter and reaction records and their counts;
- the locus of each record, and the pool bytes.

First run (2026-10-18): 14 genomes, 20,000 cases, 80,451 loads; 1,789,890
receptor, 1,533,781 emitter and 4,346,753 reaction records compared.
- Before the pad, 1 case in 2,000 differed in a half-life: a mutation had
  made a reaction gene near the end a half-life gene, so native read 256
  bytes past the last `gend` and then found a gene in heap memory. This is
  row 81. LC read past its vector; it now reads 0 past the end and its
  scan stops at the end.
- LC native: 0 differing cases.
- LC as shipped: 0 differing cases. The function has no switch.

## Brain genes: `CBrain::LoadGenome` @00404560 (CE 10.3)

The brain of a new creature: the lobe genes of both load passes
(`CLobe::LoadGenome` @004025d0), the neuron and dendrite arrays
(`AllocateRuntimeState` @00402840), `InitializeConnections` @004047c0 and
the perception wiring. The genome re-expression (0040d930) calls it at
stage 0 only, so the cases are at stage 0.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_braingenes.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The genome reader is as in the gene expression test. The native CBrain is a
zeroed block. The MFC operator new and new[] (#1507, #265) are bound to an
allocator that zeros the memory. rand is the MSVC formula on both sides,
seeded alike.

Each case: one genome and a sex. Half the cases change up to 40 bytes of
the genome, as in the gene expression test. After the load the brains must
be the same in the world format: the parameters and rules of each lobe,
and every neuron and dendrite. The rand state must also match.

EQUIVALENCE_FILL=<byte> fills new memory with that byte instead of zeros.
With it, native leaves three fields as the heap had them: each neuron's
firing and activation, and each dendrite's susceptibility (row 82).
EQUIVALENCE_MASK_STATE=1 sets those three to 0 on the native side before
the compare.

First run (2026-10-18): 14 genomes, 5,000 cases, 5,263,361 neurons and
110,453,879 dendrites.
- The test found four translation defects in LC, now fixed:
  - LC drew the type 0 and type 1 dendrite counts of each neuron in
    turn. Native draws the type 0 counts of all neurons, then the type 1
    counts, so each neuron got a different count from the same rand
    stream.
  - For spread 2 and 3, LC took |a - b| of the two draws. Native takes
    |a + (b - range)|. The distribution is the same, but not the count
    per draw.
  - LC mapped a lobe size byte outside 1..63 to ((byte - 1) % 63) + 1.
    Native maps it to (byte % 63) + 1, so 0 gives 1 and 64 gives 2.
  - LC clamped every lobe larger than 32x32 to 32x32. Native clamps only
    lobes 1 to 7; the perception lobe, the concept lobe and any lobe after
    it keep their size.
  The first two change every new brain; the last two only mutated
  genomes.
- LC native: 0 differing cases; with EQUIVALENCE_FILL=205 and
  EQUIVALENCE_MASK_STATE=1, 0 differing cases in 1,000.
- LC as shipped: 0 differing cases. The functions have no switch.

The skeleton genes have their own test, below.

## Skeleton genes: `Skeleton::LoadGenome` @0043c800 (CE 10.3)

What the function does with a genome at one life stage:
- it reads the genus, appearance and pigment genes;
- it finds each body part's sprite and attachment files, with a fall back
  to a lower life stage or variant, then to the other sex;
- it writes the creature's own sprite file, with each body-part image
  remapped through the pigment palette (`BuildCreaturePaletteRemap`
  @00413d20);
- it loads that file as the creature's gallery, chooses a render plane, and
  builds the body and its limbs.

The genome re-expression (0040d930) calls this function at each stage
change.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_skelgenes.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <data-dir> <genome.gen>...

`<data-dir>` holds the game's Images, Body Data and Palettes folders (the
install folder). The script copies them into two new scratch roots in the
build directory, one for each side. Each root also has an empty "out"
folder, the secondary image directory, where the sprite file is written.

Native runs all of the function, on real files. The test supplies only
what is outside the exe:
- MFC CFile (open, read, write, seek, close) on Win32, as MFC's does;
- CPtrArray::SetAtGrow and RemoveAt, and the MFC new and delete;
- the attachment file stream: its constructor and destructor (0043dd40,
  0043c610) and `operator>>(int&)`;
- strcpy_s, strcat_s, ceil, GetFileAttributesA (the real one), and rand (the
  MSVC formula on both sides);
- the app object with the directory paths, and palette.dta, given to both
  sides.

The sprite-file cache index is built by its own start-up initializer
(004016c0). The native skeleton starts from its initializer (0043ac70); LC's
from a new Creature, and the test first checks that the two start states
match. The down foot, a room around it and its movement bounds are the same
on both sides.

LC uses the application resource host (`C1ResourceHost`) on a Win32 file
backend: the game's backend reads through `std::ifstream`, which MSVC
opens without regard to case, but the MinGW runtime under Wine does not. The
gallery release counts references as native's does. (The game's
`WorldRuntime` also keeps released galleries for reuse; this test does not
include that.) The test is built with SSE maths, as the MSVC build of the
game is, for the float steps of the palette remap.

First, `BuildCreaturePaletteRemap` alone is compared with LC's remap on
20,000 random control sets (pigments, rotation, swap); a quarter of the
values are 0, 1, 127, 128, 129, 254 or 255. All 256 entries must match.

Then each case: one genome, a sex and a last life stage 0 to 6. Half the
cases change up to 40 bytes of the genome, as in the gene expression test.
Both sides load stage 0, then each later stage up to the last, on the same
skeleton.

When native cannot open a file during a load (its CFile throws there), the
test returns from the load, puts back the SEH chain head it saved before the
call, and closes the files the load opened for writing. LC must refuse the
same load, and the case stops there (row 84). After every other load these
must match:
- the creature's sprite file, byte for byte;
- the classifier, the genome file, the parent monikers and the render plane;
- the gallery's file and image count;
- every body part: image base, image, plane, position, pose, view and
  attachment table; the chain lengths;
- the down foot, the chain ends and the sprite bounds;
- the rand state.

First run (2026-10-18): 14 genomes, 500 cases, 2,097 loads (8 refused
by both sides), 25,068 body parts, 299 MB of sprite file; 20,000 palette
control sets.
- The test found two translation defects in LC, now fixed:
  - The creature colour remap was not the original's. The original
    measures the swap control from 127.5 and takes its size (so a swap
    below the middle mixes the same way as one above it); it computes the
    hue factor in double; and it cuts the rotated channels to whole numbers
    before the swap mixes them. With only part of the fix (the swap
    measured from 128), 13,802 of 20,000 control sets still differed.
  - A new skeleton started with drive expression 1. The original starts it
    at 0.
- The test found two defects in the original:
  - Row 83: the sprite file has two bytes too many after its frame table,
    so the original reads every body image two bytes early.
  - Row 84: when a body part has no file for any variant, stage or sex,
    the original opens the image folder as the sprite file, and its CFile
    throws. Genus 2 or 3, or a tail from an appearance gene, does this.
- LC native: 0 differing cases.
- LC as shipped: 492 differing cases, the same cases as the build with
  only row 83 fixed. (The 8 refused cases stop before row 83 shows.)

## Breeding: `CGenome::RecombineGenomeStreams` @00418a20 (CE 10.3)

How a child's genome is made from its parents' (the game's conception,
`GenerateOffspringGenomeFile` @004187f0, calls it when the father is not
zero): runs of 10 to 100 genes from one parent, a crossover at the next gene
whose identity the other parent also has, one in 81 crossovers duplicates
or omits a gene (when the gene's flags allow it), and every byte is copied
with a 1-in-1601 mutation (`CopyGenomeGeneWithMutation` @00418c70,
`CopyGenomeByteWithMutation` @00418e00).

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_breed.sh \
        <CE 10.3 Creatures.exe> <cases> <build-dir> <genome.gen>...

The native genomes are CGenome blocks in memory (vtable 00456094, bytes
+4, cursor +8, size +0xc), each parent followed by a zero pad and a gend.
rand() is the MSVC formula on both sides, from one state. The counters are
the globals 00467da8, 00467da4, 00467da0 and 00467d4c.

Each case: two stock genomes (one in four the same one twice). Half the
cases change up to 40 bytes of a parent; half flip duplication, omission
and mutation flags of random genes; one in three gives genes the identity
of the gene before them. The child's bytes, both parents' bytes, the four
counters and the number of rand() draws must match.

First run (2026-10-19), the 14 Genetics genomes: 20,000 cases (104,188
crossovers, 299 duplications, 290 omissions, 28,740 mutations), 0 differ in
the native build and as shipped. The first runs found L33 and L34 in
[`our-bugs.md`](../../docs/our-bugs.md). Row 7 (the pigment extension tag
is not mutated) is the only switch in this code; the rows build has it
fixed alone, and so far no case has hit it.

## The world file: `SFCDoc::Serialize` @004311a0 (CE 10.3)

The original loads a world and saves it again at once, and LibreCreatures
does the same with the same world; the two saves must be the same, byte for
byte.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_world.sh \
        <CE 10.3 Creatures.exe> <install-dir> <build-dir> <world.sfc> [<lc-save.sfc>] [--ticks <n>]

With `--ticks <n>` both sides run n world ticks (`SFCDoc::UpdateWorld`
@004324e0) between the load and the save.

Native side (`world_roundtrip_native.exe`): every import of the exe is
bound to the real DLL (MFC and the C++ runtime from the install folder, the
rest from Wine; `NativeImage::bind_real_imports`), and the exe's own C++
initializers run. The main frame and the document are made by their MFC
CreateObject functions. CFile and CArchive are MFC's own. The view, the
renderer and its window are zeroed fakes; the selection menu, the debug log,
the viewport request and the dirty-rectangle queue are stubbed, and the
move with redraw is redirected to the move alone. The script copies the
install's data folders and the world into a new scratch root and runs the
round trip two times; the two saves must be the same.

LC side: the game itself, because LC's world archive is MFC's CArchive. A
build with `LC_NATIVE_MODE` (c1-lab profile `librecreatures-native`) runs
once on a lab world with `C1_WORLD_ROUNDTRIP=<in .sfc>|<out .sfc>[|<ticks>]`
(c1-lab `run_once`). The hook makes a new document, starts the sound system,
sets the random seed to 1 (the C runtime's first seed, which the native side
keeps), loads, runs the ticks muted, saves and exits. The native side has no
sound manager and no autosave, and the event bar's pane text is not drawn.
A world with creatures can come from an experiment with `keep_world`; c1-lab
`stage_world` takes the kept folder. Row 18 holds the scripts that a load starts until their first turn,
so LC as shipped saves them at another point; the test uses the native
build.

First run (2026-10-18), a lab world made from `Eden.sfc`, 189 objects, no
creatures: the original's save is 186,924 bytes, the same in two runs, and
LC's is the same, byte for byte. The first comparison found L25 to L30 in
[`our-bugs.md`](../../docs/our-bugs.md). See also "The original's own load
and save" in [`verification.md`](../../docs/verification.md): what the
original's round trip changes in a world.

Second run (2026-10-18), a world the original saved with four stock norns:
0, 1 and 10 ticks are the same byte for byte; 45 ticks are the same in every
field (the original writes uninitialised bytes after a bubble's text). The
first comparison found L31 and L32. Past the first dendrite migration that
row 5 can change (tick 46 here), the original's own result depends on its
heap layout, so a longer run cannot be compared. On `neorender` a speech
bubble changes the save (shared balloon image, 0c0207e); use a `main` build
for runs that make bubbles. The tracer used here needs openc2e's `Bubble`
class registered.

### The load: where each field goes (`run_world.sh --dump`)

A round trip cannot see a field that the load puts in the wrong place when
the save puts it back the same way: the two saves are still the same, byte
for byte. The load test looks at memory instead.

- `src/c1/creatures/load_dump_fields.inc` lists Creature fields with their
  offset in the original, their kind and their LibreCreatures expression.
  Every offset there is one a function-level test in this folder proves (the
  row names the test): that test puts LC's value at the offset, runs the
  original's code and gets LC's result. Never take an offset from a struct
  name or from the order of Serialize's reads: pairing the n-th read on each
  side makes the test circular, and it can no longer see a swap.
- After its load, `world_roundtrip_native.exe` writes every field of every
  creature (the list at 0x4704bc, count 0x4704c0) when `C1_WORLD_FIELD_DUMP`
  is set. The game writes the same lines from its own fields when
  `C1_WORLD_FIELD_DUMP` is in the env of the `C1_WORLD_ROUNDTRIP` run. Both
  use the formatters in `src/c1/creatures/load_dump.hpp`.
- The original's two runs fill each new Creature (0x2cdc bytes, from the
  import slot 0x450aec in `Creature::CreateObject` @00406f40) with 0x00 and
  with 0xff first (`C1_WORLD_FILL`). A field whose line differs between the
  two is one the original never sets; `load_compare.py` lists it and leaves
  it out. The two runs' raw creature bytes (`C1_WORLD_RAW_DUMP`) go to
  `load_coverage.py`: a byte the same in both was set, and the bytes no
  table field covers are listed as ranges. That list is what to add next.
- `run_world.sh ... --lc-dump <file>` compares the dumps before the saves.

    WINEPREFIX=<throwaway prefix> tests/equivalence/run_world.sh \
        <CE 10.3 Creatures.exe> <install-dir> <build-dir> <world.sfc> \
        [<lc-save.sfc>] --lc-dump <lc-dump.txt>

First run (2026-11-07), the world with four stock norns above, 0 ticks,
with the 80 learned-word records only: the saves were the same, but one
record had its two words the other way round in LC (L35 in
[`our-bugs.md`](../../docs/our-bugs.md)).

Second run (2026-11-09), the whole table (64 fields of the Creature and the
Skeleton and Object it starts with; 5,929 lines for 4 creatures):

- The original never sets 24 of the lines: `motion_x`, `motion_y`,
  `goal_verb`, `goal_commitment`, `goal_countdown` and two control-state
  bytes keep what the heap held. They are not compared.
- 328 lines differed, from one cause (L36): LC built a loaded creature
  without what `CreateObject` does before the load. Every attention record
  had neuron 0 and was not visible (the original: 255, visible), and the
  always-on and ambient-light signals were 0 (the original: 255 and 128).
- After the fix: every compared line (5,905) is the same, and the saves at
  0 and 10 ticks are the same, byte for byte.
- Coverage: the table covers 6,550 of the 7,601 bytes the original's
  constructor and load set in a Creature (86.2%). The largest uncovered
  blocks are 0x29c0-0x2c13 (596 bytes, the member CreateObject constructs at
  +0x29c0), the two pointers at the start of each of the 36 stimulus
  contexts, 0x804-0x83f and the Object fields at 0x20-0x4c.

Third run (2026-11-09): the object registry and the brains as well.

- `load_dump_objects.inc` has rows per class (Object, SimpleObject,
  CallButton, CompoundObject, Vehicle, Lift), each with an offset the
  vehicle, simple-object or goal test proves. A row applies to every object
  of that class or one derived from it: the original's side reads the
  class from MFC's own CRuntimeClass chain (vftable slot 0, the base class
  at +0x10), LibreCreatures' side uses dynamic_cast. Both walk the object
  registry in its order (the original's array 0x4704a8, count 0x4704ac).
  `Object`'s protected timer period is read through `ObjectFieldDump`.
- `load_dump_brain.inc` has rows per lobe and per neuron for every
  creature's brain. Their offsets come from the brain test, which builds
  the original's brain at them from LibreCreatures' saved brain and finds no
  difference in 200,000 cases: that proves LC's save, and the round trip
  proves the load reads each value back into the field the save writes it
  from. The rows that matter most are those a load builds instead of
  reading: the active count, the migration candidates and the loose-dendrite
  counts. Dendrites are left out (saved, and so already covered that way).
- The original's object raw bytes (`C1_WORLD_OBJECT_RAW_DUMP`: per object
  its class chain, size and bytes) give coverage per class.
- The fill runs are now separate from the save runs: filling every new
  block changes the save (the original saves some bytes it never sets, such
  as those after a speech bubble's text, original-bugs row 87).
- Result: 40,667 lines, 328 never set by the original; every one of the
  other 40,339 is the same in LibreCreatures. No new bug.

Fourth run (2026-11-09): the Object fields the third run could not see.

- New tests prove their offsets: the object test (constructor, continuous
  sound, destructor, SimpleObject's click selectors), the r-value test
  (`obv1`, `obv2`) and two new vehicle operations (part bounds and knobs,
  clear references). See their sections above.
- New rows: Object's CAOS pointer (0x20), sound handle, id and persist flag
  (0x28, 0x2c, 0x30), object variables 1 and 2 (0x38, 0x3c), gallery (0x40)
  and tick enabled (0x4c); SimpleObject's three click selectors (0x58);
  CompoundObject's six part bounds (0xcc) and all six knob hotspots (0x12c).
- Result: 42,916 lines, 328 never set by the original; every one of the
  other 42,588 is the same in LibreCreatures. No new bug.
- Coverage per class: CallButton 95.5%, CompoundObject 98.4%, Lift 98.9%,
  SimpleObject 95.2%, Vehicle 98.6%. For each, the only bytes not covered
  are the vftable pointer (0x0-0x3), and the class is already compared
  through the class chain. Blackboard (51.4%) and PointerTool (66.1%) have
  no rows of their own yet.

Fifth run (2026-11-09): Blackboard and PointerTool.

- New rows, from the blackboard and pointer-tool tests: the board's words,
  values, display bytes and edit flag; the pointer tool's bubble and text.
  `load_dump_classes.hpp` names `brain::Blackboard` and `ui::PointerTool`
  as `objects::` classes for the dump.
- Result: 42,956 lines, 328 never set by the original; every one of the
  other 42,628 is the same in LibreCreatures.
- Coverage: Blackboard 99.2%, PointerTool 90.1%: the hotspot was not yet
  proven.
- With the shift-click test, the hotspot rows were added: 42,958 lines, all
  42,630 compared the same; PointerTool 96.7% (only the vftable pointer is
  not covered).

Sixth run (2026-11-09): the Creature's gaps.

- New rows: the body (0x5c), the six limb chains (0x60) and the normal
  render plane (0x84), from the skeleton and skeleton-gene tests; the
  floating and gait loci (0x2c46, 0x2c4e), from the locus test; the voice's
  masks, sound ids, durations and delay, from the voice test; the
  bacterium's eight bytes (0x2cd4), from the bacterium test.
- `load_coverage.py` now counts the object table's Object rows for the
  creatures too (the object dump compares them for every creature).
- Result: 43,670 lines, 328 never set by the original; every one of the
  other 43,342 is the same in LibreCreatures.
- Coverage: 7,233 of the 7,601 bytes a Creature sets (95.2%). Not covered:
  - the vftable pointer (0x0-0x3);
  - 0x804-0x83f: the locus resolver's vftable, the history record (a
    sub-object with its own vftable at 0x808, the creature pointer, five
    CStrings for the names and the birthplace, and the age counter at
    0x838);
  - the target and source pointers at the start of each of the 36
    stimulus contexts. The original writes them just before it copies the
    context into the stimulus queue and never reads them back; LC builds
    the queued record directly and keeps none. They are null after a load
    on both sides.

Seventh run (2026-11-09): the history record.

- New kind `CStr`: an MFC CString, a pointer to its text; the native dump
  follows the pointer.
- New rows, from the status test: the genome moniker (0x810), the display
  name (0x814) and the age (0x838).
- Result: all 43,354 compared lines the same; coverage 95.3%. The rest of
  the history record (the father's and mother's monikers, the birthplace)
  is not yet proven by a test.

Eighth run (2026-11-09): the rest of the history.

- New rows, from the runtime-reset test: the father's (0x818) and
  mother's (0x81c) monikers and the birthplace (0x824).
- Result: 43,694 lines, 328 never set; all 43,366 compared the same.
  Coverage 95.5%. Still not covered: the vftable pointer, the locus
  resolver's vftable (0x804), the history's vftable and creature pointer
  (0x808, 0x80c), its CStrings at 0x820 and 0x828-0x834 and the word at
  0x83c, and the 36 stimulus contexts' target and source pointers.
- Object coverage per class: SimpleObject 57%, CallButton 60%, Vehicle 51%,
  Lift 62%, CompoundObject 44%, PointerTool 40%, Blackboard 25%. The gaps
  are the same in every class: Object 0x20-0x23, 0x28-0x33, 0x38-0x43
  (object variables 1 and 2 are there, but only variable 0 is proven) and
  0x4c; SimpleObject 0x58-0x5a; CompoundObject 0xcc-0x12b. A field there
  needs a test that proves its offset first.
- Biochemistry has no rows here: the biochemistry test already loads LC's
  side through its own world archive and compares it with the original, so
  its load is proven there.
