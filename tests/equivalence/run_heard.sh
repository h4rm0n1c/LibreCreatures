#!/bin/sh
# Runs the heard-words equivalence test (Creature::ProcessHeardWords)
# against an original Creatures.exe (only read), with brains grown from
# the given genomes, and attributes LC's differences to
# docs/original-bugs.md rows.
#
#   tests/equivalence/run_heard.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
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
wine "$out/heard_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/heard-native.txt" || status=1
tail -2 "$out/heard-native.txt"
for build in fixed rows; do
    wine "$out/heard_equivalence_$build.exe" "$winexe" $genomes -cases "$cases" \
        > "$out/heard-$build.txt"
    tail -1 "$out/heard-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the functions under test: rows 9, 38 and
# 73, which the rows build fixes alone.
grep -o '^case [0-9]*' "$out/heard-fixed.txt" > "$out/heard-fixed.cases"
grep -o '^case [0-9]*' "$out/heard-rows.txt" > "$out/heard-rows.cases"
if cmp -s "$out/heard-fixed.cases" "$out/heard-rows.cases"; then
    echo "LC as shipped: every differing case is explained by rows 9, 37 and 39"
else
    echo "LC as shipped: differing cases not explained by rows 9, 37 and 39"; status=1
fi
switches=$(for f in Creature::process_heard_words Creature::apply_stimulus \
                    Creature::trigger_built_in_stimulus Creature::apply_goal_direction \
                    Creature::update_learned_word_record Creature::regenerate_learned_word_response \
                    Creature::speak_action_phrase resolve_pointer_attention_target; do
               awk "/^[A-Za-z:*0-9_ ]+ $f\\(/,/^}/" "$src/creatures/creature.cpp"
           done | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(37) LC_FIX(39) LC_FIX(9) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
