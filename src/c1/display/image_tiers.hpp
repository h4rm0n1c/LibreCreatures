#pragma once

#include "image.hpp"
#include "palette.hpp"
#include "s32.hpp"
#include "sprite_cache.hpp"

#include <array>
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

// Not native (neorender).  A gallery whose frames were copied out of other
// sprite files: a creature's generated body file is its body-part galleries
// (a006.spr, b016.spr, ...) one after another, recoloured by its genome.  Its
// S32 tiers come from those sources' own tier files, recoloured the same way.
struct DerivedGalleryPart {
    SpriteFileId source_file_id = 0;
    // Where the source .spr was found; its tier files are looked for there.
    std::string directory;
    // This part's frames are [first_image, first_image + image_count) of the
    // derived gallery and frames 0.. of the source file.
    std::uint32_t first_image = 0;
    std::uint32_t image_count = 0;
};

struct DerivedGallery {
    std::vector<DerivedGalleryPart> parts;
    CreatureColourTransform colour;
};

class ImageTierStore {
public:
    // Draw `file_id`'s tiers from these sources instead of from its own tier
    // files.  Replaces anything known about that file; a creature calls it
    // each time its body file is written or found up to date.
    void set_derived_gallery(SpriteFileId file_id, DerivedGallery derived);

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

    // Forget everything read and every derived gallery (a world change, or
    // files replaced on disk).
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

    struct DerivedState {
        DerivedGallery gallery;
        // Per part, per S32 tier: 0 not checked yet, 1 usable, 2 refused.
        std::vector<std::array<std::uint8_t, 2>> part_tiers;
    };

    std::uint8_t derived_part_tiers(const Gallery& gallery,
                                    DerivedState& state, std::size_t part,
                                    TierFileSource& files);
    const DerivedGalleryPart* derived_part_for(const DerivedState& state,
                                               std::size_t image_index,
                                               std::size_t& part) const;

    TierFile& tier_file(SpriteFileId file_id, ImageTier tier,
                        const SpriteFileSearchPaths& paths,
                        TierFileSource& files);
    bool tier_matches(const Gallery& gallery, ImageTier tier,
                      const TierFile& file);
    // Images [first_image, +image_count) of `gallery` against frames
    // first_frame.. of a tier file of `file_id`.
    bool frames_match(const Gallery& gallery, std::size_t first_image,
                      std::size_t image_count, std::size_t first_frame,
                      SpriteFileId file_id, ImageTier tier,
                      const TierFile& file);

    std::map<SpriteFileId, FileTiers> files_;
    std::map<GalleryKey, std::uint8_t> galleries_;
    std::map<SpriteFileId, DerivedState> derived_;
    std::vector<std::string> log_;
};

} // namespace creatures1::display
