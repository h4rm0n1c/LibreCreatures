#!/bin/sh
# Runs the action selection equivalence test (Creature::UpdateActionSelection with SetAction) against an
# original Creatures.exe (only read), with brains grown from the given
# genomes, and attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_action.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
shift 3
here=$(cd "$(dirname "$0")" && pwd)
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
genomes=
for g in "$@"; do genomes="$genomes $(winepath -w "$g")"; done
status=0
wine "$out/action_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/action-native.txt" || status=1
tail -2 "$out/action-native.txt"
for build in fixed row11; do
    wine "$out/action_equivalence_$build.exe" "$winexe" $genomes -cases "$cases" \
        > "$out/action-$build.txt"
    tail -1 "$out/action-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in update_action_selection and set_action; row 11 is the only one.
grep -o '^case [0-9]* step [0-9]*' "$out/action-fixed.txt" > "$out/action-fixed.cases"
grep -o '^case [0-9]* step [0-9]*' "$out/action-row11.txt" > "$out/action-row11.cases"
if cmp -s "$out/action-fixed.cases" "$out/action-row11.cases"; then
    echo "LC as shipped: every differing case is row 11's"
else
    echo "LC as shipped: differing cases not explained by row 11"; status=1
fi
switches=$(for f in update_action_selection set_action; do
               awk "/^void Creature::$f\\(/,/^}/" "$here/../../src/c1/creatures/creature.cpp"
           done | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(11) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
