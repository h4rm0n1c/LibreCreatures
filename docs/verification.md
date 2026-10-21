# Comparison with the original game

LibreCreatures is not compiled to the same bytes as the original
`Creatures.exe`. This page lists the tests that compare its behaviour with
the original executable, their results, and what they do not cover.

The reference is the Creatures 1 Community Edition executable, release
10.3 (CE 10.3). The tests only read it.

A row is a numbered entry in [Bugs in the original game](original-bugs.md).

## Native mode

LibreCreatures fixes defects of the original game. Each fix is a row. Each
row has a switch in `src/c1/common/compat.hpp`.

- The default build has all fixes on.
- The native build (`LC_NATIVE_MODE`) has all switched fixes off. It must
  behave as the original does.
- The runtime build (`LC_COMPAT_RUNTIME`) turns off the fixes that the
  environment variable `C1_NATIVE_ROWS` names. The variable overrides
  `librefixes.ini`.

Some rows have no switch. They are guards against crashes and memory damage
in the original. A crash cannot be compared.

The tests compare the native build with the original. When the default
build differs from the original, the tests find the rows that cause the
difference.

## Function tests

Directory: `tests/equivalence`.

Each test calls one function of the original and the LibreCreatures
function that replaces it, with the same input. Then it compares the
results.

- The test loads a copy of the original executable into its own process
  under Wine. No start-up code of the original runs. If the code under test
  calls a Windows import, the test stops and gives the name of the import.
- The original's `rand` and LibreCreatures' `rand` use the same formula and
  the same seed.
- The inputs are random. They stay in the range that the game can make. The
  tests do not make inputs that crash the original.
- Some functions call other parts of the game, for example scripts and
  speech. On the original side, a recorder replaces these calls. Both sides
  get the same answers, and the test compares the calls.

Results, with the 12 genomes of the original game (mum1 to mum6, dad1 to
dad6):

| Function in CE 10.3 | Address | Cases | Native build | Default build |
| --- | --- | --- | --- | --- |
| `CBiochemistry::Update` | 0042ee10 | 200,000 (899,974 updates) | 0 cases differ | 55,452 cases differ, all from rows 50 and 51 |
| `CBrain::Update` | 00404e90 | 20,000 (70,008 updates) | 0 cases differ | 6,526 cases differ, all from rows 2, 4, 5 and 6 |
| `Creature::ApplyStimulus` | 0040b8a0 | 200,000 (500,153 stimuli) | 0 cases differ | 43,866 cases differ, all from row 9 |
| `Creature::UpdateActionSelection` with `Creature::SetAction` | 0040c630, 0040b680 | 200,000 (1,068,397 updates) | 0 cases differ | 10,357 cases differ, all from row 11 |
| `Creature::ProcessDreaming` with `CInstinct::RunDreamStep` | 00409360, 00407370 | 20,000 (767,905 dream steps) | 0 cases differ | 8,250 cases differ, all from rows 2, 4, 5, 6, 50 and 51 |
| `Creature::UpdatePerception` with `Creature::CanPerceiveObject` | 0040bf10, 00409750 | 200,000 | 0 cases differ | 82,231 cases differ, all from rows 9, 38 and 73 |
| `Creature::UpdateGoalDirection` with `Creature::ApplyGoalDirection` | 0040ccc0, 0040abc0 | 200,000 | 0 cases differ | 0 cases differ (no row in this code) |
| Map geometry: `FindNearestMapRoomBoundsAtPoint`, `MapData::FindRoomContainingPoint`, `GetAmbientTemperatureAtPoint`, `Object::UpdateMovementBounds` | 00422d70, 00422df0, 00422e40, 00426470 | 200,000 (6,400,000 points, 1,600,000 objects) | 0 cases differ | 167,929 cases differ, all from row 22 |
| Creature fan-out: the seven `MapData::QueueEvent...` and `Queue...Stimulus` functions | 00422f90 to 00423470 | 100,000 (600,000 calls) | 0 cases differ | 19,406 cases differ, all from rows 17, 22 and 73 |
| Skeleton: 13 functions of body layout, stepping, poses and movement | 0043b5a0 to 0043c380 | 200,000 (3,200,000 operations) | 0 cases differ | 16,358 cases differ, all from row 22 |
| Vehicles, lifts and call buttons: 11 functions | 00429a70 to 0042c680 | 50,000 (8,045,880 operations) | 0 cases differ | 36,625 cases differ, all from rows 23, 41, 45 and 52 |
| Simple objects: pick-up, carrying, putting down and the edit drop | 00426ee0 to 00428d30 | 50,000 (2,000,000 operations) | 0 cases differ | 5,265 cases differ, all from rows 22 and 27 |
| Learned words and phrases: 7 functions | 00409e80 to 0040b160 | 50,000 (1,200,000 operations) | 0 cases differ | 12,663 cases differ, all from row 14 |
| `Creature::ProcessHeardWords` with the "look" search | 0040a630, 0040ac90 | 50,000 (400,000 sentences) | 0 cases differ | 28,016 cases differ, all from rows 9, 37 and 39 |
| `Creature::UpdateEnvironmentAndLifeStage` | 0040c8d0 | 20,000 (120,000 updates, 11,688 stage changes) | 0 cases differ | 0 cases differ (no row in this code) |
| `Creature::LoadGenome`: the creature genes at each life stage | 00408960 | 20,000 (14 genomes, half with changed bytes) | 0 cases differ | 0 cases differ (no row in this code) |
| `CBiochemistry::LoadGenome`: the biochemistry genes, stage by stage | 0042e940 | 20,000 (80,451 loads) | 0 cases differ | 0 cases differ (no row in this code) |
| `CBrain::LoadGenome`: the brain of a new creature | 00404560, 004025d0, 00402840, 004047c0 | 5,000 (5,263,361 neurons, 110,453,879 dendrites) | 0 cases differ | 0 cases differ (no row in this code) |
| `Skeleton::LoadGenome`: the body sprite file, gallery and body parts, stage by stage, with `BuildCreaturePaletteRemap` | 0043c800, 00413d20 | 500 (2,097 loads, 299 MB of sprite file; 20,000 palette control sets) | 0 cases differ | 492 cases differ, all from row 83 |
| `CGenome::RecombineGenomeStreams`: a child's genome from two parents, with the gene and byte copiers | 00418a20, 00418c70, 00418e00 | 20,000 (104,188 crossovers, 299 duplications, 290 omissions, 28,740 mutations) | 0 cases differ | 0 cases differ (row 7 is in this code; no mutation hit a pigment extension tag) |

The values that each test compares:

- Biochemistry: all 256 chemical concentrations and all loci, after each
  update.
- Brain: all fields of each neuron and dendrite, and the active count,
  active fraction and loose dendrite count of each lobe, after each update.
- Stimulus: the 40 attention records with their visible and seen flags,
  the inputs to the attention and sensory lobes, all chemical
  concentrations and the goal-direction matrix, after each stimulus.
- Action selection: the action state, each script lookup in sequence, the
  calls to the sleep indicator, the queued events and the speech choice,
  after each update.
- Dreaming: the brain, the chemicals, the instinct queue and the
  biochemistry tick, after each dream step.
- Perception: the 40 attention records, the attention, sensory and
  stimulus-source lobes, all chemical concentrations and the goal-direction
  matrix, after each update.
- Goal direction: the 40 attention records, the goal-direction state and
  the stimulus-source and verb lobes, after each update.
- Map geometry: the nearest room's rectangle, the room that contains the
  point, the temperature and the movement bounds.
- Creature fan-out: the queued object events, the queued stimuli and the
  stimulus context, after each call.
- Skeleton: every body part's image, position, pose, view and plane, the
  down foot, the chain ends, the bounds, the poses and the dirty
  rectangles, after each operation.
- Vehicles and lifts: the vehicle's parts, motion, collision and floors,
  the passengers, the call buttons, the scripts, the queued events and the
  queued stimuli, after each operation.
- Simple objects: each object's mode, carrier, position, plane and
  movement bounds, the scripts, the queued events and the queued stimuli,
  after each operation.
- Learned words: all 80 word records, the words said, the speech events
  and the rand state, after each operation.
- Heard words: the word and attention records, the stimulus-source, verb,
  sensory and attention lobes, all chemicals, the goal matrix and
  goal-direction state, and everything said, after each sentence.
- Life stage: the control-state loci, the life stage, the gamete moniker
  and the calls to express the genome and to die, after each update.
- Gene expression: the 36 stimulus contexts, the 100 poses, all bytes of
  the 8 gaits and the instinct list.
- Biochemistry genes: all chemicals, the receptor, emitter and reaction
  records with their counts and loci, and the locus bytes, after each
  stage's load.
- Brain genes: every lobe's parameters and rules, every neuron and
  dendrite, and the rand state.
- Skeleton genes: the creature's sprite file byte for byte, the
  classifier, parents and render plane, the gallery, every body part's
  image, position, pose, view, plane and attachment table, the down foot,
  chain ends and sprite bounds, and the rand state, after each stage's
  load; and all 256 entries of the palette remap.
- Breeding: the child's bytes, both parents' bytes (the original writes
  into the parent it duplicates from), the crossover, duplication,
  omission and mutation counts, and the number of rand() draws.

The perception, goal-direction, geometry, fan-out, skeleton, learned-word,
heard-word, gene-expression, brain-gene and skeleton-gene tests found
nineteen defects in LibreCreatures. All are fixed, and all are in
[`our-bugs.md`](our-bugs.md) with the other translation defects that
verification found:

- `apply_stimulus` set the seen flag of an attention record. The original
  does not.
- When an object came back into view, LibreCreatures used the seen flag of
  the record. The original uses the flag of the record for the object's
  current classifier.
- One row of the table that chooses a goal-direction verb was copied two
  places to the left. When the call button's interaction event was set,
  LibreCreatures chose the lift where the original chooses the call
  button.
- The game's host started the nearest-room function from an empty
  rectangle. The original keeps the caller's sides when no room spans the
  point.
- The overlap test of the fan-out counted a rectangle with no area as
  touching. The original uses IntersectRect.
- Four rows of the table that chooses a creature's pose toward its target
  were one row out of place.
- When a creature carried the object it moved toward, LibreCreatures did
  not see that it already held it, and chose a reach pose.
- A creature's baby-talk form of a long word was not repeated when it was
  three or four letters long.
- A sleeping creature did not hear. The original lets a sleeping creature
  hear.
- "look" also gave a verb.
- "look" from a creature in a vehicle searched the creature's own area
  instead of the vehicle's.
- A gait gene byte of 100 or 200 did not end the gait. The original
  reduces the byte modulo 100 before it tests for zero.
- A gait gene cleared the bytes after the gait's terminator. The original
  keeps them. Nothing reads them.
- A new lobe drew the two dendrite counts of each neuron in turn. The
  original draws the first count of all neurons, then the second.
- Two of the four dendrite count spreads used a different formula with
  the same distribution.
- A lobe size byte outside 1 to 63 gave a size one smaller than the
  original's.
- Every lobe larger than 32 by 32 was made 32 by 32. The original does
  this only for lobes 1 to 7.
- The creature colour remap was not the original's: the swap control's
  centre and sign, the hue factor's precision, and the cut to whole
  numbers before the swap.
- A new skeleton started with drive expression 1. The original starts it
  at 0.

The breeding test found two more (L33 and L34): breeding crossed over at a
gene whose identity repeated the gene just copied, and a duplicated gene
kept its generation number.

The biochemistry-gene, brain-gene and skeleton-gene tests also found four
defects in the original: rows 81, 82, 83 and 84 of
[`original-bugs.md`](original-bugs.md).

The procedure for each test is in
[`tests/equivalence/README.md`](../tests/equivalence/README.md).

## Table check

`tests/equivalence/check_tables.py` compares 29 tables that LibreCreatures
copies from the original executable with the bytes in the executable. All
29 are the same.

The check found two copy defects. Both are fixed:

- The table of words for actions had no word for "walk east". A creature
  said nothing when it walked east and wanted to say so.
- One unused byte of the ambient table was wrong.

## CAOS tests

Directory: `tests/caos_conformance`.

The test sends CAOS scripts through the kit pipe to the original and to
LibreCreatures, in a new Eden world. The output of each test must be the
same.

| Suite | Content | Native build |
| --- | --- | --- |
| `core.caos` | Variables, arithmetic, branches, loops, subroutines, `enum`, simple objects, the room table | 66 of 66 tests are the same |
| `objects.caos` | Compound objects, vehicles, appearance, targets, classifier wildcards, sound, scripts, timers, messages | 38 of 38 tests are the same |
| `creatures.caos` | New creatures from the original genomes: classifier, life stage, flags, chemicals, drives, position | 13 of 13 tests are the same |

Four more tests in `core.caos` (`native_error.*`) use defects that the
original shows as an error box (rows 55 and 56). The default build fixes
these defects. The comparison lists these tests separately.

## COB replay

A COB adds objects and scripts to a world. The replay installs COBs in a
new Eden world, one after the other, in the original and in LibreCreatures.
After each COB, the test records the classifier and attributes of each
object and the number of objects in each family. Then it compares the
records.

The 24 COBs in the CE install folder make 229 objects. The native build is
the same as the original. The default build differs in one attribute
(row 26).

The Creatures Caves archive has 434 COB files. The replay installs them in
8 groups.

- The replay found two defects in LibreCreatures:
  - A kit reply stopped at 4 KiB. The original allows 16 KiB. Commit
    8016615 fixes this.
  - `room` with a room number above 39 did not read its last value, so the
    rest of the script did not run. Commit b56a5c5 fixes this. `core.caos`
    has a test for it.
- After these fixes, each difference between the native build and the
  original is in a value that changes with time or random numbers: food
  that ripens, objects that make more objects, creatures that hatch. The
  two programs read the world at slightly different times, so these values
  can differ. The comparison reports attribute changes separately. COB 1802
  gave a different count in the replay and the same count when it was
  installed alone.
- Some COBs stop the original. LibreCreatures continues. The causes are
  rows of [Bugs in the original game](original-bugs.md):
  - Row 77: a `new:` object with a sprite file that is not there. COBs
    1401 and 891 do this. COB 1804 also uses a sprite file that is not in
    the archive or in the game.
  - Row 79: the renderer draws one picture past the end of a sprite range.
    COB 187 (Goldfish) does this. The original stops only when other COBs
    are also installed, because the result depends on the memory after the
    range.
  - Row 34: a box from a pipe script that stays open for more than 30
    seconds. The replay answers each box in 20 seconds.
- One file (772) is not a Creatures 1 COB.

## The original's own load and save

A world file does not come back unchanged when the original loads it and
saves it again at once. The world test will compare LibreCreatures with the
original's save, not with the input file, so these changes are expected in
both. They were measured on 2026-10-18 with the CE 10.3 executable running
in the function-test harness on the real MFC DLLs: `Eden.sfc` (177,269
bytes) was loaded with `SFCDoc::Serialize` @004311a0 and saved again at once
(186,920 bytes). The two files were compared field by field with a trace
built from openc2e's SFC reader.

| What changed | Count | Why |
| --- | --- | --- |
| Running scripts: 27 became 59 | 32 new macros | The load sends event 7 to every object (`Object::DispatchEvent7AfterBoundsUpdate` @00425c00, 189 calls). Event 7 scripts that wait are still running at the save. |
| Object variables (`obv0`, `obv1`) | 12 | Set by the event 7 scripts. |
| Animation frames set to 0 | 44 entities | The event 7 scripts start new animations. |
| Current image | 1 entity | Set by an event 7 script. |
| Gallery image `status` byte, 1 to 0 | 74 images | The save writes each image's pixel-cache flag. A world saved before anything was drawn writes 0. The value has no effect on a load. |
| Macro `gsub` cache cursor | 27 macros | `Eden.sfc` holds memory addresses here, written by the 1997 engine. CE saves an offset and makes any offset outside the script 0 on load (`Macro::Serialize` @004195f0). |
| The hand's position | 1 | The load puts the hand at the mouse (`SimpleObject::InitializeUnboundedObjectPlacement` @00428b50). |
| View scroll position | 2 values | The save writes the view's scroll position. |

With event 7 turned off in the harness, only the last four rows remain.
The last two rows depend on the view and the mouse. The harness has a
view at 0, 0, so they are 0 there; in the game they are the live values.

## The world test

`tests/equivalence/run_world.sh` loads a world in the original and saves it
again at once, two times, and compares the save with the save that
LibreCreatures makes of the same world. The LibreCreatures side is the game
itself: the `C1_WORLD_ROUNDTRIP` hook loads the world through MFC's
CArchive, saves it and exits. The test uses the build with every fix off
(`LC_NATIVE_MODE`), because row 18 holds the scripts that the load starts
until their first turn. The hook sets the random seed to 1, as the
original's C runtime starts.

First result (2026-10-18), CE 10.3, with a lab world made from `Eden.sfc`
(189 objects, no creatures): the original's save is 186,924 bytes and the
same in two runs. The LibreCreatures save is the same, byte for byte.

The first comparison found six translation bugs, now fixed: L25 to L30 in
[`our-bugs.md`](our-bugs.md). The load did not send event 7. Clearing the
map used five random numbers. A bad saved script cursor went to the end of
the script and not to 0. A completed `wait` did not end the script's turn.
`pose` deleted the animation buffer. The event bar forgot its two saved
words between a load and a save.

### World ticks

`run_world.sh --ticks <n>` runs `SFCDoc::UpdateWorld` @004324e0 n times
between the load and the save, on both sides. The native side has no sound
manager and no autosave; the hook mutes the world, so neither side takes a
random number for the ambient sound.

Second result (2026-10-18): a world that the original saved after 2,400
ticks with four stock norns (193 objects, 4 creatures, 521,765 bytes on a
round trip).

- 0 ticks: the same, byte for byte, after L31 was fixed.
- 1 and 10 ticks: the same, byte for byte, after L32 was fixed.
- 45 ticks: every field the same. The only bytes that differ are those
  after the end of a speech bubble's text: the original writes the
  uninitialised bytes of its 25-byte buffer there, and LibreCreatures
  writes zeros (original-bugs.md row 87).
- Both sides take the same number of random numbers in every tick.

Two limits:

- At tick 46 a dendrite migration in one norn succeeds in LibreCreatures
  and fails in the original. Row 5: the original's duplicate test packs
  neuron addresses into 16 bits, so its answer depends on where the brain
  is in memory. The original alone, run 60 ticks with its heap moved by a
  few bytes, gives different saves. A multi-tick comparison is exact only
  until the first migration that this can change.
- On `neorender`, speech bubbles share one balloon image and draw their
  words as an overlay (0c0207e); this is a kept deviation, also in native
  mode. The original gives every bubble its own
  copy, so with a bubble in the world the saved image flags differ, and with
  two the object references in the file differ too. The 45-tick result is
  from a `main` build, which keeps the original's bubbles.

Worlds with eggs and running kits are not yet compared.

## What the tests do not cover

- Functions without a function test. The hand's own movement has no
  function test. The world test compares two worlds, one with four
  norns, and only until the first migration that row 5 can change. The CAOS tests and the COB replay test some of their behaviour.
- Pictures and sound. The tests do not compare the picture or the sound
  output.
- The kits. The tests do not compare the kit programs.
- Inputs that crash the original. A guard row replaces the crash, so there
  is no result to compare.
- Byte identity. The tests compare behaviour, not machine code.
- Other platforms. The tests run under Wine on Linux.

## How to run the tests

- Function tests: build them with `tests/equivalence/build.sh`. Each
  `run_*.sh` script runs the native build, the default build and the row
  builds, and checks the rows that cause each difference. See
  [`tests/equivalence/README.md`](../tests/equivalence/README.md).
- CAOS tests and COB replay: see
  [`tests/caos_conformance/README.md`](../tests/caos_conformance/README.md).
  These tests use the c1-lab test harness.
