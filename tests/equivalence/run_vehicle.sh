#!/bin/sh
# Runs the vehicle equivalence test (vehicles, lifts and call
# buttons) against an original Creatures.exe (only read), and
# attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_vehicle.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
wine "$out/vehicle_equivalence_native.exe" "$winexe" -cases "$cases" | tail -2 || status=1
for build in fixed rows; do
    wine "$out/vehicle_equivalence_$build.exe" "$winexe" -cases "$cases" > "$out/vehicle-$build.txt"
    tail -1 "$out/vehicle-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: rows 23, 41, 45
# and 52, which the rows build fixes alone.
grep -o '^case [0-9]*' "$out/vehicle-fixed.txt" > "$out/vehicle-fixed.cases"
grep -o '^case [0-9]*' "$out/vehicle-rows.txt" > "$out/vehicle-rows.cases"
if cmp -s "$out/vehicle-fixed.cases" "$out/vehicle-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 23, 41, 45 and 52"
else
    echo "LC as shipped: differing cases not explained by rows 23, 41, 45 and 52"; status=1
fi
switches=$(cat "$src/objects/vehicle.cpp" "$src/objects/lift.cpp" "$src/objects/call_button.cpp" |
           grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(23) LC_FIX(41) LC_FIX(45) LC_FIX(52) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
