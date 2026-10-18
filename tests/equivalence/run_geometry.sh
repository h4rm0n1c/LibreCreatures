#!/bin/sh
# Runs the map geometry (the room functions and Object::UpdateMovementBounds) equivalence test against an original Creatures.exe (only
# read), and attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_geometry.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
wine "$out/geometry_equivalence_native.exe" "$winexe" -cases "$cases" \
    > "$out/geometry-native.txt" || status=1
tail -2 "$out/geometry-native.txt"
for build in fixed rows; do
    wine "$out/geometry_equivalence_$build.exe" "$winexe" -cases "$cases" > "$out/geometry-$build.txt"
    tail -1 "$out/geometry-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: row 22, which the
# rows build fixes alone.
grep -o '^case [0-9]*' "$out/geometry-fixed.txt" > "$out/geometry-fixed.cases"
grep -o '^case [0-9]*' "$out/geometry-rows.txt" > "$out/geometry-rows.cases"
if cmp -s "$out/geometry-fixed.cases" "$out/geometry-rows.cases"; then
    echo "LC as shipped: every differing case is explained by row 22"
else
    echo "LC as shipped: differing cases not explained by row 22"; status=1
fi
switches=$({ for f in find_nearest_room_bounds_at_point find_room_containing_point \
                     get_ambient_temperature_at_point; do
                 awk "/^[A-Za-z:0-9_* ]+ $f\\(/,/^}/" "$src/world/map.cpp"
             done
             awk "/^void Object::update_movement_bounds\\(/,/^}/" "$src/objects/object.cpp"
           } | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(22) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
