#include "gallery.hpp"

#include <stdexcept>

namespace creatures1::display {

Gallery* create_gallery(
    SpriteFileId sprite_file_id,
    std::int32_t header_record_index,
    std::uint32_t image_count,
    bool cache_protection,
    std::string_view secondary_image_directory,
    std::string_view primary_image_directory,
    GalleryHost& host,
    GalleryRegistry& registry) {
    auto gallery = std::make_unique<Gallery>();
    gallery->sprite_file_id = sprite_file_id;
    gallery->header_record_index = header_record_index;
    gallery->image_count = image_count;
    gallery->reference_count = 1;
    gallery->images = std::make_unique<Image[]>(image_count);

    const std::string filename = host.sprite_metadata_path(
        secondary_image_directory, sprite_file_id);
    std::string metadata_path = filename;
    if (!host.regular_file_exists(metadata_path)) {
        metadata_path = host.sprite_metadata_path(primary_image_directory,
                                                  sprite_file_id);
    }

    // The SPR header is a u16 image count followed by eight-byte records.
    // CGallery receives a record index, so the first record starts at 2 and
    // every requested record is read in order from that point.
    const std::size_t record_offset =
        static_cast<std::size_t>(header_record_index) * 8U + 2U;
    const std::vector<GalleryImageMetadata> metadata = host.read_image_metadata(
        metadata_path, record_offset, image_count);
    if (metadata.size() != image_count) {
        throw std::runtime_error("sprite metadata record count mismatch");
    }

    const std::uint8_t cache_flags = cache_protection
        ? static_cast<std::uint8_t>(ImageCacheFlag::cache_protected)
        : 0;
    for (std::size_t index = 0; index < image_count; ++index) {
        const GalleryImageMetadata& record = metadata[index];
        gallery->images[index].configure(
            gallery.get(), static_cast<int>(record.width),
            static_cast<int>(record.height), record.pixel_data_offset,
            cache_flags);
    }

    return registry.add_gallery(std::move(gallery));
}

Gallery* acquire_gallery(SpriteFileId sprite_file_id,
                         std::int32_t header_record_index,
                         std::uint32_t image_count,
                         bool cache_protection,
                         std::string_view secondary_image_directory,
                         std::string_view primary_image_directory,
                         GalleryHost& host,
                         GalleryRegistry& registry) {
    if (!cache_protection) {
        for (std::size_t index = 0; index < registry.gallery_count(); ++index) {
            Gallery* gallery = registry.gallery_at(index);
            if (gallery != nullptr &&
                gallery->sprite_file_id == sprite_file_id &&
                gallery->header_record_index == header_record_index) {
                ++gallery->reference_count;
                return gallery;
            }
        }
    }

    return create_gallery(sprite_file_id, header_record_index, image_count,
                          cache_protection, secondary_image_directory,
                          primary_image_directory, host, registry);
}

void serialize_gallery(Gallery& gallery, ImageArchive& archive) {
    if (archive.is_loading()) {
        gallery.image_count = archive.read_uint32();
        gallery.sprite_file_id = archive.read_uint32();
        gallery.header_record_index = archive.read_int32();
        gallery.reference_count = archive.read_uint32();
        gallery.images = std::make_unique<Image[]>(gallery.image_count);

        for (std::uint32_t index = 0; index < gallery.image_count; ++index) {
            gallery.images[index].serialize(archive);
        }
        return;
    }

    archive.write_uint32(gallery.image_count);
    archive.write_uint32(gallery.sprite_file_id);
    archive.write_int32(gallery.header_record_index);
    archive.write_uint32(gallery.reference_count);
    for (std::uint32_t index = 0; index < gallery.image_count; ++index) {
        gallery.images[index].serialize(archive);
    }
}

} // namespace creatures1::display
