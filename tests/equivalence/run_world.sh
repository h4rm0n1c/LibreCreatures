#!/bin/sh
# The world test: the original loads a world and saves it again
# (world_roundtrip_native.exe), twice, to show that its result is
# repeatable; then the save that LibreCreatures made of the same world is
# compared with it, byte for byte.
#
#   tests/equivalence/run_world.sh <Creatures.exe> <install-dir> <build-dir> <world.sfc> [<lc-save.sfc>]
#
# The install's Images, Body Data, Palettes, Genetics and Sounds folders and
# the world are copied into a new scratch root under the build directory;
# nothing in the install or the world's folder is written.
#
# The LC save comes from the game itself, on the same world, with the
# c1-lab MCP:
#
#   build_arm(commit, profile="librecreatures-native")
#   stage_world / create_world  -> a lab world with this World.sfc
#   run_once(world, exe_build=<that exe>,
#            env={"C1_WORLD_ROUNDTRIP": "<in .sfc>|<out .sfc>"})   (Windows paths)
#
# Use the native build (LC_NATIVE_MODE): row 18 of original-bugs.md holds
# a loaded script until its first turn, so LC as shipped saves the scripts
# that the load starts (event 7) at a different point.  The hook seeds
# rand with 1, as the original's C runtime starts.
#
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; install=$2; out=$3; world=$4; lc_save=$5
[ -n "$world" ] || {
    echo "usage: $0 <Creatures.exe> <install-dir> <build-dir> <world.sfc> [<lc-save.sfc>]"
    exit 2
}
here=$(cd "$(dirname "$0")" && pwd)
export WINEDEBUG=-all
root=$out/world
rm -rf "$root"
mkdir -p "$root"
for d in Images "Body Data" Palettes Genetics Sounds; do cp -r "$install/$d" "$root/"; done
cp "$world" "$root/in.sfc"
for run in 1 2; do
    wine "$out/world_roundtrip_native.exe" "$(winepath -w "$exe")" "$(winepath -w "$install")" \
        "$(winepath -w "$root")" in.sfc "native$run.sfc" > "$out/world-native$run.txt"
done
cat "$out/world-native1.txt"
if ! cmp -s "$root/native1.sfc" "$root/native2.sfc"; then
    echo "the original's round trip is not repeatable"; exit 1
fi
echo "original: $(wc -c < "$root/native1.sfc") bytes, the same in two runs"
[ -n "$lc_save" ] || exit 0
if cmp "$root/native1.sfc" "$lc_save"; then
    echo "LC native: the same save, byte for byte"
else
    echo "LC native: the save differs ($(wc -c < "$lc_save") bytes)"; exit 1
fi
