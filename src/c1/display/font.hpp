#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace creatures1::display {

class BinaryResourceFile {
public:
    virtual ~BinaryResourceFile() = default;
    virtual bool seek_from_beginning(std::uint32_t byte_offset) = 0;
    // Returns false when the file could not supply every requested byte,
    // matching PaletteDtaFile::read_exact and ResourceReadFile::read_exact.
    // A void return here let a failed sprite read reach the renderer as an
    // unwritten buffer with nothing to test.
    virtual bool read_exact(std::uint8_t* destination,
                            std::size_t byte_count) = 0;
};

class BinaryResourceFileSystem {
public:
    virtual ~BinaryResourceFileSystem() = default;
    // A missing path is reported by returning nullptr.  Concrete adapters may
    // use MFC CFile internally, but that ownership stays outside C1 logic.
    virtual std::unique_ptr<BinaryResourceFile> open_for_read(
        std::string_view path) = 0;
};

struct ImageCacheState {
    std::uint32_t access_stamp = 0;
    std::uint32_t entry_count = 0;
    std::uint32_t bytes_used = 0;
    void* lru_head = nullptr;
    void* lru_tail = nullptr;
    std::uint32_t sprite_file_access_stamp = 0;

    void reset_for_charset_load();
};

struct CharsetData {
    std::array<std::uint8_t, 0x2400> glyph_raster_data{};
    std::array<std::uint16_t, 128> glyph_advance_widths{};
};

void load_charset_data(CharsetData& charset,
                       ImageCacheState& image_cache,
                       BinaryResourceFileSystem& files,
                       std::string_view image_directory);

} // namespace creatures1::display
