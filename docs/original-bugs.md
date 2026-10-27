# Bugs in the original game that LibreCreatures fixes

This is the list of defects in Creatures 1 itself that LibreCreatures
corrects. It does not list defects in this port's own translation; those
that the verification tests found are in [`our-bugs.md`](our-bugs.md).

"Native" means the Community Edition `Creatures.exe` (Community Release
10.3) that this project was recovered from. Most of these defects are
older than the Community Edition. Where a defect was also checked in the
1996 or 1997 retail executable, or in the shipped world data, the
"Checked" column says so.

A fix that the Community Edition made first, and LibreCreatures ported,
is credited to its author and release in its row and in the source.

Each fix is marked in the source with a comment that starts
`LibreCreatures deviation` (some older ones say "Deliberate deviation" or
"not native"), and names the native function and address.

## How to keep this list

When a commit fixes a defect of the original game:

1. Mark the change in the source with a `LibreCreatures deviation`
   comment that names the native function and address.
2. Add a row to the correct table below, with the fix's commit hash,
   in the commit straight after the fix (a commit cannot name its own
   hash). A new row takes the next free number; numbers do not
   change. Add later commits for the same defect to the same row.
3. If the defect was in our translation and not in the original, do not
   add it here.
4. Give the row a native-compatibility switch: add `LC_FIX_<row>` to
   `src/c1/common/compat.hpp` and keep the native behaviour behind
   `LC_FIX(<row>)` (see "Native mode" below).  A guard against a native
   crash or memory corruption is listed there instead, without a switch.

## Native mode

Every row in the Game tables can be switched back to the native
behaviour, so LibreCreatures can be compared with the original
executable.  `src/c1/common/compat.hpp` has one switch per row:

- **Default build:** every fix on.
- **Native build** (`LC_NATIVE_MODE`; CMake `-DLC_NATIVE_MODE=ON`, lab
  profiles `librecreatures-native` and `librecreatures-neorender-native`):
  every switchable fix off.  `-DLC_FIX_<row>=1` keeps one row fixed.
- **Runtime build** (`LC_COMPAT_RUNTIME`; CMake `-DLC_COMPAT_RUNTIME=ON`,
  lab profiles `librecreatures-compat` and `librecreatures-neorender-compat`):
  the environment variable `C1_NATIVE_ROWS` names the rows that run native,
  as `all` or a comma list, and `-<row>` keeps one fixed (`all,-4,-5`).
  Shipped builds are runtime builds: the player sets the rows in
  `librefixes.ini` (Options > LibreFixes).  `C1_NATIVE_ROWS` overrides the
  file for that run, locked rows too, and is never written to it.

The lab trace's "world opened" line names the rows running native.

Some rows are not switched:

- **Guards** against native crashes, memory corruption or reads past a
  buffer: rows 3, 10, 19, 20, 21, 34, 46, 47, 48, 58, 60, 63, 66, 77, 78, 79, 80,
  81, 82, 84, 85, 86, 87 and 89.  On
  any input where native does not crash or corrupt memory they behave as
  native does; a crash cannot be compared.
- **Row 25:** native's plane depends on heap addresses, so it cannot be
  reproduced.
- **Row 65:** LibreCreatures does what native does when the syntax-error
  box's OK is pressed; a modal box would stop every automated run.

Checked: **code** = read in the native disassembly; **lab** = the defect
was reproduced in the lab (on the CE executable, or on a faithful
translation before the fix); **A/B** = measured in a lab A/B;
**retail** = seen in a retail executable; **data** = in the shipped world
files; **test** = seen in a function test that runs the native code
(`tests/equivalence`).

## Game

### Brain

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 1 | The creature update cycles through five cohorts but steps by four, so every fourth creature gets a second brain, chemistry and action pass every fifth tick. C2, C3 and DS use four. | `SFCDoc::UpdateWorld` | 61b6bf3 | code |
| 2 | Connection-rule evaluation stops at the first rule with no connections, and the rule registers are not reset per neuron, so a neuron can read the previous neuron's values. | `CLobe::UpdateLatePhase` @00405170 | a6e6969 | code |
| 3 | A lobe with zero neurons divides by zero. | `CLobe::UpdateEarlyPhase` @00405010 | afca2d6 | code, crash dump |
| 4 | Dendrite migration compares the candidate with every active neuron, the migrating neuron included, so the neuron always matches itself and its weights are zeroed instead of moved. | `MigrateRuleConnections` @00405da0 | 6d52950 | code, lab |
| 5 | The migration duplicate test packs the pointer sum and product into 16 bits each, so different dendrite sets pass as duplicates. A matching test is now confirmed by comparing the target sets. | `MigrateRuleConnections` @00405da0 | 68c25ae, e058524 | code, lab |
| 6 | A loose dendrite rejects its own previous target, or another loose dendrite's stale target, as a duplicate. | `CLobe::UpdateLatePhase` @00405170 (attach at 00405990) | 75b9911, a0c2773 | code, lab |
| 82 | A new brain's neurons start with the firing and activation, and its dendrites with the susceptibility, that the heap held at that place: the lobe allocation never sets them, and the first brain updates read them. | `CLobe::AllocateRuntimeState` @00402840 | d0a9bcd | code, test |

### Genetics

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 7 | Mutation can change the four bytes of the `gext` pigment tag, which deletes the pigment extension from the creature and all its descendants (since version 1.04). | `CopyGenomeGeneWithMutation` @00418c70 | e4bfe75 | code, model |
| 50 | A reaction keeps only the low byte of a product's yield before adding it, so 1 A -> 2 B with 128 A makes no B (256 wraps to 0). | `CBiochemistry::Update` @0042ee10 | f3050aa | code |
| 51 | A reaction with the same chemical in both reactant slots counts its supply twice, so 1 A + 1 A -> B makes 10 B from 10 A; the saturating removal hides the shortfall. | `CBiochemistry::Update` @0042ee10 | dbde8a1 | code |
| 81 | A gene whose payload runs past the last `gend` (a mutated header can make a short gene a 256-byte half-life gene near the end) makes the genome reader read past its buffer, and the next gene scan then searches heap memory for a gene tag and can load a gene from there. | `CGenome::FindNextMatchingGene` @00419040, `CBiochemistry::LoadGenome` @0042e940 | 925e885 | code, test |

### Creatures

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 8 | Attention reads the stored sense value as a signed byte and clamps negative values to zero, so every sense value of 128 or more reaches the brain as 0. | `UpdateAttention` @0040bbc0 | 3239019 | code |
| 9 | `kill` parks an object at (1000, 4000) but leaves creatures' references to it. Creatures then remember that off-world position and go "down" through lifts for ever (issue #11). | `Object::InitializeRuntimeState` @0042cd70, `ApplyStimulus` @0040b8a0 | 68c25ae, 3239019, de8ba26, 9074ffa | code, retail (1997) |
| 73 | A creature riding a vehicle outside every room perceives the whole world, which also takes in objects `new:` has just made at the world origin before their scripts place them (the egg in the sky). A creature parked outside the world, such as a norn in its egg, keeps perceiving. See [perception-filter.md](perception-filter.md). | `Creature::CanPerceiveObject` @00409750 | d17fed8 | code, lab |
| 10 | An object whose genus has no attention record (simple 40+, compound 15+, creature 5+) is indexed past the 40 records and overwrites other creature memory. | `GetAttentionRecordIndex` @00426430, `Creature::UpdatePerception` @0040bf10 | 0775a23, 02ffd93 | code |
| 11 | When an involuntary action has no script, its "active" mark stays set and the creature ignores its brain until its attention changes. | `UpdateActionSelection` @0040c630 | 734992d | code |
| 12 | Gait selection tests the animation of the next gait up, not the gait it chooses, and never tests gait 7, so a creature can choose an empty gait. | `SelectWalkGait` @004082c0 | 671c403 | code |
| 13 | A `drop` with empty hands gives the creature Disappointment. Food, drink, herb and weed scripts start with `drop`, so eating disappoints the norn. | `NotifyDependentsOnRemoval` @00408420 | ffe8e5c, e6126ca | code, A/B |
| 14 | The lonely and crowded phrases have no space: "comeNorn", "runNorn". | `SpeakDominantDrivePhrase` @0040b160 | ba1dab0 | code |
| 15 | With a zero gestation chemical, the status report repeats the previous field as the pregnancy stage. | creature status report | 71431ba | code |
| 16 | If a creature's generated body sprite file is missing, the creature stays invisible for ever. | `Skeleton` sprite check on world open | b1f6c2d | code, lab |
| 17 | `stm# tact` sends the stimulus to every creature in the world when one creature touches the source. | `QueueTactStimulusForOverlappingCreatures` @00423470 | c5a6a2d | code, lab |
| 49 | An imported creature whose genome is renamed to avoid a clash keeps its gamete under the old name, so it breeds with whichever creature's genome now has that name. | `Creature::Deserialize` @0040dda0 | 81f4fc2 | code |
| 37 | Heard speech is split into words in the shared buffer, and the spaces are not put back, so after the first listener every creature hears only the first word: "push food" becomes "push". | `ProcessHeardWords` @0040a630 | 3fc38cf | code |
| 38 | Two creatures with the same unknown (0) mother or father are taken as siblings. | `UpdatePerception` @0040bf10 | 3fc38cf | code |
| 39 | A second space, or a trailing space, in heard speech makes an empty word, which is learnt as the attended object's name and weakens or erases the real one. | `ProcessHeardWords` @0040a630 | a0c2773 | code |
| 40 | A command whose noun is not the attended object waits for attention to move to it, but is delivered only when the new target is an object, so "rest" (category 0, no object) is lost when attention clears. | `UpdateAttention` @0040bbc0 | 9357a5f | code |
| 42 | A mutated bacterium's output chemical loses the -0x18 offset new bacteria use, so it becomes chemical 0-3 (nothing, pain, need for pleasure, hunger) instead of a disease chemical 232-235 (histamine A or B, sleep toxin, fever toxin). | `ReplicateAndMutate` @00401c70 | 41dbe9a | code |
| 43 | The creature status report names the first room spanning the norn's x that has a floor, so where rooms are stacked it reports the upper room. | `FormatStatusForExternalQuery` @0040e520 | 41dbe9a | code, lab |
| 44 | A bacterial threshold mutation adds its step into the byte before clamping, so a step past 255 or below 0 wraps: kill threshold 250 + 10 becomes 130, activation threshold 0 - 1 becomes 130. | `ReplicateAndMutate` @00401c70 | 0693c52 | code |
| 80 | A creature that hears a word of vowels only ("a", "oo") makes its baby-talk form by scanning past the end of the word, because the vowel test also matches the end of the string, and the copy writes over the bytes after the word: its response, its reinforcement and the next word. | `RegenerateLearnedWordResponse` @0040a1f0 | a9c267d | code |
| 83 | The body sprite file that a creature gets from its genes has two bytes too many after its frame table: the loader writes the table with two more bytes than it fills, and those two bytes are unset heap memory. The records give each image's offset without them, so the game reads every body image two bytes early: the first two pixels of an image come from the image before it (from the unset bytes for the first image), and its last two pixels are lost. | `Skeleton::LoadGenome` @0043c800 | 1649770 | code, test |
| 84 | A creature with a body part that has no sprite file for any variant, life stage or sex cannot be loaded: the body loader goes on with file id 0, takes the image folder itself as the sprite file, and MFC's CFile throws a file exception in the middle of the load. A mutated genus gene can give genus 2 or 3, which has no art, and a mutated appearance gene can ask for a tail, which the CE data does not have. | `Skeleton::LoadGenome` @0043c800, `ResolveExistingBodyPartFilenameWithFallback` @0043c3e0 | 209c91f | code, test |
| 86 | A pose string longer than 15 characters in a saved world makes the load's `strcpy_s` into the 16-byte pose field fail, which ends the game. (Gait strings: longer than 17 characters, the same.) | `Skeleton::Serialize` @0043ade0 | b3bac6d | code |
| 88 | A norn never shows its angry face. The head art has five south expressions, frames 8 to 12: content, look away, happy, sad and angry, and the head picks one by the drive state + 1, which skips the look-away frame. The drive state has only three values (no drive past its lower threshold, one past it, one past its upper threshold), so the face goes no further than sad, and frame 12 is never chosen. The fix shows the angry face when Anger is past its upper threshold and is the strongest drive the face reacts to; a norn with a stronger other need still looks sad. The world saves the angry face as sad (state 2): the original recomputes the state only in world phase 7, once every 16 ticks, and until then a saved 3 would draw head frame 13, or 26 with the eyes closed, past the head's frames. Saving it as sad is as LibreC2e (Jeff Dehut) does. | `UpdateDriveThresholdState` @00409110, `Skeleton::RecomputeBodyPartLayout` @0043b5a0, `Skeleton::Serialize` @0043ade0 | 2603267, a80de12 | code, lab |
| 89 | A creature's render plane is drawn again, with no limit, while another creature's plane is 0 to 8 above the draw. The 2001 planes (1000 to 3000) are full at about 223 creatures, and then the next birth or life stage hangs the game. The guard keeps the last draw after 1000 draws. Found by Jeff Dehut in LibreC2e, whose `Creature::choose_plane` has the limit. | `Skeleton::LoadGenome` @0043c800 | a80de12 | code |

### Scripts

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 18 | The egg in the sky: `new:` ends a script's turn with the new object still at the world origin as 2 0 0, so anything that stops the script before its next turn (an event replacing it, its owner dying or being removed, a save) leaves the egg there. C2 and C3 avoided it in their scripts, not their engines: their egg scripts start an `inst` block before `new:`, so the egg is made and placed in one turn (C2 Eden.sfc: `inst,new: simp …,mvto …`; C3 creatureBreeding.cos: `inst`, `new: simp 3 4 1 "eggs"`, `mvsf`, `gene move`, `slow`). C1 has `inst` too (it sets the flag that keeps the interpreter running past `new:`), but its egg script does not use it. Their engine (as openc2e has it) still lets an event replace a running script unless the script called `lock`. | `ExecuteScriptForClassifier`, owner purge, `ExecuteInterpreter` (new: at 0x0041e1bf) | c3d446e, ac8bfa4, 7c7f309, 13c6477 | code, lab (CE) |
| 19 | A script value used as an object pointer is not checked, so a bad value crashes the game. | `stim writ`, `targ`, `mesg writ` and others | bd4c3b5 | code, crash |
| 20 | `rndv` over the whole 32-bit range divides by zero. | `rndv` | a886d6b | code |
| 58 | `ltcy` divides by zero when its upper bound is one below the lower (`ltcy 0 10 9`) or the bounds cover the whole 32-bit range. | `ltcy` (ExecuteInterpreter) | e80b81f | code, lab |
| 60 | `divv` and `modv` check only for a zero divisor, so -2147483648 divided by -1 overflows the signed division and the game stops with a divide error. | `divv`, `modv` (ExecuteInterpreter, IDIV at 00420884 and 00420af2) | b60f4c6 | code, lab |
| 21 | `evnt` with no object stores a null pointer and then uses it. | `evnt` | 140ccd7 | code |
| 53 | `next` resumes at the saved registry index + 1, but `kill` deletes a creature from the registry at once, so `enum 4 0 0,kill targ,next` skips every other creature. | `next` (ExecuteInterpreter) | 217e83a | code, lab |
| 54 | An `enum` with no match skips to the first `next`, so with an `enum` nested in its body it resumes inside the body and its own `next` ends the script. | `ExecuteInterpreter` @0041dc40 (scan at 0041fbb8) | 1a04ab7 | code, lab |
| 55 | The scans that skip an `enum` body or a `doif` branch read the raw script, so control words inside bracketed text count: `[next]` or `[else]` resumes inside the text, `[ending]` closes a `doif` early. | `ExecuteInterpreter` @0041dc40 | b6cee7d | code, lab |
| 56 | `gsub` finds its `subr` label by scanning the raw script, so `subr <id>` inside bracketed text before the real label sends it to the wrong place, and its cache keeps that place. | `ExecuteInterpreter` @0041dc40 (scan at 0041e420) | dce8d60 | code, lab |
| 61 | A script that ends is removed by shifting the later scripts down a slot, but the scheduler still steps to the next slot, so the script after it loses its turn that tick and its `wait` runs a tick long. | `SFCDoc::UpdateWorld` @004324e0, `RemoveFromRunningSchedulerAndDestroy` @0041a3e0 | 0d4dcf4 | code, lab |
| 65 | A syntax error in a script shows a modal box, "Press OK to continue, or Cancel to kill the macro", from inside the running script. Cancel on a script a kit sent through the pipe ends the game: the C runtime aborts while a string is released. | `Macro::ReportSyntaxError` @0041daa0, `MsvcBasicString_Reset` @00406420 | 71431ba, 02dea96 | code, lab |
| 85 | A saved script cursor between the end of the script text and the script's capacity is kept on load, so the script then runs on from bytes after the text that the load never set. | `Macro::Serialize` @004195f0 | cb549ba | code, test |

### World, objects and vehicles

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 22 | The Black Hole: in a gap between rooms, an object's floor becomes y 9999 and the object falls out of the world. | `FindNearestMapRoomBoundsAtPoint` | fc1628c | code |
| 23 | The Flying Lemon: a vehicle moves every object linked to it, even after a script has moved the object away. | `Vehicle::Tick` @0042bd90 | c9b8633 | code, lab (CE) |
| 24 | A lift called to its own floor arrives at once, and its `dpas` runs before the queued `gpas` pickups, which then bind the passengers to the lift for ever. | `gpas` / `dpas` | 5e557a3 | lab (CE) |
| 25 | The plane of a carried object is taken from a table indexed by the low byte of a pointer, so in the incubator it depends on heap addresses. | `UpdateEntityForExplicitRectBoundsAndRedraw` @00428d30 | 274c2ba, 53c46ad | code |
| 26 | Ocean Dome Sound (2 8 15) does not have the "creatures cannot see it" flag, so norns go into the dome to find a vendor (issue #10). | World.sfc, Eden.sfc | 57eeedc | data |
| 69 | The creature Retreat script (4 0 0 21) closes its pain branch with a stray `endi`, so every retreat ends in that branch's "pose 12 ... stop" and the plain retreat (`anim [49505152R]`) never runs. | World.sfc, Eden.sfc | a2da5b5 | data, lab |
| 70 | The creature Say Need scripts (4 0 0 24 and 40) test drive 5 twice, so sleepiness (drive 6) does not get the tired pose. | World.sfc, Eden.sfc | a2da5b5 | data, lab |
| 71 | The drum (2 9 2) is saved active, and its Activate 1 script never clears that state, so norns cannot play it: a creature's activation is refused while the object is already active. | World.sfc, Eden.sfc | 16d136e | data, lab |
| 72 | The island boat's timer script (3 1 7 9) is left from an older boat: it stops at the wrong places with its directions swapped, poses a part the boat does not have, and nothing starts it; its looping trips would block it anyway, since a timer does not run while its object runs another script. | World.sfc, Eden.sfc | bee07f8 | data, lab |
| 27 | An object let go of in the upper half of a room jumps up onto the floor of the room above. | hand drop placement | 1a663b2 | code |
| 90 | A creature let go of by the hand with its feet in the upper half of a room jumps up onto the floor of the room above, as an object did before row 27: the drop keeps the room whose floor is nearest the creature's bottom-left corner, above or below. The fix lands it on the first floor at or below its middle. Only a privileged hand (`Privileges` "skirty") picks up creatures. Found while checking LibreC2e (Jeff Dehut), whose landing rule for a dropped creature differs from the original's. | `Creature::HandleDropEvent` @00409ae0, `Object::UpdateMovementBounds` | a6c8a47 | code, lab |
| 41 | A lift arrives only when its cabin and the floor fall in the same band of whole-pixel speed, so a lift moving at a fractional speed (yvec 384) can step over its floor and never stop. Stock lifts (yvec 768) are not affected. | `Lift::Tick` @0042c4f0 | 8463856 | code, lab |
| 45 | Call buttons queue lift calls in eight slots, but the lift only reads slots 0-3, so with more than four calls waiting the later ones are never answered. Stock lifts have fewer buttons. | `SelectNearestCallButtonAndStartMove` @0042c590, `RequestLiftCall` @00429c10 | 0693c52 | code |
| 52 | A vehicle wraps its pixel X once but keeps the unwrapped fixed-point position, so one that keeps circling the world goes off it from the second circuit, and terrain following reads past the ground table. | `Vehicle::Tick` @0042bd90 | 241e275 | code |
| 57 | `mvto`, `mvby` and `mcrt` move a vehicle's drawn parts but not its fixed-point position, so a moving vehicle, or one a script then restarts with `setv xvec`, goes straight back to where it was. | `Vehicle::Tick` @0042bd90 (position resynchronised only in `HandleQueuedEvent1/2`) | 6fc9ef3 | code, lab |
| 59 | A moving vehicle decodes its animation frames as byte - '0', so a lowercase frame code ('a' = frame 10) shows a different frame while moving than while standing. No stock vehicle is affected. | `Vehicle::Tick` @0042bd90 | 211f7e2 | code |
| 46 | A lift and its call buttons hold pointers to each other but report neither as a reference, so a button deleted with a call pending (or freed at save) leaves the lift reading freed memory, and a button whose lift is deleted keeps using it. | Lift, CallButton (no ReferencesObject / ClearReferencesToObject; `~CallButton` @00423840, `~Lift` @0042c3a0) | 1fe2091 | code, lab |
| 47 | Placing a call button writes the lift's eight-entry floor table at floor_count unchecked, so a ninth button writes past it, and a button in no room indexes the room table with -1. | `CallButton::UpdateLiftStateAndQueueRedraw` @00429a70 | 1fe2091 | code |
| 48 | Exporting a creature deletes it before the file is flushed and closed, and a failed write leaves it removed from the world but not deleted, so a full disk loses the norn. | `OnExportCurrentCreature` @00431d20 | bd85a4e, 31af2db | code |
| 77 | A `new:` object whose sprite file is missing crashes the game. The object is added to the object list before its sprite is loaded; the load fails, the half-built object is freed, and the next world tick runs the freed object. COBs that use a sprite they do not ship (Creatures Caves 891 "Duck", 1401) crash the original after its "Cross your fingers" box. | `Object::Object` @00425750, `SimpleObject::SimpleObject` @00426cf0, `CompoundObject::CompoundObject` @0042a800 | 3ec81ce | code, lab (CE) |
| 78 | `mvto` on a compound object, vehicle or blackboard that has no part 0 yet crashes the game: the move reads part 0's picture without checking that the part exists. | `CompoundObject::MoveToAndRedraw` @0042ae40 (fault at 0042ae94) | 86b7204 | code, lab (CE) |
| 79 | An object whose picture number equals the number of pictures in its sprite range is drawn from past the end of the picture list: the renderer reads whatever memory follows, and can crash. Creatures Caves 187 "Goldfish" (`anim [5678R]` on 8-picture ranges) crashes the original this way when other COBs are installed beside it. | `CWorldRenderer::RenderWorldRectToDIB` @00412aa0 (`cmp`/`jg`), crash in `CImage::GetPixelData` @00442ac0 | 5924b70, 19036e8 | code, lab (CE, gdb) |
| 87 | A speech bubble's 25-byte text field is never cleared, so a saved world holds whatever the heap had after the end of the text (seen: a piece of a script, "it var0,pose 46,d"). Nothing reads those bytes, but no two saves of the same world are alike. | `Bubble::Bubble` @00429da0, `Bubble::SetText` @0042a1a0 | 1402c65 | code, test |

### The hand and the interface

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 28 | A click goes to the highest render plane, so a lift, the incubator or the pianola takes clicks meant for a creature or object in or behind it. A norn standing at a lift takes clicks meant for the lift arrows. | `FindTopmostOverlappingObject` @00425c30 | f31a3a0, a94bdf0 | code, lab |
| 29 | The click is processed before the hand moves to the mouse, so a click on a small target just after moving finds nothing and must be made again. | `UpdateUnboundedPositionAndRedraw` @00427fd0 | cc5fb80 | code, lab |
| 30 | Deleting any creature deselects the selected creature and closes its kits. | `DeleteObjectAndPurgeRuntimeReferences` @0040e9f0 | bb79273 | code |
| 31 | If an object in the hand is killed or deleted, the hand stays frozen. | `kill`, object delete | ba7dae8 | code |
| 32 | Return with nothing typed makes the hand say an empty word. | Return key handling | b30f6fd | code |
| 33 | Save, Save As and a speed change start the world timer again even when the world is paused. | @00434950, @004349b0, @00417f80 | c0d8352, 40962a3 | code |
| 91 | A held object hangs from the hand by its top-left corner at the mouse, below the hand's left edge and off to one side of the grip of the grabbing frame (syst.spr frame 6). The fix centres it under the gap between the fingers and the thumb, 22 pixels from the hand's left; its top stays at the mouse. The object itself moves, so it drops where it is drawn. From LibreC2e (Jeff Dehut), which grips held objects the same way. | `SimpleObject::UpdateUnboundedPositionAndRedraw` @00427fd0 | a6c8a47 | code, lab |
| 67 | Kit toolbar buttons take their tooltips from a fixed table for slots 0-9: in English the Biochemistry, Observation and Injector kits' entries are empty, the languages disagree, and a kit in slot 10 or later has none. | string table 32902-32911 (`CFrameWnd::OnToolTipText`) | f421ff2 | code, lab |
| 68 | A click on an event bar entry that is not a creature requests the camera move and then turns tracking off, which clears the smooth-scroll request: with Smooth Scrolling on, an object within a view of the camera is not scrolled to. | `CEventBar::OnLButtonDown` @0x00416da0 | fb17df2 | code, lab |

### Kit protocol and sound

| # | Defect in the original | Native | Fix | Checked |
| --- | --- | --- | --- | --- |
| 34 | The pipe server gives the UI thread the address of a stack variable and returns after 30 s, so a slow command reads freed memory. | `PipeServerMarshalCommandToMainThread` @00446620 | c0d8352 | code, crash |
| 35 | Over SFC.OLE a brain report does not run its holder's script, so the kits always get firing strength, whatever measure they ask for. | `DispatchFormatBrainActivityReport` @00419400 | 2dcb4fc | code, kit traffic |
| 36 | The 512 KB sound cache is smaller than one looping sound plus a few others, so sounds are dropped with no error. | sound cache | f410c47 | lab |
| 62 | Mute, and losing focus under `sndf fore`, stop the mixer's channels but leave each object's channel handle set; the next sound update takes the silent channel as a finished sound and clears it, so a `sndl` loop never comes back. | `ToggleMuteAndStopSounds` @004333b0, `Object::UpdateSound` @00426190 | c4cd30d | code |
| 63 | A brain activity report is written into the caller's buffer, three bytes for each active neuron, with no limit, so a brain of more than about 1,365 active neurons writes past a kit's 4,096-byte buffer. | `CMacroHolder::DispatchFormatBrainActivityReport` @0x00419400 | 43359e8 | code |
| 64 | Under Wine, a kit that crashes or is killed never tells the game, so its slot stays open: each later message to it fails, and the first click on its Tools item only closes the slot instead of opening the kit. | `CPipeDispatchProxy::Invoke` @00449950, `ExecuteEmbeddedKitTool` @004444e0 | 37cf5e1 | code, lab |
| 66 | A snapshot (`dde: pict`, the Owner's Kit photograph) is copied from one row below its rectangle: the file loses the top row, and a rectangle at the bottom of the view reads one row past the back buffer. | `WriteDibRectToFile` @0x00445900 | 095195f | code |
| 74 | A macro that writes no output leaves the kit holding the script it sent instead of an empty reply, so the Observation Kit, polling `getb ovvd` with no creatures in the world, reads its own script as creature data. **Fixed by lisdude in Creatures 1 Community Edition 10.4** ([changelog](https://lisdude.com/Creatures/Community/C1/)); LibreCreatures ports that fix. | `CMacroHolder::ExecuteAndPublishMacroOutput` @00419340 (10.4: @004191f0) | 0ffa948 | code (10.3 and 10.4), lab |
| 75 | A kit opened from the Tools menu is left to Windows' foreground rules, so on modern Windows it often opens behind the game window. **Fixed by lisdude in Creatures 1 Community Edition 10.4** ([changelog](https://lisdude.com/Creatures/Community/C1/)): the game raises the kit's window itself; LibreCreatures ports that fix. A kit whose window is not yet visible when the launch returns is still not raised, as in 10.4. | `ExecuteEmbeddedKitTool` @004444e0 (10.4: @00444af0, raise @004449b0) | 809243d | code (10.3 and 10.4), lab |
| 76 | Under Wine the game sends to a kit through its named pipe, and tries three times 50 ms apart, taking a missing pipe to mean the kit has closed. Straight after one message the pipe can be missing for a moment, so the message after a kit starts is lost: clicking a dead creature opens the Funeral Kit, which never hears of the death. The game now waits up to a second for the pipe. | `CPipeDispatchProxy::Invoke` @00449950 (SendToKit) | 6714a65 | code, lab |

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
| Object Injector | 3 | [ORIGINAL.md](../src/kits/injector/ORIGINAL.md) |
| Observation Kit | 18 | [ORIGINAL.md](../src/kits/observation/ORIGINAL.md) |
| Owner's Kit | 9 | [ORIGINAL.md](../src/kits/owner/ORIGINAL.md) |
| Science Kit | 5 | [ORIGINAL.md](../src/kits/science/ORIGINAL.md) |
| Score Kit | 12 | [ORIGINAL.md](../src/kits/score/ORIGINAL.md) |
| **Total** | **63** | |

Science Kit bug 5 is the game defect in row 35.
