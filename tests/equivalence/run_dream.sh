#!/bin/sh
# Runs the dreaming equivalence test (Creature::ProcessDreaming with
# CInstinct::RunDreamStep) against an original Creatures.exe (only read),
# with brains grown from the given genomes, and attributes LC's differences
# to docs/original-bugs.md rows.
#
#   tests/equivalence/run_dream.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
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
wine "$out/dream_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/dream-native.txt" || status=1
tail -1 "$out/dream-native.txt"
for build in fixed rows; do
    wine "$out/dream_equivalence_$build.exe" "$winexe" $genomes -cases "$cases" \
        > "$out/dream-$build.txt"
    tail -1 "$out/dream-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, and the dreaming code has no
# switch of its own, so LC as shipped differs exactly where the brain and
# biochemistry rows (2, 4, 5, 6, 50, 51) together do.
grep -o '^case [0-9]* step [0-9]*' "$out/dream-fixed.txt" > "$out/dream-fixed.cases"
grep -o '^case [0-9]* step [0-9]*' "$out/dream-rows.txt" > "$out/dream-rows.cases"
if cmp -s "$out/dream-fixed.cases" "$out/dream-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 2, 4, 5, 6, 50 and 51"
else
    echo "LC as shipped: differing cases not explained by the brain and biochemistry rows"; status=1
fi
switches=$( (awk '/^void Creature::process_dreaming\(/,/^}/' "$src/creatures/creature.cpp"
             awk '/^void Instinct::run_dream_step\(/,/^}/' "$src/brain/instinct.cpp"
             cat "$src"/brain/*.cpp "$src"/biochemistry/*.cpp) |
           grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(2) LC_FIX(4) LC_FIX(5) LC_FIX(50) LC_FIX(51) LC_FIX(6) " ] ||
    { echo "unexpected switches: $switches"; status=1; }
exit $status
