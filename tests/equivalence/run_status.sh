#!/bin/sh
# Runs the status test (Creature::FormatStatusForExternalQuery) against an
# original Creatures.exe (only read), with the install's MFC for its
# CStrings, and attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_status.sh <Creatures.exe> <cases> <build-dir> <install-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3; install=$4
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
wininstall=$(winepath -w "$install")
status=0
wine "$out/status_equivalence_native.exe" "$winexe" "$wininstall" "$cases" > "$out/status-native.txt" || status=1
tail -2 "$out/status-native.txt"
grep -q ': 0 differ' "$out/status-native.txt" || status=1
for build in fixed rows; do
    wine "$out/status_equivalence_$build.exe" "$winexe" "$wininstall" "$cases" > "$out/status-$build.txt"
    tail -1 "$out/status-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: rows 15 and 43,
# which the rows build fixes alone.
grep -o '^case [0-9]*' "$out/status-fixed.txt" > "$out/status-fixed.cases" || true
grep -o '^case [0-9]*' "$out/status-rows.txt" > "$out/status-rows.cases" || true
if cmp -s "$out/status-fixed.cases" "$out/status-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 15 and 43"
else
    echo "LC as shipped: differing cases not explained by rows 15 and 43"; status=1
fi
exit $status
