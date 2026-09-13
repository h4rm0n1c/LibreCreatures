#include "resources.hpp"

namespace creatures1::application {

char* copy_primary_resource_directory_path(
    const PrimaryResourceDirectories& directories,
    std::size_t directory_index,
    char* path_out,
    SecureStringCopyApi& string_copy) {
    string_copy.copy(path_out, 0x104, directories.paths[directory_index].data());
    return path_out;
}

} // namespace creatures1::application
