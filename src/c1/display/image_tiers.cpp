#include "image_tiers.hpp"

namespace creatures1::display {
namespace {

constexpr std::uint8_t tier_bit(ImageTier tier) {
    return static_cast<std::uint8_t>(1u << static_cast<unsigned>(tier));
}

std::string join_path(std::string_view directory, const std::string& name) {
    std::string path(directory);
    if (!path.empty() && path.back() != '\\' && path.back() != '/') {
        path.push_back('\\');
    }
    return path + name;
}

} // namespace

std::string tier_file_name(SpriteFileId file_id, ImageTier tier) {
    std::string stem = sprite_file_name(file_id);  // "NAME.spr"
    stem.resize(stem.size() - 4);
    switch (tier) {
    case ImageTier::spr:
        return stem + ".spr";
    case ImageTier::s32:
        return stem + ".s32";
    case ImageTier::s32_2x:
        return stem + "@2x.s32";
    }
    return stem;
}

ImageTierStore::TierFile& ImageTierStore::tier_file(
    SpriteFileId file_id, ImageTier tier, const SpriteFileSearchPaths& paths,
    TierFileSource& files) {
    FileTiers& tiers = files_[file_id];
    TierFile& file = tier == ImageTier::s32 ? tiers.s32 : tiers.s32_2x;
    if (file.loaded) {
        return file;
    }
    file.loaded = true;
    const std::string name = tier_file_name(file_id, tier);
    // The same places a .spr is looked for: the world's images, then the
    // install's.
    for (std::string_view directory :
         {std::string_view(paths.secondary_image_directory),
          std::string_view(paths.primary_image_directory)}) {
        if (directory.empty() ||
            !files.read_whole_file(join_path(directory, name), file.bytes)) {
            continue;
        }
        if (parse_s32(file.bytes.data(), file.bytes.size(), file.index)) {
            file.usable = true;
        } else {
            log_.push_back(join_path(directory, name) +
                           ": not a valid S32 file, ignored");
            file.bytes.clear();
        }
        break;
    }
    return file;
}

bool ImageTierStore::tier_matches(const Gallery& gallery, ImageTier tier,
                                  const TierFile& file) {
    return frames_match(gallery, 0, gallery.image_count,
                        static_cast<std::size_t>(gallery.header_record_index),
                        gallery.sprite_file_id, tier, file);
}

bool ImageTierStore::frames_match(const Gallery& gallery,
                                  std::size_t first_image,
                                  std::size_t image_count,
                                  std::size_t first_frame,
                                  SpriteFileId file_id, ImageTier tier,
                                  const TierFile& file) {
    if (!file.usable || gallery.images == nullptr ||
        first_image + image_count > gallery.image_count) {
        return false;
    }
    const int scale = tier_scale(tier);
    for (std::size_t index = 0; index < image_count; ++index) {
        const std::size_t frame = first_frame + index;
        const Image& image = gallery.images[first_image + index];
        if (frame >= file.index.frames.size()) {
            log_.push_back(tier_file_name(file_id, tier) + ": has no frame " +
                           std::to_string(frame) + ", ignored");
            return false;
        }
        const S32Frame& s32 = file.index.frames[frame];
        if (s32.width != static_cast<std::uint32_t>(image.width() * scale) ||
            s32.height != static_cast<std::uint32_t>(image.height() * scale)) {
            log_.push_back(tier_file_name(file_id, tier) + ": frame " +
                           std::to_string(frame) + " is " +
                           std::to_string(s32.width) + "x" +
                           std::to_string(s32.height) + ", expected " +
                           std::to_string(image.width() * scale) + "x" +
                           std::to_string(image.height() * scale) +
                           ", ignored");
            return false;
        }
    }
    return true;
}

void ImageTierStore::set_derived_gallery(SpriteFileId file_id,
                                         DerivedGallery derived) {
    DerivedState& state = derived_[file_id];
    state.part_tiers.assign(derived.parts.size(), {0, 0});
    state.gallery = std::move(derived);
}

const DerivedGalleryPart* ImageTierStore::derived_part_for(
    const DerivedState& state, std::size_t image_index,
    std::size_t& part) const {
    for (part = 0; part < state.gallery.parts.size(); ++part) {
        const DerivedGalleryPart& candidate = state.gallery.parts[part];
        if (image_index >= candidate.first_image &&
            image_index - candidate.first_image < candidate.image_count) {
            return &candidate;
        }
    }
    return nullptr;
}

std::uint8_t ImageTierStore::derived_part_tiers(const Gallery& gallery,
                                                DerivedState& state,
                                                std::size_t part,
                                                TierFileSource& files) {
    // Each part stands alone: a part whose source has no usable tier file is
    // drawn from the .spr while the others use their art.
    const DerivedGalleryPart& source = state.gallery.parts[part];
    const SpriteFileSearchPaths paths(source.directory, source.directory);
    std::uint8_t tiers = tier_bit(ImageTier::spr);
    for (ImageTier tier : {ImageTier::s32, ImageTier::s32_2x}) {
        std::uint8_t& known =
            state.part_tiers[part][static_cast<std::size_t>(tier) - 1];
        if (known == 0) {
            known = frames_match(gallery, source.first_image,
                                 source.image_count, 0, source.source_file_id,
                                 tier,
                                 tier_file(source.source_file_id, tier, paths,
                                           files))
                        ? 1
                        : 2;
        }
        if (known == 1) {
            tiers |= tier_bit(tier);
        }
    }
    return tiers;
}

std::uint8_t ImageTierStore::tiers_for(const Gallery& gallery,
                                       const SpriteFileSearchPaths& paths,
                                       TierFileSource& files) {
    const auto derived = derived_.find(gallery.sprite_file_id);
    if (derived != derived_.end()) {
        // A tier any part can draw with.
        std::uint8_t tiers = tier_bit(ImageTier::spr);
        for (std::size_t part = 0; part < derived->second.gallery.parts.size();
             ++part) {
            tiers |= derived_part_tiers(gallery, derived->second, part, files);
        }
        return tiers;
    }
    const GalleryKey key{gallery.sprite_file_id, gallery.header_record_index,
                         gallery.image_count};
    const auto found = galleries_.find(key);
    if (found != galleries_.end()) {
        return found->second;
    }
    std::uint8_t tiers = tier_bit(ImageTier::spr);
    for (ImageTier tier : {ImageTier::s32, ImageTier::s32_2x}) {
        if (tier_matches(gallery, tier,
                         tier_file(gallery.sprite_file_id, tier, paths,
                                   files))) {
            tiers |= tier_bit(tier);
        }
    }
    galleries_[key] = tiers;
    return tiers;
}

ImageTier ImageTierStore::best_tier(const Gallery& gallery,
                                    std::size_t image_index, ImageTier cap,
                                    const SpriteFileSearchPaths& paths,
                                    TierFileSource& files) {
    if (gallery.images == nullptr || image_index >= gallery.image_count ||
        gallery.images[image_index].runtime_drawn()) {
        return ImageTier::spr;
    }
    std::uint8_t tiers = 0;
    const auto derived = derived_.find(gallery.sprite_file_id);
    if (derived != derived_.end()) {
        std::size_t part = 0;
        if (derived_part_for(derived->second, image_index, part) == nullptr) {
            return ImageTier::spr;
        }
        tiers = derived_part_tiers(gallery, derived->second, part, files);
    } else {
        tiers = tiers_for(gallery, paths, files);
    }
    for (int tier = static_cast<int>(cap); tier > 0; --tier) {
        if ((tiers & tier_bit(static_cast<ImageTier>(tier))) != 0) {
            return static_cast<ImageTier>(tier);
        }
    }
    return ImageTier::spr;
}

bool ImageTierStore::decode(const Gallery& gallery, std::size_t image_index,
                            ImageTier tier, const SpriteFileSearchPaths& paths,
                            TierFileSource& files, RgbaImage& out) {
    out = {};
    if (tier == ImageTier::spr || image_index >= gallery.image_count) {
        return false;
    }
    const auto derived = derived_.find(gallery.sprite_file_id);
    if (derived != derived_.end()) {
        std::size_t part = 0;
        const DerivedGalleryPart* source =
            derived_part_for(derived->second, image_index, part);
        if (source == nullptr ||
            (derived_part_tiers(gallery, derived->second, part, files) &
             tier_bit(tier)) == 0) {
            return false;
        }
        const SpriteFileSearchPaths source_paths(source->directory,
                                                 source->directory);
        TierFile& file =
            tier_file(source->source_file_id, tier, source_paths, files);
        const std::size_t frame = image_index - source->first_image;
        if (frame >= file.index.frames.size() ||
            !decode_s32_frame(file.bytes.data(), file.bytes.size(),
                              file.index.frames[frame], out)) {
            out = {};
            return false;
        }
        apply_creature_colour_transform(
            derived->second.gallery.colour, out.rgba.data(),
            static_cast<std::size_t>(out.width) * out.height);
        return true;
    }
    if ((tiers_for(gallery, paths, files) & tier_bit(tier)) == 0) {
        return false;
    }
    TierFile& file = tier_file(gallery.sprite_file_id, tier, paths, files);
    const std::size_t frame =
        static_cast<std::size_t>(gallery.header_record_index) + image_index;
    if (frame >= file.index.frames.size()) {
        return false;
    }
    return decode_s32_frame(file.bytes.data(), file.bytes.size(),
                            file.index.frames[frame], out);
}

void ImageTierStore::clear() {
    files_.clear();
    galleries_.clear();
    derived_.clear();
}

std::vector<std::string> ImageTierStore::take_log() {
    std::vector<std::string> taken;
    taken.swap(log_);
    return taken;
}

} // namespace creatures1::display
