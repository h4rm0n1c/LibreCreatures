#include "windows_environment_host.hpp"

#include "windows_prelude.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace creatures1::platform {

std::string current_directory_with_separator() {
    std::array<char, MAX_PATH> buffer{};
    DWORD length = GetCurrentDirectoryA(
        static_cast<DWORD>(buffer.size()), buffer.data());
    if (length == 0) {
        return {};
    }
    std::string directory(buffer.data(), length);
    if (!directory.empty() && directory.back() != '\\') {
        directory.push_back('\\');
    }
    return directory;
}


bool command_line_contains(std::string_view command_line,
                           std::string_view option) {
    std::string lowered(command_line);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    std::string lowered_option(option);
    std::transform(lowered_option.begin(), lowered_option.end(),
                   lowered_option.begin(), [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return lowered.find(lowered_option) != std::string::npos;
}


} // namespace creatures1::platform
