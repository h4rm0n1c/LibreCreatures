#!/bin/sh
# Runs the skeleton gene test (Skeleton::LoadGenome, with the creature's
# sprite file) against an original Creatures.exe (only read), with the given
# genomes.  The game's Images, Body Data and Palettes folders are copied from
# <data-dir> (only read) into two new scratch roots under the build
# directory, one for each side.
#
#   tests/equivalence/run_skelgenes.sh <Creatures.exe> <cases> <build-dir> <data-dir> <genome.gen>...
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3; data=$4
shift 4
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
genomes=
for g in "$@"; do genomes="$genomes $(winepath -w "$g")"; done
for side in native lc; do
    rm -rf "$out/skel/$side"
    mkdir -p "$out/skel/$side/out"
    for d in Images "Body Data" Palettes; do cp -r "$data/$d" "$out/skel/$side/"; done
done
roots="$(winepath -w "$out/skel/native") $(winepath -w "$out/skel/lc")"
status=0
wine "$out/skelgenes_equivalence_native.exe" "$winexe" $roots $genomes -cases "$cases" \
    > "$out/skelgenes-native.txt" || status=1
grep -v '^case ' "$out/skelgenes-native.txt" | tail -3
for build in fixed rows; do
    wine "$out/skelgenes_equivalence_$build.exe" "$winexe" $roots $genomes -cases "$cases" \
        > "$out/skelgenes-$build.txt"
    tail -1 "$out/skelgenes-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: row 83, which the rows
# build fixes alone.
grep -o '^case [0-9]*' "$out/skelgenes-fixed.txt" > "$out/skelgenes-fixed.cases"
grep -o '^case [0-9]*' "$out/skelgenes-rows.txt" > "$out/skelgenes-rows.cases"
if cmp -s "$out/skelgenes-fixed.cases" "$out/skelgenes-rows.cases"; then
    echo "LC as shipped: every differing case is explained by row 83"
else
    echo "LC as shipped: differing cases not explained by row 83"; status=1
fi
switches=$( (for f in load_genome load_genus_identity load_appearance_genes build_creature_sprite_gallery \
                 construct_body_parts select_normal_render_plane apply_pose_string; do
                 awk "/^[A-Za-z:0-9_]+ Skeleton::$f\\(/,/^}/" "$src/creatures/skeleton.cpp"
             done
             awk '/^[A-Za-z:0-9_]+ SpriteIndexWriter::/,/^}/' "$src/display/sprite_cache.cpp"
             cat "$src/display/palette.cpp") | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(83) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
