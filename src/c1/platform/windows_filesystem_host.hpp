#pragma once

#include "windows_prelude.hpp"

#include <string>
#include <string_view>
#include <vector>

#include "../common/filesystem.hpp"

namespace creatures1::platform {

// Concrete Win32 file enumeration and copy/delete behind common::FileSystemApi.
class C1NativeBackupFileSystem final
    : public creatures1::common::FileSystemApi {
public:
    std::vector<std::string> find_matching_files(
        std::string_view search_pattern) override {
        std::vector<std::string> result;
        WIN32_FIND_DATAA data{};
        const std::string pattern(search_pattern);
        HANDLE search = FindFirstFileA(pattern.c_str(), &data);
        if (search == INVALID_HANDLE_VALUE) {
            return result;
        }
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                result.emplace_back(data.cFileName);
            }
        } while (FindNextFileA(search, &data) != FALSE);
        FindClose(search);
        return result;
    }

    void remove_file(std::string_view path) override {
        DeleteFileA(std::string(path).c_str());
    }

    void copy_file(std::string_view source_path,
                   std::string_view destination_path,
                   bool fail_if_exists) override {
        CopyFileA(std::string(source_path).c_str(),
                  std::string(destination_path).c_str(),
                  fail_if_exists ? TRUE : FALSE);
    }
};

} // namespace creatures1::platform
