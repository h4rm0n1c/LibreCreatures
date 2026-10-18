#!/bin/sh
# Runs the stimulus equivalence test (Creature::ApplyStimulus) against an
# original Creatures.exe (only read), with brains grown from the given
# genomes, and attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_stimulus.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
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
wine "$out/stimulus_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/stimulus-native.txt" || status=1
tail -1 "$out/stimulus-native.txt"
for build in fixed row9; do
    wine "$out/stimulus_equivalence_$build.exe" "$winexe" $genomes -cases "$cases" \
        > "$out/stimulus-$build.txt"
    tail -1 "$out/stimulus-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in apply_stimulus; row 9 is the only one.
grep -o '^case [0-9]* stimulus [0-9]*' "$out/stimulus-fixed.txt" > "$out/stimulus-fixed.cases"
grep -o '^case [0-9]* stimulus [0-9]*' "$out/stimulus-row9.txt" > "$out/stimulus-row9.cases"
if cmp -s "$out/stimulus-fixed.cases" "$out/stimulus-row9.cases"; then
    echo "LC as shipped: every differing case is row 9's"
else
    echo "LC as shipped: differing cases not explained by row 9"; status=1
fi
switches=$(awk '/^void Creature::apply_stimulus\(/,/^}/' "$here/../../src/c1/creatures/creature.cpp" |
           grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(9) " ] || { echo "unexpected switches in apply_stimulus: $switches"; status=1; }
exit $status
