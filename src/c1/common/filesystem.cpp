#include "filesystem.hpp"

#include <string>

namespace creatures1::common {

void remove_first_matching_file(FileSystemApi& file_system,
                                std::string_view directory_path,
                                std::string_view filename_pattern) {
    const std::string search_pattern =
        std::string(directory_path) + std::string(filename_pattern);
    const std::vector<std::string> matches =
        file_system.find_matching_files(search_pattern);
    if (!matches.empty()) {
        file_system.remove_file(std::string(directory_path) + matches.front());
    }
}

void copy_files_matching_pattern(FileSystemApi& file_system,
                                 std::string_view source_directory,
                                 std::string_view destination_directory,
                                 std::string_view filename_pattern) {
    const std::string search_pattern =
        std::string(source_directory) + std::string(filename_pattern);
    const std::vector<std::string> matches =
        file_system.find_matching_files(search_pattern);
    for (const std::string& filename : matches) {
        const std::string source_path =
            std::string(source_directory) + filename;
        const std::string destination_path =
            std::string(destination_directory) + filename;
        file_system.copy_file(source_path, destination_path, false);
    }
}

} // namespace creatures1::common
