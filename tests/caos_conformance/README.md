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

Suites: `core.caos`, `objects.caos` and `creatures.caos` (same
experiment, another `caos_suite`; the creature suite also needs
`"genetics": [<install>/Genetics]`).

`core.caos` covers the language core: variables and arithmetic (with the
edges: overflow, negative division, division by zero), comparisons and
branches, loops, subroutines, `enum`, `dde:` output, and simple objects
(classification, attributes, object variables, pose, size, movement,
limits, `touc`, `tick`, `kill`) and the room table. Tests print only what
does not depend on handles, the clock or rand.

`objects.caos` covers compound objects and parts, vehicles (`xvec`,
`yvec`, `cabn`), appearance (`base`, `anim`), targets (`targ`, `rtar`, the
pipe's empty `ownr`/`from`/`_it_`), classifier wildcards, sound commands,
scripts installed with `scrp`, timers, and messages. A message or timer
acts on a later tick, so those tests are split into a setup and a check
further down. A pipe script has no owner when no creature is selected, so
`mesg writ` from the pipe sends nothing; a sender object's timer script
sends the messages instead.

Tests named `native_error.*` hit a defect the original reports as a syntax
error (original-bugs.md rows 55 and 56), which it shows as a modal box (row
65), so its pipe call times out; `compare.py` lists them apart.

First run (2026-10-16), CE 10.3 against LibreCreatures 856e4ae:
- LC native: 65 of 65 tests match.
- LC as shipped: 64 of 65; `enum.nested_empty` differs by row 54. The four
  `native_error` tests run correctly with the fixes (rows 55 and 56).

`objects.caos`, first run (2026-10-16), CE 10.3 against 856e4ae: LC native
and LC as shipped both match on 38 of 38 tests. Messages 0, 1 and 2 run a
simple object's scripts 1, 2 and 0 and set `actv` to 1, 2 and 0 in both.

`creatures.caos`, first run (2026-10-16), CE 10.3 against 856e4ae: LC
native and LC as shipped both match on 13 of 13 tests. A creature's brain
is wired at random when it is made, so the suite reads what the genome
fixes, in the turn the creature is made: classifier, life stage, flags,
all 256 chemicals and 16 drives, the winning drive, position and limits.
`enum 4 0 0` does not find a creature made in the same turn in either.
