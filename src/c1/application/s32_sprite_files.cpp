#include "s32_sprite_files.hpp"

#include "../display/s32.hpp"
#include "../display/s32_to_spr.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace creatures1::application {
namespace {

bool ends_with_spr(std::string_view path) {
    if (path.size() < 4) {
        return false;
    }
    std::string tail(path.substr(path.size() - 4));
    std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return tail == ".spr";
}

std::size_t name_start(std::string_view path) {
    const std::size_t separator = path.find_last_of("\\/");
    return separator == std::string_view::npos ? 0 : separator + 1;
}

// The whole file, refused past the S32 size limit before anything is read.
bool read_s32_file(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return false;
    }
    const std::streamoff size = in.tellg();
    if (size < 0 ||
        static_cast<std::uint64_t>(size) > display::kS32MaxFileBytes) {
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    in.seekg(0);
    return static_cast<bool>(
        in.read(reinterpret_cast<char*>(out.data()), size));
}

class MemoryReadFile final : public ResourceReadFile {
public:
    explicit MemoryReadFile(std::shared_ptr<const std::vector<std::uint8_t>> bytes)
        : bytes_(std::move(bytes)) {}

    bool seek_from_beginning(std::uint32_t byte_offset) override {
        if (byte_offset > bytes_->size()) {
            return false;
        }
        position_ = byte_offset;
        return true;
    }

    bool read_exact(std::uint8_t* destination,
                    std::size_t byte_count) override {
        if (byte_count > bytes_->size() - position_) {
            return false;
        }
        std::copy_n(bytes_->data() + position_, byte_count, destination);
        position_ += byte_count;
        return true;
    }

private:
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
    std::size_t position_ = 0;
};

} // namespace

S32SpriteFileBackend::S32SpriteFileBackend(ResourceFileBackend& files)
    : files_(files) {}

std::vector<std::string> S32SpriteFileBackend::take_log() {
    std::vector<std::string> log;
    log.swap(log_);
    return log;
}

S32SpriteFileBackend::Made S32SpriteFileBackend::made_spr(
    std::string_view path) const {
    if (!ends_with_spr(path)) {
        return nullptr;
    }
    const std::size_t start = name_start(path);
    const std::string name(path.substr(start));
    // A real .spr in any image directory is the one the game uses.
    if (sources_.image_directories) {
        const display::SpriteFileSearchPaths directories =
            sources_.image_directories();
        for (const std::string* directory :
             {&directories.secondary_image_directory,
              &directories.primary_image_directory}) {
            if (!directory->empty() &&
                files_.regular_file_exists(*directory + name)) {
                return nullptr;
            }
        }
    }
    const std::string key(path);
    if (const auto found = made_.find(key); found != made_.end()) {
        return found->second;
    }
    const std::string s32_name = name.substr(0, name.size() - 4) + ".s32";
    std::vector<std::uint8_t> s32;
    if (!read_s32_file(std::string(path.substr(0, start)) + s32_name, s32)) {
        return nullptr;  // no .s32 either: simply missing
    }
    const display::PaletteDtaBuffer* colours =
        sources_.game_colours ? sources_.game_colours() : nullptr;
    if (colours == nullptr) {
        log_.push_back(s32_name + ": no game palette to make " + name + " with");
        return nullptr;
    }
    auto spr = std::make_shared<std::vector<std::uint8_t>>();
    std::string error;
    if (!display::build_spr_from_s32(s32.data(), s32.size(), *colours, *spr,
                                     error)) {
        log_.push_back(s32_name + ": " + error + "; no " + name);
        made_[key] = nullptr;
        return nullptr;
    }
    log_.push_back("made " + name + " from " + s32_name + " (" +
                   std::to_string(spr->size()) + " bytes)");
    made_[key] = spr;
    return spr;
}

std::unique_ptr<ResourceReadFile> S32SpriteFileBackend::open_for_read(
    std::string_view path) {
    if (std::unique_ptr<ResourceReadFile> file = files_.open_for_read(path)) {
        return file;
    }
    if (Made spr = made_spr(path)) {
        return std::make_unique<MemoryReadFile>(std::move(spr));
    }
    return nullptr;
}

std::unique_ptr<ResourceWriteFile> S32SpriteFileBackend::open_for_create(
    std::string_view path) {
    // Writing a real .spr (a creature's body images) replaces a made one.
    made_.erase(std::string(path));
    return files_.open_for_create(path);
}

bool S32SpriteFileBackend::regular_file_exists(std::string_view path) const {
    return files_.regular_file_exists(path) || made_spr(path) != nullptr;
}

bool S32SpriteFileBackend::read_binary_file(
    std::string_view path, std::vector<std::uint8_t>& bytes) const {
    if (files_.read_binary_file(path, bytes)) {
        return true;
    }
    if (Made spr = made_spr(path)) {
        bytes = *spr;
        return true;
    }
    return false;
}

bool S32SpriteFileBackend::read_text_file(std::string_view path,
                                          std::string& text) const {
    return files_.read_text_file(path, text);
}

} // namespace creatures1::application
