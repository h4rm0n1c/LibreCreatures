#!/bin/sh
# Runs the rvalue test, the object variables (CAOS r-values obv0, obv1, obv2), against an original Creatures.exe (only read).
# The code under test has no switch of docs/original-bugs.md, so LC native
# and LC as shipped must both match on every case.
#
#   tests/equivalence/run_rvalue.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
for build in native fixed; do
    wine "$out/rvalue_equivalence_$build.exe" "$winexe" "$cases" > "$out/rvalue-$build.txt" || status=1
    tail -1 "$out/rvalue-$build.txt" | sed "s/^/$build: /"
done
exit $status
