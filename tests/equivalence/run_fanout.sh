#!/bin/sh
# Runs the creature fan-out (MapData::QueueEvent... and Queue...Stimulus) equivalence test against an original Creatures.exe (only
# read), and attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_fanout.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
wine "$out/fanout_equivalence_native.exe" "$winexe" -cases "$cases" \
    > "$out/fanout-native.txt" || status=1
tail -2 "$out/fanout-native.txt"
for build in fixed rows; do
    wine "$out/fanout_equivalence_$build.exe" "$winexe" -cases "$cases" > "$out/fanout-$build.txt"
    tail -1 "$out/fanout-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: rows 17, 22 and 73, which the
# rows build fixes alone.
grep -o '^case [0-9]*' "$out/fanout-fixed.txt" > "$out/fanout-fixed.cases"
grep -o '^case [0-9]*' "$out/fanout-rows.txt" > "$out/fanout-rows.cases"
if cmp -s "$out/fanout-fixed.cases" "$out/fanout-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 17, 22 and 73"
else
    echo "LC as shipped: differing cases not explained by rows 17, 22 and 73"; status=1
fi
switches=$({ cat "$src/creatures/events.cpp"
             awk "/^bool Creature::can_perceive\\(/,/^}/" "$src/creatures/creature.cpp"
             awk "/^void find_nearest_room_bounds_at_point\\(/,/^}/" "$src/world/map.cpp"
           } | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(17) LC_FIX(22) LC_FIX(73) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
