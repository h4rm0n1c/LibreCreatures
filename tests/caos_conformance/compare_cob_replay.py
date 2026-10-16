#!/usr/bin/env python3
"""Compare a COB replay run with a snapshot after each COB
(c1-lab experiment with "caos_suite": world_snapshot.caos and
"caos_suite_each_cob": true).

    compare_cob_replay.py <experiment dir> <original instance> [other instance...]

Objects are compared as a multiset of (classifier, attributes): COB
scripts make, change and kill objects over several ticks, so when exactly a
snapshot catches them, and the order they were made in, depends on timing.
For each other instance it walks the COBs in order and prints the
differences each one introduces, marking a difference "transient" when it is
gone by the end of the run.  A COB that failed to inject on one side only is
always reported.  Family counts follow from the objects and are reported at the end only.
Only persistent differences and one-sided failures fail the comparison; a difference already there before the COB (such as a fixed
build's world-data repair) is not repeated.
"""
import json
import sys
from pathlib import Path


def objects(text):
    if text is None or text.startswith("<error"):
        return None
    return [tuple(part.strip("|").split("|")) for part in text.split("|;") if part.strip("|")]


def classifier(packed):
    value = int(packed)
    return f"{value >> 24} {(value >> 16) & 255} {(value >> 8) & 255}"


def differences(original, other, include_families=False):
    """The set of differences between two snapshot pairs: objects present
    on one side more times than on the other, and family counts."""
    found = set()
    a, b = objects(original[0]), objects(other[0])
    if a is None or b is None:
        if a != b:
            found.add(("snapshot", repr(original[0])[:60], repr(other[0])[:60]))
    else:
        from collections import Counter
        ca, cb = Counter(a), Counter(b)
        for item in set(ca) | set(cb):
            if ca[item] != cb[item]:
                found.add(("object", item, ca[item], cb[item]))
    if include_families and original[1] != other[1]:
        found.add(("families", original[1], other[1]))
    return found


def describe(item):
    if item[0] == "object":
        _, fields, count_a, count_b = item
        name = classifier(fields[0]) if fields and fields[0].lstrip("-").isdigit() else repr(fields)
        attr = fields[1] if len(fields) > 1 else "?"
        return f"{name} attr {attr}: original {count_a}, other {count_b}"
    if item[0] == "families":
        return f"family counts: original {item[1]}, other {item[2]}"
    return f"snapshot: original {item[1]}, other {item[2]}"


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    folder = Path(sys.argv[1])
    original = sys.argv[2]
    others = sys.argv[3:] or sorted(
        p.stem[len("caos-"):] for p in folder.glob("caos-*.json")
        if p.stem != f"caos-{original}")
    reference = json.loads((folder / f"caos-{original}.json").read_text())
    reference_state = json.loads((folder / f"state-{original}.json").read_text())
    cobs = [Path(c["cob"]).name for c in reference_state.get("injected", [])]
    failed = False
    for instance in others:
        outputs = json.loads((folder / f"caos-{instance}.json").read_text())
        state = json.loads((folder / f"state-{instance}.json").read_text())
        errors = {Path(c["cob"]).name: c.get("error") for c in state.get("injected", [])}
        reference_errors = {Path(c["cob"]).name: c.get("error")
                            for c in reference_state.get("injected", [])}
        final = differences(
            tuple(reference.get(k) for k in ("snapshot.objects", "snapshot.families")),
            tuple(outputs.get(k) for k in ("snapshot.objects", "snapshot.families")),
            include_families=True)
        seen = set()
        persistent = transient = one_sided = 0
        for cob in cobs + [""]:
            prefix = f"{cob}/" if cob else ""
            keys = (prefix + "snapshot.objects", prefix + "snapshot.families")
            now = differences(tuple(reference.get(k) for k in keys),
                              tuple(outputs.get(k) for k in keys))
            label = cob or "(end of run)"
            if cob and bool(reference_errors.get(cob)) != bool(errors.get(cob)):
                print(f"{instance}  {label}: injection failed on one side: original "
                      f"{reference_errors.get(cob)!r}, {instance} {errors.get(cob)!r}")
                one_sided += 1
            for item in sorted(now - seen, key=str):
                lasting = item in final
                print(f"{instance}  {label}: {describe(item)}"
                      f"{'' if lasting else '  (transient)'}")
                persistent += lasting
                transient += not lasting
            seen = now
        for item in final:
            if item[0] == "families":
                print(f"{instance}  (end of run): {describe(item)}")
        introduced = persistent + one_sided
        print(f"{instance}: {len(cobs)} COBs; {persistent} persistent differences, "
              f"{one_sided} one-sided failures, {transient} transient")
        failed |= introduced > 0
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
