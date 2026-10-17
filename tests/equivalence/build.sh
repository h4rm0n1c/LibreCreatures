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
# The stimulus sink: creature.cpp and what it reaches.  Symbols it reaches
# but the function under test never calls are trap stubs (trap_stubs.py).
creature="$src/creatures/creature.cpp $src/creatures/attention.cpp $src/objects/object.cpp \
          $src/creatures/skeleton.cpp $src/creatures/registry.cpp $src/creatures/learned_words.cpp \
          $src/world/geometry.cpp $src/creatures/bacterium.cpp $src/brain/instinct.cpp \
          $src/creatures/voice.cpp $src/common/time.cpp $brain $src/biochemistry/biochemistry.cpp"
for mode in fixed native row9; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        row*) define="-DLC_NATIVE_MODE -DLC_FIX_${mode#row}=1 -DEQUIVALENCE_EXPECT=1" ;;
    esac
    exe="$out/stimulus_equivalence_$mode.exe"
    $cxx $flags $define -Wl,--no-demangle -o "$exe" "$here/stimulus_equivalence.cpp" \
        "$here/native_image.cpp" $creature > "$out/stimulus_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/stimulus_$mode.link" "$out/stimulus_traps_$mode.cpp"
    $cxx $flags $define -o "$exe" "$here/stimulus_equivalence.cpp" "$here/native_image.cpp" \
        "$out/stimulus_traps_$mode.cpp" $creature
done
# Action selection: the same closure, rand shared as for the brain.
for mode in fixed native row11; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        row*) define="-DLC_NATIVE_MODE -DLC_FIX_${mode#row}=1 -DEQUIVALENCE_EXPECT=1" ;;
    esac
    exe="$out/action_equivalence_$mode.exe"
    wrap="-Wl,--wrap=rand -Wl,--wrap=srand"
    $cxx $flags $define $wrap -Wl,--no-demangle -o "$exe" "$here/action_equivalence.cpp" \
        "$here/native_image.cpp" $creature > "$out/action_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/action_$mode.link" "$out/action_traps_$mode.cpp"
    $cxx $flags $define $wrap -o "$exe" "$here/action_equivalence.cpp" "$here/native_image.cpp" \
        "$out/action_traps_$mode.cpp" $creature
done
# Dreaming: the creature closure, with the brain and biochemistry builders.
# rows: native with only the brain and biochemistry rows fixed, the only
# switches the dreaming code reaches.
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_2=1 -DLC_FIX_4=1 \
                      -DLC_FIX_5=1 -DLC_FIX_6=1 -DLC_FIX_50=1 -DLC_FIX_51=1" ;;
    esac
    exe="$out/dream_equivalence_$mode.exe"
    wrap="-Wl,--wrap=rand -Wl,--wrap=srand"
    $cxx $flags $define $wrap -Wl,--no-demangle -o "$exe" "$here/dream_equivalence.cpp" \
        "$here/native_image.cpp" $creature > "$out/dream_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/dream_$mode.link" "$out/dream_traps_$mode.cpp"
    $cxx $flags $define $wrap -o "$exe" "$here/dream_equivalence.cpp" "$here/native_image.cpp" \
        "$out/dream_traps_$mode.cpp" $creature
done
# Attention: the creature closure, rand shared.
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_8=1 -DLC_FIX_40=1" ;;
    esac
    exe="$out/attention_equivalence_$mode.exe"
    wrap="-Wl,--wrap=rand -Wl,--wrap=srand"
    $cxx $flags $define $wrap -Wl,--no-demangle -o "$exe" "$here/attention_equivalence.cpp" \
        "$here/native_image.cpp" $creature > "$out/attention_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/attention_$mode.link" "$out/attention_traps_$mode.cpp"
    $cxx $flags $define $wrap -o "$exe" "$here/attention_equivalence.cpp" "$here/native_image.cpp" \
        "$out/attention_traps_$mode.cpp" $creature
done
# Perception: the creature closure, rand shared.
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_9=1 -DLC_FIX_38=1 -DLC_FIX_73=1" ;;
    esac
    exe="$out/perception_equivalence_$mode.exe"
    wrap="-Wl,--wrap=rand -Wl,--wrap=srand"
    $cxx $flags $define $wrap -Wl,--no-demangle -o "$exe" "$here/perception_equivalence.cpp" \
        "$here/native_image.cpp" $creature > "$out/perception_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/perception_$mode.link" "$out/perception_traps_$mode.cpp"
    $cxx $flags $define $wrap -o "$exe" "$here/perception_equivalence.cpp" "$here/native_image.cpp" \
        "$out/perception_traps_$mode.cpp" $creature
done
# Goal direction: the creature closure, rand shared.  The code under test has
# no switch, so there is no row build.
for mode in fixed native; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
    esac
    exe="$out/goal_equivalence_$mode.exe"
    wrap="-Wl,--wrap=rand -Wl,--wrap=srand"
    $cxx $flags $define $wrap -Wl,--no-demangle -o "$exe" "$here/goal_equivalence.cpp" \
        "$here/native_image.cpp" $creature > "$out/goal_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/goal_$mode.link" "$out/goal_traps_$mode.cpp"
    $cxx $flags $define $wrap -o "$exe" "$here/goal_equivalence.cpp" "$here/native_image.cpp" \
        "$out/goal_traps_$mode.cpp" $creature
done
# Map geometry: the room functions and Object::UpdateMovementBounds.
geometry="$src/world/map.cpp $src/platform/mfc_adapters.cpp $src/objects/object.cpp \
          $src/world/geometry.cpp $src/common/compat.cpp"
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_22=1" ;;
    esac
    exe="$out/geometry_equivalence_$mode.exe"
    $cxx $flags $define -Wl,--no-demangle -o "$exe" "$here/geometry_equivalence.cpp" \
        "$here/native_image.cpp" $geometry > "$out/geometry_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/geometry_$mode.link" "$out/geometry_traps_$mode.cpp"
    $cxx $flags $define -o "$exe" "$here/geometry_equivalence.cpp" "$here/native_image.cpp" \
        "$out/geometry_traps_$mode.cpp" $geometry
done
# Creature fan-out: the creature closure with the event producers and the map.
fanout="$src/creatures/events.cpp $src/world/map.cpp $creature"
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_17=1 -DLC_FIX_22=1 -DLC_FIX_73=1" ;;
    esac
    exe="$out/fanout_equivalence_$mode.exe"
    $cxx $flags $define -Wl,--no-demangle -o "$exe" "$here/fanout_equivalence.cpp" \
        "$here/native_image.cpp" $fanout > "$out/fanout_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/fanout_$mode.link" "$out/fanout_traps_$mode.cpp"
    $cxx $flags $define -o "$exe" "$here/fanout_equivalence.cpp" "$here/native_image.cpp" \
        "$out/fanout_traps_$mode.cpp" $fanout
done
# Skeleton: the creature closure with the body, the entity, the image and
# the map.
skeleton="$src/creatures/body.cpp $src/objects/entity.cpp $src/display/image.cpp \
          $src/world/map.cpp $creature"
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_22=1" ;;
    esac
    exe="$out/skeleton_equivalence_$mode.exe"
    $cxx $flags $define -Wl,--no-demangle -o "$exe" "$here/skeleton_equivalence.cpp" \
        "$here/native_image.cpp" $skeleton > "$out/skeleton_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/skeleton_$mode.link" "$out/skeleton_traps_$mode.cpp"
    $cxx $flags $define -o "$exe" "$here/skeleton_equivalence.cpp" "$here/native_image.cpp" \
        "$out/skeleton_traps_$mode.cpp" $skeleton
done
# Vehicles, lifts and call buttons: the skeleton closure with the objects.
vehicle="$src/objects/vehicle.cpp $src/objects/lift.cpp $src/objects/call_button.cpp \
         $src/objects/simple_object.cpp $src/objects/compound_object.cpp $skeleton"
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_23=1 -DLC_FIX_41=1 -DLC_FIX_45=1 -DLC_FIX_52=1" ;;
    esac
    exe="$out/vehicle_equivalence_$mode.exe"
    $cxx $flags $define -Wl,--no-demangle -o "$exe" "$here/vehicle_equivalence.cpp" \
        "$here/native_image.cpp" $vehicle > "$out/vehicle_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/vehicle_$mode.link" "$out/vehicle_traps_$mode.cpp"
    $cxx $flags $define -o "$exe" "$here/vehicle_equivalence.cpp" "$here/native_image.cpp" \
        "$out/vehicle_traps_$mode.cpp" $vehicle
done
# Simple objects: the vehicle closure with the creature fan-out.
simple="$src/creatures/events.cpp $vehicle"
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_22=1 -DLC_FIX_27=1" ;;
    esac
    exe="$out/simple_object_equivalence_$mode.exe"
    $cxx $flags $define -Wl,--no-demangle -o "$exe" "$here/simple_object_equivalence.cpp" \
        "$here/native_image.cpp" $simple > "$out/simple_object_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/simple_object_$mode.link" "$out/simple_object_traps_$mode.cpp"
    $cxx $flags $define -o "$exe" "$here/simple_object_equivalence.cpp" "$here/native_image.cpp" \
        "$out/simple_object_traps_$mode.cpp" $simple
done
# Learned words and phrases: the creature closure, rand shared.
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_14=1" ;;
    esac
    exe="$out/words_equivalence_$mode.exe"
    wrap="-Wl,--wrap=rand -Wl,--wrap=srand"
    $cxx $flags $define $wrap -Wl,--no-demangle -o "$exe" "$here/words_equivalence.cpp" \
        "$here/native_image.cpp" $creature > "$out/words_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/words_$mode.link" "$out/words_traps_$mode.cpp"
    $cxx $flags $define $wrap -o "$exe" "$here/words_equivalence.cpp" "$here/native_image.cpp" \
        "$out/words_traps_$mode.cpp" $creature
done
# Heard words: the creature closure, rand shared.
for mode in fixed native rows; do
    case $mode in
        fixed) define= ;;
        native) define=-DLC_NATIVE_MODE ;;
        rows) define="-DLC_NATIVE_MODE -DEQUIVALENCE_EXPECT=1 -DLC_FIX_9=1 -DLC_FIX_37=1 -DLC_FIX_39=1" ;;
    esac
    exe="$out/heard_equivalence_$mode.exe"
    wrap="-Wl,--wrap=rand -Wl,--wrap=srand"
    $cxx $flags $define $wrap -Wl,--no-demangle -o "$exe" "$here/heard_equivalence.cpp" \
        "$here/native_image.cpp" $creature > "$out/heard_$mode.link" 2>&1 || true
    python3 "$here/trap_stubs.py" "$out/heard_$mode.link" "$out/heard_traps_$mode.cpp"
    $cxx $flags $define $wrap -o "$exe" "$here/heard_equivalence.cpp" "$here/native_image.cpp" \
        "$out/heard_traps_$mode.cpp" $creature
done
echo "built in $out"

