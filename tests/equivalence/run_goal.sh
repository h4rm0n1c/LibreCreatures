#!/bin/sh
# Runs the goal-direction equivalence test (Creature::UpdateGoalDirection with
# Creature::ApplyGoalDirection) against an original Creatures.exe (only
# read), with brains grown from the given genomes.
#
#   tests/equivalence/run_goal.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
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
wine "$out/goal_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/goal-native.txt" || status=1
tail -2 "$out/goal-native.txt"
wine "$out/goal_equivalence_fixed.exe" "$winexe" $genomes -cases "$cases" > "$out/goal-fixed.txt"
tail -1 "$out/goal-fixed.txt" | sed "s/^/fixed: /"
# The code under test has no switch, so LC as shipped must match too.
if grep -q '^case ' "$out/goal-fixed.txt"; then
    echo "LC as shipped: differing cases, but the code under test has no switch"; status=1
fi
switches=$(for f in Creature::update_goal_direction Creature::apply_goal_direction; do
               awk "/^[A-Za-z:0-9_]+ $f\\(/,/^}/" "$src/creatures/creature.cpp"
           done | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ -z "$switches" ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
