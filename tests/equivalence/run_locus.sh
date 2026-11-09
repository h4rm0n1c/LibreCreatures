#!/bin/sh
# Runs the genome-locus test against an original Creatures.exe (only read).
# The code under test has no switch of docs/original-bugs.md, so LC native
# and LC as shipped must both match on every case.
#
#   tests/equivalence/run_locus.sh <Creatures.exe> <cases (unused)> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
for build in native fixed; do
    wine "$out/locus_equivalence_$build.exe" "$winexe"  > "$out/locus-$build.txt" || status=1
    tail -1 "$out/locus-$build.txt" | sed "s/^/$build: /"
done
exit $status
