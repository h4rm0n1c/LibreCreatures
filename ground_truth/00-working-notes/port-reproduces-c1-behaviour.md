# The port reproduces C1 behaviour: the differential results

Recorded 2026-09-18. Every differential run against the 1996 retail
`Creatures.exe` on the same world has come back equivalent within measurement
noise. This note collects those results in one place, because the working notes
that produced them are long and open with premises that turned out to be wrong.

## What was compared, and how

The 1996 binary and the port were run on separate fresh stagings of the same
world (`knowngood`), driven through the **same** DDE client
(`external/creaturesstructs/Creatures1/dde-service/vivarium_client.cpp`), so
the measurement path is identical on both sides. See the rig section of
[norns-do-not-feed-themselves.md](norns-do-not-feed-themselves.md).

### Feeding and drive dynamics -- equivalent

Matched 12-minute windows, identical starting state (Hunger 254-255,
Glucose 207-213, Glycogen 122-124), sampled every 10s:

|  | port | reference |
| --- | --- | --- |
| hunger-zeroing meals in 720s | 1 | 1 |
| Hunger min / max | 6 / 255 | 0 / 255 |
| samples pinned >= 250 | 50/72 (69%) | 45/72 (62%) |
| Hunger recovery slope | 1.07 /s | 0.95 /s |

Both engines spend most of a window pinned at Hunger 255 between meals; the
1996 engine sat pinned for the last 350 seconds of its window unbroken.

### Biochemistry -- equivalent, by measurement as well as by reading

Hand-injected Saccharin (`stim writ norn 10 255 0 0 35 <n> 0 0 0 0 0 0`):

  - reference: Hunger 255 -> 160 for 100 (-95), Reward paid out
  - port:      Hunger 255 ->  66 for 200 (-189)

Both 1:1, as `1 Hunger Decrease + 1 Hunger -> 1 Reward` requires. Glucose and
Glycogen follow near-identical trajectories into the high-glycogen regime
(port 218->243->246, glyc 124->145->169; reference 242->246->246,
glyc 148->176->197).

### Brain -- equivalent

Same save in both, `BrainActivity` sampled every 2 minutes over 6 minutes:

| dendrite type 0 | port | reference |
| --- | --- | --- |
| avg target weight, neurons/sum @r0 | 610 / 4932 | 604 / 4919 |
| ... @r3 | 638 / 5376 | 665 / 5580 |
| avg dendrite state @r0 | 600 / 1415 | 596 / 1415 |
| ... @r3 | 640 / 1774 | 665 / 2045 |
| max current weight | 2-6 / 10-21 | 3-4 / 2-5 |

Type-1 dendrites are identical in both (45-46 neurons, sum 600, flat).
Dendrite state **rises in both**, which is the point: the earlier real defect
showed as the port bleeding ~34 per 15s while the reference held steady, and
that signature is gone. Current weights sit near zero in both engines, so that
is C1 behaviour and not a port artifact.

The port's dendrite-state sum ends ~13% below the reference's, but the
reference's creature ate during that window and the port's did not, and reward
drives dendrite state -- so the gap tracks the meal, not the brain.

### Read-level verification

Separately, these were re-read line by line against the CE disassembly and
match: `CBiochemistry::Update @0x0042ee10` in full; `Creature::UpdatePerception
@0x0040bf10`; `Creature::UpdateAttention @0x0040bbc0`; `CLobe::LoadGenome
@0x004025d0`; `CLobe::UpdateLatePhase @0x00405170`; `SFCDoc::UpdateWorld
@0x004324e0`'s creature pass (stride 4, cohort 0..4, dream skip, update order);
`ProcessQueuedObjectEventsAndStimuli @0x00432d20`; `QueueObjectEvent
@0x00422e80`; `Creature::Creature @0x0040d580`.

## The variance, and the method lesson

Meal timing is effectively random, and it dominates any short measurement:

  - 12-minute run: port ate at t=420s, reference at t=80s -- one meal each
  - 6-minute run:  reference ate, port did not

**Three separate wrong conclusions in this investigation came from windows of a
few minutes**: "the port's norns never feed themselves", the glucose/glycogen
"byte-ceiling trap", and "the port eats the wrong objects". Each looked clean
and each was a sampling artifact. With roughly one meal per creature per ten
minutes, a window shorter than ~30 minutes cannot separate equivalence from a
sizeable real difference, and a single window can show either engine "failing".

Minimum standard for a behavioural differential here: both engines, same world,
same client, >= 30 minutes, and a metric that integrates over many events
rather than catching one.

## Scope of the claim

What is established: on drive dynamics, biochemistry and brain-weight
aggregates, over windows of minutes to tens of minutes, the port and the 1996
engine are indistinguishable, and the code paths behind them match the CE
disassembly on inspection.

What is not yet measured: **long-run survival**. The symptom that started this
work was creatures dying -- "a world left running for ~80 minutes had all three
dead". Hunger was used as a proxy and turned out to be a poor one. The
outstanding test is an hour or more per engine on the same world, sampling
`dde: getb ovvd` (which reports life force per creature on both engines) every
30s and comparing life-force trajectories and time-of-death. Until that is run,
equivalence is established on the measured axes and assumed, not shown, on
survival.
