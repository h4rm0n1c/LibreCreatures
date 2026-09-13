#include "backup.hpp"

#include <string>

namespace creatures1::archive {

namespace {

constexpr std::string_view kTemporaryDirectory = "TempBu\\";
constexpr std::string_view kBackupDirectory = "Backup\\";
constexpr std::string_view kWorldFile = "world.sfc";
constexpr std::string_view kSpritePattern = "*.spr";

std::string append_path(std::string_view directory, std::string_view suffix) {
    std::string path(directory);
    path.append(suffix);
    return path;
}

std::string sprite_filename(GenomeFilenameId filename_id) {
    std::string filename;
    filename.reserve(8);
    filename.push_back(static_cast<char>(filename_id & 0xffU));
    filename.push_back(static_cast<char>((filename_id >> 8U) & 0xffU));
    filename.push_back(static_cast<char>((filename_id >> 16U) & 0xffU));
    filename.push_back(static_cast<char>((filename_id >> 24U) & 0xffU));
    filename.append(".spr");
    return filename;
}

void remove_backup_contents(common::FileSystemApi& file_system,
                            std::string_view directory) {
    common::remove_first_matching_file(file_system, directory, kWorldFile);
    common::remove_first_matching_file(file_system, directory, kSpritePattern);
}

} // namespace

void refresh_temporary_world_backup(
    common::FileSystemApi& file_system,
    const WorldBackupPaths& paths,
    CreatureGenomeSources creature_genomes) {
    const std::string temporary_directory =
        append_path(paths.secondary_resource_directory, kTemporaryDirectory);
    remove_backup_contents(file_system, temporary_directory);

    common::copy_files_matching_pattern(file_system,
                                        paths.secondary_resource_directory,
                                        temporary_directory,
                                        kWorldFile);

    for (std::size_t index = 0; index < creature_genomes.count; ++index) {
        const std::string filename = sprite_filename(creature_genomes.values[index]);
        common::copy_files_matching_pattern(file_system,
                                            paths.image_directory,
                                            temporary_directory,
                                            filename);
    }
}

void promote_temporary_world_backup(
    common::FileSystemApi& file_system,
    std::string_view secondary_resource_directory) {
    const std::string temporary_directory =
        append_path(secondary_resource_directory, kTemporaryDirectory);
    const std::string backup_directory =
        append_path(secondary_resource_directory, kBackupDirectory);
    remove_backup_contents(file_system, backup_directory);

    common::copy_files_matching_pattern(file_system,
                                        temporary_directory,
                                        backup_directory,
                                        kWorldFile);
    common::copy_files_matching_pattern(file_system,
                                        temporary_directory,
                                        backup_directory,
                                        kSpritePattern);
}

} // namespace creatures1::archive
