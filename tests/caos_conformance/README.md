# CAOS conformance

CAOS scripts run through the kit pipe in the original Creatures.exe and in
LibreCreatures, on a fresh Eden world with no creatures; every test's output
must be the same.

    # c1-lab experiment, one instance per exe:
    {"name": "caos-core", "minutes": 1, "world_source": "fresh", "seed": "none",
     "caos_suite": "<repo>/tests/caos_conformance/core.caos",
     "arms": {"original": {"exe": "<CE 10.3 Creatures.exe>"},
              "lc_native": {"exe": "<LibreCreatures, LC_NATIVE_MODE>"},
              "lc_fixed": {"exe": "<LibreCreatures as shipped>"}},
     "instances": {"ab-a1": "original", "ab-a2": "lc_native", "ab-b1": "lc_fixed"}}
    tests/caos_conformance/compare.py <experiment dir> <original's instance>

`core.caos` covers the language core: variables and arithmetic (with the
edges: overflow, negative division, division by zero), comparisons and
branches, loops, subroutines, `enum`, `dde:` output, and simple objects
(classification, attributes, object variables, pose, size, movement,
limits, `touc`, `tick`, `kill`) and the room table. Tests print only what
does not depend on handles, the clock or rand.

Tests named `native_error.*` hit a defect the original reports as a syntax
error (original-bugs.md rows 55 and 56), which it shows as a modal box (row
65), so its pipe call times out; `compare.py` lists them apart.

First run (2026-10-16), CE 10.3 against LibreCreatures 856e4ae:
- LC native: 65 of 65 tests match.
- LC as shipped: 64 of 65; `enum.nested_empty` differs by row 54. The four
  `native_error` tests run correctly with the fixes (rows 55 and 56).
