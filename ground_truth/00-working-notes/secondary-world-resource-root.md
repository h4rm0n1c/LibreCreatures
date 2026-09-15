# Selected world resources must use the Windows secondary tree

Date: 2026-09-15 (Australia/Perth)

## Finding

The Windows launcher has two resource trees.  The primary tree belongs to the
installation and supplies ordinary immutable assets such as the installed
background galleries and sound files.  The secondary tree belongs to the
currently selected world under the user's Documents directory.  Its root
contains `World.sfc`, `Images`, `Genetics`, `Backup`, and `TempBu`.

The port already had a save-local lazy read fallback for existing creature
`.SPR` files.  It still routed several secondary operations through the
primary install paths, however:

- generated creature body sprites were written to the primary `Images` path;
- body-sprite validation and gallery acquisition preferred the primary path;
- creature genome lookup could ignore the opened save's `Genetics` directory;
- temporary backup refresh and promotion used the primary root;
- legacy-world background acquisition used the primary `Images` path.

On a real Windows installation this can make a selected world's creatures
invisible or make save/generation behavior depend on write access to the
installation directory.  It is especially easy to miss in the existing Wine
harness because its staged primary and secondary roots are intentionally the
same directory.

## Fix

`C1WindowsDocument` now remembers the parent directory of the opened
`World.sfc` and resolves the selected secondary paths from it:

- main/world root -> the `World.sfc` parent;
- images -> `<world>\\Images\\`;
- genetics -> `<world>\\Genetics\\`.

When no save-local parent is available, the launcher registry's active
secondary paths remain the fallback.  Primary paths are still retained as the
fallback for installed assets and lazy sprite reads.  Sound remains primary,
matching the recovered native host's sound initialization.

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir /tmp/bm-cache`:
  139 source files compiled, resource compilation passed, link passed.
- Conditional pacing, vehicle cabin, and horizontal-scroll regression
  harnesses pass.
- Source files are byte-identical between `byte_match` and `LibreCreatures`
  apart from private graphify output.
- A save-backed Wine launch remained alive without a crash, but the existing
  automation smoke prefix did not publish its CAOS pipe, so this run does not
  claim a gameplay-level visibility result.  The pipe/startup issue is a
  separate harness/runtime item.
