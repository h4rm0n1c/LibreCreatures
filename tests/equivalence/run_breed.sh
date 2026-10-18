#!/bin/sh
# Runs the breeding test (CGenome::RecombineGenomeStreams with its gene and
# byte copiers) against an original Creatures.exe (only read), with the given
# genomes as parents.
#
#   tests/equivalence/run_breed.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
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
wine "$out/breed_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/breed-native.txt" || status=1
tail -2 "$out/breed-native.txt"
for build in fixed rows; do
    wine "$out/breed_equivalence_$build.exe" "$winexe" $genomes -cases "$cases" > "$out/breed-$build.txt"
    tail -1 "$out/breed-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: row 7, which the rows
# build fixes alone.
grep -o '^case [0-9]*' "$out/breed-fixed.txt" > "$out/breed-fixed.cases" || true
grep -o '^case [0-9]*' "$out/breed-rows.txt" > "$out/breed-rows.cases" || true
if cmp -s "$out/breed-fixed.cases" "$out/breed-rows.cases"; then
    echo "LC as shipped: every differing case is explained by row 7"
else
    echo "LC as shipped: differing cases not explained by row 7"; status=1
fi
switches=$(for f in recombine_genome_streams copy_gene_with_mutation copy_byte_with_mutation; do
               awk "/^[A-Za-z:0-9_]+ Genome::$f\\(/,/^}/" "$src/creatures/genome.cpp"
           done | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(7) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
