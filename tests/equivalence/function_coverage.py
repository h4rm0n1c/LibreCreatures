#!/usr/bin/env python3
"""Function coverage of the equivalence tests: which of the original's
functions run while the tests compare it with LibreCreatures.

    function_coverage.py <function_inventory.tsv> <hits> [<report.md>]

<hits> is what EQUIVALENCE_COVERAGE_OUT collected (native_image.cpp): one
"<address> <program>" line for each function a test program's native code
entered.  A function that ran is one whose effects the test compared: it
ran inside a call to the original that the test checks against
LibreCreatures.  A function the test replaced (a recorder or a stub) does
not run and is not counted.

The report gives, for the game's own functions (owner "game"), the share
that ran: in all, by category, by LibreCreatures source file and by test;
then the functions that did not run, by source file.
"""
import collections
import re
import sys


def read_inventory(path):
    rows = []
    for line in open(path, encoding="utf-8"):
        if line.startswith("#") or line.startswith("address\t"):
            continue
        address, owner, category, source, name = line.rstrip("\n").split("\t")
        rows.append({"address": int(address, 16), "owner": owner, "category": category,
                     "source": source or "(none)", "name": name})
    return rows


def read_hits(path):
    by_address = collections.defaultdict(set)
    for line in open(path, encoding="utf-8", errors="replace"):
        parts = line.split()
        if len(parts) >= 2:
            program = parts[1].rsplit(".", 1)[0]
            # brain_equivalence_row4.exe -> brain: one line per test.
            program = re.sub(r"_(native|fixed|rows|row\d+(_\d+)*)$", "", program)
            program = program.replace("_equivalence", "")
            by_address[int(parts[0], 16)].add(program)
    return by_address


def percent(a, b):
    return f"{100.0 * a / b:.1f}%" if b else "-"


def main():
    rows = read_inventory(sys.argv[1])
    hits = read_hits(sys.argv[2])
    out = []
    w = out.append
    w("# Function coverage of the equivalence tests\n")
    w("A function is covered when a test ran it as the original's code, inside a call whose")
    w("result the test compares with LibreCreatures.\n")
    w("| Owner | Functions | Covered | Share |")
    w("| --- | ---: | ---: | ---: |")
    for owner in ("game", "toolchain", "sdk"):
        group = [r for r in rows if r["owner"] == owner]
        ran = sum(1 for r in group if r["address"] in hits)
        w(f"| {owner} | {len(group)} | {ran} | {percent(ran, len(group))} |")
    game = [r for r in rows if r["owner"] == "game"]

    w("\n## Game functions by category\n")
    w("| Category | Functions | Covered | Share |")
    w("| --- | ---: | ---: | ---: |")
    for category, group in sorted(collections.Counter(r["category"] for r in game).items()):
        members = [r for r in game if r["category"] == category]
        ran = sum(1 for r in members if r["address"] in hits)
        w(f"| {category} | {len(members)} | {ran} | {percent(ran, len(members))} |")

    w("\n## Game functions by LibreCreatures source file\n")
    w("| Source | Functions | Covered | Share |")
    w("| --- | ---: | ---: | ---: |")
    sources = collections.defaultdict(list)
    for r in game:
        sources[r["source"]].append(r)
    for source in sorted(sources, key=lambda s: (-len(sources[s]), s)):
        members = sources[source]
        ran = sum(1 for r in members if r["address"] in hits)
        w(f"| {source} | {len(members)} | {ran} | {percent(ran, len(members))} |")

    w("\n## Game functions each test reaches\n")
    w("| Test | Game functions run |")
    w("| --- | ---: |")
    per_test = collections.Counter()
    game_addresses = {r["address"] for r in game}
    for address, programs in hits.items():
        if address in game_addresses:
            for p in programs:
                per_test[p] += 1
    for test, n in sorted(per_test.items(), key=lambda kv: -kv[1]):
        w(f"| {test} | {n} |")

    w("\n## Game functions no test runs, by source file\n")
    for source in sorted(sources):
        missing = [r for r in sources[source] if r["address"] not in hits]
        if not missing:
            continue
        w(f"### {source} ({len(missing)} of {len(sources[source])})\n")
        for r in sorted(missing, key=lambda r: r["address"]):
            w(f"- `{r['address']:08x}` {r['name']} ({r['category']})")
        w("")
    text = "\n".join(out) + "\n"
    if len(sys.argv) > 3:
        open(sys.argv[3], "w", encoding="utf-8").write(text)
    else:
        sys.stdout.write(text)
    covered = sum(1 for r in game if r["address"] in hits)
    print(f"game functions covered: {covered} of {len(game)} ({percent(covered, len(game))})", file=sys.stderr)


if __name__ == "__main__":
    main()
