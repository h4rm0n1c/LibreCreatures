#!/usr/bin/env python3
"""The comparison half of the load test (run_world.sh --dump / --lc-dump).

    load_compare.py <native-fill-00.txt> <native-fill-ff.txt> [<lc.txt>]

The two native dumps come from the original's load with its new Creature
objects filled with 0x00 and with 0xff.  A line that differs between them is
a field the original never sets: it is listed and not compared.  Every other
line must be the same in LibreCreatures' dump.  Exit 1 when a line differs
or the dumps do not have the same lines.
"""
import sys


def lines(path):
    with open(path, encoding="latin-1") as f:
        return [l.rstrip("\n") for l in f]


def key(line):
    # "creature <c> <field>[<i>] <value>" -> "creature <c> <field>[<i>]"
    parts = line.split(" ", 3)
    return " ".join(parts[:3]) if len(parts) >= 3 else line


def main():
    zero, ones = lines(sys.argv[1]), lines(sys.argv[2])
    if [key(l) for l in zero] != [key(l) for l in ones]:
        sys.exit("the original's two dumps do not have the same fields")
    stable = [a == b for a, b in zip(zero, ones)]
    unset = [key(a) for a, s in zip(zero, stable) if not s]
    print(f"original: {len(zero)} lines, {len(unset)} of them fields it never sets (not compared)")
    names = sorted({k.split(" ")[2].split("[")[0] for k in unset})
    if names:
        print("  never set: " + ", ".join(names))
    if len(sys.argv) < 4:
        return 0
    lc = lines(sys.argv[3])
    if [key(l) for l in lc] != [key(l) for l in zero]:
        print("LC load: the dump does not have the same fields as the original's")
        return 1
    differ = [(a, b) for a, b, s in zip(zero, lc, stable) if s and a != b]
    if not differ:
        print(f"LC load: every compared field is the same ({stable.count(True)} lines)")
        return 0
    print(f"LC load: {len(differ)} compared fields differ")
    for a, b in differ:
        print(f"  original: {a}")
        print(f"  LC:       {b}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
