#!/bin/sh
# Runs the learned-word equivalence test (word records and
# phrases) against an original Creatures.exe (only read), and
# attributes LC's differences to docs/original-bugs.md rows.
#
#   tests/equivalence/run_words.sh <Creatures.exe> <cases> <build-dir>
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
status=0
wine "$out/words_equivalence_native.exe" "$winexe" -cases "$cases" | tail -2 || status=1
for build in fixed rows; do
    wine "$out/words_equivalence_$build.exe" "$winexe" -cases "$cases" > "$out/words-$build.txt"
    tail -1 "$out/words-$build.txt" | sed "s/^/$build: /"
done
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test: row 14, which the
# rows build fixes alone.
grep -o '^case [0-9]*' "$out/words-fixed.txt" > "$out/words-fixed.cases"
grep -o '^case [0-9]*' "$out/words-rows.txt" > "$out/words-rows.cases"
if cmp -s "$out/words-fixed.cases" "$out/words-rows.cases"; then
    echo "LC as shipped: every differing case is explained by row 14"
else
    echo "LC as shipped: differing cases not explained by row 14"; status=1
fi
switches=$({ for f in regenerate_learned_word_response update_learned_word_record \
                     initialize_default_vocabulary initialize_learned_word_records \
                     speak_action_phrase speak_dominant_drive_phrase append_default_response_prefix; do
                 awk "/^[A-Za-z:0-9_ ]+Creature::$f\\(/,/^}/" "$src/creatures/creature.cpp"
             done
             cat "$src/creatures/learned_words.cpp"
           } | grep -o 'LC_FIX([0-9]*)' | sort -u | tr '\n' ' ')
[ "$switches" = "LC_FIX(14) " ] || { echo "unexpected switches: $switches"; status=1; }
exit $status
