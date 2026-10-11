# Dendrite migration and re-attachment: changes from CE

This page shows each LibreCreatures change to dendrite migration and loose
dendrite re-attachment as a pseudocode diff against the native code. It
covers rows 4, 5 and 6 of [original-bugs.md](original-bugs.md).

"Native" means the Creatures 1 Community Edition 10.3 `Creatures.exe`. The
native pseudocode was read from the disassembly, and the addresses are given
so it can be checked. Offsets are the evidence; the names below are C1's
own terms.

| C1 term | Offset | LibreCreatures name |
|---|---|---|
| dendrite type (`dtype`, 0 or 1) | | `rule` / `connection_rule_index` |
| the neuron a dendrite reads (`d.source`) | `+0` | `target_neuron` |
| the lobe a dendrite type reads (`src`) | | the rule's `target_lobe` |
| the lobe that owns the dendrites (`owner`) | | `source_lobe` (migration) or `this` |
| susceptibility | `+6` | `current_weight` |
| STW, the weight the activation sums | `+7` | `target_weight` |
| LTW | `+8` | `baseline_weight` |
| strength (0 = loose) | `+9` | `dendrite_state` |
| min/max LTW, min/max strength | | `baseline_weight_min/max`, `dendrite_state_min/max` |

Layout facts the argument depends on:

- A neuron is 16 bytes. The neurons of a lobe sit in one array
  (`UpdateLatePhase` steps `+0x10` per neuron).
- A dendrite is 12 bytes, with fields at the offsets above.
- Strength decaying to 0 zeroes susceptibility, STW and LTW, but it does
  **not** clear `d.source`.

## 1. `MigrateRuleConnections` (@00405da0), native

`this` is the migrating neuron. `owner` is the lobe that owns it.

```c
void CLobeNeuron::MigrateRuleConnections(CBrain* brain, CLobe* owner, int dtype)
{
    n    = this->dendrite_count[dtype];
    src  = &brain->lobes[owner->dtypes[dtype].source_lobe];
    m    = src->active_neuron_count;
    if (m < n) return;
    dens = this->dendrites[dtype];
    for (i = 0; i < n; i++) saved[i] = dens[i].source;

    for (i = 0; i < m; i++) swap(src->active[i], src->active[rand() % m]);

    for (start = 0; start <= m - n; start++) {
        if (two of src->active[start .. start+n) share a nonzero byte +0xf)
            continue;

        // The candidate is written into this neuron's own dendrites.
        for (i = 0; i < n; i++) {
            dens[i].source   = src->active[start + i];
            dens[i].LTW      = dens[i].STW = rand_in(dtype.min_LTW, dtype.max_LTW);
            dens[i].strength = rand_in(dtype.min_strength, dtype.max_strength);
        }

        sig = pack(dens, n);                                   // 00405f83..00406004
        for (j = 0; j < owner->active_neuron_count; j++) {     // 00406024..004060cc
            other = owner->active[j];
            if (other->dendrite_count[dtype] != n) continue;
            if (pack(other->dendrites[dtype], n) == sig)       // 004060b0 CMP / JZ
                goto next_start;                               // duplicate
        }
        if (owner->loose_count[dtype]) owner->loose_count[dtype]--;
        return;                                                // migrated
    next_start: ;
    }

    // Fallback: no start was accepted.                        // 00406116..0040613f
    for (i = 0; i < n; i++) {
        dens[i].source = saved[i];
        dens[i].STW = dens[i].LTW = dens[i].strength = 0;
    }
}

uint32 pack(dens, n)
{
    sum = 0; product = 1;
    for (i = 0; i < n; i++) {
        sum     += (uint32)dens[i].source;
        product *= (uint32)dens[i].source;
    }
    return (sum << 16) | (product & 0xffff);   // SHL ECX,0x10 / MOVZX EAX,DI / OR ECX,EAX
}
```

LibreCreatures: `migrate_rule_connections` in `src/c1/brain/neuron.cpp`.

### Change 1 (row 4, `6d52950`): the migrating neuron does not match itself

```diff
         for (j = 0; j < owner->active_neuron_count; j++) {
             other = owner->active[j];
+            if (other == this) continue;
             if (other->dendrite_count[dtype] != n) continue;
```

**Why.** The candidate is written into `this->dendrites[dtype]` before the
scan. If `this` fired this tick, it is in `owner->active`, so at
`other == this` the signatures are equal by construction. Every start is
rejected, and the fallback zeroes STW, LTW and strength. So for an active
neuron, native `MigrateRuleConnections` never migrates: it always just
breaks that dendrite type's dendrites. The loop does not test
`other == this` anywhere (00406024..004060cc).

**Measured.** 10 minutes per arm: before the fix, 11 migrations (of about
60,000) failed only because the neuron matched itself; after it, 0. About
92% of migrations fail in both builds, because a start repeats the
`+0xf` byte or really duplicates another neuron. That is normal native
retrying. This change is rare in practice.

### Change 2 (row 5, `68c25ae` + `e058524`): a signature match is confirmed against the actual source neurons

```diff
-            if (pack(other->dendrites[dtype], n) == sig)
+            if (sum(other) == sum(cand)                              // 68c25ae: full 32-bit sum
+                && (uint16)product(other) == (uint16)product(cand)
+                && sorted(other sources) == sorted(cand sources))    // e058524
                 goto next_start;
```

**Why.** `pack` keeps only the low 16 bits of the sum and the low 16 bits of
the product. For one dendrite type, every source neuron is in the same
lobe's array, so `source = base + 16*k`:

- **Sum:** `low16(sum)` is equal for two sets when `Σk` is equal mod 4096.
  For real lobe sizes, that means the same sum of neuron indices.
- **Product:** each pointer is a multiple of 8 (heap alignment) or 16, so a
  product of 4 or 5 or more pointers has no information left in its low
  16 bits.

So with 4 or more dendrites, native's test is in practice "same count and
the same sum of source neuron indices". Example: sources {0,1,2,3,9} and
{0,1,2,4,8} both sum to 15, so they are a "duplicate". With fewer
dendrites, the result also depends on heap addresses.

A false match does not break anything directly. It only skips that start;
the fallback runs only when every start is rejected.

After the change, a candidate is a duplicate only if another active neuron
has exactly the same source neurons for that dendrite type (order ignored,
repeats counted). The decision no longer depends on addresses.
`68c25ae` (the whole sum) is now redundant: the set comparison decides, and
the signature is only a fast pre-filter. It could go back to native packing
with no change in behaviour.

**Measured** (10 min): migration success went from 8.0% to 8.7%. The packed
test matched falsely 2,014 times, and it would have blocked 486 of the
5,969 successful migrations.

## 2. Loose dendrite re-attachment in `CLobe::UpdateLatePhase` (@00405170, attach block at 00405990), native

```c
for (dtype = 0; dtype < 2; dtype++) {
    if (dtypes[dtype].mode != ATTACH_LOOSE_DENDRITES) continue;
    src = &brain->lobes[dtypes[dtype].source_lobe];
    for (each neuron nr of this lobe) {
        if (nr.firing_strength == 0 || src->active_neuron_count == 0) continue;  // byte nr+2
        dens = nr.dendrites[dtype]; n = nr.dendrite_count[dtype];
        loose = first dens[i] with strength == 0;
        if (!loose) { DebugLog("Migrate failed - no loose dens"); continue; }
        selected = src->active[rand() % src->active_neuron_count];
        for (i = 0; i < n; i++)
            if (dens[i].source == selected) goto next_neuron;    // duplicate
        loose->source   = selected;
        loose->LTW      = loose->STW = rand_in(dtype.min_LTW, dtype.max_LTW);
        loose->strength = rand_in(dtype.min_strength, dtype.max_strength);
        if (loose_count[dtype]) loose_count[dtype]--;
    next_neuron: ;
    }
}
```

LibreCreatures: `Lobe::update_late_phase` in `src/c1/brain/lobe.cpp`.

### Changes 3 and 4 (row 6, `75b9911` and `a0c2773`): only another attached dendrite is a duplicate

```diff
         for (i = 0; i < n; i++)
-            if (dens[i].source == selected) goto next_neuron;
+            if (&dens[i] != loose                  // 75b9911
+                && dens[i].strength != 0           // a0c2773
+                && dens[i].source == selected)
+                goto next_neuron;
```

**Why.** A loose dendrite keeps its old source pointer, so the native scan
also rejects neurons that no dendrite is attached to:

- **`75b9911`.** The loose dendrite rejects its own old source. Native
  draws a new `rand()` next tick, so this blocks every time only when the
  source lobe offers the same neuron every time, for example when only one
  neuron is active. Then the dendrite cannot re-attach while that input is
  the only one firing.
- **`a0c2773`.** A later loose dendrite's stale source blocks the first
  loose one. The repair always takes the *first* loose dendrite, so the
  later one is never re-attached itself and never moves off that source.

What native clearly intends still holds after the change: no two
**attached** dendrites of a neuron share a source.

Caveat for a skeptic: a loose dendrite is not completely inert in native.
The "all inputs firing" register (`svrule_registers[14 + dtype]`) looks at
every dendrite's source firing, loose ones included.

**Measured.**

- `75b9911`: 237 of 17,409 re-attachments (1.4%) went back to the dendrite's
  own old source, all with 4 or more source neurons firing. The
  single-source stuck case never happened. In native, those re-attachments
  would have waited a tick and picked another source.
- `a0c2773`: in an A/B, about 20% more re-attachments (1,135,457 against
  945,402, in the two worlds that reached 20 norns). This is the largest
  effect of the four changes. An earlier "2.7 times" figure came from
  comparing two different worlds, so it was not a real effect.

## Restoring native behaviour

Each change is an isolated condition, so each can be reverted alone:

| Change | Commit | To restore native |
|---|---|---|
| 1 | `6d52950` | remove the `existing == &neuron` skip (`neuron.cpp` ~208) |
| 2 | `e058524` | remove the `same_target_set` term (`neuron.cpp` ~222) |
| 2 | `68c25ae` | compare `(sum << 16) \| (product & 0xffff)` instead |
| 3 | `75b9911` | remove `&connection_array[index] != loose` (`lobe.cpp` ~767) |
| 4 | `a0c2773` | remove `dendrite_state != 0` (`lobe.cpp` ~768) |
