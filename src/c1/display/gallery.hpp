#pragma once

#include "image.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <vector>

namespace creatures1::display {

struct GalleryImageMetadata {
    std::uint32_t pixel_data_offset = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
};

// File attributes, path fallback, CFile reads, image-array construction, and
// the native gallery registry are platform/owner services.  C1 owns the
// metadata interpretation and image/cache policy around them.
class GalleryHost {
public:
    virtual ~GalleryHost() = default;

    virtual std::string sprite_metadata_path(std::string_view directory,
                                             SpriteFileId file_id) const = 0;
    virtual bool regular_file_exists(std::string_view path) const = 0;
    virtual std::vector<GalleryImageMetadata> read_image_metadata(
        std::string_view path,
        std::size_t file_offset,
        std::size_t image_count) = 0;
};

class GalleryRegistry {
public:
    virtual ~GalleryRegistry() = default;

    virtual std::size_t gallery_count() const = 0;
    virtual Gallery* gallery_at(std::size_t index) const = 0;
    virtual Gallery* add_gallery(std::unique_ptr<Gallery> gallery) = 0;
};

Gallery* create_gallery(
    SpriteFileId sprite_file_id,
    std::int32_t header_record_index,
    std::uint32_t image_count,
    bool cache_protection,
    std::string_view secondary_image_directory,
    std::string_view primary_image_directory,
    GalleryHost& host,
    GalleryRegistry& registry);

Gallery* acquire_gallery(SpriteFileId sprite_file_id,
                         std::int32_t header_record_index,
                         std::uint32_t image_count,
                         bool cache_protection,
                         std::string_view secondary_image_directory,
                         std::string_view primary_image_directory,
                         GalleryHost& host,
                         GalleryRegistry& registry);

void serialize_gallery(Gallery& gallery, ImageArchive& archive);

} // namespace creatures1::display
