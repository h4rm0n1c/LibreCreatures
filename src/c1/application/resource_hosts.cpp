#include "resource_hosts.hpp"

#include "../display/sprite_cache.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <utility>

namespace creatures1::application {
namespace {

class StandardReadFile final : public ResourceReadFile {
public:
    explicit StandardReadFile(std::string_view path)
        : file_(std::string(path), std::ios::in | std::ios::binary) {}

    bool is_open() const { return file_.is_open(); }

    bool seek_from_beginning(std::uint32_t byte_offset) override {
        file_.clear();
        file_.seekg(static_cast<std::streamoff>(byte_offset), std::ios::beg);
        return file_.good();
    }

    bool read_exact(std::uint8_t* destination,
                    std::size_t byte_count) override {
        if (byte_count == 0) {
            return true;
        }
        if (destination == nullptr ||
            byte_count > static_cast<std::size_t>(
                              std::numeric_limits<std::streamsize>::max())) {
            return false;
        }
        file_.read(reinterpret_cast<char*>(destination),
                   static_cast<std::streamsize>(byte_count));
        return file_.good() &&
               file_.gcount() == static_cast<std::streamsize>(byte_count);
    }

private:
    std::ifstream file_;
};

class StandardWriteFile final : public ResourceWriteFile {
public:
    explicit StandardWriteFile(std::string_view path)
        : file_(std::string(path), std::ios::out | std::ios::binary |
                                      std::ios::trunc) {}

    bool is_open() const { return file_.is_open(); }

    void write(const std::uint8_t* bytes, std::size_t byte_count) override {
        if (byte_count == 0) {
            return;
        }
        if (bytes == nullptr ||
            byte_count > static_cast<std::size_t>(
                              std::numeric_limits<std::streamsize>::max())) {
            file_.setstate(std::ios::failbit);
            return;
        }
        file_.write(reinterpret_cast<const char*>(bytes),
                    static_cast<std::streamsize>(byte_count));
    }

    bool seek_from_beginning() override {
        file_.clear();
        file_.seekp(0, std::ios::beg);
        return file_.good();
    }

    void close() override { file_.close(); }

private:
    std::ofstream file_;
};

class BinaryReadAdapter final : public creatures1::display::BinaryResourceFile {
public:
    explicit BinaryReadAdapter(std::unique_ptr<ResourceReadFile> file)
        : file_(std::move(file)) {}

    bool seek_from_beginning(std::uint32_t byte_offset) override {
        return file_ != nullptr && file_->seek_from_beginning(byte_offset);
    }

    bool read_exact(std::uint8_t* destination,
                    std::size_t byte_count) override {
        return file_ != nullptr && file_->read_exact(destination, byte_count);
    }

private:
    std::unique_ptr<ResourceReadFile> file_;
};

class PaletteReadAdapter final : public creatures1::display::PaletteDtaFile {
public:
    explicit PaletteReadAdapter(std::unique_ptr<ResourceReadFile> file)
        : file_(std::move(file)) {}

    bool skip(std::size_t byte_count) override {
        if (byte_count > std::numeric_limits<std::uint32_t>::max() - offset_) {
            return false;
        }
        offset_ += static_cast<std::uint32_t>(byte_count);
        return file_ != nullptr && file_->seek_from_beginning(offset_);
    }

    bool read_exact(std::uint8_t* destination,
                    std::size_t byte_count) override {
        if (file_ == nullptr || !file_->seek_from_beginning(offset_) ||
            !file_->read_exact(destination, byte_count) ||
            byte_count > std::numeric_limits<std::uint32_t>::max() - offset_) {
            return false;
        }
        offset_ += static_cast<std::uint32_t>(byte_count);
        return true;
    }

private:
    std::unique_ptr<ResourceReadFile> file_;
    std::uint32_t offset_ = 0;
};

class SpriteIndexWriteAdapter final
    : public creatures1::display::SpriteIndexOutputFile {
public:
    explicit SpriteIndexWriteAdapter(std::unique_ptr<ResourceWriteFile> file)
        : file_(std::move(file)) {}

    void write(const std::uint8_t* bytes, std::size_t byte_count) override {
        if (file_ != nullptr) {
            file_->write(bytes, byte_count);
        }
    }

    bool seek_from_beginning() override {
        return file_ != nullptr && file_->seek_from_beginning();
    }

    void close() override {
        if (file_ != nullptr) {
            file_->close();
        }
    }

private:
    std::unique_ptr<ResourceWriteFile> file_;
};

std::uint16_t read_u16_le(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(bytes[1]) << 8U;
}

std::uint32_t read_u32_le(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           static_cast<std::uint32_t>(bytes[1]) << 8U |
           static_cast<std::uint32_t>(bytes[2]) << 16U |
           static_cast<std::uint32_t>(bytes[3]) << 24U;
}

} // namespace

std::unique_ptr<ResourceReadFile> StandardResourceFileBackend::open_for_read(
    std::string_view path) {
    auto file = std::make_unique<StandardReadFile>(path);
    if (!file->is_open()) {
        return nullptr;
    }
    return file;
}

std::unique_ptr<ResourceWriteFile>
StandardResourceFileBackend::open_for_create(std::string_view path) {
    auto file = std::make_unique<StandardWriteFile>(path);
    if (!file->is_open()) {
        return nullptr;
    }
    return file;
}

bool StandardResourceFileBackend::regular_file_exists(
    std::string_view path) const {
    std::ifstream file(std::string(path), std::ios::in | std::ios::binary);
    return file.is_open();
}

bool StandardResourceFileBackend::read_binary_file(
    std::string_view path, std::vector<std::uint8_t>& bytes) const {
    std::ifstream file(std::string(path), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0 || static_cast<std::uintmax_t>(size) >
                        static_cast<std::uintmax_t>(
                            std::numeric_limits<std::size_t>::max())) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(size));
    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    return file.good() || file.eof();
}

bool StandardResourceFileBackend::read_text_file(std::string_view path,
                                                 std::string& text) const {
    std::ifstream file(std::string(path), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof()) {
        return false;
    }
    text = contents.str();
    return true;
}

C1ResourceHost::C1ResourceHost(C1ResourceDirectories directories,
                               ResourceFileBackend& files)
    : directories_(std::move(directories)), files_(files) {}

C1ResourceHost::~C1ResourceHost() = default;

std::string C1ResourceHost::resource_directory(int directory_index) const {
    if (directory_index < 0 ||
        static_cast<std::size_t>(directory_index) >=
            directories_.primary_resource_directories.size()) {
        return {};
    }
    return directories_.primary_resource_directories[
        static_cast<std::size_t>(directory_index)];
}

std::string C1ResourceHost::body_data_directory() const {
    return directories_.body_data_directory;
}

bool C1ResourceHost::regular_file_exists(std::string_view path) const {
    return files_.regular_file_exists(path);
}

bool C1ResourceHost::read_text_file(std::string_view path,
                                    std::string& text) const {
    return files_.read_text_file(path, text);
}

std::string C1ResourceHost::sprite_metadata_path(
    std::string_view directory,
    creatures1::display::SpriteFileId file_id) const {
    std::string path(directory);
    path += creatures1::display::sprite_file_name(file_id);
    return path;
}

std::vector<creatures1::display::GalleryImageMetadata>
C1ResourceHost::read_image_metadata(std::string_view path,
                                     std::size_t file_offset,
                                     std::size_t image_count) {
    if (file_offset > std::numeric_limits<std::uint32_t>::max() ||
        image_count > (std::numeric_limits<std::size_t>::max() / 8U)) {
        return {};
    }

    std::unique_ptr<ResourceReadFile> file = files_.open_for_read(path);
    if (file == nullptr ||
        !file->seek_from_beginning(static_cast<std::uint32_t>(file_offset))) {
        return {};
    }

    std::vector<creatures1::display::GalleryImageMetadata> metadata;
    metadata.reserve(image_count);
    for (std::size_t index = 0; index < image_count; ++index) {
        std::array<std::uint8_t, 8> record{};
        if (!file->read_exact(record.data(), record.size())) {
            return {};
        }
        metadata.push_back({read_u32_le(record.data()),
                            read_u16_le(record.data() + 4),
                            read_u16_le(record.data() + 6)});
    }
    return metadata;
}

std::size_t C1ResourceHost::gallery_count() const {
    return galleries_.size();
}

creatures1::display::Gallery* C1ResourceHost::gallery_at(
    std::size_t index) const {
    return index < galleries_.size() ? galleries_[index].get() : nullptr;
}

creatures1::display::Gallery* C1ResourceHost::add_gallery(
    std::unique_ptr<creatures1::display::Gallery> gallery) {
    if (gallery == nullptr) {
        return nullptr;
    }
    creatures1::display::Gallery* result = gallery.get();
    galleries_.push_back(std::move(gallery));
    return result;
}

std::unique_ptr<creatures1::display::BinaryResourceFile>
C1ResourceHost::open_for_read(std::string_view path) {
    std::unique_ptr<ResourceReadFile> file = files_.open_for_read(path);
    if (file == nullptr) {
        return nullptr;
    }
    return std::make_unique<BinaryReadAdapter>(std::move(file));
}

std::unique_ptr<creatures1::display::SpriteIndexOutputFile>
C1ResourceHost::open_for_create(std::string_view path) {
    std::unique_ptr<ResourceWriteFile> file = files_.open_for_create(path);
    if (file == nullptr) {
        return nullptr;
    }
    return std::make_unique<SpriteIndexWriteAdapter>(std::move(file));
}

bool C1ResourceHost::release_gallery(creatures1::display::Gallery& gallery) {
    const auto found = std::find_if(
        galleries_.begin(), galleries_.end(),
        [&gallery](const std::unique_ptr<creatures1::display::Gallery>& item) {
            return item.get() == &gallery;
        });
    if (found == galleries_.end()) {
        return false;
    }
    galleries_.erase(found);
    return true;
}

void C1ResourceHost::clear_galleries() {
    galleries_.clear();
}

C1PaletteDtaHost::C1PaletteDtaHost(ResourceFileBackend& files) : files_(files) {}

std::unique_ptr<creatures1::display::PaletteDtaFile>
C1PaletteDtaHost::open_for_read(std::string_view path) {
    std::unique_ptr<ResourceReadFile> file = files_.open_for_read(path);
    if (file == nullptr) {
        return nullptr;
    }
    return std::make_unique<PaletteReadAdapter>(std::move(file));
}

} // namespace creatures1::application
