# Newborn brain: concept lobe never re-wired after dreaming — 2026-09-17

## Symptom
Newborns placed correctly, dreamed through every instinct, woke, and then held
`action=0` forever: no scripts, no gait, no speech, and nil activity in the
kits. Retail creatures loaded from a save in the same world acted normally.

## Measured
Per-lobe probe (`Creatures.motor.log`, default-on): lobe 8 (concept) keeps its
~1350 dendrites, but their summed baseline weight goes 173k -> 0 during the
first instinct, in steps every 3 ticks, and never recovers. With zero weights
lobe 8 never fires, so the decision lobe sees only its quiescent bias; action 0
equals the starting action, so `set_action` is never called.

Lobe 8 rule 0 (mum1 x dad1): mode 2 (migrate), decay_int=3, decay expr [6]
(load register 6, a chemically-driven lobe locus), growth int 2 expr [10,22,5].
Dendrites retiring during a dream is by design; the defect was that retired
dendrites never came back.

## Three native divergences fixed (all read from instructions, not decompile)

1. **Connection registers 16..21 were never loaded.**
   UpdateLatePhase @00405450..0040547d loads, per connection:
   16 <- source neuron firing_strength, 18 <- current_weight,
   19 <- target_weight, 20 <- baseline_weight, 21 <- dendrite_state
   (register file base ESP+0x4c; 17 never written). The decompiler hides these
   stores; the Ghidra DB comment on this function claiming tokens 16..21 "read
   zero" is WRONG (DB is read-only for us — not edited). Same loop also counts
   already-loose dendrites (LEA EBP,[ECX+1]; TEST AL,AL; CMOVNZ EBP,ECX).

2. **Loose-dendrite attach gated on an invented budget.** @00405990
   `CMP byte ptr [EDI+ECX+1],0` with EDI=&rule{n}_connection_count, ECX=-0xb/-0xc
   tests neuron+2 = firing_strength (+2 zeroed / +3 relaxed at 004053c3 confirm
   the field). Native attaches when the neuron fires and only decrements the
   loose count afterwards. The port skipped whenever the count was 0.

3. **Migration candidate walk stalled.** @00405b46 compares neuron_index to the
   STORED candidate and writes only the LOCAL candidate (ESP+0x44+rule*4); the
   stored value is updated after the loop: 99999 when no candidate (walk wraps),
   else migrate that neuron (and its other rule if also migrate mode) and store
   the candidate. The port wrote the stored value inside the loop, picked the
   lowest index, never reset to 99999 — migration stopped permanently.

## Result
Newborn wakes and selects actions (7, 8, 9 observed within ~900 awake ticks);
lobe 8 fires; action scripts drive target poses. Not yet observed choosing a
locomotion action; concept weights remain far below a retail brain (~1.8k vs
~160k summed baseline) — migration is one neuron per update.

## Fixed: brain rule save/load misalignment
Native CBrain::Serialize @00402c50 (LOAD path, destinations relative to the
60-byte CLobeConnectionRule: target +0..3, bytes +4..+7, +8..+16 ending with
growth interval) reads ONE byte into +27 (decay interval) and only then the
10-byte expressions into +17..+26 (growth), +28..+37 (decay), then current-
and target-weight. The port read growth interval, growth expr, decay interval,
decay expr -- every field after the growth interval one byte out. Load and save
both reordered to native (a native file is read by native load, so save order
matches load order).

Also: native CLobeRuleExpression::LoadFromGenome @00402250 pads tokens[8..9]
with 0 (END; evaluator stops on TEST AL,AL at 004053b4). The port padded with
30, the normalisation modulus. Fixed.

Verified: retail creatures loaded from knowngood now decode lobe 8 rule 0 as
grow_int=2 grow=[10,22,5] decay_int=3 decay=[6] -- identical to the genome
loader for a newborn, including zero padding. Retail creatures remain active
with more varied actions (0..11); their concept-lobe weight now falls
160k -> ~10-15k over ~1300 ticks as native decay/migration run. Save->reload
round trip not yet exercised.
