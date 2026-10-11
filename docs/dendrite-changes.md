# Dendrite migration and re-attachment: changes from CE

This page shows each LibreCreatures change to dendrite migration and loose
dendrite re-attachment as a pseudocode diff against the native code. It
covers rows 4, 5 and 6 of [original-bugs.md](original-bugs.md).

"Native" means the Creatures 1 Community Edition 10.3 `Creatures.exe`. The
native pseudocode was read from the disassembly, and the addresses are given
so it can be checked. Field names are ours; offsets are the evidence.

Layout facts the argument depends on:

- A neuron is 16 bytes. The neurons of a lobe sit in one array
  (`UpdateLatePhase` steps `+0x10` per neuron).
- A connection (dendrite) is 12 bytes: target neuron pointer at `+0`,
  target weight `+7`, baseline weight `+8`, dendrite state `+9`. State 0
  means "loose".
- The decay that makes a dendrite loose zeroes its three weights, but it
  does **not** clear its target pointer.

## 1. `MigrateRuleConnections` (@00405da0), native

`this` is the migrating neuron. `source_lobe` is the lobe that owns it.

```c
void CLobeNeuron::MigrateRuleConnections(CBrain* brain, CLobe* source_lobe, int rule)
{
    n      = this->connection_count[rule];
    target = &brain->lobes[source_lobe->rules[rule].target_lobe];
    m      = target->active_neuron_count;
    if (m < n) return;
    conns  = this->connections[rule];
    for (i = 0; i < n; i++) saved[i] = conns[i].target;

    for (i = 0; i < m; i++) swap(target->active[i], target->active[rand() % m]);

    for (start = 0; start <= m - n; start++) {
        if (two of target->active[start .. start+n) share a nonzero source_lobe_index)
            continue;

        // The candidate is written into this neuron's own connections.
        for (i = 0; i < n; i++) {
            conns[i].target = target->active[start + i];
            conns[i].baseline = conns[i].target_weight = rand_in(rule.weight_min, rule.weight_max);
            conns[i].state = rand_in(rule.state_min, rule.state_max);
        }

        sig = pack(conns, n);                                  // 00405f83..00406004
        for (j = 0; j < source_lobe->active_neuron_count; j++) {   // 00406024..004060cc
            other = source_lobe->active[j];
            if (other->connection_count[rule] != n) continue;
            if (pack(other->connections[rule], n) == sig)      // 004060b0 CMP / JZ
                goto next_start;                               // duplicate
        }
        if (source_lobe->loose_count[rule]) source_lobe->loose_count[rule]--;
        return;                                                // migrated
    next_start: ;
    }

    // Fallback: no start was accepted.                        // 00406116..0040613f
    for (i = 0; i < n; i++) {
        conns[i].target = saved[i];
        conns[i].target_weight = conns[i].baseline = conns[i].state = 0;
    }
}

uint32 pack(conns, n)
{
    sum = 0; product = 1;
    for (i = 0; i < n; i++) {
        sum     += (uint32)conns[i].target;
        product *= (uint32)conns[i].target;
    }
    return (sum << 16) | (product & 0xffff);   // SHL ECX,0x10 / MOVZX EAX,DI / OR ECX,EAX
}
```

LibreCreatures: `migrate_rule_connections` in `src/c1/brain/neuron.cpp`.

### Change 1 (row 4, `6d52950`): the migrating neuron does not match itself

```diff
         for (j = 0; j < source_lobe->active_neuron_count; j++) {
             other = source_lobe->active[j];
+            if (other == this) continue;
             if (other->connection_count[rule] != n) continue;
```

**Why.** The candidate is written into `this->connections[rule]` before the
scan. If `this` fired this tick, it is in `source_lobe->active`, so at
`other == this` the signatures are equal by construction. Every start is
rejected and the fallback zeroes the weights. For an active neuron, native
`MigrateRuleConnections` therefore never migrates: it always just zeroes
that rule's weights. The loop does not test `other == this` anywhere
(00406024..004060cc).

**Measured.** 10 minutes per arm: before the fix, 11 migrations (of about
60,000) failed only because the neuron matched itself; after it, 0. About
92% of migrations fail in both builds, because a start repeats a source
lobe or really duplicates another neuron. That is normal native retrying.
This change is rare in practice.

### Change 2 (row 5, `68c25ae` + `e058524`): a signature match is confirmed against the actual targets

```diff
-            if (pack(other->connections[rule], n) == sig)
+            if (sum(other) == sum(cand)                              // 68c25ae: full 32-bit sum
+                && (uint16)product(other) == (uint16)product(cand)
+                && sorted(other targets) == sorted(cand targets))    // e058524
                 goto next_start;
```

**Why.** `pack` keeps only the low 16 bits of the sum and the low 16 bits of
the product. For one rule, every target is in the same target lobe's
array, so `target = base + 16*k`:

- **Sum:** `low16(sum)` is equal for two sets when `Σk` is equal mod 4096.
  For real lobe sizes, that means the same sum of target indices.
- **Product:** each pointer is a multiple of 8 (heap alignment) or 16, so a
  product of 4 or 5 or more pointers has no information left in its low
  16 bits.

So with 4 or more dendrites, native's test is in practice "same count and
the same sum of target indices". Example: targets {0,1,2,3,9} and
{0,1,2,4,8} both sum to 15, so they are a "duplicate". For fewer
dendrites, the result also depends on heap addresses.

A false match does not zero anything directly. It skips that start, and
zeroing happens only when every start is rejected.

After the change, a candidate is a duplicate only if another active neuron
has exactly the same targets for that rule (order ignored, repeats
counted). The decision no longer depends on addresses. `68c25ae` (whole
sum) is now redundant: the set comparison decides, and the signature is
only a fast pre-filter. It could be put back to native packing without
any change in behaviour.

**Measured** (10 min): migration success went from 8.0% to 8.7%. The packed
test matched falsely 2,014 times, and it would have blocked 486 of the
5,969 successful migrations.

## 2. Loose dendrite re-attachment in `CLobe::UpdateLatePhase` (@00405170, attach block at 00405990), native

```c
for (rule = 0; rule < 2; rule++) {
    if (rules[rule].mode != ATTACH_LOOSE_DENDRITES) continue;
    target = &brain->lobes[rules[rule].target_lobe];
    for (each neuron nr of this lobe) {
        if (nr.firing_strength == 0 || target->active_neuron_count == 0) continue;  // byte nr+2
        conns = nr.connections[rule]; n = nr.connection_count[rule];
        loose = first conns[i] with state == 0;
        if (!loose) { DebugLog("Migrate failed - no loose dens"); continue; }
        selected = target->active[rand() % target->active_neuron_count];
        for (i = 0; i < n; i++)
            if (conns[i].target == selected) goto next_neuron;   // duplicate
        loose->target = selected;
        loose->baseline = loose->target_weight = rand_in(rule.weight_min, rule.weight_max);
        loose->state = rand_in(rule.state_min, rule.state_max);
        if (loose_count[rule]) loose_count[rule]--;
    next_neuron: ;
    }
}
```

LibreCreatures: `Lobe::update_late_phase` in `src/c1/brain/lobe.cpp`.

### Changes 3 and 4 (row 6, `75b9911` and `a0c2773`): only another attached dendrite is a duplicate

```diff
         for (i = 0; i < n; i++)
-            if (conns[i].target == selected) goto next_neuron;
+            if (&conns[i] != loose                 // 75b9911
+                && conns[i].state != 0             // a0c2773
+                && conns[i].target == selected)
+                goto next_neuron;
```

**Why.** A loose dendrite keeps its old target pointer, so the native scan
also rejects targets that are not connections at all:

- **`75b9911`.** The loose dendrite rejects its own old target. Native
  draws a new `rand()` next tick, so this is a deterministic block only when
  the target lobe offers the same neuron every time, for example a single
  active neuron. Then the dendrite cannot re-attach while that input is the
  only one firing.
- **`a0c2773`.** A later loose dendrite's stale target blocks the first
  loose one. The repair always takes the *first* loose dendrite, so the
  later one is never re-attached itself and never moves off that target.

The rule native clearly wants still holds after the change: no two
**attached** dendrites of a neuron share a target.

Caveat for a skeptic: a loose dendrite is not completely inert in native.
The "all inputs firing" register (`svrule_registers[14 + rule]`) looks at
every dendrite's target firing, loose ones included.

**Measured.**

- `75b9911`: 237 of 17,409 reattachments (1.4%) went back to the dendrite's
  own old target, all with 4 or more target neurons firing. The
  single-target stuck case never happened. In native, those reattachments
  would have waited a tick and picked another target.
- `a0c2773`: in an A/B, about 20% more reattachments (1,135,457 against
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
