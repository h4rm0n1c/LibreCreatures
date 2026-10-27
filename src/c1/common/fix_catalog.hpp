#pragma once

#include <cstddef>

// The switchable fixes (docs/original-bugs.md), as data: one entry per row
// that has an LC_FIX_<row> in compat.hpp. This is the single source the
// LibreFixes dialog, librefixes.ini and compat.cpp's descriptions all read,
// so the three cannot drift apart.
//
// Not here: the ~25 rows docs/original-bugs.md's "Native mode" section
// lists as guards, heap-dependent or a modal box (3, 10, 19, 20, 21, 25,
// 34, 46, 47, 48, 58, 60, 63, 65, 66, 77-82, 84-87, 89). Those have no
// LC_FIX(<row>) call at all -- the fix is simply what the code does -- so
// there is nothing to list, show or toggle for them.

namespace creatures1::common {

// Whether a row is a real player-facing choice, and if so what it starts
// as. `locked` rows have no credible case for running native in play (a
// crash guard, permanent data loss, corrupted output, or an interpreter
// contract violation) -- LibreFixes shows them checked and disabled rather
// than hiding them, so the list stays a complete, honest account of every
// switchable row. `C1_NATIVE_ROWS`/the native build can still force one off
// for A/B comparison; that is a developer path, not this dialog's.
enum class FixTier {
    locked,       // always on; shown, checked, not clickable
    default_off,  // a real trade-off; native out of the box
    default_on,   // a good idea by default; still a player's call
};

struct FixCatalogEntry {
    int row;
    const char* label;     // one line, from compat.hpp's own comment
    const char* category;  // compat.hpp's section grouping
    FixTier tier;
};

// Whether `tier`'s row starts fixed (on) in a fresh librefixes.ini, or after
// Reset to recommended.
inline bool fix_tier_default_fixed(FixTier tier) {
    return tier != FixTier::default_off;
}

// Sorted by row number.
extern const FixCatalogEntry kFixCatalog[];
extern const std::size_t kFixCatalogSize;

// nullptr if `row` is not a catalog entry (a guard row, or not a row at all).
const FixCatalogEntry* find_fix_catalog_entry(int row);

}  // namespace creatures1::common
