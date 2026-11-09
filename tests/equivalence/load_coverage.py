#!/usr/bin/env python3
"""The coverage half of the load test (run_world.sh --lc-dump).

The original's load runs twice with its new Creature objects filled with
0x00 and with 0xff (world_roundtrip_native.exe, C1_WORLD_FILL), and the
creatures' bytes are written after each load (C1_WORLD_RAW_DUMP).  A byte
that is the same in both runs was set by the constructor or the load; a
byte that differs was never set.

This lists, over all creatures, the set bytes that no field of
load_dump_fields.inc covers (what the load test cannot see yet), as ranges.
Which table fields the original never sets is decided from the two field
dumps instead (run_world.sh): a text field's bytes after its terminator are
never set, but its text is.

    load_coverage.py <load_dump_fields.inc> <raw-0x00.bin> <raw-0xff.bin>
                     [<load_dump_objects.inc> <objects-0x00.bin> <objects-0xff.bin>]

The object raw dumps hold, per registry object, its MFC class chain (most
derived first, '/'-joined, 128 bytes), its size (4) and its bytes; their
coverage is given per class, with the rows of the class and of its bases.
"""
import re
import sys

OBJECT_SIZE = 0x2CDC

SIZES = {"U8": 1, "Bool": 1, "U32": 4, "I32": 4, "Ptr": 4, "Rect": 16, "CursorFrom": 4}


def fields(path):
    row = re.compile(r"^LOAD_DUMP_FIELD\((\w+),\s*(0x[0-9a-fA-F]+),\s*(\w+)\{(\w*)\},\s*(\d+),\s*(\d+|0x[0-9a-fA-F]+),")
    for line in open(path, encoding="utf-8"):
        m = row.match(line)
        if not m:
            continue
        name, offset, kind, argument, count, stride = m.groups()
        size = int(argument, 0) if kind in ("Text", "Bytes") else SIZES[kind]
        yield name, int(offset, 0), size, int(count), int(stride, 0)


def object_fields(path):
    row = re.compile(r"^LOAD_DUMP_OBJECT_FIELD\((\w+),\s*(\w+),\s*(0x[0-9a-fA-F]+),\s*(\w+)\{(\w*)\},\s*(\d+),\s*(\d+|0x[0-9a-fA-F]+),")
    for line in open(path, encoding="utf-8"):
        m = row.match(line)
        if not m:
            continue
        cls, name, offset, kind, argument, count, stride = m.groups()
        size = int(argument, 0) if kind in ("Text", "Bytes") else SIZES[kind]
        yield cls, name, int(offset, 0), size, int(count), int(stride, 0)


def read_objects(path):
    data, out, at = open(path, "rb").read(), [], 0
    while at < len(data):
        names = data[at:at + 128].split(b"\0")[0].decode("latin-1").split("/")
        size = int.from_bytes(data[at + 128:at + 132], "little")
        out.append((names, data[at + 132:at + 132 + size]))
        at += 132 + size
    return out


def object_coverage(table, zero_path, ones_path):
    zero, ones = read_objects(zero_path), read_objects(ones_path)
    if [n for n, _ in zero] != [n for n, _ in ones]:
        sys.exit("the two object dumps do not match")
    rows = list(object_fields(table))
    by_class, chains = {}, {}
    for (names, a), (_, b) in zip(zero, ones):
        name = names[0]
        chains[name] = names
        set_bytes = {o for o in range(len(a)) if a[o] == b[o]}
        entry = by_class.setdefault(name, [0, len(a), None])
        entry[0] += 1
        entry[2] = set_bytes if entry[2] is None else entry[2] & set_bytes
    print("objects, per class (bytes every object of the class sets):")
    for name in sorted(by_class):
        count, size, set_bytes = by_class[name]
        covered = set()
        for cls, _, offset, fsize, n, stride in rows:
            if cls in chains[name]:
                for i in range(n):
                    covered |= set(range(offset + i * stride, offset + i * stride + fsize))
        hit = len(set_bytes & covered)
        print(f"  {name}: {count} objects, 0x{size:x} bytes, {len(set_bytes)} set, {hit} covered"
              f" ({100.0 * hit / max(1, len(set_bytes)):.1f}%)")
        for a, b in ranges(set_bytes - covered):
            print(f"    0x{a:04x}-0x{b:04x} ({b - a + 1})")


def ranges(offsets):
    out, start, previous = [], None, None
    for o in sorted(offsets):
        if start is None:
            start = previous = o
        elif o == previous + 1:
            previous = o
        else:
            out.append((start, previous))
            start = previous = o
    if start is not None:
        out.append((start, previous))
    return out


def main():
    table, zero, ones = sys.argv[1], open(sys.argv[2], "rb").read(), open(sys.argv[3], "rb").read()
    if len(zero) != len(ones) or len(zero) % OBJECT_SIZE:
        sys.exit("the two raw dumps do not match")
    count = len(zero) // OBJECT_SIZE
    set_bytes = set(range(OBJECT_SIZE))
    for c in range(count):
        base = c * OBJECT_SIZE
        for o in range(OBJECT_SIZE):
            if zero[base + o] != ones[base + o]:
                set_bytes.discard(o)
    covered = set()
    for name, offset, size, n, stride in fields(table):
        for i in range(n):
            start = offset + i * stride
            covered |= set(range(start, start + size))
    uncovered = set_bytes - covered
    print(f"creatures: {count}; object 0x{OBJECT_SIZE:x} bytes")
    print(f"set by the original's constructor or load: {len(set_bytes)} bytes")
    print(f"covered by the table: {len(set_bytes & covered)} of them "
          f"({100.0 * len(set_bytes & covered) / len(set_bytes):.1f}%)")
    print(f"set but not covered: {len(uncovered)} bytes")
    for a, b in ranges(uncovered):
        print(f"  0x{a:04x}-0x{b:04x} ({b - a + 1})")
    if len(sys.argv) > 4:
        object_coverage(sys.argv[4], sys.argv[5], sys.argv[6])


if __name__ == "__main__":
    main()
