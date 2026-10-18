#!/bin/sh
# Runs the environment and life-stage equivalence test
# (Creature::UpdateEnvironmentAndLifeStage) against an original
# Creatures.exe (only read).
#
#   tests/equivalence/run_life.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
wine "$out/life_equivalence_native.exe" "$winexe" -cases "$cases" > "$out/life-native.txt" || status=1
tail -2 "$out/life-native.txt"
wine "$out/life_equivalence_fixed.exe" "$winexe" -cases "$cases" > "$out/life-fixed.txt"
tail -1 "$out/life-fixed.txt" | sed "s/^/fixed: /"
# The code under test has no switch, so LC as shipped must match too.
if grep -q '^case ' "$out/life-fixed.txt"; then
    echo "LC as shipped: differing cases, but the code under test has no switch"; status=1
fi
switches=$(for f in Creature::update_environment_and_life_stage; do
               awk "/^[A-Za-z:0-9_]+ $f\\(/,/^}/" "$src/creatures/creature.cpp"
           done | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ -z "$switches" ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
