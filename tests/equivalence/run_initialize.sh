#!/bin/sh
# Runs the runtime-reset test against an original Creatures.exe (only read).
# The code under test has no switch of docs/original-bugs.md, so LC native
# and LC as shipped must both match on every case.
#
#   tests/equivalence/run_initialize.sh <Creatures.exe> <cases> <build-dir> <install-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3; install=$4
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
for build in native fixed; do
    wine "$out/initialize_equivalence_$build.exe" "$winexe" "$(winepath -w "$install")" "$cases" > "$out/initialize-$build.txt" || status=1
    tail -1 "$out/initialize-$build.txt" | sed "s/^/$build: /"
done
exit $status
