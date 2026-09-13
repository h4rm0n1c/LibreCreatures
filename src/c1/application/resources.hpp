#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace creatures1::application {

constexpr std::size_t kPrimaryResourceDirectoryCount = 8;

struct PrimaryResourceDirectories {
    std::array<std::string_view, kPrimaryResourceDirectoryCount> paths{};
};

// The original routine delegates the bounded copy to the imported CRT.  Keep
// that implementation at an explicit platform boundary in the clean source.
class SecureStringCopyApi {
public:
    virtual ~SecureStringCopyApi() = default;
    virtual int copy(char* destination,
                     std::size_t destination_capacity,
                     const char* source) const = 0;
};

char* copy_primary_resource_directory_path(
    const PrimaryResourceDirectories& directories,
    std::size_t directory_index,
    char* path_out,
    SecureStringCopyApi& string_copy);

} // namespace creatures1::application
