# Bugs in the original game that LibreCreatures fixes

This is the list of defects in Creatures 1 itself that LibreCreatures
corrects. It does not list defects in this port's own translation; those
are ordinary bug fixes and are in the commit history.

"Native" means the Community Edition `Creatures.exe` (Community Release
10.3) that this project was recovered from. Most of these defects are
older than the Community Edition. Where a defect was also checked in the
1996 or 1997 retail executable, or in the shipped world data, the
"Checked" column says so.

Each fix is marked in the source with a comment that starts
`LibreCreatures deviation` (some older ones say "Deliberate deviation" or
"not native"), and names the native function and address.

## How to keep this list

When a commit fixes a defect of the original game:

1. Mark the change in the source with a `LibreCreatures deviation`
   comment that names the native function and address.
2. Add a row to the correct table below in the same commit, with the
   commit hash. Add later commits for the same defect to the same row.
3. If the defect was in our translation and not in the original, do not
   add it here.

Checked: **code** = read in the native disassembly; **lab** = the defect
was reproduced in the lab (on the CE executable, or on a faithful
translation before the fix); **A/B** = measured in a lab A/B;
**retail** = seen in a retail executable; **data** = in the shipped world
files.

## Game

### Brain

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 1 | The creature update cycles through five cohorts but steps by four, so every fourth creature gets a second brain, chemistry and action pass every fifth tick. C2, C3 and DS use four. | `SFCDoc::UpdateWorld` | 61b6bf3 | code |
| 2 | Connection-rule evaluation stops at the first rule with no connections, and the rule registers are not reset per neuron, so a neuron can read the previous neuron's values. | `CLobe::UpdateLatePhase` @00405170 | a6e6969 | code |
| 3 | A lobe with zero neurons divides by zero. | `CLobe::UpdateEarlyPhase` @00405010 | afca2d6 | code, crash dump |
| 4 | Dendrite migration compares the candidate with every active neuron, the migrating neuron included, so the neuron always matches itself and its weights are zeroed instead of moved. | `MigrateRuleConnections` @00405da0 | 6d52950 | code, lab |
| 5 | The migration duplicate test packs the pointer sum and product into 16 bits each, so different dendrite sets pass as duplicates. A matching test is now confirmed by comparing the target sets. | `MigrateRuleConnections` @00405da0 | 68c25ae, e058524 | code, lab |
| 6 | A loose dendrite rejects its own previous target as a duplicate. | `CLobe::UpdateLatePhase` @00405170 (attach at 00405990) | 75b9911 | code, lab |

### Genetics

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 7 | Mutation can change the four bytes of the `gext` pigment tag, which deletes the pigment extension from the creature and all its descendants (since version 1.04). | `CopyGenomeGeneWithMutation` @00418c70 | e4bfe75 | code, model |

### Creatures

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 8 | Attention reads the stored sense value as a signed byte and clamps negative values to zero, so every sense value of 128 or more reaches the brain as 0. | `UpdateAttention` @0040bbc0 | 3239019 | code |
| 9 | `kill` parks an object at (1000, 4000) but leaves creatures' references to it. Creatures then remember that off-world position and go "down" through lifts for ever (issue #11). | `Object::InitializeRuntimeState` @0042cd70, `ApplyStimulus` @0040b8a0 | 68c25ae, 3239019, de8ba26, 9074ffa | code, retail (1997) |
| 10 | An object whose genus has no attention record (simple 40+, compound 15+, creature 5+) is indexed past the 40 records and overwrites other creature memory. | `GetAttentionRecordIndex` @00426430, `Creature::UpdatePerception` @0040bf10 | 0775a23, 02ffd93 | code |
| 11 | When an involuntary action has no script, its "active" mark stays set and the creature ignores its brain until its attention changes. | `UpdateActionSelection` @0040c630 | 734992d | code |
| 12 | Gait selection tests the animation of the next gait up, not the gait it chooses, and never tests gait 7, so a creature can choose an empty gait. | `SelectWalkGait` @004082c0 | 671c403 | code |
| 13 | A `drop` with empty hands gives the creature Disappointment. Food, drink, herb and weed scripts start with `drop`, so eating disappoints the norn. | `NotifyDependentsOnRemoval` @00408420 | ffe8e5c, e6126ca | code, A/B |
| 14 | The lonely and crowded phrases have no space: "comeNorn", "runNorn". | `SpeakDominantDrivePhrase` @0040b160 | ba1dab0 | code |
| 15 | With a zero gestation chemical, the status report repeats the previous field as the pregnancy stage. | creature status report | 71431ba | code |
| 16 | If a creature's generated body sprite file is missing, the creature stays invisible for ever. | `Skeleton` sprite check on world open | b1f6c2d | code, lab |
| 17 | `stm# tact` sends the stimulus to every creature in the world when one creature touches the source. | `QueueTactStimulusForOverlappingCreatures` @00423470 | c5a6a2d | code, lab |

### Scripts

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 18 | The egg in the sky: an event that reaches a norn between `new: simp` and the next turn of her egg-laying script replaces the script, and the egg stays at the world origin. | `ExecuteScriptForClassifier`, owner purge | c3d446e, ac8bfa4, 7c7f309 | code, lab (CE) |
| 19 | A script value used as an object pointer is not checked, so a bad value crashes the game. | `stim writ`, `targ`, `mesg writ` and others | bd4c3b5 | code, crash |
| 20 | `rndv` over the whole 32-bit range divides by zero. | `rndv` | a886d6b | code |
| 21 | `evnt` with no object stores a null pointer and then uses it. | `evnt` | 140ccd7 | code |

### World, objects and vehicles

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 22 | The Black Hole: in a gap between rooms, an object's floor becomes y 9999 and the object falls out of the world. | `FindNearestMapRoomBoundsAtPoint` | fc1628c | code |
| 23 | The Flying Lemon: a vehicle moves every object linked to it, even after a script has moved the object away. | `Vehicle::Tick` @0042bd90 | c9b8633 | code, lab (CE) |
| 24 | A lift called to its own floor arrives at once, and its `dpas` runs before the queued `gpas` pickups, which then bind the passengers to the lift for ever. | `gpas` / `dpas` | 5e557a3 | lab |
| 25 | The plane of a carried object is taken from a table indexed by the low byte of a pointer, so in the incubator it depends on heap addresses. | `UpdateEntityForExplicitRectBoundsAndRedraw` @00428d30 | 274c2ba, 53c46ad | code |
| 26 | Ocean Dome Sound (2 8 15) does not have the "creatures cannot see it" flag, so norns go into the dome to find a vendor (issue #10). | World.sfc, Eden.sfc | 57eeedc | data |
| 27 | An object let go of in the upper half of a room jumps up onto the floor of the room above. | hand drop placement | 1a663b2 | code |

### The hand and the interface

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 28 | A click goes to the highest render plane, so a lift, the incubator or the pianola takes clicks meant for a creature or object in or behind it. A norn standing at a lift takes clicks meant for the lift arrows. | `FindTopmostOverlappingObject` @00425c30 | f31a3a0, a94bdf0 | code, lab |
| 29 | The click is processed before the hand moves to the mouse, so a click on a small target just after moving finds nothing and must be made again. | `UpdateUnboundedPositionAndRedraw` @00427fd0 | cc5fb80 | code, lab |
| 30 | Deleting any creature deselects the selected creature and closes its kits. | `DeleteObjectAndPurgeRuntimeReferences` @0040e9f0 | bb79273 | code |
| 31 | If an object in the hand is killed or deleted, the hand stays frozen. | `kill`, object delete | ba7dae8 | code |
| 32 | Return with nothing typed makes the hand say an empty word. | Return key handling | b30f6fd | code |
| 33 | Save, Save As and a speed change start the world timer again even when the world is paused. | @00434950, @004349b0, @00417f80 | c0d8352, 40962a3 | code |

### Kit protocol and sound

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 34 | The pipe server gives the UI thread the address of a stack variable and returns after 30 s, so a slow command reads freed memory. | `PipeServerMarshalCommandToMainThread` @00446620 | c0d8352 | code, crash |
| 35 | Over SFC.OLE a brain report does not run its holder's script, so the kits always get firing strength, whatever measure they ask for. | `DispatchFormatBrainActivityReport` @00419400 | 2dcb4fc | code, kit traffic |
| 36 | The 512 KB sound cache is smaller than one looping sound plus a few others, so sounds are dropped with no error. | sound cache | f410c47 | lab |

## Kits

Each kit's `ORIGINAL.md` has a "Bugs in the original" section, and the
kit source marks its fixes `Fix (bug N)`.

| Kit | Bugs listed | List |
| --- | --- | --- |
| Biochemistry Kit | 2 | [ORIGINAL.md](../src/kits/biochem/ORIGINAL.md) |
| Breeder's Kit | 3 | [ORIGINAL.md](../src/kits/breeder/ORIGINAL.md) |
| Funeral Kit | 6 | [ORIGINAL.md](../src/kits/funeral/ORIGINAL.md) |
| Hatchery | 2 | [ORIGINAL.md](../src/kits/hatchery/ORIGINAL.md) |
| Health Kit | 3 | [ORIGINAL.md](../src/kits/health/ORIGINAL.md) |
| Object Injector | 2 | [ORIGINAL.md](../src/kits/injector/ORIGINAL.md) |
| Observation Kit | 17 | [ORIGINAL.md](../src/kits/observation/ORIGINAL.md) |
| Owner's Kit | 9 | [ORIGINAL.md](../src/kits/owner/ORIGINAL.md) |
| Science Kit | 5 | [ORIGINAL.md](../src/kits/science/ORIGINAL.md) |
| Score Kit | 12 | [ORIGINAL.md](../src/kits/score/ORIGINAL.md) |
| **Total** | **61** | |

Science Kit bug 5 is the game defect in row 35.
