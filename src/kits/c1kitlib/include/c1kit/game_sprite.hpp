#pragma once

// The game's own sprite and palette files, as the game reads them
// (display/sprite and display/palette in the LibreCreatures game).
// Header-only and portable.
//
//   Images\*.spr        a u16 frame count, then per frame a u32 offset into
//                       the file and u16 width and height; each frame is
//                       width * height palette indices, top row first, and
//                       index 0 is transparent.
//   Palettes\palette.dta 256 entries of three 6-bit components (red, green,
//                       blue), each widened to 8 bits by shifting left 2.
//
// Kits read these from the player's install rather than carrying copies.

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace c1kit {

struct GameSprite {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;  // palette indices, 0 transparent
};

inline bool parse_game_sprites(const std::vector<std::uint8_t>& bytes,
                               std::vector<GameSprite>& out) {
    out.clear();
    const auto u16 = [&](std::size_t at) {
        return static_cast<unsigned>(bytes[at] | (bytes[at + 1] << 8));
    };
    const auto u32 = [&](std::size_t at) {
        return static_cast<std::size_t>(bytes[at]) | (static_cast<std::size_t>(bytes[at + 1]) << 8) |
               (static_cast<std::size_t>(bytes[at + 2]) << 16) |
               (static_cast<std::size_t>(bytes[at + 3]) << 24);
    };
    if (bytes.size() < 2) {
        return false;
    }
    const unsigned count = u16(0);
    if (bytes.size() < 2 + static_cast<std::size_t>(count) * 8) {
        return false;
    }
    for (unsigned i = 0; i < count; ++i) {
        const std::size_t entry = 2 + static_cast<std::size_t>(i) * 8;
        GameSprite sprite;
        const std::size_t offset = u32(entry);
        sprite.width = static_cast<int>(u16(entry + 4));
        sprite.height = static_cast<int>(u16(entry + 6));
        const std::size_t size = static_cast<std::size_t>(sprite.width) * sprite.height;
        if (offset > bytes.size() || bytes.size() - offset < size) {
            out.clear();
            return false;
        }
        sprite.pixels.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                             bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
        out.push_back(std::move(sprite));
    }
    return true;
}

struct PaletteColour {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

using GamePalette = std::array<PaletteColour, 256>;

inline bool parse_palette_dta(const std::vector<std::uint8_t>& bytes, GamePalette& out) {
    if (bytes.size() < 256 * 3) {
        return false;
    }
    for (std::size_t i = 0; i < 256; ++i) {
        out[i].red = static_cast<std::uint8_t>(bytes[i * 3] << 2);
        out[i].green = static_cast<std::uint8_t>(bytes[i * 3 + 1] << 2);
        out[i].blue = static_cast<std::uint8_t>(bytes[i * 3 + 2] << 2);
    }
    return true;
}

// eggs.spr: six egg designs of eight frames each, in the Hatchery's egg
// order -- three growing, the whole egg, three incubating (the norn showing
// through), and the cracked shell.
constexpr int kEggSpriteFramesPerEgg = 8;
constexpr int kEggSpriteWhole = 3;
constexpr int kEggSpriteIncubating = 5;
constexpr int kEggSpriteCracked = 7;

inline int egg_sprite_frame(int egg, int stage) {
    return egg * kEggSpriteFramesPerEgg + stage;
}

} // namespace c1kit
