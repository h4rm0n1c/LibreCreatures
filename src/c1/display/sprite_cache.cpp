#include "sprite_cache.hpp"

#include "image.hpp"

#include <cstring>
#include <limits>
#include <string>

namespace creatures1::display {

namespace {

std::string path_with_sprite_file(std::string_view directory,
                                  SpriteFileId file_id) {
    std::string path(directory);
    path.append(sprite_file_name(file_id));
    return path;
}

void write_little_endian_u16(std::uint8_t* destination,
                             std::uint16_t value) {
    destination[0] = static_cast<std::uint8_t>(value);
    destination[1] = static_cast<std::uint8_t>(value >> 8U);
}

void write_little_endian_u32(std::uint8_t* destination,
                             std::uint32_t value) {
    destination[0] = static_cast<std::uint8_t>(value);
    destination[1] = static_cast<std::uint8_t>(value >> 8U);
    destination[2] = static_cast<std::uint8_t>(value >> 16U);
    destination[3] = static_cast<std::uint8_t>(value >> 24U);
}

} // namespace

std::string sprite_file_name(SpriteFileId file_id) {
    std::string filename;
    filename.reserve(8);
    filename.push_back(static_cast<char>(file_id & 0xffU));
    filename.push_back(static_cast<char>((file_id >> 8U) & 0xffU));
    filename.push_back(static_cast<char>((file_id >> 16U) & 0xffU));
    filename.push_back(static_cast<char>((file_id >> 24U) & 0xffU));
    filename.append(".spr");
    return filename;
}

std::string sprite_index_path(std::string_view directory,
                              SpriteFileId file_id) {
    return path_with_sprite_file(directory, file_id);
}

SpriteIndexWriter::SpriteIndexWriter(SpriteIndexOutputFile& output,
                                     std::uint32_t expected_frame_count)
    : output_(output), expected_frame_count_(expected_frame_count) {}

bool SpriteIndexWriter::begin() {
    if (started_ || expected_frame_count_ >
                         (std::numeric_limits<std::size_t>::max() - 2U) /
                             kRecordSize ||
        expected_frame_count_ > 0xffffU) {
        return false;
    }

    const std::size_t table_size =
        static_cast<std::size_t>(expected_frame_count_) * kRecordSize;
    table_bytes_.assign(table_size, 0);

    std::array<std::uint8_t, 2> initial_frame_count{};
    output_.write(initial_frame_count.data(), initial_frame_count.size());
    if (!table_bytes_.empty()) {
        output_.write(table_bytes_.data(), table_bytes_.size());
    }
    started_ = true;
    return true;
}

bool SpriteIndexWriter::append_frame(std::uint16_t width,
                                     std::uint16_t height,
                                     const std::uint8_t* pixels,
                                     std::size_t pixel_byte_count) {
    if (!started_ || frame_count_ >= expected_frame_count_ ||
        (pixel_byte_count != 0 && pixels == nullptr) ||
        pixel_byte_count > std::numeric_limits<std::uint32_t>::max() -
                                pixel_byte_count_ ||
        pixel_byte_count_ >
            std::numeric_limits<std::uint32_t>::max() -
                (2U + expected_frame_count_ * static_cast<std::uint32_t>(kRecordSize))) {
        return false;
    }

    const std::uint32_t record_offset =
        2U + expected_frame_count_ * static_cast<std::uint32_t>(kRecordSize) +
        pixel_byte_count_;
    std::uint8_t* record = table_bytes_.data() +
                           static_cast<std::size_t>(frame_count_) * kRecordSize;
    write_little_endian_u32(record, record_offset);
    write_little_endian_u16(record + 4, width);
    write_little_endian_u16(record + 6, height);

    if (pixel_byte_count != 0) {
        output_.write(pixels, pixel_byte_count);
    }
    pixel_byte_count_ += static_cast<std::uint32_t>(pixel_byte_count);
    ++frame_count_;
    return true;
}

bool SpriteIndexWriter::append_gallery_frames(
    Gallery& gallery,
    const PaletteRemapTable& palette,
    PixelCacheState& cache,
    SpriteFileCache& sprite_files,
    const SpriteFileSearchPaths& paths,
    BinaryResourceFileSystem& files) {
    if (!started_ || gallery.image_count > expected_frame_count_ - frame_count_ ||
        (gallery.image_count != 0 && gallery.images == nullptr)) {
        return false;
    }

    for (std::uint32_t image_index = 0; image_index < gallery.image_count;
         ++image_index) {
        Image& image = gallery.images[image_index];
        if (!image.remap_palette_indices(palette, cache, sprite_files, paths,
                                         files)) {
            return false;
        }

        const int width = image.width();
        const int height = image.height();
        if (width < 0 || height < 0 ||
            width > std::numeric_limits<std::uint16_t>::max() ||
            height > std::numeric_limits<std::uint16_t>::max()) {
            return false;
        }

        if (width != 0 &&
            static_cast<std::size_t>(height) >
                std::numeric_limits<std::size_t>::max() /
                    static_cast<std::size_t>(width)) {
            return false;
        }
        const std::size_t pixel_count =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        const std::uint8_t* pixels =
            image.get_pixel_data(cache, sprite_files, paths, files);
        if (pixels == nullptr && pixel_count != 0) {
            return false;
        }
        if (!append_frame(static_cast<std::uint16_t>(width),
                          static_cast<std::uint16_t>(height), pixels,
                          pixel_count)) {
            return false;
        }
    }
    return true;
}

bool SpriteIndexWriter::finish() {
    if (!started_ || frame_count_ != expected_frame_count_ ||
        !output_.seek_from_beginning()) {
        return false;
    }

    std::array<std::uint8_t, 2> final_frame_count{};
    write_little_endian_u16(final_frame_count.data(),
                            static_cast<std::uint16_t>(frame_count_));
    output_.write(final_frame_count.data(), final_frame_count.size());
    if (!table_bytes_.empty()) {
        output_.write(table_bytes_.data(), table_bytes_.size());
    }
    output_.close();
    return true;
}

BinaryResourceFile* SpriteFileCache::acquire(
    SpriteFileId file_id,
    const SpriteFileSearchPaths& paths,
    BinaryResourceFileSystem& files) {
    const auto existing = index_.find(file_id);
    if (existing != index_.end()) {
        Entry& entry = entries_[existing->second];
        if (entry.file) {
            entry.lru_stamp = ++access_stamp_;
            return entry.file.get();
        }
        index_.erase(existing);
    }

    Entry* selected = &entries_.front();
    std::size_t selected_index = 0;
    for (Entry& entry : entries_) {
        if (!entry.file || (selected->file && entry.lru_stamp < selected->lru_stamp)) {
            selected = &entry;
            selected_index = static_cast<std::size_t>(&entry - entries_.data());
        }
    }

    if (selected->file) {
        index_.erase(selected->file_id);
    }
    selected->file.reset();
    selected->file_id = file_id;
    selected->lru_stamp = 0;

    std::unique_ptr<BinaryResourceFile> file = files.open_for_read(
        path_with_sprite_file(paths.secondary_image_directory, file_id));
    if (!file) {
        file = files.open_for_read(
            path_with_sprite_file(paths.primary_image_directory, file_id));
    }
    if (!file) {
        return nullptr;
    }

    selected->file = std::move(file);
    selected->lru_stamp = ++access_stamp_;
    index_[file_id] = selected_index;
    return selected->file.get();
}

void SpriteFileCache::clear() {
    for (Entry& entry : entries_) {
        entry.file.reset();
        entry.file_id = 0;
        entry.lru_stamp = 0;
    }
    index_.clear();
    access_stamp_ = 0;
}

} // namespace creatures1::display
