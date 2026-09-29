#include "s32_to_spr.hpp"

#include "s32.hpp"

#include <limits>
#include <unordered_map>

namespace creatures1::display {
namespace {

constexpr std::uint8_t kFirstGameColour = 10;

void put_u16(std::vector<std::uint8_t>& out, std::size_t at,
             std::uint16_t value) {
    out[at] = static_cast<std::uint8_t>(value);
    out[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(std::vector<std::uint8_t>& out, std::size_t at,
             std::uint32_t value) {
    for (int byte = 0; byte < 4; ++byte) {
        out[at + static_cast<std::size_t>(byte)] =
            static_cast<std::uint8_t>(value >> (8 * byte));
    }
}

std::uint8_t nearest_game_colour(const PaletteDtaBuffer& colours,
                                 std::uint8_t red, std::uint8_t green,
                                 std::uint8_t blue) {
    std::size_t best = 0;
    int best_distance = std::numeric_limits<int>::max();
    for (std::size_t index = 0; index < colours.colours.size(); ++index) {
        const PaletteColour& colour = colours.colours[index];
        const int dr = static_cast<int>(colour.red) - red;
        const int dg = static_cast<int>(colour.green) - green;
        const int db = static_cast<int>(colour.blue) - blue;
        const int distance = dr * dr + dg * dg + db * db;
        if (distance < best_distance) {
            best_distance = distance;
            best = index;
            if (distance == 0) {
                break;
            }
        }
    }
    return static_cast<std::uint8_t>(kFirstGameColour + best);
}

} // namespace

bool build_spr_from_s32(const std::uint8_t* bytes, std::size_t size,
                        const PaletteDtaBuffer& game_colours,
                        std::vector<std::uint8_t>& spr, std::string& error) {
    spr.clear();
    S32Index index;
    if (!parse_s32(bytes, size, index)) {
        error = "not a readable S32 file";
        return false;
    }
    const std::size_t count = index.frames.size();
    if (count == 0 || count > 0xffff) {
        error = "no frames";
        return false;
    }
    std::uint64_t total_pixels = 0;
    for (const S32Frame& frame : index.frames) {
        total_pixels += static_cast<std::uint64_t>(frame.width) * frame.height;
    }
    if (total_pixels > kSprFromS32MaxPixels) {
        error = "too many pixels to make a .spr from";
        return false;
    }

    // u16 count, then an eight-byte record (u32 offset, u16 width, u16
    // height) per frame, then each frame's pixels, rows top-down.
    const std::size_t table_end = 2 + 8 * count;
    spr.assign(table_end, 0);
    spr.reserve(table_end + static_cast<std::size_t>(total_pixels));
    put_u16(spr, 0, static_cast<std::uint16_t>(count));

    std::unordered_map<std::uint32_t, std::uint8_t> nearest;
    RgbaImage decoded;
    for (std::size_t frame_index = 0; frame_index < count; ++frame_index) {
        if (!decode_s32_frame(bytes, size, index.frames[frame_index],
                              decoded)) {
            error = "frame " + std::to_string(frame_index) +
                    " does not decode";
            spr.clear();
            return false;
        }
        const std::size_t record = 2 + 8 * frame_index;
        put_u32(spr, record, static_cast<std::uint32_t>(spr.size()));
        put_u16(spr, record + 4, static_cast<std::uint16_t>(decoded.width));
        put_u16(spr, record + 6, static_cast<std::uint16_t>(decoded.height));

        const std::size_t pixels =
            static_cast<std::size_t>(decoded.width) * decoded.height;
        for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
            const std::uint8_t* rgba = decoded.rgba.data() + pixel * 4;
            if (rgba[3] < 128) {
                spr.push_back(0);
                continue;
            }
            const std::uint32_t key = static_cast<std::uint32_t>(rgba[0]) << 16 |
                                      static_cast<std::uint32_t>(rgba[1]) << 8 |
                                      rgba[2];
            auto found = nearest.find(key);
            if (found == nearest.end()) {
                found = nearest
                            .emplace(key, nearest_game_colour(
                                              game_colours, rgba[0], rgba[1],
                                              rgba[2]))
                            .first;
            }
            spr.push_back(found->second);
        }
    }
    return true;
}

} // namespace creatures1::display
