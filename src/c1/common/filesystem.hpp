#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace creatures1::common {

class FileSystemApi {
public:
    virtual ~FileSystemApi() = default;

    virtual std::vector<std::string> find_matching_files(
        std::string_view search_pattern) = 0;
    virtual void remove_file(std::string_view path) = 0;
    virtual void copy_file(std::string_view source_path,
                           std::string_view destination_path,
                           bool fail_if_exists) = 0;
};

void remove_first_matching_file(FileSystemApi& file_system,
                                std::string_view directory_path,
                                std::string_view filename_pattern);

void copy_files_matching_pattern(FileSystemApi& file_system,
                                 std::string_view source_directory,
                                 std::string_view destination_directory,
                                 std::string_view filename_pattern);

} // namespace creatures1::common
