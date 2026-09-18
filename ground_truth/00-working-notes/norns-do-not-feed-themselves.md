# Feeding: the differential, and the sampling artifact that misled it

Recorded 2026-09-18.  **Read the CORRECTION at the end first.**  The premise of
this note -- that the port fails to feed its creatures -- was falsified by a
longer, matched measurement.  The sections below are kept in order because the
fixes found along the way are real, but their framing is not.

## The measurement

`knowngood` staged twice: once for the port, once for the 1996 `Creatures.exe`.
Both queried through the Vivarium DDE service with the same client
(`external/creaturesstructs/Creatures1/dde-service/vivarium_client.cpp`),
sampling the selected creature's Hunger (chemical 3), Starch (57) and
Saccharin (35):

    original   hunger 0 5 7 35 6 1 25 53      starch spikes 94, 73, 35, 28
    port       hunger 254..255 for the run    starch 0, saccharin ~2

The original's norn eats repeatedly and its hunger never saturates.  The port's
hunger pegs at maximum and stays there until the creature starves.  Every
creature in a port world eventually dies of this; a world left running for
~80 minutes had all three dead.

## What is NOT broken

Each link was measured working in the port before the differential was run:

  - drives rise (Hunger reaches and holds 255)
  - biochemistry accepts and reacts (injected Pain 0 -> 69, then decays)
  - attention lands on food (`_it_` = class 2.6.2)
  - approach completes (432 `appr-ready` in one run)
  - `TOUC` completes (96 successes, including 15 while attending food)
  - messages dispatch (129 creature->object sends)
  - the food's own reward script pays out: firing its push event by hand took
    Hunger 255 -> 62 with starch 47
  - instincts load and dream: a newborn counts 5 -> 0, and both the gene load
    and `run_dream_step` match the native disassembly field for field

## The symptom, precisely

Creature->object messages, one port run:

    class 3.3.1 (the computer)   event 0 (push) x363
    class 2.1.1 / 2.2.0 / 3.2.1  event 0 x17
    FOOD 2.4.x / 2.6.x / 2.8.x   event 0 x0      -- only event 4 ever arrives

The creature pushes constantly and never pushes food.  Eating in C1 is the
push (activate 1) event on a food object, so nothing feeds it.  `_it_` is
captured at action start, which matches native `ResetExecutionState`, so the
15 successful touches while attending food belong to actions that began aimed
at something else.  Attention itself is stable (holds 50-74 ticks), so this is
not attention flapping.

That leaves the decision: given hunger and food in attention, the decision lobe
selects the wrong action.  Note also `SysInfo` fields 11/12 (selected action,
activation boost) read 2/3 in the original against 0/0 in the port at rest.

Caveat for anyone re-testing: the adults in `knowngood` were saved from worlds
run under older builds, when `_it_`, `appr` and pose dispatch were all broken,
so their learned wiring is contaminated.  A creature hatched under the current
build is the only clean subject.

## The reference rig (new capability)

The 1996 `Creatures.exe` in the install runs under Wine and serves DDE, so
live behaviour can now be compared directly:

  1. Copy `MFC42.DLL`, `MSVCRT40.DLL`, `MSVCIRT.DLL`, `CTL3D32.DLL`,
     `Wing32.dll`, `MSVCRT.DLL` from `1996install/Dlls/` into the staged world
     folder -- the working directory, so the install is never modified.
  2. Write `SOFTWARE\Millennium Interactive\Creatures\1.0` (HKCU and HKLM,
     32-bit view) with `Main Directory` pointing at the staged world and
     `Image`/`Sound`/`Palette`/`Body Data`/`Macro Directory` at the install.
     The 1996 build uses that key; the CE build uses
     `Gameware Development\Creatures 1\1.0`, and with the wrong one it reports
     "Creatures could not locate its Main Directory from the registry".
  3. Launch it with cwd = the staged world.  It has no CAOS pipe -- that is
     CE-only -- so drive it with `vivarium_client` over DDE.

## Root cause: dendrite state decays continuously in the port

`UpdateLatePhase` compared against native (0x00405170), and measured in both
engines on the same save.  Sampling lobe 8's per-neuron dendrite_state sums
every 15s:

    reference   258 162 5 108 10 108 17 15 61 6 14   -- unchanged over 45s
    port        237 141 0  94  0  94  2  1 54 0  1
                204 108 0  72  0  72  0  0 43 0  0
                168  72 0  48  0  48  0  0 34 0  0
                135  39 0  26  0  26  0  0 48 0  0

The port loses roughly 34 per 15 seconds on every dendrite; the reference holds
steady.  When a dendrite's state reaches zero both engines release it --

    *dendrite_state -= 1;
    if (*dendrite_state == 0) { current_weight = target_weight =
                                baseline_weight = 0; ... }

-- which is why the port shows concept-lobe neurons with intact dendrite counts
and baselines but target weight 0, eight of fifteen at load and rising.  The
concept lobe is the decision lobe's only input, so the decision progressively
loses the signal it needs and the creature never selects eating.

What is NOT the cause, each checked against the disassembly:

  - the rule register file: the port writes 16 <- target neuron firing_strength
    and 18..21 <- current/target/baseline/dendrite_state at the top of each
    connection iteration, exactly as native does at 0x00405459..0x0040547d.
    (The plate comment on the native function claiming tokens 16..21 "read
    zero" is stale and contradicts its own instruction listing.)
  - the expression evaluator: register reads, stop_if_zero, saturating add and
    subtract, the q8 multiply, increment and decrement all match.
  - the release code itself, and the dendrite counts and baseline weights,
    which are identical across engines.

### Where to look next

The decay fires when `dendrite_decay_expression` evaluates nonzero.  For lobe 8
rule 0 the genome's expression is simply `[6]` -- register 6, which is
`late_phase_runtime_state.token_evaluation_state[1]`.  Those four bytes are
written by genome receptor loci (biochemistry into the brain) and are also
serialized with the lobe, so both engines start from the same value.  The port
must therefore be writing a nonzero value into lobe 8's
`token_evaluation_state[1]` at runtime where the original leaves it zero.

So the next step is the receptor that targets that locus: which chemical drives
it, and whether `resolve_genome_locus` maps it to the same byte the original
does.  That is one locus mapping, not a subsystem.

## Traced to the receptor that gates the decay -- and the port matches CE

The decay is gated by one byte.  Lobe 8's rule 0, decoded from the genome with
CLobeConnectionRule::LoadFromGenome's 47-byte order (0x004023a0):

    growth_interval = 2   growth_expr = [10, 22, 5]
    decay_interval  = 3   decay_expr  = [6]

Register 6 is `late_phase_runtime_state.token_evaluation_state[1]`, and the
genome points a receptor straight at it:

    receptor: tissue(lobe) 8, locus 14, chemical 52 (ConASH),
              threshold 0, nominal 4, gain 255, flags 0x01 (reduce)

With flags 0x01 the locus is `nominal - signal`, so it reads **4 when ConASH
is absent** and 0 once the chemical is present.  The receptor exists to SUPPRESS
the decay; with no ConASH the decay runs every third tick forever.

Measured: chemical 52 is 0 in the port AND in the 1996 engine.  Causal proof in
the port -- injecting chemical 52 stopped the decay dead, dendrite states held
at 318/212 for two minutes where they had been losing ~34 per 15s.

### The port is faithful to CE on every step of this path

Checked against the CE disassembly, all matching:

  - receptor arithmetic and flags: CBiochemistry::Update @ 0x0042ee10 computes
    excess, gain, digital bit 0x2, reduce bit 0x1, and writes the locus
    unconditionally every tick.  Identical.
  - locus domain normalisation: `domain < 2 ? domain : domain & 1`.  Identical.
  - receptor locus table: CBrainLocusResolver::ResolveGenomeLocus @ 0x004043b0
    maps loci 13..16 to lobe+195..198, which with CLobe's layout
    (late_phase_runtime_state at 193, token_evaluation_state at +2) is
    token_evaluation_state[0..3].  Identical.
  - the SVRule register file, the expression evaluator, and the release path
    that zeroes a dendrite's three weights at state 0.  Identical.
  - the lobe gene decode itself, verified byte for byte against the payload.

So on this path the port reproduces the Community Edition exactly, and the
engine that behaves differently is the 1996 build.  The likely difference is
1996's own CLobe layout or locus table sending that receptor somewhere other
than the byte its decay expression reads -- which needs the 1996 binary in
Ghidra to confirm, since the project's database is the CE build.

**Open decision.** If CE starves norns the same way, the port is not the
regression and the question becomes which engine to follow: CE, which the port
targets, or 1996, whose creatures feed themselves.  Running the CE binary
against this world settles it.

## A separate, real port defect: emitter loci 0..2

While tracing the above: CE's emitter locus table (same resolver, the non-
receptor branch) maps

    locus 0 -> lobe+192  active_fraction
    locus 1 -> lobe+193  per_rule_loose_dendrite_count[0]
    locus 2 -> lobe+194  per_rule_loose_dendrite_count[1]

and the port's `Lobe::resolve_genome_locus` maps all three onto
`token_evaluation_state[0..2]` instead.  Emitter loci 3..18 (neuron
firing_strength) do match.

This is not cosmetic: CE emitter flag 0x1 is CLEAR_SOURCE_LOCUS, so an emitter
writes through that pointer.  In the port such an emitter zeroes the wrong
bytes -- and `token_evaluation_state[1]` is precisely the decay gate above.
Fix is in `byte_match/src/c1/brain/lobe.cpp`, the emitter branch of
`Lobe::resolve_genome_locus`.

## Fixed, and what the fix changed

`Lobe::resolve_genome_locus`'s emitter branch now returns `active_fraction` for
locus 0 and `per_rule_loose_dendrite_count[0]/[1]` for loci 1 and 2, matching
CE.  The port's emitter runtime loop, receptor loop, domain normalisation and
all thirteen receptor loci were each checked against the CE disassembly and
already matched; this mapping was the only divergence.

The standard genome builds a pruning homeostat out of exactly these two genes:

    emitter  lobe 8 locus 1  <- loose_dendrite_count[0], emits chemical 52 (ConASH),
                                CLEAR_SOURCE_LOCUS
    receptor lobe 8 locus 14 <- chemical 52, nominal 4, reduce
                             -> token_evaluation_state[1], the whole of the
                                lobe's dendrite decay expression

Loose dendrites emit ConASH, which drives the decay gate to zero and stops
the pruning.  Reading the wrong source meant the emitter never fired, the
chemical never appeared, the gate stayed at its nominal 4 and the lobe pruned
itself to nothing.

Measured after the fix, same world:

    dendrite state, lobe 8   [258, 162, 5, 108, 10, 108, 17, 15, 61, 6, 14]
                             stable, and identical to the 1996 reference's
                             values; before the fix it fell ~34 per 15s
    life force               47% -> 65%, where before it fell monotonically
                             to death (three creatures dead in ~80 minutes)
    food                     the first push events ever recorded on food
                             classes (2.4.13); before the fix, zero in every run

Not yet settled: the adult in this save still runs a high hunger and its life
force drifts slowly down from the 65% peak.  Its learned wiring was shaped in
worlds run under the broken build, so the clean test is a creature hatched and
raised under the fix, over a long run.

## Second fix: genome gene loading ignored the life stage

Native passes MATCH_GENOME_LOAD_STAGE to every FindNextMatchingGene call for
creature genes -- stimulus (0), pose (3), gait (4), instinct (5) -- and for all
five biochemistry gene types (receptor, emitter, reaction, half-lives, initial
concentrations).  The port passed `GenomeStageFilter::ignore` at all of them, so
a creature loaded every life stage's genes over each other and ended up with the
last stage's data.  Skeleton and Body genes really are IGNORE_STAGE in the
native; those calls were left alone.

Fixed in `creatures/creature.cpp` (kLoadStage) and the five sites in
`biochemistry/biochemistry.cpp`.

Measured after both fixes, fresh world, norn hatched under the build:

    dendrite state, lobe 8   stable
    adult life force         47% -> 78%
    newborn life force       47% -> 58%, then steady; the touch log records
                             its own eat push, so it is feeding itself
                             (before the fixes a newborn sat flat at 47%)

## Resolved: `new: crea` needs `slim` -- newborns were never unbounded-free

Not a port defect.  `Creature::Creature @0x0040d580` ends with
`SetBoundsMode(UNBOUNDED_2)` + `SetDownFootPositionAndRecomputeLayout(1000, 2000)`
and never restores DEFAULT_WORLD, so *every* creature the engine builds starts
with `movement_bounds = {0, 0, 0x7fff, 0x7fff}`.  The port does the same thing,
faithfully.

With `max_y = 32767` the foot-swap test in `UpdateAnchorAndBounds`

    bounds.max_y < limb_chain_end_y[opposite]

can never be true, so the creature animates its gait, cycles poses, picks
actions and eats what is adjacent -- but never commits a step.

The world's own egg hatch script supplies the missing call:

    ... new: crea obv0 obv1, ... targ objp, mvto var1 var2, slim, rmev ownr ...

`slim` is `SetBoundsMode(DEFAULT_WORLD)` + `UpdateMovementBounds`, which with
the creature's `0x40` (kUseCurrentMapRoom) flag resolves the room it stands in.
A hand-made test norn must be created the same way:

    new: crea tokn TSTA 1, mvto <x> <y>, slim, drea 1

Issuing `slim` on the two inert test norns after the fact started both walking
within seconds.  See
[a-new-creature-starts-unbounded.md](a-new-creature-starts-unbounded.md).

## Eating works end to end

### The whole chain does work

A norn hatched under the current build, placed beside food, was caught doing
the complete loop:

```
tick=11857  hunger=190 hdec=102 starch=90 gluc=159 glyc=122 rew=0
tick=11862  hunger=112 hdec=21  starch=87 gluc=171 glyc=120 rew=36
tick=11867  hunger=95  hdec=6   starch=87 gluc=165 glyc=122 rew=23
```

That is: attention latched onto the food (`link=<food>`, Attention-lobe neuron
15 firing 32/30), the decision lobe selected action 1 (activate 1), the push
reached the object (34 `event=1` messages to classes 2.15.1, 2.15.6 and 2.4.13
in one run), the food's `stim writ from 10 255 0 0 35 100 34 10 57 100 0 0`
landed -- Saccharin +101, Starch +90 -- and the genome's
`1 Hunger Decrease + 1 Hunger -> 1 Reward` took Hunger 190 -> 95 and paid out
Reward. So "the decision never selects eating" from the section above is no
longer the symptom.

Food in a shipped world is family 2, genus 4 and genus 15, and the feeding
script is **event 1** (activate 1), which `get_attention_record_index` maps
onto attention records 4 and 15.

### What is verified faithful to CE on this path

Re-read line by line against the disassembly, all identical:

  - `CBiochemistry::Update @0x0042ee10` in full -- emitter flags, receptor
    threshold/gain/digital/reduce, reaction extent and its tick-mask gating,
    and the half-life decay.
  - `Creature::UpdatePerception @0x0040bf10` and `Creature::UpdateAttention
    @0x0040bbc0`, including the winner scan that seeds from record 0
    unconditionally and requires a non-null target for records 1..0x27.
  - `CLobe::LoadGenome @0x004025d0` field order (percept, threshold,
    relaxation selector, baseline, gain, expression, WTA flags, two rules).
  - `SFCDoc::UpdateWorld @0x004324e0`'s creature pass: stride 4 with the cohort
    counter cycling 0..4, the `dream_countdown != 0` skip, and the
    brain/biochemistry/action-selection/tick-increment order.

### Harness note: a dreaming creature has no brain update

`SFCDoc::UpdateWorld` skips brain, biochemistry, action selection and the
biochemistry tick entirely while `dream_countdown != 0`, and the port does the
same. A test norn created with `drea 1` therefore sits completely inert -- no
firing anywhere, attention never latches -- until the dream finishes. An
earlier run of this investigation was spent on a creature that was simply still
dreaming. Wait for `dream=0` before drawing conclusions.

## The 1996 differential on Glucose/Glycogen: the ratio is NOT the cause

Run 2026-09-18 on two fresh stagings of `knowngood`, one per engine, sampled
through the *same* DDE client (`vivarium_client`) every 20s for 5 minutes,
reading Hunger(3) | Saccharin(35) | Starch(57) | Glucose(58) | Glycogen(59):

```
            port                                reference (1996)
 254| 2|  0|207|124                     255| 1|  0|211|122
 255| 1|  0|211|122                     254| 2|  0|204|124
 255| 1|  0|210|122                     255| 1|  0|209|122
 254| 2| 81|218|124   <- meal             0| 6| 94|242|148   <- meal
 251| 1| 36|243|145                       0|11| 43|246|176
 255| 2| 13|246|159                       5| 9| 18|246|192
 254| 2|  0|233|169                      16| 3|  0|246|197
 255| 1|  0|241|166                      41| 2|  0|255|194
 254| 3|  0|231|169                      61| 3|  0|239|197
 255| 1|  0|241|165                      84| 3|  0|239|197
```

Both engines start at the same operating point, eat at about the same time for
about the same meal size, and follow **nearly identical** Glucose/Glycogen
trajectories into the high-glycogen regime. The reference reaches
Glucose 246 / Glycogen 197 -- the exact state an earlier revision of this note
called a "byte-ceiling trap" -- and its Hunger there is 16, then 41, 61, 84,
climbing steadily. **That hypothesis is falsified and has been removed.** The
glucose/glycogen ratio is not the discriminator, and hunger accumulates at
comparable rates in both engines at the same glycogen.

The reaction is identical too. Injecting saccharin by hand
(`stim writ norn 10 255 0 0 35 100 0 0 0 0 0 0`):

  - reference: Hunger 255 -> 160 for Saccharin 100 (-95, and Reward paid out)
  - port:      Hunger 255 ->  66 for Saccharin 200 (-189)

Both 1:1, as `1 Hunger Decrease + 1 Hunger -> 1 Reward` requires. So
`CBiochemistry::Update` is exonerated by measurement as well as by reading.

### Where the two engines actually diverge: the meal delivers no Saccharin

The whole difference is in one column. At the meal the reference's Saccharin
rises to 6/11/9/3 and Hunger goes 255 -> 0; the port's Saccharin never leaves
baseline (1/2/2/1) and Hunger stays 254.

Scanning the port's per-tick trace for chemical deliveries over that run finds
exactly two, one per creature, and both are Starch only:

```
tick  8544  4b5a4633  (hunger 255 sacc 1 starch 0 gluc 206 glyc 122)
                   -> (hunger 255 sacc 1 starch 90 gluc 206 glyc 122)
tick 10240  56424d31  (hunger 255 sacc 1 starch 0 gluc 126 glyc 116)
                   -> (hunger 255 sacc 1 starch 90 gluc 126 glyc 116)
```

Meanwhile the touch log shows **seven** `event=1` pushes onto food classes in
that run (2.15.6 x2, 2.15.1 x2, 2.4.9, 2.4.13, 2.4.9). So five of seven pushes
delivered no chemicals at all, and the two that did delivered only Starch.

That matters because in this world only two food classes reduce hunger:

| class | event 1 payload | effect on Hunger |
| --- | --- | --- |
| 2.4.9  | Pain Decrease 100, **Saccharin 100**, Starch 90 | -100 |
| 2.4.3  | **Saccharin 50**, NFP Decrease 10, Pain Inc 5, Starch 50 | -50 |
| 2.4.10 | Sleepiness Dec 100, Tiredness Dec 100, Starch 90 | none |
| 2.4.13 | Hotness Dec 100, Coldness Dec 100, Starch 90 | none |
| 2.15.1 | Pain Increase 100, **Hunger Increase 100**, Starch 90 | **+100** |
| 2.15.2 | Sleepiness Inc 100, Tiredness Inc 100, Starch 90 | none |
| 2.15.3 | Sex Drive Dec 100, Adrenaline 10, Starch 90 | none |
| 2.15.4 / 2.15.6 / 2.4.12 | chem 232..235 100, Starch 90 | none |

(Read out of `World.sfc`; the 2.15.x set are the vendor drinks, not food.)

So the port's creature got Starch -- calories, hence the healthy life force --
without the satiety signal, and one of the objects it pushed twice actively
raises Hunger.

### Delivery experiment: the push path works, the object choice is wrong

Firing the eat message by hand on the saccharin food (2.4.9, handle 21993424)
in `porteat`:

```
mesg writ <2.4.9> 0
before:  hunger 254  saccharin 2  starch  0
after:   hunger 160  saccharin 1  starch 76
```

Hunger -94, Starch +76, the Saccharin consumed on arrival. **Candidate 1 is
falsified: event dispatch, the food script, `stim writ from`, and the
chemical delivery all work.**

It also settles the numbering. C1's `mesg` message numbers are one below the
script event numbers: `mesg writ x 0` reaches the object's *event 1* script.
The port does not offset in `mesg` itself -- `Macro`'s `mesg writ` passes the
id straight to the queue, matching `QueueObjectEvent @0x00422e80` -- the +1
lives in the per-event `Object` handlers, and it is correct. `mesg writ x 1`
delivers nothing, which is why the first attempt at this experiment looked like
a failure.

The eating routine is the **event 17** script, i.e. the creature's action 1
(activate 1) dispatched as `target_classifier + action + 0x10`:

```
2.4.9 event 17:
  drop,impt 3,aim: 0,appr,touc,wait 4,mesg writ _it_ 4,wait 10,pose 73,wait 5,
  snde chwl,mesg writ _it_ 0,pose 74,wait 7,pose 12,wait 4,mesg writ _it_ 5,
  impt 0,wait 20,done,endm
```

`mesg writ _it_ 0` in the middle is the bite. The food's event 1 then pays out
the chemicals and arms `tick 1000`; its event 9 timer later does `tick 0`. A
message to a tick-disabled object is dropped before any handler --
`ProcessQueuedObjectEventsAndStimuli @0x00432d20` has the same
`target_object->tick_enabled != 0` guard the port has -- so a food that was
bitten recently legitimately ignores the next bite in both engines.

### What the run actually shows

Creature -> food messages in the 5-minute `porteat` run, with the four
completed bites (`event=0`) marked:

```
  2.15.1  event 1 x2,  event 4
  2.15.6  event 0 x2 <-,  event 1 x2, event 2, event 4 x2, event 5
  2.4.11  event 0 <-,  event 1, event 4
  2.4.13  event 0 <-,  event 1, event 2, event 4 x2, event 5
  2.4.9   event 0 <-,  event 1 x2
```

Four bites, two chemical deliveries (both `Starch +90`, no Saccharin). The two
that delivered were on classes with no Saccharin in their payload; the one bite
on the Saccharin food, 2.4.9, delivered nothing, which its `tick 1000` cooldown
explains.

So the port's creature does eat -- it just spends its bites on the vendor
drinks (2.15.x) and the non-Saccharin foods, and when it does reach 2.4.9 the
object is on cooldown. The reference, in the same world over the same interval,
took one bite that zeroed its Hunger.

**This is suggestive, not yet conclusive** -- one reference meal against four
port bites is a small sample. The measurement that settles it is a longer run
on both engines counting bites by class. If the port's bites stay skewed toward
2.15.x while the reference's land on 2.4.x, the defect is in object choice, and
the goal-direction weight matrix is where to look: `Creature::apply_stimulus`
only learns from chemicals `0x21..0x30`, keyed by attention record, so a
creature that never lands a Saccharin bite never learns that record 4 is food.

## The reference rig, corrected

The 1996 engine is `~/.c1-hatch-test-run/Creatures.exe` (387,072 bytes), which
is already in the harness install and listed in `protected_exe_names`. c1-lab
cannot launch it through config alone: `registry_vendor` only produces
`SOFTWARE\<vendor>\Creatures 1\1.0`, and the 1996 build reads
`SOFTWARE\Millennium Interactive\Creatures\1.0` -- a different key *shape*.

To run it:

```
WINEPREFIX=~/.c1-hatch-test-prefix DISPLAY=:100 \
  cd <staged world> && wine ~/.c1-hatch-test-run/Creatures.exe /Embedding
```

  - **`/Embedding` is required.** Without it the process registers SFC.OLE,
    writes `Millennium Interactive\Creatures\Patch`, and exits silently with
    code 0 -- no window, no message. That silent exit cost most of one session.
  - Set `Main Directory` and `Genetics Directory` (HKCU *and* HKLM) to the
    staged world; `Image`/`Sound`/`Palette`/`Body Data` may point at the
    harness install.
  - Dismiss `Tip of the Day` before poking it over DDE; a script that runs
    while a modal dialog is up can crash the game.
  - It has no CAOS pipe. Drive it with `vivarium_client` (build with
    `i686-w64-mingw32-g++ -O2 -static vivarium_client.cpp -luser32`). Use the
    `Macro` item -- POKE+REQUEST -- which runs the script and returns its
    output; `--execute` (XTYP_EXECUTE) was refused in this session.
  - **Side effect:** it rewrites `CLSID\{77C733E1-...}\LocalServer32` to point
    at itself, so the kits will talk to the 1996 engine until the port is
    launched again and re-registers.

## CORRECTION: matched 12-minute windows show no feeding difference

Run 2026-09-18, after the short-window results above. Two fresh stagings of
`knowngood`, one per engine, identical starting state (Hunger 254-255,
Glucose 207-213, Glycogen 122-124), sampled through the same DDE client every
10s for 720s.

```
 t(s)   PORT hunger/starch     REF hunger/starch
    0    255 /   0              255 /   0
   80    255 /   0                5 /  77   <- reference meal
  100    255 /  81                5 /  35
  160    255 /   0               47 /   0
  240    255 /   0              139 /   0
  340    255 /   0              252 /   0
  400    255 /  58              254 /   0
  420     20 / 125              254 /   0   <- port meal
  500    112 /   0              254 /   0
  620    249 /   0              255 /   0
  700    255 /   0              255 /   0
```

|  | port | reference |
| --- | --- | --- |
| hunger-zeroing meals in 720s | 1 | 1 |
| Hunger min / max | 6 / 255 | 0 / 255 |
| samples pinned >= 250 | 50/72 (69%) | 45/72 (62%) |
| Hunger recovery slope | 1.07 /s | 0.95 /s |
| starch spikes (any food) | 4 | 1 |

**The two engines are behaviourally equivalent on this measure.** Each took one
meal that zeroed Hunger, each climbed back at the same rate, and the 1996
engine spent the last 350 seconds of its window pinned at 255 -- longer,
unbroken, than anything the port did.

So **the claim at the top of this note is wrong**. "Hunger pinned at 255" is
not a port defect; it is what C1 does between meals, in both engines. The
original differential (`original 0 5 7 35 6 1 25 53` against
`port 254..255`) was a sampling artifact: two short windows that happened to
catch the reference just after a meal and the port just before one. The same
artifact produced two later wrong conclusions in this note -- the "byte-ceiling
trap" and "the port eats the wrong objects" -- both of which were also drawn
from windows of a few minutes.

Bite counts back this up. Over the port's 12 minutes the touch log records
eight completed bites (`event=0`) spread across four classes, including two on
the Saccharin food:

```
  2.4.13 x3    2.4.9 x2 (Saccharin)    2.4.10 x2    2.15.6 x1
```

The creature is not avoiding food and is not fixated on the vendor drinks.

### What this does and does not invalidate

Still standing, because each was verified against the disassembly and had a
measured effect:

  - the emitter locus 0..2 mapping fix (`Lobe::resolve_genome_locus`),
  - the `MATCH_GENOME_LOAD_STAGE` fix in `Creature::load_genome` and the five
    biochemistry sites,
  - the `slim` / UNBOUNDED_2 finding for newly constructed creatures,
  - every "port matches CE" verification recorded above.

No longer supported:

  - that the port's creatures fail to feed themselves,
  - that Hunger pinning indicates a defect,
  - the glycogen/glucose "trap",
  - that the port's creatures eat the wrong objects.

### The symptom that is still real, and how to test it

What actually prompted this investigation was creatures **dying** -- "a world
left running for ~80 minutes had all three dead". Hunger was a proxy, and it
turns out to be a bad one. The right measurement is life force and deaths over
a long run on both engines:

```
vivarium_client Macro "dde: getb ovvd"
```

which returns name|moniker|sex|age|pregnancy|**life force**|health|room|x|y for
every creature, on either engine. Run both for an hour on the same world,
sample every 30s, and compare life-force trajectories and time-of-death. That
is the differential that matches the symptom; Hunger is not.

See [port-reproduces-c1-behaviour.md](port-reproduces-c1-behaviour.md) for the
collected differential results, including the brain comparison run after this
note's correction.
