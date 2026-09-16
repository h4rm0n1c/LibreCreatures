# Fresh Norns were constructed with zero life force

Date: 2026-09-16 (Australia/Perth)

## Symptom

Freshly created or hatched Norns could have a body and brain-shaped sprite
layout, but reported `Dead`, could not behave normally, and were easy to
confuse with the earlier newborn-freeze and sprite failures.

## Root cause

`Biochemistry::load_genome()` searched genome family `0` for all five
biochemistry gene subtypes (receptors, emitters, reactions, half-lives, and
initial chemical concentrations). C1's genome families are brain `0`,
biochemistry `1`, and creature `2`. The valid Norn genomes contain an initial
concentration gene for chemical `0x3b` (life force) with value `122` in family
`1`; the family-0 scan therefore left a newly constructed creature's life
force at zero.

Native evidence from `CBiochemistry::LoadGenome` at `0x0042e940` shows the
matching-gene call receiving family `1`. The port's existing creature-genome
counting code also already used the biochemistry enum, exposing the mismatch
inside the loader.

## Fix

All five scans now use
`GenomeGeneFamily::biochemistry`, rather than a duplicated numeric family-0
literal. The change is in `src/c1/biochemistry/biochemistry.cpp`.

## Verification

- Clean source build completed: `139/139` files compiled, resources compiled,
  and the executable linked successfully.
- In the managed GUI runtime, `NEW: CREA` using real Norn genome `1WER`
  reported `47%` life force and `Healthy`; after ticks it reported `48%` and
  continued updating.
- The same controlled creation path also produced a healthy `TEST` newborn.
- `DDE: GENE` for the selected real genome reported the expected gene-count
  shape beginning `9|40|22|55|1|8`, confirming the real genome was loaded.

The direct newborn-construction path is verified. A fresh Hatchery UI
egg-to-Norn run remains an end-to-end verification item. World saving still
has a separate `Access violation - no RTTI data!` failure and is not fixed by
this change.
