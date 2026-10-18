#!/bin/sh
# Runs the simple-object equivalence test (pick-up, carrying
# and putting down) against an original Creatures.exe (only read), and
# attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_simple_object.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
wine "$out/simple_object_equivalence_native.exe" "$winexe" -cases "$cases" \
    > "$out/simple_object-native.txt" || status=1
tail -2 "$out/simple_object-native.txt"
for build in fixed rows; do
    wine "$out/simple_object_equivalence_$build.exe" "$winexe" -cases "$cases" > "$out/simple_object-$build.txt"
    tail -1 "$out/simple_object-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: rows 22 and 27,
# which the rows build fixes alone.
grep -o '^case [0-9]*' "$out/simple_object-fixed.txt" > "$out/simple_object-fixed.cases"
grep -o '^case [0-9]*' "$out/simple_object-rows.txt" > "$out/simple_object-rows.cases"
if cmp -s "$out/simple_object-fixed.cases" "$out/simple_object-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 22 and 27"
else
    echo "LC as shipped: differing cases not explained by rows 22 and 27"; status=1
fi
# Row 28 is in find_topmost_overlapping_object, on the branch for the hand's
# own search, which this test does not generate.
switches=$({ for f in tick update_entity_for_explicit_rect_bounds_and_redraw handle_queued_event_4 \
                     handle_queued_event_5 end_interaction_with_source advance_fall \
                     finalize_object_edit; do
                 awk "/^[A-Za-z:0-9_ ]+SimpleObject::$f\\(/,/^}/" "$src/objects/simple_object.cpp"
             done
             for f in update_movement_bounds set_bounds_mode find_topmost_overlapping_object; do
                 awk "/^[A-Za-z:*0-9_ ]+Object::$f\\(/,/^}/" "$src/objects/object.cpp"
             done
             awk "/^void find_nearest_room_bounds_at_point\\(/,/^}/" "$src/world/map.cpp"
           } | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(22) LC_FIX(27) LC_FIX(28) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
