#pragma once

#include "image.hpp"
#include "s32.hpp"
#include "sprite_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace creatures1::display {

// Not native (neorender).  The ways a gallery's frames can be drawn, lowest
// to highest: the C1 .spr itself, NAME.s32 at the same size, NAME@2x.s32 at
// twice the size.  Game logic only ever sees the .spr's 1x frames; a tier is
// a drawing choice.
enum class ImageTier : std::uint8_t { spr = 0, s32 = 1, s32_2x = 2 };

constexpr int tier_scale(ImageTier tier) {
    return tier == ImageTier::s32_2x ? 2 : 1;
}

class TierFileSource {
public:
    virtual ~TierFileSource() = default;
    // The whole file, or false when there is no such file.
    virtual bool read_whole_file(std::string_view path,
                                 std::vector<std::uint8_t>& out) = 0;
};

// "Back.spr" -> "Back.s32" / "Back@2x.s32".
std::string tier_file_name(SpriteFileId file_id, ImageTier tier);

class ImageTierStore {
public:
    // Bit n set: tier n can draw every frame of this gallery.  The .spr bit
    // is always set.  A tier counts only if the file parses and has a frame
    // for each of the gallery's images at exactly its 1x size (or twice it
    // for @2x); anything else is ignored and logged once.
    std::uint8_t tiers_for(const Gallery& gallery,
                           const SpriteFileSearchPaths& paths,
                           TierFileSource& files);

    // The highest tier at or below `cap` this image can draw with.  An
    // image the game has drawn into while running is always .spr.
    ImageTier best_tier(const Gallery& gallery, std::size_t image_index,
                        ImageTier cap, const SpriteFileSearchPaths& paths,
                        TierFileSource& files);

    // One frame of an S32 tier as RGBA.  False for the .spr tier, a tier the
    // gallery doesn't have, or a frame that fails to decode.
    bool decode(const Gallery& gallery, std::size_t image_index,
                ImageTier tier, const SpriteFileSearchPaths& paths,
                TierFileSource& files, RgbaImage& out);

    // Forget everything read (a world change, or files replaced on disk).
    void clear();

    // Messages about tiers that were found but refused; each once.
    std::vector<std::string> take_log();

private:
    struct TierFile {
        bool loaded = false;
        bool usable = false;
        std::vector<std::uint8_t> bytes;
        S32Index index;
    };
    struct FileTiers {
        TierFile s32;
        TierFile s32_2x;
    };
    using GalleryKey = std::tuple<SpriteFileId, std::int32_t, std::uint32_t>;

    TierFile& tier_file(SpriteFileId file_id, ImageTier tier,
                        const SpriteFileSearchPaths& paths,
                        TierFileSource& files);
    bool tier_matches(const Gallery& gallery, ImageTier tier,
                      const TierFile& file);

    std::map<SpriteFileId, FileTiers> files_;
    std::map<GalleryKey, std::uint8_t> galleries_;
    std::vector<std::string> log_;
};

} // namespace creatures1::display
