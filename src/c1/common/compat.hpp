#pragma once

#include <string>

// Native-compatibility switches for the fixes of original C1 defects.
//
// Each switchable row of docs/original-bugs.md has a macro LC_FIX_<row>:
// 1 runs LibreCreatures' fix, 0 runs the native behaviour.  Code that
// differs reads LC_FIX(<row>) and keeps both behaviours:
//
//     if (LC_FIX(4)) {
//         ... the fix ...
//     } else {
//         ... native ...
//     }
//
// Three builds:
//   default             every LC_FIX_<row> is 1; the unused branches compile
//                       out.  Any row can be built native with -DLC_FIX_<row>=0.
//   -DLC_NATIVE_MODE    every LC_FIX_<row> is 0: the native game, with the
//                       guards below.  A row can be kept fixed with
//                       -DLC_FIX_<row>=1.
//   -DLC_COMPAT_RUNTIME LC_FIX(<row>) reads a live, in-memory table instead
//                       of a compile-time constant.  This is the build every
//                       shipped LibreCreatures exe uses: librefixes.ini sets
//                       each row's starting value (see load_fix_config()
//                       below), the Options > LibreFixes window changes it
//                       live through set_compat_fixed(), and the lab's own
//                       A/B runs still set C1_NATIVE_ROWS ("all" or a comma
//                       list of rows to run native, "-<row>" keeping one
//                       fixed: "all,-4,-5") on top, unchanged from before and
//                       without touching the ini.
//
// A row that is not in this list cannot be switched: LC_FIX(<row>) on it
// does not compile.  Guards against native crashes and memory corruption
// (rows 3, 10, 19, 20, 21, 25, 34, 46, 47, 48, 58, 60, 63, 66) are not switched:
// on any input that does not crash or corrupt the native game they behave as
// native does, and a crash cannot be compared.  Row 25 is listed with them
// because native's result there depends on heap addresses.  Row 65 (the
// native modal box for a script syntax error) is not switched either: LC
// always does what native does when the box's OK is pressed, and a modal
// box would stop every automated run.

#if defined(LC_NATIVE_MODE)
#define LC_FIX_DEFAULT 0
#else
#define LC_FIX_DEFAULT 1
#endif

// Brain, genetics, biochemistry.
#ifndef LC_FIX_1
#define LC_FIX_1 LC_FIX_DEFAULT   // creature cohorts: four, stepped by four
#endif
#ifndef LC_FIX_2
#define LC_FIX_2 LC_FIX_DEFAULT   // rule registers reset per neuron
#endif
#ifndef LC_FIX_4
#define LC_FIX_4 LC_FIX_DEFAULT   // migrating neuron does not match itself
#endif
#ifndef LC_FIX_5
#define LC_FIX_5 LC_FIX_DEFAULT   // migration duplicate test by target sets
#endif
#ifndef LC_FIX_6
#define LC_FIX_6 LC_FIX_DEFAULT   // loose dendrite duplicate test
#endif
#ifndef LC_FIX_7
#define LC_FIX_7 LC_FIX_DEFAULT   // mutation keeps the gext tag
#endif
#ifndef LC_FIX_50
#define LC_FIX_50 LC_FIX_DEFAULT  // reaction product yield not cut to a byte
#endif
#ifndef LC_FIX_51
#define LC_FIX_51 LC_FIX_DEFAULT  // same reactant twice counted once
#endif

// Creatures.
#ifndef LC_FIX_8
#define LC_FIX_8 LC_FIX_DEFAULT   // sense values of 128+ reach the brain
#endif
#ifndef LC_FIX_9
#define LC_FIX_9 LC_FIX_DEFAULT   // parked (killed) objects are forgotten
#endif
#ifndef LC_FIX_11
#define LC_FIX_11 LC_FIX_DEFAULT  // involuntary action with no script
#endif
#ifndef LC_FIX_12
#define LC_FIX_12 LC_FIX_DEFAULT  // gait selection tests the chosen gait
#endif
#ifndef LC_FIX_13
#define LC_FIX_13 LC_FIX_DEFAULT  // empty-handed drop does not disappoint
#endif
#ifndef LC_FIX_14
#define LC_FIX_14 LC_FIX_DEFAULT  // "come Norn", "run Norn"
#endif
#ifndef LC_FIX_15
#define LC_FIX_15 LC_FIX_DEFAULT  // status report pregnancy stage
#endif
#ifndef LC_FIX_16
#define LC_FIX_16 LC_FIX_DEFAULT  // missing body sprite rebuilt
#endif
#ifndef LC_FIX_17
#define LC_FIX_17 LC_FIX_DEFAULT  // stm# tact reaches only the toucher
#endif
#ifndef LC_FIX_37
#define LC_FIX_37 LC_FIX_DEFAULT  // every listener hears every word
#endif
#ifndef LC_FIX_38
#define LC_FIX_38 LC_FIX_DEFAULT  // unknown parents are not siblings
#endif
#ifndef LC_FIX_39
#define LC_FIX_39 LC_FIX_DEFAULT  // no empty heard words
#endif
#ifndef LC_FIX_40
#define LC_FIX_40 LC_FIX_DEFAULT  // a command to category 0 is delivered
#endif
#ifndef LC_FIX_42
#define LC_FIX_42 LC_FIX_DEFAULT  // mutated bacterium output keeps its offset
#endif
#ifndef LC_FIX_43
#define LC_FIX_43 LC_FIX_DEFAULT  // status report names the norn's own room
#endif
#ifndef LC_FIX_44
#define LC_FIX_44 LC_FIX_DEFAULT  // bacterial threshold mutation clamps
#endif
#ifndef LC_FIX_49
#define LC_FIX_49 LC_FIX_DEFAULT  // renamed import keeps its gamete's name
#endif
#ifndef LC_FIX_73
#define LC_FIX_73 LC_FIX_DEFAULT  // perception outside every room
#endif
#ifndef LC_FIX_83
#define LC_FIX_83 LC_FIX_DEFAULT  // body sprite file without two stray bytes
#endif

// Scripts.
#ifndef LC_FIX_18
#define LC_FIX_18 LC_FIX_DEFAULT  // the egg in the sky
#endif
#ifndef LC_FIX_53
#define LC_FIX_53 LC_FIX_DEFAULT  // enum/next survives kill
#endif
#ifndef LC_FIX_54
#define LC_FIX_54 LC_FIX_DEFAULT  // empty enum skips to its own next
#endif
#ifndef LC_FIX_55
#define LC_FIX_55 LC_FIX_DEFAULT  // block scans skip bracketed text
#endif
#ifndef LC_FIX_56
#define LC_FIX_56 LC_FIX_DEFAULT  // gsub label scan skips bracketed text
#endif
#ifndef LC_FIX_61
#define LC_FIX_61 LC_FIX_DEFAULT  // ended script does not cost the next a turn
#endif

// World, objects and vehicles.
#ifndef LC_FIX_22
#define LC_FIX_22 LC_FIX_DEFAULT  // the Black Hole
#endif
#ifndef LC_FIX_23
#define LC_FIX_23 LC_FIX_DEFAULT  // the Flying Lemon
#endif
#ifndef LC_FIX_24
#define LC_FIX_24 LC_FIX_DEFAULT  // lift called to its own floor
#endif
#ifndef LC_FIX_27
#define LC_FIX_27 LC_FIX_DEFAULT  // drop in a room's upper half
#endif
#ifndef LC_FIX_41
#define LC_FIX_41 LC_FIX_DEFAULT  // lift arrival at fractional speeds
#endif
#ifndef LC_FIX_45
#define LC_FIX_45 LC_FIX_DEFAULT  // all eight lift call slots answered
#endif
#ifndef LC_FIX_52
#define LC_FIX_52 LC_FIX_DEFAULT  // circling vehicle wraps its position
#endif
#ifndef LC_FIX_57
#define LC_FIX_57 LC_FIX_DEFAULT  // mvto/mvby/mcrt move a vehicle's position
#endif
#ifndef LC_FIX_59
#define LC_FIX_59 LC_FIX_DEFAULT  // moving vehicle frame codes
#endif

// World data (repaired on load).
#ifndef LC_FIX_26
#define LC_FIX_26 LC_FIX_DEFAULT  // Ocean Dome Sound unseen by creatures
#endif
#ifndef LC_FIX_69
#define LC_FIX_69 LC_FIX_DEFAULT  // Retreat script
#endif
#ifndef LC_FIX_70
#define LC_FIX_70 LC_FIX_DEFAULT  // Say Need scripts
#endif
#ifndef LC_FIX_71
#define LC_FIX_71 LC_FIX_DEFAULT  // the drum
#endif
#ifndef LC_FIX_72
#define LC_FIX_72 LC_FIX_DEFAULT  // the island boat
#endif

// The hand and the interface.
#ifndef LC_FIX_28
#define LC_FIX_28 LC_FIX_DEFAULT  // click target by plane
#endif
#ifndef LC_FIX_29
#define LC_FIX_29 LC_FIX_DEFAULT  // hand moves before the click
#endif
#ifndef LC_FIX_30
#define LC_FIX_30 LC_FIX_DEFAULT  // deleting a creature keeps the selection
#endif
#ifndef LC_FIX_31
#define LC_FIX_31 LC_FIX_DEFAULT  // hand freed when its object goes
#endif
#ifndef LC_FIX_32
#define LC_FIX_32 LC_FIX_DEFAULT  // no empty spoken word
#endif
#ifndef LC_FIX_33
#define LC_FIX_33 LC_FIX_DEFAULT  // save/speed change keep a pause
#endif
#ifndef LC_FIX_67
#define LC_FIX_67 LC_FIX_DEFAULT  // kit toolbar tooltips
#endif
#ifndef LC_FIX_68
#define LC_FIX_68 LC_FIX_DEFAULT  // event bar smooth scroll
#endif

// Kit protocol and sound.
#ifndef LC_FIX_35
#define LC_FIX_35 LC_FIX_DEFAULT  // SFC.OLE brain report runs its script
#endif
#ifndef LC_FIX_36
#define LC_FIX_36 LC_FIX_DEFAULT  // sound cache size
#endif
#ifndef LC_FIX_62
#define LC_FIX_62 LC_FIX_DEFAULT  // looping sounds survive mute
#endif
#ifndef LC_FIX_64
#define LC_FIX_64 LC_FIX_DEFAULT  // dead kit's slot is freed
#endif
#ifndef LC_FIX_74
#define LC_FIX_74 LC_FIX_DEFAULT  // empty macro reply (CE 10.4)
#endif
#ifndef LC_FIX_75
#define LC_FIX_75 LC_FIX_DEFAULT  // kit window raised (CE 10.4)
#endif
#ifndef LC_FIX_76
#define LC_FIX_76 LC_FIX_DEFAULT  // kit pipe waits for a starting kit
#endif

namespace creatures1::common {

// Runtime build: whether `row` runs its fix. The other builds never call
// this; LC_FIX(row) is LC_FIX_##row's compile-time value there instead.
bool compat_fixed(int row);

// The rows running native, for logs: "none", or "all" / a row list.
std::string compat_summary();

// LibreFixes: the player-facing side of this, for a shipped runtime build.
// This header and compat.cpp stay portable (no file or OS access); reading
// and writing librefixes.ini is platform/windows_fix_config_host.cpp's job,
// through these two pure in-memory operations:
//
// set_compat_fixed(row, fixed) updates the live flag for one catalog row
// (fix_catalog.hpp). Every LC_FIX(row) call site re-reads it, so this takes
// effect at once, no restart. A row outside the catalog (a guard row, or
// not a row at all) is ignored. This is what the LibreFixes dialog's OK
// button, and the ini loader, both call for each row they set.
//
// compat_fixed(row) (above) already reads a row back, so the ini writer and
// the dialog can read starting values from it too.
#if defined(LC_COMPAT_RUNTIME)
void set_compat_fixed(int row, bool fixed);
#endif

} // namespace creatures1::common

#if defined(LC_COMPAT_RUNTIME)
// LC_FIX_##row is there so a row not in the list does not compile; a row
// built native with -DLC_FIX_<row>=0 stays native.
#define LC_FIX(row) (LC_FIX_##row && ::creatures1::common::compat_fixed(row))
#else
#define LC_FIX(row) (LC_FIX_##row != 0)
#endif
