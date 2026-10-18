#!/bin/sh
# Runs the brain equivalence test against an original Creatures.exe (only
# read) with brains grown from the given genomes, and attributes LC's
# differences to the rows of docs/original-bugs.md.
#
#   tests/equivalence/run_brain.sh <Creatures.exe> <cases> <build-dir> <genome.gen>...
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1; cases=$2; out=$3
shift 3
here=$(cd "$(dirname "$0")" && pwd)
export WINEDEBUG=-all
winexe=$(winepath -w "$exe")
genomes=
for g in "$@"; do genomes="$genomes $(winepath -w "$g")"; done
status=0
wine "$out/brain_equivalence_native.exe" "$winexe" $genomes -cases "$cases" \
    > "$out/brain-native.txt" || status=1
tail -2 "$out/brain-native.txt"
for build in fixed row2 row4 row5 row6; do
    EQUIVALENCE_DIFFS="$(winepath -w "$out/brain-$build.diffs")" \
        wine "$out/brain_equivalence_$build.exe" "$winexe" $genomes -cases "$cases" |
        tail -1 | sed "s/^/$build: /"
done
sort "$out/brain-fixed.diffs" > "$out/brain-fixed.sorted"
sort -u "$out"/brain-row?.diffs > "$out/brain-rows.sorted"
combined=$(comm -23 "$out/brain-fixed.sorted" "$out/brain-rows.sorted" | wc -l)
# The native build matches on every case, so each difference of LC as
# shipped comes from a switch in the code under test.  Check that rows 2,
# 4, 5 and 6 are its only switches; a case no single row explains needs
# several of them together.
switches=$(grep -oh 'LC_FIX([0-9]*)' "$here/../../src/c1/brain/"*.cpp | sort -u | tr '\n' ' ')
echo "LC as shipped: $(wc -l < "$out/brain-fixed.sorted") differing cases; a single row" \
     "explains: row 2 $(wc -l < "$out/brain-row2.diffs"), row 4 $(wc -l < "$out/brain-row4.diffs")," \
     "row 5 $(wc -l < "$out/brain-row5.diffs"), row 6 $(wc -l < "$out/brain-row6.diffs");" \
     "rows together: $combined (switches in the code under test: $switches)"
[ "$switches" = "LC_FIX(2) LC_FIX(4) LC_FIX(5) LC_FIX(6) " ] || { echo "unexpected switches"; status=1; }
exit $status
