# Dendrite migration and re-attachment: changes from CE

This page shows each LibreCreatures change to dendrite migration and loose
dendrite re-attachment as a pseudocode diff against the native code. It
covers rows 4, 5 and 6 of [original-bugs.md](original-bugs.md).

Thanks to Altimor of the CAOS Coding Cave Discord, who asked for this
review of the changes and supplied the C1 dendrite terms used here.

"Native" means the Creatures 1 Community Edition 10.3 `Creatures.exe`. The
native pseudocode was read from the disassembly, and the addresses are given
so it can be checked. Offsets are the evidence; the names are C1's own
terms, spelled as openc2e's C1 brain spells them (`oldBrain.h`:
`oldNeuron`, `oldDendrite`, `oldLobe`), so the pseudocode here reads
alongside openc2e's code.

| C1 term (openc2e) | Offset | Meaning | LibreCreatures source |
|---|---|---|---|
| `type` (0 or 1) | | a dendrite type; a neuron has `dendrites[0]` and `dendrites[1]` | `DendriteType`, `type0_dendrites`, `type1_dendrites` |
| `src` | `+0` | the neuron a dendrite reads | `Dendrite::source` |
| `suscept` | `+6` | susceptibility | `Dendrite::susceptibility` |
| `stw` | `+7` | short-term weight, the weight the state sums | `Dendrite::stw` |
| `ltw` | `+8` | long-term weight | `Dendrite::ltw` |
| `strength` | `+9` | 0 = loose | `Dendrite::strength` |
| `state` | | a neuron's activation | `LobeNeuron::activation` |
| `output` | `+2` | a neuron's firing strength | `LobeNeuron::firing_strength` |
| `percept_src` | `+0xf` | the input lobe a perception-lobe neuron copies; 0 = none | `LobeNeuron::source_lobe_index` |
| `active_neurons` | | the lobe's neurons firing this tick | `active_neurons` |
| `loose_dendrites[type]` | | the lobe's count of loose dendrites | `per_type_loose_dendrite_count` |
| source lobe | | the lobe a dendrite type reads | `DendriteType::source_lobe` |
| min/max LTW, min/max strength | | the dendrite type's ranges | `min_ltw`, `max_ltw`, `min_strength`, `max_strength` |

The source uses these terms too (`src/c1/brain/lobe.hpp`), and the other
dendrite type fields follow openc2e's C1 names: `relax_suscept`,
`relax_stw`, `ltw_gain_rate`, `gain_rate` and `gain_svrule`, `lose_rate` and
`lose_svrule`, `suscept_svrule`, `reinforce_svrule`, `spread`, `fanout`,
`migration`.  SVRule tokens use the Genetics Kit names (`SVRuleToken`,
`src/c1/brain/rules.hpp`).

In each diff below, lines without a sign are native C1, lines marked `-`
are native lines LibreCreatures changes, and lines marked `+` are what
LibreCreatures has instead.  A `+` line on its own is something native C1
does not have at all.

Layout facts the argument depends on:

- A neuron is 16 bytes. The neurons of a lobe sit in one array (the late
  phase steps `+0x10` per neuron).
- A dendrite is 12 bytes, with fields at the offsets above.
- `strength` decaying to 0 zeroes `suscept`, `stw` and `ltw`, but it does
  **not** clear `src`.

## 1. Migrating a neuron's dendrites (@00405da0), native

When every dendrite of a type is loose, the neuron moves all of them to a
new set of sources at once. openc2e: `oldLobe::neuronTryAllLooseMigration`,
with `migrationTryCandidateSlice`.

`neu` is the migrating neuron; `lobe` is the lobe it belongs to.

```c
void migrate(brain, lobe, neu, type)                         // @00405da0
{
    n    = neu.dendrites[type].count;
    src  = brain.lobes[lobe.dendrite_type[type].source_lobe];
    m    = src.active_neurons.count;
    if (m < n) return;
    dens = neu.dendrites[type];
    for (i = 0; i < n; i++) saved[i] = dens[i].src;

    for (i = 0; i < m; i++) swap(src.active_neurons[i], src.active_neurons[rand() % m]);

    for (start = 0; start <= m - n; start++) {
        if (two of src.active_neurons[start .. start+n) share a nonzero percept_src)
            continue;

        // The candidate is written into this neuron's own dendrites.
        for (i = 0; i < n; i++) {
            dens[i].src      = src.active_neurons[start + i];
            dens[i].ltw      = dens[i].stw = rand_in(type.min_ltw, type.max_ltw);
            dens[i].strength = rand_in(type.min_strength, type.max_strength);
        }

        hash = magicalHash(dens, n);                           // 00405f83..00406004
        for (j = 0; j < lobe.active_neurons.count; j++) {      // 00406024..004060cc
            other = lobe.active_neurons[j];
            if (other.dendrites[type].count != n) continue;
            if (magicalHash(other.dendrites[type], n) == hash) // 004060b0 CMP / JZ
                goto next_start;                               // duplicate
        }
        if (lobe.loose_dendrites[type]) lobe.loose_dendrites[type]--;
        return;                                                // migrated
    next_start: ;
    }

    // Fallback: no start was accepted.                        // 00406116..0040613f
    for (i = 0; i < n; i++) {
        dens[i].src = saved[i];
        dens[i].stw = dens[i].ltw = dens[i].strength = 0;
    }
}

uint32 magicalHash(dens, n)        // openc2e's name for it: oldNeuron::magicalHash
{
    sum = 0; product = 1;
    for (i = 0; i < n; i++) {
        sum     += (uint32)dens[i].src;
        product *= (uint32)dens[i].src;
    }
    return (sum << 16) | (product & 0xffff);   // SHL ECX,0x10 / MOVZX EAX,DI / OR ECX,EAX
}
```

LibreCreatures: `migrate_rule_connections` in `src/c1/brain/neuron.cpp`.

### Change 1 (row 4, `6d52950`): the migrating neuron does not match itself

```diff
         for (j = 0; j < lobe.active_neurons.count; j++) {
             other = lobe.active_neurons[j];
+            if (other == neu) continue;
             if (other.dendrites[type].count != n) continue;
```

Native C1 has no `other == neu` test anywhere: LibreCreatures adds it; it
does not remove one.

**Why.** The candidate is written into `neu.dendrites[type]` before the
scan. If `neu` fired this tick, it is in `lobe.active_neurons`, so at
`other == neu` the hashes are equal by construction. Every start is
rejected, and the fallback zeroes `stw`, `ltw` and `strength`. So for an
active neuron, native migration never migrates: it always just breaks that
type's dendrites. The loop does not test `other == neu` anywhere
(00406024..004060cc).

**Measured.** 10 minutes per arm: before the fix, 11 migrations (of about
60,000) failed only because the neuron matched itself; after it, 0. About
92% of migrations fail in both builds, because a start repeats a
`percept_src` or really duplicates another neuron. That is normal native
retrying. This change is rare in practice.

### Change 2 (row 5, `68c25ae` + `e058524`): a hash match is confirmed against the actual source neurons

```diff
-            if (magicalHash(other.dendrites[type], n) == hash)
+            if (sum(other) == sum(cand)                              // 68c25ae: full 32-bit sum
+                && (uint16)product(other) == (uint16)product(cand)
+                && sorted(other srcs) == sorted(cand srcs))          // e058524
                 goto next_start;
```

**Why.** `magicalHash` keeps only the low 16 bits of the sum and the low 16
bits of the product. For one dendrite type, every `src` is in the same
lobe's array, so `src = base + 16*k`:

- **Sum:** `low16(sum)` is equal for two sets when `Σk` is equal mod 4096.
  For real lobe sizes, that means the same sum of neuron indices.
- **Product:** each pointer is a multiple of 8 (heap alignment) or 16, so a
  product of 4 or 5 or more pointers has no information left in its low
  16 bits.

So with 4 or more dendrites, native's test is in practice "same count and
the same sum of source neuron indices". Example: sources {0,1,2,3,9} and
{0,1,2,4,8} both sum to 15, so they are a "duplicate". With fewer
dendrites, the result also depends on heap addresses. (openc2e's own
`magicalHash` carries a "TODO: make this less stupid".)

A false match does not break anything directly. It only skips that start;
the fallback runs only when every start is rejected.

After the change, a candidate is a duplicate only if another active neuron
has exactly the same `src` neurons for that dendrite type (order ignored,
repeats counted). The decision no longer depends on addresses.
`68c25ae` (the whole sum) is now redundant: the set comparison decides, and
the hash is only a fast pre-filter. It could go back to native packing with
no change in behaviour.

**Measured** (10 min): migration success went from 8.0% to 8.7%. The packed
test matched falsely 2,014 times, and it would have blocked 486 of the
5,969 successful migrations.

## 2. Re-attaching one loose dendrite, in the lobe's late phase (@00405170, block at 00405990), native

When a type's `migration` mode is "attach loose dendrites", a firing neuron
re-attaches its first loose dendrite to a firing neuron of the source lobe.
openc2e: `oldLobe::neuronTryMigration`, then `connectDendrite`.

```c
for (type = 0; type < 2; type++) {
    if (lobe.dendrite_type[type].migration != ATTACH_LOOSE_DENDRITES) continue;
    src = brain.lobes[lobe.dendrite_type[type].source_lobe];
    for (each neuron neu of this lobe) {
        if (neu.output == 0 || src.active_neurons.count == 0) continue;   // byte neu+2
        dens = neu.dendrites[type]; n = dens.count;
        loose = first dens[i] with strength == 0;
        if (!loose) { DebugLog("Migrate failed - no loose dens"); continue; }
        selected = src.active_neurons[rand() % src.active_neurons.count];
        for (i = 0; i < n; i++)
            if (dens[i].src == selected) goto next_neuron;    // duplicate
        loose.src      = selected;
        loose.ltw      = loose.stw = rand_in(type.min_ltw, type.max_ltw);
        loose.strength = rand_in(type.min_strength, type.max_strength);
        if (lobe.loose_dendrites[type]) lobe.loose_dendrites[type]--;
    next_neuron: ;
    }
}
```

LibreCreatures: `Lobe::update_late_phase` in `src/c1/brain/lobe.cpp`.

### Changes 3 and 4 (row 6, `75b9911` and `a0c2773`): only another attached dendrite is a duplicate

```diff
         for (i = 0; i < n; i++)
-            if (dens[i].src == selected) goto next_neuron;
+            if (&dens[i] != loose                  // 75b9911
+                && dens[i].strength != 0           // a0c2773
+                && dens[i].src == selected)
+                goto next_neuron;
```

**Why.** A loose dendrite keeps its old `src`, so the native scan also
rejects neurons that no dendrite is attached to:

- **`75b9911`.** The loose dendrite rejects its own old `src`. Native
  draws a new `rand()` next tick, so this blocks every time only when the
  source lobe offers the same neuron every time, for example when only one
  neuron is active. Then the dendrite cannot re-attach while that input is
  the only one firing.
- **`a0c2773`.** A later loose dendrite's stale `src` blocks the first
  loose one. The repair always takes the *first* loose dendrite, so the
  later one is never re-attached itself and never moves off that `src`.

What native clearly intends still holds after the change: no two
**attached** dendrites of a neuron share a `src`.

Caveat for a skeptic: a loose dendrite is not completely inert in native.
The "all inputs firing" input (the SVRule's `anded0` / `anded1`) looks at
every dendrite's `src` output, loose ones included.

**Measured.**

- `75b9911`: 237 of 17,409 re-attachments (1.4%) went back to the dendrite's
  own old `src`, all with 4 or more source neurons firing. The
  single-source stuck case never happened. In native, those re-attachments
  would have waited a tick and picked another source.
- `a0c2773`: in an A/B, about 20% more re-attachments (1,135,457 against
  945,402, in the two worlds that reached 20 norns). This is the largest
  effect of the four changes. An earlier "2.7 times" figure came from
  comparing two different worlds, so it was not a real effect.

## Restoring native behaviour

Each change is an isolated condition, so each can be reverted alone. The
names in this table are LibreCreatures' own source identifiers.

| Change | Commit | To restore native |
|---|---|---|
| 1 | `6d52950` | remove the `existing == &neuron` skip, which LibreCreatures added (`neuron.cpp` ~232) |
| 2 | `e058524` | remove the `same_target_set` term (`neuron.cpp` ~246) |
| 2 | `68c25ae` | compare `(sum << 16) \| (product & 0xffff)` instead |
| 3 | `75b9911` | remove `&connection_array[index] != loose` (`lobe.cpp` ~817) |
| 4 | `a0c2773` | remove `strength != 0` (`lobe.cpp` ~818) |
