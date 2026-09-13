#include "font.hpp"

namespace creatures1::display {

void ImageCacheState::reset_for_charset_load() {
    sprite_file_access_stamp = 0;
    access_stamp = 0;
    entry_count = 0;
    bytes_used = 0;
    lru_head = nullptr;
    lru_tail = nullptr;
}

void load_charset_data(CharsetData& charset,
                       ImageCacheState& image_cache,
                       BinaryResourceFileSystem& files,
                       std::string_view image_directory) {
    image_cache.reset_for_charset_load();

    std::string path(image_directory);
    path += "CHARSET.DTA";
    // open_for_read reports a missing path by returning nullptr, so calling
    // through the result unchecked faults on the null vtable rather than
    // leaving the charset at its zeroed default.
    std::unique_ptr<BinaryResourceFile> file = files.open_for_read(path);
    if (file == nullptr) {
        return;
    }
    if (!file->read_exact(charset.glyph_raster_data.data(),
                          charset.glyph_raster_data.size())) {
        return;
    }
    if (!file->read_exact(
            reinterpret_cast<std::uint8_t*>(charset.glyph_advance_widths.data()),
            charset.glyph_advance_widths.size() * sizeof(std::uint16_t))) {
        return;
    }
}

} // namespace creatures1::display
