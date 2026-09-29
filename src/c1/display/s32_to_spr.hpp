#pragma once

#include "palette.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace creatures1::display {

// Not native (neorender): a C1 .spr made from an .s32, for art that ships
// only as .s32.  The game needs an 8-bit .spr behind every gallery -- the
// world file records frames by their place in it, and frame sizes, hit
// tests, pigment remapping, the 8-bit renderer and snapshots all read its
// pixels -- so each frame is matched to the game palette: alpha under 128
// becomes index 0 (transparent), every other pixel the nearest of the
// game's own colours (indices 10..245; `game_colours` holds them from index
// 10 on, as PALETTE.DTA does).  An S32 made from a .spr that uses those
// colours comes back with the same colours.
// The SDL renderer still draws the .s32 itself, in true colour.

// Refused beyond this many pixels in all frames together (64 MB of .spr).
constexpr std::uint64_t kSprFromS32MaxPixels = 64u * 1024u * 1024u;

bool build_spr_from_s32(const std::uint8_t* bytes, std::size_t size,
                        const PaletteDtaBuffer& game_colours,
                        std::vector<std::uint8_t>& spr, std::string& error);

} // namespace creatures1::display
