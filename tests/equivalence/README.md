# Function-level equivalence tests

These tests call a function of the original `Creatures.exe` and the
LibreCreatures function that replaces it with the same inputs, and compare
every byte of the result.

- The original exe is only read. Its sections are copied into the test
  process at 0x400000; the test is linked at that base with an empty first
  section for them (`image_hole.ld`, `native_image.cpp`). No entry point,
  C runtime start-up or TLS callback of the original runs, and every
  import it has is a trap that names the import and exits.
- LC is built twice. Native (`LC_NATIVE_MODE`, every fix in
  `docs/original-bugs.md` off) must match the original on every case. As
  shipped, every differing case must be explained by the rows whose
  switches are in the code under test.
- Inputs the original would crash on are not generated; those are the
  ledger's guards.

## Biochemistry: `CBiochemistry::Update` @0042ee10 (CE 10.3)

    tests/equivalence/build.sh [build-dir]
    WINEPREFIX=<throwaway prefix> tests/equivalence/run_biochemistry.sh \
        <CE 10.3 Creatures.exe> [cases] [build-dir]

Each case is random chemistry: 256 concentrations and half-lives, up to 128
emitters, receptors and reactions, the loci they read and write, and 1 to 8
ticks. `EQUIVALENCE_ONLY=decay|emitters|receptors|reactions` narrows the
cases to one kind of record.

First run (2026-10-16): 200,000 cases, 899,974 updates.
- LC native: 0 differing cases.
- LC as shipped: 55,452 differing cases; row 50 alone explains 24,256, row
  51 alone 41,608, the two together 3.

Generated values stay in what the game can reach: reaction amounts 1-16 (the
genome load makes them so; LC's world load maps other bytes into that
range), loci in the brain and creature domains, emission periods 1-255.
