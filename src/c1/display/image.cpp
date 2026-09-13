#include "image.hpp"

#include "blit.hpp"
#include "../world/geometry.hpp"

#include <cstring>
#include <new>

namespace creatures1::display {

std::size_t Image::resident_allocation_size(int width, int height) {
    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    return (pixel_count + 1U) & ~std::size_t{1U};
}

bool Image::has_flag(ImageCacheFlag flag) const {
    return (cache_flags_ & static_cast<std::uint8_t>(flag)) != 0;
}

void Image::clear_flag(ImageCacheFlag flag) {
    cache_flags_ &= static_cast<std::uint8_t>(~static_cast<std::uint8_t>(flag));
}

bool Image::is_pixel_data_resident() const {
    return has_flag(ImageCacheFlag::pixel_data_resident);
}

Image::~Image() {
    if (resident_cache_ != nullptr) {
        release_pixel_data(*resident_cache_);
    }
}

void Image::configure(Gallery* gallery,
                      int width,
                      int height,
                      std::uint32_t sprite_data_offset,
                      std::uint8_t cache_flags) {
    gallery_ = gallery;
    width_ = width;
    height_ = height;
    sprite_data_offset_ = sprite_data_offset;
    cache_flags_ = cache_flags;
    lru_stamp_ = 0;
    cache_prev_ = nullptr;
    cache_next_ = nullptr;
    resident_cache_ = nullptr;
    pixel_data_.reset();
}

void Image::release_pixel_data(PixelCacheState& cache) {
    if (!is_pixel_data_resident()) {
        return;
    }

    if (cache_prev_ != nullptr) {
        cache_prev_->cache_next_ = cache_next_;
    } else {
        cache.lru_head = cache_next_;
    }
    if (cache_next_ != nullptr) {
        cache_next_->cache_prev_ = cache_prev_;
    } else {
        cache.lru_tail = cache_prev_;
    }

    clear_flag(ImageCacheFlag::pixel_data_resident);
    cache_prev_ = nullptr;
    cache_next_ = nullptr;
    resident_cache_ = nullptr;
    pixel_data_.reset();
    cache.bytes_used -= static_cast<std::uint32_t>(
        resident_allocation_size(width_, height_));
    --cache.entry_count;
}

std::uint8_t* Image::get_pixel_data(
    PixelCacheState& cache,
    SpriteFileCache& sprite_files,
    const SpriteFileSearchPaths& paths,
    BinaryResourceFileSystem& files) {
    lru_stamp_ = ++cache.access_stamp;
    if (!has_flag(ImageCacheFlag::owns_pixel_data_directly) &&
        !is_pixel_data_resident()) {
        return load_pixel_data(cache, sprite_files, paths, files);
    }

    if (is_pixel_data_resident() && cache.lru_head != this) {
        if (cache_prev_ != nullptr) {
            cache_prev_->cache_next_ = cache_next_;
        } else {
            cache.lru_head = cache_next_;
        }
        if (cache_next_ != nullptr) {
            cache_next_->cache_prev_ = cache_prev_;
        } else {
            cache.lru_tail = cache_prev_;
        }

        cache_prev_ = nullptr;
        cache_next_ = cache.lru_head;
        if (cache.lru_head != nullptr) {
            cache.lru_head->cache_prev_ = this;
        } else {
            cache.lru_tail = this;
        }
        cache.lru_head = this;
    }
    return pixel_data_.get();
}

bool Image::remap_palette_indices(
    const PaletteRemapTable& palette,
    PixelCacheState& cache,
    SpriteFileCache& sprite_files,
    const SpriteFileSearchPaths& paths,
    BinaryResourceFileSystem& files) {
    std::uint8_t* pixels =
        get_pixel_data(cache, sprite_files, paths, files);
    if (pixels == nullptr || width_ < 0 || height_ < 0) {
        return false;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    for (std::size_t index = 0; index < pixel_count; ++index) {
        pixels[index] = palette.remapped_palette_index[pixels[index]];
    }
    return true;
}

void Image::serialize(ImageArchive& archive) {
    if (archive.is_loading()) {
        gallery_ = archive.read_gallery();
        cache_flags_ = archive.read_byte();
        width_ = archive.read_int32();
        height_ = archive.read_int32();
        sprite_data_offset_ = archive.read_uint32();

        cache_flags_ &= static_cast<std::uint8_t>(
            ~static_cast<std::uint8_t>(ImageCacheFlag::pixel_data_resident));
        lru_stamp_ = 0;
        if (has_flag(ImageCacheFlag::owns_pixel_data_directly)) {
            pixel_data_.reset(new std::uint8_t[
                static_cast<std::size_t>(width_) *
                static_cast<std::size_t>(height_)]);
        } else {
            pixel_data_.reset();
        }
        return;
    }

    archive.write_gallery(gallery_);
    archive.write_byte(cache_flags_);
    archive.write_int32(width_);
    archive.write_int32(height_);
    archive.write_uint32(sprite_data_offset_);
}

void Image::blit_to_dib(
    std::uint8_t* dib_pixels,
    int world_x,
    int world_y,
    const world::WorldRect& clip_rect,
    const world::WorldRect& view_rect,
    bool direct_copy,
    PixelCacheState& cache,
    SpriteFileCache& sprite_files,
    const SpriteFileSearchPaths& paths,
    BinaryResourceFileSystem& files) {
    int source_right = width_ - 1;
    int source_bottom = height_ - 1;
    int relative_x = world_x - clip_rect.min_x;
    const int clip_origin_y = clip_rect.min_y;
    int relative_y = world_y - clip_origin_y;

    if (relative_x < (world::kWorldWidth / 2 + 1)) {
        if (relative_x < -(world::kWorldWidth / 2)) {
            relative_x += world::kWorldWidth;
        }
    } else {
        relative_x -= world::kWorldWidth;
    }

    const int image_right = relative_x + source_right;
    const int image_bottom = relative_y + source_bottom;
    const int clip_right = clip_rect.max_x - clip_rect.min_x - 1;
    const int clip_bottom = clip_rect.max_y - clip_origin_y - 1;
    if (relative_x > clip_right || relative_y > clip_bottom ||
        image_right < 0 || image_bottom < 0) {
        return;
    }

    const int destination_x_offset = relative_x >= 0 ? relative_x : 0;
    int source_x = relative_x < 0 ? -relative_x : 0;
    const int destination_y_offset = relative_y >= 0 ? relative_y : 0;
    int source_y = relative_y < 0 ? -relative_y : 0;

    if (clip_right < image_right) {
        source_right += clip_right - image_right;
    }
    if (clip_bottom < image_bottom) {
        source_bottom += clip_bottom - image_bottom;
    }

    const int unwrapped_destination_x =
        clip_rect.min_x + destination_x_offset - view_rect.min_x;
    const int destination_y = destination_y_offset - view_rect.min_y +
                              clip_origin_y;
    const int destination_x = unwrapped_destination_x >= 0
                                  ? unwrapped_destination_x
                                  : unwrapped_destination_x + world::kWorldWidth;

    std::uint8_t* pixels =
        get_pixel_data(cache, sprite_files, paths, files);
    if (pixels == nullptr) {
        return;
    }

    const int row_count = source_bottom - source_y + 1;
    const int source_stride = width_;
    const int destination_stride = view_rect.max_x - view_rect.min_x;
    const std::uint32_t copy_width =
        static_cast<std::uint32_t>(source_right - source_x + 1);
    if (!direct_copy) {
        blit_zero_transparent_pixels(
            {pixels, source_stride}, source_x, source_y,
            {dib_pixels, destination_stride}, destination_x, destination_y,
            copy_width, row_count);
        return;
    }

    std::uint8_t* source_row = pixels + source_y * source_stride + source_x;
    std::uint8_t* destination_row =
        dib_pixels + destination_y * destination_stride + destination_x;
    for (int row = 0; row < row_count; ++row) {
        std::memcpy(destination_row, source_row, copy_width);
        source_row += source_stride;
        destination_row += destination_stride;
    }
}

std::uint8_t* Image::load_pixel_data(
    PixelCacheState& cache,
    SpriteFileCache& sprite_files,
    const SpriteFileSearchPaths& paths,
    BinaryResourceFileSystem& files) {
    if (gallery_ == nullptr || width_ < 0 || height_ < 0) {
        return nullptr;
    }

    BinaryResourceFile* sprite_file =
        sprite_files.acquire(gallery_->sprite_file_id, paths, files);
    if (sprite_file == nullptr ||
        !sprite_file->seek_from_beginning(sprite_data_offset_)) {
        return nullptr;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    const std::size_t allocation_size =
        resident_allocation_size(width_, height_);
    constexpr std::uint32_t kPixelCacheByteLimit = 0x0c800000U;

    for (;;) {
        const std::uint64_t projected_size =
            static_cast<std::uint64_t>(cache.bytes_used) + allocation_size;
        if (projected_size <= kPixelCacheByteLimit) {
            // Value-initialised: resident_allocation_size rounds the pixel
            // count up to an even length, so the trailing byte is never
            // written by the read.
            std::unique_ptr<std::uint8_t[]> pixel_buffer(
                new (std::nothrow) std::uint8_t[allocation_size]());
            if (pixel_buffer != nullptr) {
                if (!sprite_file->read_exact(pixel_buffer.get(), pixel_count)) {
                    // Publishing the allocation regardless is what made a
                    // failed read invisible: the image was marked resident and
                    // then blitted from a buffer nothing had written.  Report
                    // it the way an unavailable image is already reported.
                    return nullptr;
                }
                pixel_data_ = std::move(pixel_buffer);
                cache.bytes_used += static_cast<std::uint32_t>(allocation_size);
                ++cache.entry_count;
                cache_prev_ = nullptr;
                cache_next_ = cache.lru_head;
                if (cache.lru_head != nullptr) {
                    cache.lru_head->cache_prev_ = this;
                } else {
                    cache.lru_tail = this;
                }
                cache.lru_head = this;
                cache_flags_ |= static_cast<std::uint8_t>(
                    ImageCacheFlag::pixel_data_resident);
                resident_cache_ = &cache;
                return pixel_data_.get();
            }
        }

        Image* candidate = cache.lru_tail;
        while (candidate != nullptr &&
               candidate->has_flag(ImageCacheFlag::cache_protected)) {
            candidate = candidate->cache_prev_;
        }
        if (candidate == nullptr) {
            return nullptr;
        }
        candidate->release_pixel_data(cache);
    }
}

} // namespace creatures1::display
