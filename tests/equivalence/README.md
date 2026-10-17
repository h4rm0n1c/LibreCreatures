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
(target, position, sense neuron and activation), the attention (2) and
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

First run (2026-10-17), the 12 stock genomes: 200,000 cases, 499,582
stimuli.
- LC native: 0 differing cases.
- LC as shipped: 43,365 differing cases, the same cases as with row 9
  alone fixed; row 9 is the only switch in apply_stimulus.

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
