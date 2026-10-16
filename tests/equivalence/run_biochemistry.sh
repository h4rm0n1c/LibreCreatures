#!/bin/sh
# Runs the biochemistry equivalence test against an original Creatures.exe
# (only read) and attributes every difference of LC as shipped to the rows
# of docs/original-bugs.md that explain it.
#
#   tests/equivalence/run_biochemistry.sh <Creatures.exe> [cases] [build-dir]
# WINEPREFIX is used as set; a throwaway prefix is best.
set -e
exe=$1
cases=${2:-200000}
here=$(cd "$(dirname "$0")" && pwd)
out=${3:-$here/build}
[ -n "$exe" ] || { echo "usage: $0 <Creatures.exe> [cases] [build-dir]"; exit 2; }
winexe=$(winepath -w "$exe")
export WINEDEBUG=-all
status=0
wine "$out/biochemistry_equivalence_native.exe" "$winexe" "$cases" || status=1
for build in fixed row50 row51; do
    EQUIVALENCE_DIFFS="$(winepath -w "$out/$build.diffs")" \
        wine "$out/biochemistry_equivalence_$build.exe" "$winexe" "$cases" | tail -1 |
        sed "s/^/$build: /"
done
# Every case LC as shipped differs on must differ with one row alone fixed.
sort "$out/fixed.diffs" > "$out/fixed.sorted"
sort -u "$out/row50.diffs" "$out/row51.diffs" > "$out/rows.sorted"
together=$(comm -23 "$out/fixed.sorted" "$out/rows.sorted" | wc -l)
# The native build matches on every case, so each difference comes from a
# switch in the code under test.  Check that rows 50 and 51 are its only
# switches; a case neither alone explains then needs both.
switches=$(grep -oh 'LC_FIX([0-9]*)' "$here/../../src/c1/biochemistry/biochemistry.cpp" |
           sort -u | tr '\n' ' ')
echo "LC as shipped: $(wc -l < "$out/fixed.sorted") differing cases:" \
     "row 50 alone explains $(wc -l < "$out/row50.diffs"), row 51 alone" \
     "$(wc -l < "$out/row51.diffs"), both together $together" \
     "(switches in the code under test: $switches)"
[ "$switches" = "LC_FIX(50) LC_FIX(51) " ] || { echo "unexpected switches"; status=1; }
exit $status
