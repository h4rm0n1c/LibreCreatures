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
  environment variable `C1_NATIVE_ROWS` names.

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

The perception, goal-direction, geometry, fan-out and skeleton tests
found seven defects in LibreCreatures. All are fixed:

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

## What the tests do not cover

- Functions without a function test. Falling and other movement of
  simple objects, and the world file have no function test. The CAOS tests and the COB replay test some of their behaviour.
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
