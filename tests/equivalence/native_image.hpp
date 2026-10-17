#pragma once

// Loads a copy of an original Creatures.exe into this process so its
// functions can be called directly and compared with LibreCreatures'
// (function-level equivalence tests).  The exe on disk is only read.
//
// The image goes at its preferred base: CE 10.3 has no relocations, so its
// code must sit at 0x400000.  Under Wine the process heap is already in that
// range when main runs, so the test is itself linked at 0x400000 with an
// empty first section (image_hole.ld) covering 0x401000-0x4a0000, and the
// original's sections are copied into it.  The original's headers are not
// copied; the test's own are at 0x400000.  No
// entry point, CRT start-up or TLS callback runs.  Every import is pointed
// at a trap that names the import and exits, so a function that reaches
// outside the code under test fails loudly instead of running half a game.

#include <cstdint>
#include <string>
#include <vector>

namespace equivalence {

class NativeImage {
public:
    // Maps `path`; false (with `error` set) when it cannot.
    bool load(const std::string& path, std::string& error);

    std::uint32_t base() const { return base_; }

    // A pointer into the mapped image by its virtual address (as Ghidra
    // shows it).
    template <typename T>
    T at(std::uint32_t address) const {
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(address));
    }

    // Points every import whose name ends with `name` (e.g. "!rand") at
    // `function`; false when there is none.  Only the copy changes.
    bool bind_import(const std::string& name, void* function);

    // Makes the cdecl function at `address` return at once (a C3 at its
    // entry), in the copy.
    void make_return(std::uint32_t address);

    // Replaces the function at `address` with a jump to `function`, in the
    // copy.  `function` must take the same arguments in the same places.
    void redirect(std::uint32_t address, const void* function);

private:
    std::uint32_t base_ = 0;
};

} // namespace equivalence
