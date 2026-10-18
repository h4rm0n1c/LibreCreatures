#!/bin/sh
# Runs the brain gene test (CBrain::LoadGenome) against an original Creatures.exe (only read), with the
# given genomes.
#
#   tests/equivalence/run_braingenes.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
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
wine "$out/braingenes_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/braingenes-native.txt" || status=1
tail -2 "$out/braingenes-native.txt"
wine "$out/braingenes_equivalence_fixed.exe" "$winexe" $genomes -cases "$cases" > "$out/braingenes-fixed.txt"
tail -1 "$out/braingenes-fixed.txt" | sed "s/^/fixed: /"
# The code under test has no switch, so LC as shipped must match too.
if grep -q '^case ' "$out/braingenes-fixed.txt"; then
    echo "LC as shipped: differing cases, but the code under test has no switch"; status=1
fi
switches=$( (for f in 'void Brain::load_genome' 'void Brain::initialize_connections'; do awk "/^$f\\(/,/^}/" "$src/brain/brain.cpp"; done; for f in 'void Lobe::load_genome' 'void Lobe::allocate_runtime_state'; do awk "/^$f\\(/,/^}/" "$src/brain/lobe.cpp"; done) | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ -z "$switches" ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
