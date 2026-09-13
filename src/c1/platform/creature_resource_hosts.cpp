#include "creature_resource_hosts.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>

namespace creatures1::platform {

C1CreatureResourceHost::C1CreatureResourceHost(
    application::ResourceFileBackend& files,
    std::string secondary_genetics_directory,
    std::string primary_genetics_directory,
    std::string primary_main_directory)
    : files_(files),
      secondary_genetics_directory_(std::move(secondary_genetics_directory)),
      primary_genetics_directory_(std::move(primary_genetics_directory)),
      primary_main_directory_(std::move(primary_main_directory)) {}

std::string C1CreatureResourceHost::genome_filename(
    creatures::GenomeFilenameId id) const {
    std::array<char, sizeof(id)> raw{};
    std::memcpy(raw.data(), &id, sizeof(id));
    return std::string(raw.data(), raw.size()) + ".gen";
}

std::string C1CreatureResourceHost::secondary_genetics_path(
    creatures::GenomeFilenameId id) const {
    return secondary_genetics_directory_ + genome_filename(id);
}

std::string C1CreatureResourceHost::primary_genetics_path(
    creatures::GenomeFilenameId id) const {
    return primary_genetics_directory_ + genome_filename(id);
}

bool C1CreatureResourceHost::regular_file_exists(
    const std::string& path) const {
    return files_.regular_file_exists(path);
}

std::vector<std::uint8_t> C1CreatureResourceHost::read_file(
    const std::string& path) {
    std::vector<std::uint8_t> bytes;
    if (!files_.read_binary_file(path, bytes)) {
        throw std::runtime_error("C1 creature resource could not be read");
    }
    return bytes;
}

void C1CreatureResourceHost::write_secondary_genetics_file(
    creatures::GenomeFilenameId id,
    const std::vector<std::uint8_t>& payload) {
    std::unique_ptr<application::ResourceWriteFile> file =
        files_.open_for_create(secondary_genetics_path(id));
    if (file == nullptr) {
        throw std::runtime_error(
            "C1 secondary genetics resource could not be created");
    }
    file->write(payload.data(), payload.size());
    file->close();
}

std::vector<std::uint8_t> C1CreatureResourceHost::read_voice_file(
    std::string_view filename) {
    return read_file(primary_main_directory_ + std::string(filename));
}

} // namespace creatures1::platform
