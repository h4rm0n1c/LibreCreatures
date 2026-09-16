# Generated newborns used 1024-cell brain lobes

Date: 2026-09-16 (Australia/Perth)

## Symptom

Newly created or hatched creatures could be invisible, silent, immobile, or
unable to save correctly. Existing creatures loaded from a save had normal
brain state, while a creature made from `NEW: GENE` followed by `NEW: CREA`
made `World.sfc` grow from roughly 345 KB to roughly 3.1 MB. The generated
genome payload was byte-for-byte the same as its maternal source apart from
the intended parent-moniker bytes.

## Root cause

`Lobe::load_genome` had inverted the native full-grid test. It changed every
applicable lobe whose area was **below** `0x400` cells into a `32x32` lobe.
The ordinary C1 brain lobes are intentionally smaller: the known-good genome
loads them as 16, 40, 16, 40, 32, 16, and 40 cells for lobes 1 through 7.

The malformed newborn therefore allocated six 1024-neuron lobes, including
262144 connections in lobe 6. `Brain::serialize` then archived those arrays,
which explains both the multi-megabyte save and the downstream runtime
failures.

## Native evidence

Ghidra `CLobe::LoadGenome` at `0x004025d0` first grows the lobe width only to
the standard minimum area (`0x10`, `0x28`, or `0x20`, depending on lobe). Its
later full-area branch is the compiler's signed comparison equivalent of
`grid_area > 0x400`; it excludes exactly `0x400` and does not fire for the
normal smaller lobes. The previous port condition used `< 0x400`, reversing
that behavior.

## Fix

Changed the clean-source condition to `grid_area > 0x400` and removed the
invented `applies_full_grid_fallback` predicate. The minimum-area growth and
native 32x32 clamp remain intact.

## Verification

- Ghidra decompilation confirmed the comparison and the native 32x32 clamp.
- Private clean-source build passed: 139/139 translation units, resources,
  and link in `/tmp/bm-cache`.
- Controlled `NEW: GENE` + `NEW: CREA` run now loads brain grids as
  `7x16, 8x2, 8x5, 8x2, 20x2, 8x4, 1x16, 5x8, 40x16`, matching the native
  source genome and known-good saved creatures.
- The fresh-world save was 428,859 bytes instead of approximately 3.1 MB.
- Reloading that saved world reported three family-4 creatures, proving the
  newly created creature survives the archive round trip.
- Temporary archive and brain tracing probes were removed after diagnosis.
