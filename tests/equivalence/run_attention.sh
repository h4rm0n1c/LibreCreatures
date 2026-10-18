#!/bin/sh
# Runs the attention equivalence test (Creature::UpdateAttention with
# Creature::SetAction) against an original Creatures.exe (only read),
# with brains grown from the given genomes, and attributes LC's differences
# to docs/original-bugs.md rows.
#
#   tests/equivalence/run_attention.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
shift 3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
genomes=
for g in "$@"; do genomes="$genomes $(winepath -w "$g")"; done
status=0
wine "$out/attention_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/attention-native.txt" || status=1
tail -2 "$out/attention-native.txt"
for build in fixed rows; do
    wine "$out/attention_equivalence_$build.exe" "$winexe" $genomes -cases "$cases" \
        > "$out/attention-$build.txt"
    tail -1 "$out/attention-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in update_attention and set_action: rows 8 and
# 40, which the rows build fixes alone.
grep -o '^case [0-9]*' "$out/attention-fixed.txt" > "$out/attention-fixed.cases"
grep -o '^case [0-9]*' "$out/attention-rows.txt" > "$out/attention-rows.cases"
if cmp -s "$out/attention-fixed.cases" "$out/attention-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 8 and 40"
else
    echo "LC as shipped: differing cases not explained by rows 8 and 40"; status=1
fi
switches=$(for f in update_attention set_action; do
               awk "/^void Creature::$f\\(/,/^}/" "$src/creatures/creature.cpp"
           done | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(40) LC_FIX(8) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
