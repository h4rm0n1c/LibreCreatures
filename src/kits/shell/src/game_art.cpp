// The game's art for the kits: .spr, .s32 and @2x.s32 (c1kitshell/game_art.hpp).

#include "c1kitshell/game_art.hpp"

#include "../../../c1/display/s32.hpp"

#include <cstdio>

namespace c1kitshell {
namespace {

namespace display = creatures1::display;

// The whole file; empty when missing or bigger than an S32 may be.
std::vector<std::uint8_t> read_file(const std::string& path) {
    std::vector<std::uint8_t> bytes;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return bytes;
    }
    if (std::fseek(file, 0, SEEK_END) == 0) {
        const long size = std::ftell(file);
        if (size > 0 && static_cast<std::uint64_t>(size) <= display::kS32MaxFileBytes &&
            std::fseek(file, 0, SEEK_SET) == 0) {
            bytes.resize(static_cast<std::size_t>(size));
            if (std::fread(bytes.data(), 1, bytes.size(), file) != bytes.size()) {
                bytes.clear();
            }
        }
    }
    std::fclose(file);
    return bytes;
}

struct Size {
    int width;
    int height;
};

// Every frame of an S32 whose first `sizes.size()` frames are exactly
// `scale` times those sizes (all its frames when `sizes` is empty).
bool decode_file(const std::string& path, const std::vector<Size>& sizes, int scale,
                 std::vector<GameImage>& out) {
    out.clear();
    const std::vector<std::uint8_t> bytes = read_file(path);
    display::S32Index index;
    if (bytes.empty() || !display::parse_s32(bytes.data(), bytes.size(), index)) {
        return false;
    }
    std::size_t count = index.frames.size();
    if (!sizes.empty()) {
        if (count < sizes.size()) {
            return false;
        }
        count = sizes.size();
        for (std::size_t i = 0; i < count; ++i) {
            if (index.frames[i].width != static_cast<std::uint32_t>(sizes[i].width * scale) ||
                index.frames[i].height != static_cast<std::uint32_t>(sizes[i].height * scale)) {
                return false;
            }
        }
    }
    display::RgbaImage decoded;
    for (std::size_t i = 0; i < count; ++i) {
        if (!display::decode_s32_frame(bytes.data(), bytes.size(), index.frames[i], decoded)) {
            out.clear();
            return false;
        }
        GameImage image;
        image.width = decoded.width;
        image.height = decoded.height;
        image.pixels.resize(static_cast<std::size_t>(decoded.width) * decoded.height);
        for (std::size_t p = 0; p < image.pixels.size(); ++p) {
            const std::uint8_t* rgba = decoded.rgba.data() + p * 4;
            image.pixels[p] = static_cast<std::uint32_t>(rgba[3]) << 24 |
                              static_cast<std::uint32_t>(rgba[0]) << 16 |
                              static_cast<std::uint32_t>(rgba[1]) << 8 | rgba[2];
        }
        out.push_back(std::move(image));
    }
    return true;
}

// The first of the directories whose `file` passes decode_file.
bool decode_tier(const std::vector<std::string>& directories, const std::string& file,
                 const std::vector<Size>& sizes, int scale, std::vector<GameImage>& out) {
    for (const std::string& directory : directories) {
        if (decode_file(directory + file, sizes, scale, out)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool load_game_art(const std::vector<std::string>& directories, const std::string& name,
                   const c1kit::GamePalette& palette, bool double_size, GameArt& out) {
    out = {};
    // The game's frames: the first NAME.spr found.
    std::vector<c1kit::GameSprite> sprites;
    bool have_spr = false;
    for (const std::string& directory : directories) {
        if (c1kit::parse_game_sprites(read_file(directory + name + ".spr"), sprites)) {
            have_spr = true;
            break;
        }
    }

    // The 1x frame sizes the tiers are checked against: the .spr's, or with
    // no .spr the .s32's own.
    std::vector<Size> sizes;
    std::vector<GameImage> s32_frames;
    const bool have_s32_alone =
        !have_spr && decode_tier(directories, name + ".s32", {}, 1, s32_frames);
    if (have_spr) {
        for (const c1kit::GameSprite& sprite : sprites) {
            sizes.push_back({sprite.width, sprite.height});
        }
    } else if (have_s32_alone) {
        for (const GameImage& frame : s32_frames) {
            sizes.push_back({frame.width, frame.height});
        }
    } else {
        return false;
    }

    if (double_size && decode_tier(directories, name + "@2x.s32", sizes, 2, out.frames)) {
        out.tier = GameArtTier::s32_2x;
        return true;
    }
    if (have_s32_alone) {
        out.tier = GameArtTier::s32;
        out.frames = std::move(s32_frames);
        return true;
    }
    if (decode_tier(directories, name + ".s32", sizes, 1, out.frames)) {
        out.tier = GameArtTier::s32;
        return true;
    }
    out.tier = GameArtTier::spr;
    for (const c1kit::GameSprite& sprite : sprites) {
        GameImage image;
        image.width = sprite.width;
        image.height = sprite.height;
        image.pixels.reserve(sprite.pixels.size());
        for (const std::uint8_t index : sprite.pixels) {
            const c1kit::PaletteColour& colour = palette[index];
            image.pixels.push_back(index == 0 ? 0u
                                              : 0xff000000u |
                                                    static_cast<std::uint32_t>(colour.red) << 16 |
                                                    static_cast<std::uint32_t>(colour.green) << 8 |
                                                    colour.blue);
        }
        out.frames.push_back(std::move(image));
    }
    return true;
}

} // namespace c1kitshell
