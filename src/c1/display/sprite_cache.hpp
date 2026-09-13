#pragma once

#include "font.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace creatures1::display {

using SpriteFileId = std::uint32_t;

struct Gallery;
struct PaletteRemapTable;
struct PixelCacheState;
class SpriteFileCache;
struct SpriteFileSearchPaths;

struct SpriteFileSearchPaths {
    std::string_view secondary_image_directory;
    std::string_view primary_image_directory;
};

// The eight-byte records in a C1 SPR index are deliberately represented as
// fields rather than as a compiler-dependent packed byte array.  The file
// writer below serializes these fields little-endian at the platform edge.
struct SpriteFrameRecord {
    std::uint32_t offset = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
};

static_assert(sizeof(SpriteFrameRecord) == 8,
              "C1 sprite frame records must remain eight bytes");

class SpriteIndexOutputFile {
public:
    virtual ~SpriteIndexOutputFile() = default;
    virtual void write(const std::uint8_t* bytes, std::size_t byte_count) = 0;
    virtual bool seek_from_beginning() = 0;
    virtual void close() = 0;
};

class SpriteIndexOutputFileSystem {
public:
    virtual ~SpriteIndexOutputFileSystem() = default;

    // The platform adapter maps this semantic create/write operation to the
    // native CFile open mode used by the original executable (0x1001).
    virtual std::unique_ptr<SpriteIndexOutputFile> open_for_create(
        std::string_view path) = 0;
};

std::string sprite_index_path(std::string_view directory,
                              SpriteFileId file_id);

// Owns the C1 operation that creates a temporary creature SPR index, appends
// remapped frame pixels, then seeks back to rewrite the final frame count and
// index table.  File handles and the Windows/MFC implementation remain in the
// platform adapter.
class SpriteIndexWriter {
public:
    SpriteIndexWriter(SpriteIndexOutputFile& output,
                      std::uint32_t expected_frame_count);

    bool begin();
    bool append_frame(std::uint16_t width,
                      std::uint16_t height,
                      const std::uint8_t* pixels,
                      std::size_t pixel_byte_count);

    // Appends one already-acquired C1 gallery in image order.  Image remains
    // the owner of lazy pixel loading, palette mutation, and cache/LRU state;
    // this class remains the owner of SPR record order and byte layout.
    bool append_gallery_frames(
        Gallery& gallery,
        const PaletteRemapTable& palette,
        PixelCacheState& cache,
        SpriteFileCache& sprite_files,
        const SpriteFileSearchPaths& paths,
        BinaryResourceFileSystem& files);

    bool finish();

    std::uint32_t frame_count() const { return frame_count_; }
    std::uint32_t pixel_byte_count() const { return pixel_byte_count_; }

private:
    static constexpr std::size_t kRecordSize = 8;

    SpriteIndexOutputFile& output_;
    std::uint32_t expected_frame_count_ = 0;
    std::uint32_t frame_count_ = 0;
    std::uint32_t pixel_byte_count_ = 0;
    std::vector<std::uint8_t> table_bytes_;
    bool started_ = false;
};

// The original program keeps at most 256 open .SPR files.  The cache owns the
// file objects; the MFC CFile implementation is supplied by the platform
// adapter through BinaryResourceFileSystem.
class SpriteFileCache {
public:
    static constexpr std::size_t kCapacity = 256;

    BinaryResourceFile* acquire(SpriteFileId file_id,
                                const SpriteFileSearchPaths& paths,
                                BinaryResourceFileSystem& files);

    void clear();

private:
    struct Entry {
        SpriteFileId file_id = 0;
        std::unique_ptr<BinaryResourceFile> file;
        std::uint32_t lru_stamp = 0;
    };

    struct SpriteFileIdHash {
        std::size_t operator()(SpriteFileId value) const noexcept {
            std::uint32_t hash = 0x811c9dc5U;
            for (unsigned shift = 0; shift < 32; shift += 8) {
                hash ^= (value >> shift) & 0xffU;
                hash *= 0x01000193U;
            }
            return hash;
        }
    };

    std::array<Entry, kCapacity> entries_{};
    std::unordered_map<SpriteFileId, std::size_t, SpriteFileIdHash> index_;
    std::uint32_t access_stamp_ = 0;
};

std::string sprite_file_name(SpriteFileId file_id);

} // namespace creatures1::display
