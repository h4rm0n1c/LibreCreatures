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
    if (!file.usable || gallery.images == nullptr) {
        return false;
    }
    const int scale = tier_scale(tier);
    const std::size_t first =
        static_cast<std::size_t>(gallery.header_record_index);
    for (std::uint32_t index = 0; index < gallery.image_count; ++index) {
        const std::size_t frame = first + index;
        const Image& image = gallery.images[index];
        if (frame >= file.index.frames.size()) {
            log_.push_back(tier_file_name(gallery.sprite_file_id, tier) +
                           ": has no frame " + std::to_string(frame) +
                           ", ignored");
            return false;
        }
        const S32Frame& s32 = file.index.frames[frame];
        if (s32.width != static_cast<std::uint32_t>(image.width() * scale) ||
            s32.height != static_cast<std::uint32_t>(image.height() * scale)) {
            log_.push_back(tier_file_name(gallery.sprite_file_id, tier) +
                           ": frame " + std::to_string(frame) + " is " +
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

std::uint8_t ImageTierStore::tiers_for(const Gallery& gallery,
                                       const SpriteFileSearchPaths& paths,
                                       TierFileSource& files) {
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
    const std::uint8_t tiers = tiers_for(gallery, paths, files);
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
    if (tier == ImageTier::spr || image_index >= gallery.image_count ||
        (tiers_for(gallery, paths, files) & tier_bit(tier)) == 0) {
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
}

std::vector<std::string> ImageTierStore::take_log() {
    std::vector<std::string> taken;
    taken.swap(log_);
    return taken;
}

} // namespace creatures1::display
