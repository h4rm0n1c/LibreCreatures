#!/usr/bin/env python3
"""Compare CAOS conformance outputs (c1-lab experiments with a caos_suite).

    compare.py <experiment dir> <original instance> [other instance...]

Each instance's caos-<instance>.json holds every test's output.  Prints each
test whose output differs from the original's, and a summary per instance;
exits 1 when any instance differs.  Instances default to every caos-*.json
in the folder other than the original's.
"""
import json
import sys
from pathlib import Path


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
    failed = False
    for instance in others:
        outputs = json.loads((folder / f"caos-{instance}.json").read_text())
        boxed = [name for name in reference if name.startswith("native_error.")]
        for name in boxed:
            print(f"{instance}  {name} (the original stops with its syntax-error box)\n"
                  f"    original: {reference[name]!r}\n    {instance}: {outputs.get(name)!r}")
        differing = [name for name in reference
                     if name not in boxed and outputs.get(name) != reference[name]]
        for name in differing:
            print(f"{instance}  {name}\n    original: {reference[name]!r}\n"
                  f"    {instance}: {outputs.get(name)!r}")
        compared = len(reference) - len(boxed)
        print(f"{instance}: {compared - len(differing)} of {compared} tests match the original"
              f" ({len(boxed)} native_error tests listed apart)")
        failed |= bool(differing)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
