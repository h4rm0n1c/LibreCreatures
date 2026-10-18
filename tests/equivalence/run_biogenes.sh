#!/bin/sh
# Runs the biochemistry gene test (CBiochemistry::LoadGenome) against an original Creatures.exe (only read), with the
# given genomes.
#
#   tests/equivalence/run_biogenes.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
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
wine "$out/biogenes_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/biogenes-native.txt" || status=1
tail -2 "$out/biogenes-native.txt"
wine "$out/biogenes_equivalence_fixed.exe" "$winexe" $genomes -cases "$cases" > "$out/biogenes-fixed.txt"
tail -1 "$out/biogenes-fixed.txt" | sed "s/^/fixed: /"
# The code under test has no switch, so LC as shipped must match too.
if grep -q '^case ' "$out/biogenes-fixed.txt"; then
    echo "LC as shipped: differing cases, but the code under test has no switch"; status=1
fi
switches=$( (awk '/^void Biochemistry::load_genome\(/,/^}/' "$src/biochemistry/biochemistry.cpp") | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ -z "$switches" ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
