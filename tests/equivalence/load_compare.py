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
    # Everything but the value, the last word (values have no spaces: text
    # is escaped).  "creature 2 lobe 6 neuron 14 firing 0" ->
    # "creature 2 lobe 6 neuron 14 firing".
    return line.rsplit(" ", 1)[0] if " " in line else line


def field_name(k):
    # "creature 2 lobe 6 neuron 14 dendrite_count[1]" -> "lobe neuron dendrite_count"
    words = [w for w in k.split(" ")[2:] if not w.isdigit()]
    return " ".join(words).split("[")[0]


def main():
    zero, ones = lines(sys.argv[1]), lines(sys.argv[2])
    if [key(l) for l in zero] != [key(l) for l in ones]:
        sys.exit("the original's two dumps do not have the same fields")
    stable = [a == b for a, b in zip(zero, ones)]
    unset = [key(a) for a, s in zip(zero, stable) if not s]
    print(f"original: {len(zero)} lines, {len(unset)} of them fields it never sets (not compared)")
    names = sorted({field_name(k) for k in unset})
    if names:
        print("  never set: " + ", ".join(names))
    if len(sys.argv) < 4:
        return 0
    lc = lines(sys.argv[3])
    if [key(l) for l in lc] != [key(l) for l in zero]:
        print("LC load: the dump does not have the same fields as the original's")
        for a, b in zip(zero, lc):
            if key(a) != key(b):
                print(f"  first difference: original [{a}], LC [{b}]")
                break
        else:
            print(f"  lines: original {len(zero)}, LC {len(lc)}")
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
