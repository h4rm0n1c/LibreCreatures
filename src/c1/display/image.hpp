#pragma once

#include "palette.hpp"
#include "sprite_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace creatures1::world {
struct WorldRect;
}

namespace creatures1::display {

struct Gallery;
class Image;

enum class ImageCacheFlag : std::uint8_t {
    // Bit values are the serialised on-disk flags.  CImage::LoadPixelData
    // sets bit 0 on a successful load (OR byte ptr [ESI+8], 1) and
    // CImage::Serialize clears bit 0 then tests bit 1 to decide whether the
    // image carries its own allocation (AND ...,0xFE / TEST ...,2), so
    // residency is 0x01 and direct ownership is 0x02.
    pixel_data_resident = 0x01,
    owns_pixel_data_directly = 0x02,
    cache_protected = 0x04,
};

struct PixelCacheState {
    std::uint32_t access_stamp = 0;
    std::uint32_t entry_count = 0;
    std::uint32_t bytes_used = 0;
    Image* lru_head = nullptr;
    Image* lru_tail = nullptr;
};

// The MFC CArchive object protocol is supplied by the archive adapter.  The
// image class owns the record order and cache-state semantics around it.
class ImageArchive {
public:
    virtual ~ImageArchive() = default;
    virtual bool is_loading() const = 0;
    virtual Gallery* read_gallery() = 0;
    virtual void write_gallery(Gallery* gallery) = 0;
    virtual std::uint8_t read_byte() = 0;
    virtual std::int32_t read_int32() = 0;
    virtual std::uint32_t read_uint32() = 0;
    virtual void write_byte(std::uint8_t value) = 0;
    virtual void write_int32(std::int32_t value) = 0;
    virtual void write_uint32(std::uint32_t value) = 0;
};

class Image {
public:
    Image() = default;
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    void release_pixel_data(PixelCacheState& cache);
    std::uint8_t* get_pixel_data(
        PixelCacheState& cache,
        SpriteFileCache& sprite_files,
        const SpriteFileSearchPaths& paths,
        BinaryResourceFileSystem& files);

    // Applies the creature palette remap to the image's resident indexed
    // pixels.  Lazy loading and cache ownership remain Image responsibilities.
    bool remap_palette_indices(
        const PaletteRemapTable& palette,
        PixelCacheState& cache,
        SpriteFileCache& sprite_files,
        const SpriteFileSearchPaths& paths,
        BinaryResourceFileSystem& files);

    void serialize(ImageArchive& archive);

    void blit_to_dib(
        std::uint8_t* dib_pixels,
        int world_x,
        int world_y,
        const world::WorldRect& clip_rect,
        const world::WorldRect& view_rect,
        bool direct_copy,
        PixelCacheState& cache,
        SpriteFileCache& sprite_files,
        const SpriteFileSearchPaths& paths,
        BinaryResourceFileSystem& files);

    void configure(Gallery* gallery,
                   int width,
                   int height,
                   std::uint32_t sprite_data_offset,
                   std::uint8_t cache_flags = 0);

    std::uint8_t* pixel_data() const { return pixel_data_.get(); }
    int width() const { return width_; }
    int height() const { return height_; }
    bool is_pixel_data_resident() const;

private:
    friend struct PixelCacheState;

    static std::size_t resident_allocation_size(int width, int height);
    bool has_flag(ImageCacheFlag flag) const;
    void clear_flag(ImageCacheFlag flag);
    std::uint8_t* load_pixel_data(
        PixelCacheState& cache,
        SpriteFileCache& sprite_files,
        const SpriteFileSearchPaths& paths,
        BinaryResourceFileSystem& files);

    Gallery* gallery_ = nullptr;
    std::uint8_t cache_flags_ = 0;
    std::unique_ptr<std::uint8_t[]> pixel_data_;
    int width_ = 0;
    int height_ = 0;
    std::uint32_t sprite_data_offset_ = 0;
    std::uint32_t lru_stamp_ = 0;
    Image* cache_prev_ = nullptr;
    Image* cache_next_ = nullptr;
    PixelCacheState* resident_cache_ = nullptr;
};

// A gallery owns the fixed Image array described by the C1 sprite metadata
// file.  The native implementation used an MFC allocation header and a
// CPtrArray registry; those implementation details are represented by the
// owning unique_ptr and the GalleryHost boundary in gallery.hpp.
struct Gallery {
    Gallery() = default;
    ~Gallery() = default;

    Gallery(const Gallery&) = delete;
    Gallery& operator=(const Gallery&) = delete;
    Gallery(Gallery&&) = delete;
    Gallery& operator=(Gallery&&) = delete;

    SpriteFileId sprite_file_id = 0;
    std::int32_t header_record_index = 0;
    std::uint32_t image_count = 0;
    std::uint32_t reference_count = 0;
    std::unique_ptr<Image[]> images;
};

} // namespace creatures1::display
