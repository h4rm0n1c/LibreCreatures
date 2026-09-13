#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../common/filesystem.hpp"

namespace creatures1::archive {

using GenomeFilenameId = std::uint32_t;

struct WorldBackupPaths {
    std::string_view secondary_resource_directory;
    std::string_view image_directory;
};

struct CreatureGenomeSources {
    const GenomeFilenameId* values = nullptr;
    std::size_t count = 0;
};

void refresh_temporary_world_backup(
    common::FileSystemApi& file_system,
    const WorldBackupPaths& paths,
    CreatureGenomeSources creature_genomes);

void promote_temporary_world_backup(
    common::FileSystemApi& file_system,
    std::string_view secondary_resource_directory);

} // namespace creatures1::archive
