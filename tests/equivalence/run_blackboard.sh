#!/bin/sh
# Runs the blackboard test, its words, values and display, against an
# original Creatures.exe (only read).
# The code under test has no switch of docs/original-bugs.md, so LC native
# and LC as shipped must both match on every case.
#
#   tests/equivalence/run_blackboard.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
for build in native fixed; do
    wine "$out/blackboard_equivalence_$build.exe" "$winexe" "$cases" > "$out/blackboard-$build.txt" || status=1
    tail -1 "$out/blackboard-$build.txt" | sed "s/^/$build: /"
done
exit $status
