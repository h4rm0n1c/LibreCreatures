# Our bugs

[`original-bugs.md`](original-bugs.md) lists the defects of the original
Creatures 1. This file lists the other kind: defects that LibreCreatures
added itself, in translation from the original executable, and that the
verification work found.

This work started because Altimor and Eem Foo told us, clearly and
correctly, that "it is a faithful port" is a claim that needs proof. They
were right. The function tests in [`tests/equivalence`](../tests/equivalence)
and the CAOS and COB replay tests in
[`tests/caos_conformance`](../tests/caos_conformance) are that proof, and
the table below is what they found on the way. Every row is fixed. A test
now runs the original code beside ours, so the same defect cannot come
back unnoticed.

How to read a row:

- **What we got wrong** is the technical description.
- **In plain words** is what a player could see.
- **Native** is the function of the original (CE 10.3) that the translation
  got wrong.
- **Fix** is the commit on `neorender`. The fixes are also on `main`, with
  other hashes.
- **Found by** is the test that found it.

A defect of the original goes in `original-bugs.md`, not here. A defect
here has no `LC_FIX` switch: LibreCreatures now does what the original
does.

## Creatures

| # | What we got wrong | In plain words | Native | Fix | Found by |
| --- | --- | --- | --- | --- | --- |
| L1 | `apply_stimulus` set the seen flag of the attention record of the stimulus source. The original sets the visible flag only. | A norn that a toy had poked acted as if it had already seen the toy, so the first real sight of it was less interesting than it should have been. | `ApplyStimulus` @0040b8a0 | b89bde1 | perception test |
| L2 | When an object came back into view, `update_perception` used the seen flag of that record. The original uses the flag of the slot for the object's current classifier. | After a script changed what an object was, norns remembered seeing the old thing instead of the new one. | `UpdatePerception` @0040bf10 | b89bde1 | perception test |
| L3 | The third row of the goal-direction interaction table was two places to the left. | When a lift's call button was lit, norns headed for the lift itself instead of pressing the button. | table @0045426c | 9dafe55 | goal-direction test |
| L4 | Four rows of the table that chooses a pose toward a target were one row out of place (a row was there twice). | A norn reaching for something a little below it reached one step too high. | table @00454690 | 621a1fa | skeleton test |
| L5 | `select_target_pose_for_motion` tested `motion_link == this`, which is never true. The original tests the link's bounds reference. | Norns reached out for the thing that was already in their hand. | `SelectTargetPoseForMotion` @0043bf40 | ce41488 | skeleton test |
| L6 | The baby-talk form of a heard word was repeated only when it was shorter than three letters. The original also repeats it when it is shorter than five and the word is longer than five. | A norn said "nri" for "friendly". The original norn says "nrinri". | `RegenerateLearnedWordResponse` @0040a1f0 | 9c5df3b | learned-word test |
| L7 | `process_heard_words` returned at once for a sleeping creature. The original has no sleep test there. | Norns went deaf while they slept. | `ProcessHeardWords` @0040a630 | 7d0e90c | heard-word test |
| L8 | "look" also set the verb neuron (0xff). The original sets only the attention. | "Look" also told the norn to do something. | `ProcessHeardWords` @0040aa51 | d3538c6 | heard-word test |
| L9 | The "look" search used the creature's own movement bounds. The original uses those of its bounds reference (the vehicle) when there is one. | A norn in a lift looked for things around itself, not around the lift. | "look" search @0040ac90 | 75f33e8 | heard-word test |
| L10 | The action-phrase word table had no word for action 11. | A norn walking east had nothing to say about it. | table, `kActionPhraseWordIndices` | c9dc60d | table check |
| L11 | A new skeleton started with drive expression 1. The original starts it at 0. | A newborn's face showed the wrong mood until its first drive update. | `InitializePoseAndMotionState` @0043ac70 | 8360812 | skeleton-gene test |

## Genetics and the brain

| # | What we got wrong | In plain words | Native | Fix | Found by |
| --- | --- | --- | --- | --- | --- |
| L12 | A gait gene byte of 100 or 200 did not end the gait: LC tested for zero before it reduced the byte modulo 100. | A mutant norn could get a longer walk than the original gives it. | `Creature::LoadGenome` @00408960 | 8ab61a5 | gene-expression test |
| L13 | A gait gene cleared the bytes after the gait's end mark. The original keeps them. | Nothing a player can see. Nothing reads those bytes. We changed it to be exact. | `Creature::LoadGenome` @00408960 | 8ab61a5 | gene-expression test |
| L14 | A new lobe drew the two dendrite counts of each neuron in turn. The original draws the first count for all neurons, then the second. | Every new norn got a brain wired a little differently from the one the original would give it from the same random numbers. | `AllocateRuntimeState` @00402840 | 19b7fa6 | brain-gene test |
| L15 | Dendrite spreads 2 and 3 took \|a - b\| of the two draws. The original takes \|a + (b - range)\|. | The same as L14: the right spread of counts, the wrong count for each neuron. | `AllocateRuntimeState` @00402840 | 19b7fa6 | brain-gene test |
| L16 | A lobe size byte outside 1 to 63 became ((byte - 1) % 63) + 1. The original makes it (byte % 63) + 1. | A mutant lobe could come out one neuron row smaller than in the original. | `CLobe::LoadGenome` @004025d0 | 19b7fa6 | brain-gene test |
| L17 | Every lobe larger than 32 by 32 was made 32 by 32. The original does this only for lobes 1 to 7. | A mutant norn with a big perception or concept lobe had it cut down; the original lets it keep it. | `CLobe::LoadGenome` @004025d0 | 19b7fa6 | brain-gene test |
| L18 | `Genome::read_payload_byte` read past the end of its buffer when a gene ran past the last end mark. (The original does this too, and worse: row 81.) | A broken genome could make LC read memory that was not its own. | `CGenome` reader | 925e885 | biochemistry-gene test |
| L19 | The creature colour remap was not the original's: the swap control is measured from 127.5 and only its size counts, the hue factor is computed in double, and the rotated channels are cut to whole numbers before the swap mixes them. | Norns came out in slightly wrong colours, and a swap gene below the middle swapped the wrong way. | `BuildCreaturePaletteRemap` @00413d20 | 8060f25 | skeleton-gene test |

## World and objects

| # | What we got wrong | In plain words | Native | Fix | Found by |
| --- | --- | --- | --- | --- | --- |
| L20 | The game's host started the nearest-room function from an empty rectangle. The original changes the caller's rectangle in place, so an object outside every room keeps the sides of its last room. | An object that left every room lost its walls; the original lets it keep the old ones. | `FindNearestMapRoomBoundsAtPoint` @00422d70 | 9ef59da | geometry test |
| L21 | The overlap test of the creature fan-out counted a rectangle with no area as touching. The original uses IntersectRect. | Things with no size could still bump into things. | fan-out @00423090, @004231f0, @00423470 | 6f31fff | fan-out test |
| L22 | One unused byte of ambient environment record 9 was wrong. | Nothing a player can see. | ambient table | 165ef7f | table check |

## Scripts and kits

| # | What we got wrong | In plain words | Native | Fix | Found by |
| --- | --- | --- | --- | --- | --- |
| L23 | `room` with an index past the 40 room records skipped its last operand. The original reads it and discards it. | One COB ("Creatures Update 2") stopped reading its script half way, like a recipe with the last page torn out. | `room` @004202fa | b56a5c5 | COB replay |
| L24 | The host copied at most 4,095 bytes of a kit's macro reply. The original returns up to 16 KiB. | A kit that asked about a big world got the first 4 KB of the answer and nothing more. | `ExecuteAndPublishMacroOutput` @00419340 | 8016615 | COB replay |
