#!/bin/sh
# Builds the equivalence tests as 32-bit Windows programs (MinGW), to run
# under Wine beside an original Creatures.exe.  Two of each: LC as shipped
# (fixes on) and LC built native (LC_NATIVE_MODE).  The test is linked at
# the original's base with image_hole.ld, which leaves room for the
# original's sections (see native_image.hpp).
#
#   tests/equivalence/build.sh [out-dir]
set -e
here=$(cd "$(dirname "$0")" && pwd)
src=$here/../../src/c1
out=${1:-$here/build}
mkdir -p "$out"
cxx=i686-w64-mingw32-g++
flags="-std=c++17 -O2 -Wall -I$src -static -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase \
    -Wl,-T,$here/image_hole.ld"
lc="$src/biochemistry/biochemistry.cpp $src/common/compat.cpp \
    $src/common/static_data.cpp $src/common/logging.cpp $src/creatures/genome.cpp"
# fixed: LC as shipped; native: every fix off; row50/row51: native except
# that one row, to attribute the fixed build's differences.
for mode in fixed native row50 row51; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        row*) define="-DLC_NATIVE_MODE -DLC_FIX_${mode#row}=1 -DEQUIVALENCE_EXPECT=1" ;;
    esac
    $cxx $flags $define -o "$out/biochemistry_equivalence_$mode.exe" \
        "$here/biochemistry_equivalence.cpp" "$here/native_image.cpp" \
        "$here/recorder_stubs.cpp" $lc
done
# The brain: native, as shipped, and native with one brain row fixed each.
brain="$src/brain/brain.cpp $src/brain/lobe.cpp $src/brain/neuron.cpp \
       $src/brain/rules.cpp $src/brain/brain_recorder.cpp $src/common/lab_trace.cpp \
       $src/common/compat.cpp $src/common/static_data.cpp $src/common/logging.cpp \
       $src/creatures/genome.cpp"
# rowN: native with row N fixed, to attribute the fixed build's differences.
for mode in fixed native row2 row4 row5 row6; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        row*) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1"
              for row in $(echo "${mode#row}" | tr _ ' '); do
                  define="$define -DLC_FIX_$row=1"
              done ;;
    esac
    $cxx $flags $define -Wl,--wrap=rand -Wl,--wrap=srand \
        -o "$out/brain_equivalence_$mode.exe" \
        "$here/brain_equivalence.cpp" "$here/native_image.cpp" $brain
done
echo "built in $out"
