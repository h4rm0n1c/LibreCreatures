#!/bin/sh
# Runs the creature gene expression test (Creature::LoadGenome) against an
# original Creatures.exe (only read), with the given genomes at every life
# stage.
#
#   tests/equivalence/run_express.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
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
wine "$out/express_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/express-native.txt" || status=1
tail -2 "$out/express-native.txt"
wine "$out/express_equivalence_fixed.exe" "$winexe" $genomes -cases "$cases" > "$out/express-fixed.txt"
tail -1 "$out/express-fixed.txt" | sed "s/^/fixed: /"
# The code under test has no switch, so LC as shipped must match too.
if grep -q '^case ' "$out/express-fixed.txt"; then
    echo "LC as shipped: differing cases, but the code under test has no switch"; status=1
fi
switches=$(awk '/^[A-Za-z:0-9_]+ Creature::load_genome\(/,/^}/' "$src/creatures/creature.cpp" |
           grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ -z "$switches" ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
