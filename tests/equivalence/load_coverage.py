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


if __name__ == "__main__":
    main()
