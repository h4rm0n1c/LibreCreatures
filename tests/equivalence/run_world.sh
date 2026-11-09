#!/bin/sh
# The world test: the original loads a world, runs <ticks> world ticks (0
# by default) and saves it (world_roundtrip_native.exe), twice, to show that
# its result is repeatable; then the save that LibreCreatures made of the
# same world with the same ticks is compared with it, byte for byte.
#
#   tests/equivalence/run_world.sh <Creatures.exe> <install-dir> <build-dir> <world.sfc>
#                                  [<lc-save.sfc>] [--ticks <n>] [--dump] [--lc-dump <file>]
#
# --dump: the load equivalence test.  After its load, the original writes
# the fields in src/c1/creatures/load_dump_fields.inc (every creature),
# load_dump_brain.inc (every creature's lobes and neurons) and
# load_dump_objects.inc (every object of the registry), C1_WORLD_FIELD_DUMP.
# This takes two more runs of the original, apart from the two whose saves
# are compared: every new block is filled with 0x00 in one and 0xff in the
# other first (C1_WORLD_FILL).  A line that differs between them is a field
# the original never sets: it is listed and left out.  Their raw creature
# and object bytes (C1_WORLD_RAW_DUMP, C1_WORLD_OBJECT_RAW_DUMP) go to
# load_coverage.py, which lists what the tables do not cover yet.  With
# --lc-dump, the dump LibreCreatures wrote after its load of the same world
# (C1_WORLD_FIELD_DUMP in run_once's env, beside C1_WORLD_ROUNDTRIP) is
# compared with the original's, line by line.  A round trip cannot see a
# field that the load puts in the wrong place and the save puts back; this
# can.
#
# The install's Images, Body Data, Palettes, Genetics and Sounds folders, the
# Genetics and Images folders beside the world (its creatures' genomes and
# sprites), and the world are copied into a new scratch root under the build
# directory; nothing in the install or the world's folder is written.
#
# The LC save comes from the game itself, on the same world, with the
# c1-lab MCP:
#
#   build_arm(commit, profile="librecreatures-native")
#   stage_world / create_world  -> a lab world with this World.sfc
#   run_once(world, exe_build=<that exe>,
#            env={"C1_WORLD_ROUNDTRIP": "<in .sfc>|<out .sfc>|<ticks>"})   (Windows paths)
#
# Use the native build (LC_NATIVE_MODE): row 18 of original-bugs.md holds
# a loaded script until its first turn, so LC as shipped saves the scripts
# that the load starts (event 7) at a different point.  The hook seeds
# rand with 1, as the original's C runtime starts, and mutes the world: the
# native side has no sound manager, so it takes no random number for the
# ambient sound.
#
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; install=$2; out=$3; world=$4
[ -n "$world" ] || {
    echo "usage: $0 <Creatures.exe> <install-dir> <build-dir> <world.sfc> [<lc-save.sfc>] [--ticks <n>] [--dump] [--lc-dump <file>]"
    exit 2
}
shift 4
lc_save=; ticks=0; dump=; lc_dump=
while [ $# -gt 0 ]; do
    case $1 in
        --ticks) shift; ticks=$1 ;;
        --dump) dump=1 ;;
        --lc-dump) shift; lc_dump=$1; dump=1 ;;
        *) lc_save=$1 ;;
    esac
    shift
done
here=$(cd "$(dirname "$0")" && pwd)
export WINEDEBUG=-all
root=$out/world
rm -rf "$root"
mkdir -p "$root"
for d in Images "Body Data" Palettes Genetics Sounds; do cp -r "$install/$d" "$root/"; done
world_dir=$(dirname "$world")
for d in Genetics Images; do
    if [ -d "$world_dir/$d" ]; then cp -r "$world_dir/$d/." "$root/$d/"; fi
done
cp "$world" "$root/in.sfc"
for run in 1 2; do
    wine "$out/world_roundtrip_native.exe" "$(winepath -w "$exe")" "$(winepath -w "$install")" \
        "$(winepath -w "$root")" in.sfc "native$run.sfc" "$ticks" > "$out/world-native$run.txt"
done
if [ -n "$dump" ]; then
    # The load test's runs: new blocks filled with 0x00, then 0xff.  Their
    # saves are not compared (the original saves some bytes it never sets).
    for run in 1 2; do
        fill=0x00; [ $run = 2 ] && fill=0xff
        C1_WORLD_FILL=$fill \
        C1_WORLD_FIELD_DUMP="$(winepath -w "$out/world-dump-native$run.txt")" \
        C1_WORLD_RAW_DUMP="$(winepath -w "$out/world-raw-native$run.bin")" \
        C1_WORLD_OBJECT_RAW_DUMP="$(winepath -w "$out/world-objects-native$run.bin")" \
        wine "$out/world_roundtrip_native.exe" "$(winepath -w "$exe")" "$(winepath -w "$install")" \
            "$(winepath -w "$root")" in.sfc "fill$run.sfc" 0 > /dev/null
    done
fi
cat "$out/world-native1.txt"
if ! cmp -s "$root/native1.sfc" "$root/native2.sfc"; then
    echo "the original's round trip is not repeatable"; exit 1
fi
echo "original, $ticks ticks: $(wc -c < "$root/native1.sfc") bytes, the same in two runs"
if [ -n "$dump" ]; then
    status=0
    python3 -I "$here/load_compare.py" "$out/world-dump-native1.txt" "$out/world-dump-native2.txt" \
        ${lc_dump:+"$lc_dump"} > "$out/world-dump.txt" || status=$?
    head -30 "$out/world-dump.txt"
    echo "(all in $out/world-dump.txt)"
    python3 -I "$here/load_coverage.py" "$here/../../src/c1/creatures/load_dump_fields.inc" \
        "$out/world-raw-native1.bin" "$out/world-raw-native2.bin" \
        "$here/../../src/c1/creatures/load_dump_objects.inc" \
        "$out/world-objects-native1.bin" "$out/world-objects-native2.bin" > "$out/world-coverage.txt"
    grep -E '^(creatures|set by|covered by|set but|objects|  [A-Za-z]+:)' "$out/world-coverage.txt"
    echo "(uncovered ranges in $out/world-coverage.txt)"
    [ $status = 0 ] || exit 1
fi
[ -n "$lc_save" ] || exit 0
if cmp "$root/native1.sfc" "$lc_save"; then
    echo "LC native: the same save, byte for byte"
else
    echo "LC native: the save differs ($(wc -c < "$lc_save") bytes)"; exit 1
fi
