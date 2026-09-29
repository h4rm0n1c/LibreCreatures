#pragma once

// The game's own art for the kits, read the way the game reads it: NAME.spr
// through the game palette, or NAME.s32 in true colour, or NAME@2x.s32 at
// double resolution (see docs/neorender.md in the game):
//
// - Each file is looked for in the image directories in turn, as the game
//   looks in the world's Images and then the installation's.
// - NAME.s32 stands in for NAME.spr when it has a frame at exactly each of
//   the .spr's frame sizes, or on its own when there is no .spr anywhere.
// - NAME@2x.s32, when asked for, needs each frame at exactly twice that.
// - A tier that fails any check is passed over for the next one down.
//
// Portable (no MFC); the kit shell turns the frames into bitmaps.

#include "c1kit/game_sprite.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace c1kitshell {

// One frame: 0xAARRGGBB, top row first.
struct GameImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

enum class GameArtTier { spr, s32, s32_2x };

struct GameArt {
    GameArtTier tier = GameArtTier::spr;
    // At the frames' 1x size, or twice it for s32_2x.
    std::vector<GameImage> frames;
    int scale() const { return tier == GameArtTier::s32_2x ? 2 : 1; }
};

// `directories` end in a separator, first choice first; `name` has no
// extension ("eggs").  With `double_size`, NAME@2x.s32 is tried first.
// False when there is nothing usable at all.
bool load_game_art(const std::vector<std::string>& directories, const std::string& name,
                   const c1kit::GamePalette& palette, bool double_size, GameArt& out);

} // namespace c1kitshell
