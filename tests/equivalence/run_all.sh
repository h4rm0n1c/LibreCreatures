#!/bin/sh
# Builds and runs every equivalence test against an original Creatures.exe
# (only read), and the table check, then gives one line for each test and
# exits 1 if any test failed.
#
#   tests/equivalence/run_all.sh <Creatures.exe> <install-dir> [build-dir] [--full]
#                                [--no-build] [--only name,name,...]
#
# <install-dir> is the game folder: its Genetics folder gives the genomes
# (all *.gen and *.GEN), and its Images, Body Data and Palettes folders go,
# as copies, to the skeleton-gene test.  Nothing in it is written.
#
# Case counts: quick by default (about one twentieth of the published first
# runs); --full uses the published counts.  --no-build uses the programs
# already in the build directory.  --only runs the named tests (the names in
# the list below).
#
# WINEPREFIX is used as set; a throwaway prefix is best.  The SHA-256 of the
# exe is taken before the runs and checked after them.
set -e
here=$(cd "$(dirname "$0")" && pwd)
exe=$1; install=$2
[ -n "$exe" ] && [ -n "$install" ] || {
    echo "usage: $0 <Creatures.exe> <install-dir> [build-dir] [--full] [--no-build] [--only names]"
    exit 2
}
shift 2
out=$here/build
full=0; build=1; only=
while [ $# -gt 0 ]; do
    case $1 in
        --full) full=1 ;;
        --no-build) build=0 ;;
        --only) shift; only=",$1," ;;
        *) out=$1 ;;
    esac
    shift
done
mkdir -p "$out"
out=$(cd "$out" && pwd)

before=$(sha256sum "$exe" | cut -d' ' -f1)
genomes=$(find "$install/Genetics" -maxdepth 1 -type f \( -name '*.gen' -o -name '*.GEN' \) | sort)
[ -n "$genomes" ] || { echo "no genomes in $install/Genetics"; exit 2; }

if [ $build = 1 ]; then
    echo "building in $out"
    sh "$here/build.sh" "$out" > "$out/build.log" 2>&1 || { echo "build failed: see $out/build.log"; exit 1; }
fi

# name, full cases, quick cases, kind (g: takes genomes, -: does not).
tests="biochemistry 200000 10000 -
brain 20000 1000 g
stimulus 200000 10000 g
action 200000 10000 g
dream 20000 1000 g
perception 200000 10000 g
attention 200000 10000 g
goal 200000 10000 g
geometry 200000 10000 -
fanout 100000 5000 -
skeleton 200000 10000 -
vehicle 50000 2500 -
simple_object 50000 2500 -
words 50000 2500 -
heard 50000 2500 g
life 20000 1000 -
express 20000 1000 g
biogenes 20000 1000 g
braingenes 5000 250 g
skelgenes 500 50 s
breed 20000 1000 g"

summary=$out/run_all.summary
: > "$summary"
failed=0
run_one() {
    name=$1; cases=$2; kind=$3
    log="$out/run_all.$name.log"
    start=$(date +%s)
    set +e
    case $kind in
        g) sh "$here/run_$name.sh" "$exe" "$cases" "$out" $genomes > "$log" 2>&1 ;;
        s) sh "$here/run_$name.sh" "$exe" "$cases" "$out" "$install" $genomes > "$log" 2>&1 ;;
        *) sh "$here/run_$name.sh" "$exe" "$cases" "$out" > "$log" 2>&1 ;;
    esac
    status=$?
    set -e
    seconds=$(( $(date +%s) - start ))
    if [ $status = 0 ]; then result=PASS; else result=FAIL; failed=1; fi
    printf '%-14s %s  %7s cases  %5ss  %s\n' "$name" "$result" "$cases" "$seconds" \
        "$(grep -v '^case ' "$log" | grep -v '^ ' | tail -1)" | tee -a "$summary"
}

echo "$tests" | while read -r name full_cases quick_cases kind; do
    case $only in ""|*",$name,"*) ;; *) continue ;; esac
    if [ $full = 1 ]; then cases=$full_cases; else cases=$quick_cases; fi
    run_one "$name" "$cases" "$kind"
done
# The while loop runs in a subshell; read the result back from the summary.
grep -q ' FAIL ' "$summary" && failed=1

case $only in ""|*",tables,"*)
    if python3 "$here/check_tables.py" "$exe" > "$out/run_all.tables.log" 2>&1; then result=PASS; else result=FAIL; failed=1; fi
    printf '%-14s %s  %s\n' tables "$result" "$(tail -1 "$out/run_all.tables.log")" | tee -a "$summary"
;;
esac

after=$(sha256sum "$exe" | cut -d' ' -f1)
if [ "$before" != "$after" ]; then
    echo "THE EXE CHANGED during the runs ($before -> $after)"; failed=1
else
    echo "exe unchanged (sha256 $before)"
fi
[ $failed = 0 ] && echo "all passed" || echo "some tests failed: see $out/run_all.<name>.log"
exit $failed
