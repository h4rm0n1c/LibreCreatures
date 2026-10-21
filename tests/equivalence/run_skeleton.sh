#!/bin/sh
# Runs the creature skeleton equivalence test (body layout, stepping, poses
# and movement) against an original Creatures.exe (only read), and
# attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_skeleton.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
wine "$out/skeleton_equivalence_native.exe" "$winexe" -cases "$cases" \
    > "$out/skeleton-native.txt" || status=1
tail -2 "$out/skeleton-native.txt"
for build in fixed rows; do
    wine "$out/skeleton_equivalence_$build.exe" "$winexe" -cases "$cases" > "$out/skeleton-$build.txt"
    tail -1 "$out/skeleton-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: rows 22 and 88,
# which the rows build fixes alone.
grep -o '^case [0-9]*' "$out/skeleton-fixed.txt" > "$out/skeleton-fixed.cases"
grep -o '^case [0-9]*' "$out/skeleton-rows.txt" > "$out/skeleton-rows.cases"
if cmp -s "$out/skeleton-fixed.cases" "$out/skeleton-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 22 and 88"
else
    echo "LC as shipped: differing cases not explained by rows 22 and 88"; status=1
fi
switches=$({ for f in recompute_body_part_layout update_limb_frames_for_pose \
                     update_anchor_and_bounds apply_pose_string set_target_pose_string \
                     is_pose_at_target set_target_pose_from_table_index \
                     select_target_pose_for_motion set_down_foot_position_and_recompute_layout \
                     translate_by set_down_foot_position_and_invalidate_bounds \
                     update_and_invalidate_bounds; do
                 awk "/^[A-Za-z:0-9_ ]+Skeleton::$f\\(/,/^}/" "$src/creatures/skeleton.cpp"
             done
             awk "/^void Creature::advance_pose_animation\\(/,/^}/" "$src/creatures/creature.cpp"
             awk "/^void Object::update_movement_bounds\\(/,/^}/" "$src/objects/object.cpp"
             awk "/^void find_nearest_room_bounds_at_point\\(/,/^}/" "$src/world/map.cpp"
           } | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(22) LC_FIX(88) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
