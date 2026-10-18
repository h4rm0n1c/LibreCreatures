#!/bin/sh
# Runs the perception equivalence test (Creature::UpdatePerception with
# Creature::CanPerceiveObject) against an original Creatures.exe (only read),
# with brains grown from the given genomes, and attributes LC's differences
# to docs/original-bugs.md rows.
#
#   tests/equivalence/run_perception.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
shift 3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
genomes=
for g in "$@"; do genomes="$genomes $(winepath -w "$g")"; done
status=0
wine "$out/perception_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/perception-native.txt" || status=1
tail -2 "$out/perception-native.txt"
for build in fixed rows; do
    wine "$out/perception_equivalence_$build.exe" "$winexe" $genomes -cases "$cases" \
        > "$out/perception-$build.txt"
    tail -1 "$out/perception-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the functions under test: rows 9, 38 and
# 73, which the rows build fixes alone.
grep -o '^case [0-9]*' "$out/perception-fixed.txt" > "$out/perception-fixed.cases"
grep -o '^case [0-9]*' "$out/perception-rows.txt" > "$out/perception-rows.cases"
if cmp -s "$out/perception-fixed.cases" "$out/perception-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 9, 38 and 73"
else
    echo "LC as shipped: differing cases not explained by rows 9, 38 and 73"; status=1
fi
switches=$(for f in Creature::can_perceive Creature::update_perception \
                    Creature::trigger_built_in_stimulus Creature::apply_stimulus \
                    Creature::first_seen_stimulus_magnitude motion_link_facts_for; do
               awk "/^[A-Za-z:0-9_]+ $f\\(/,/^}/" "$src/creatures/creature.cpp"
           done | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(38) LC_FIX(73) LC_FIX(9) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
