#pragma once

#include <cstddef>

// The switchable fixes (docs/original-bugs.md), as data: one entry per row
// that has an LC_FIX_<row> in compat.hpp. This is the single source the
// LibreFixes dialog, librefixes.ini and compat.cpp's descriptions all read,
// so the three cannot drift apart.
//
// Not here: the ~25 rows docs/original-bugs.md's "Native mode" section
// lists as guards, heap-dependent or a modal box (3, 10, 19, 20, 21, 25,
// 34, 46, 47, 48, 58, 60, 63, 65, 66, 77-82, 84-87). Those have no
// LC_FIX(<row>) call at all -- the fix is simply what the code does -- so
// there is nothing to list, show or toggle for them.

namespace creatures1::common {

struct FixCatalogEntry {
    int row;
    const char* label;     // one line, from compat.hpp's own comment
    const char* category;  // compat.hpp's section grouping
    // Whether the fix changes what the simulation does (brain, genetics,
    // biochemistry, creature behaviour, scripts, world physics) as opposed
    // to UI, kit/IPC or repairing broken world data on load. Decides the
    // row's default in a fresh librefixes.ini: off when true (native
    // behaviour out of the box), on when false.
    bool changes_simulation;
};

// Sorted by row number.
extern const FixCatalogEntry kFixCatalog[];
extern const std::size_t kFixCatalogSize;

// nullptr if `row` is not a catalog entry (a guard row, or not a row at all).
const FixCatalogEntry* find_fix_catalog_entry(int row);

}  // namespace creatures1::common
