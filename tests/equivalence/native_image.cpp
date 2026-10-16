#include "native_image.hpp"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

namespace equivalence {

namespace {

std::vector<std::string> g_import_names;

// Reached from an import thunk: `push index; call trap`.
extern "C" void __cdecl import_trap(std::uint32_t index) {
    std::fprintf(stderr, "native code called import %s, which the test does not provide\n",
                 index < g_import_names.size() ? g_import_names[index].c_str() : "?");
    std::fflush(stderr);
    std::exit(3);
}

template <typename T>
T read(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

// The empty section image_hole.ld puts first: 0x401000 up to 0x4a0000.
constexpr std::size_t kHoleSize = 0x9f000;
__attribute__((section(".orig"), used)) unsigned char g_image_hole[kHoleSize];

} // namespace

bool NativeImage::load(const std::string& path, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
    if (bytes.size() < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z') {
        error = "not an exe";
        return false;
    }
    const std::uint32_t pe = read<std::uint32_t>(bytes, 0x3c);
    if (read<std::uint32_t>(bytes, pe) != IMAGE_NT_SIGNATURE) {
        error = "no PE header";
        return false;
    }
    const auto file_header = read<IMAGE_FILE_HEADER>(bytes, pe + 4);
    const auto optional = read<IMAGE_OPTIONAL_HEADER32>(bytes, pe + 24);
    base_ = optional.ImageBase;

    const auto hole_start =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_image_hole));
    if (hole_start != base_ + 0x1000 ||
        hole_start + kHoleSize < base_ + optional.SizeOfImage) {
        error = "the test's image hole does not cover the original image "
                "(link with image_hole.ld at the original's base)";
        return false;
    }
    DWORD old_protection = 0;
    if (!VirtualProtect(g_image_hole, kHoleSize, PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        error = "cannot make the image hole writable and executable";
        return false;
    }
    void* const mapped = g_image_hole;
    const std::size_t sections = pe + 24 + file_header.SizeOfOptionalHeader;
    for (unsigned i = 0; i < file_header.NumberOfSections; ++i) {
        const auto section =
            read<IMAGE_SECTION_HEADER>(bytes, sections + i * sizeof(IMAGE_SECTION_HEADER));
        const std::size_t size = (std::min)(section.SizeOfRawData,
                                            section.Misc.VirtualSize);
        if (section.PointerToRawData + size > bytes.size()) {
            error = "section past the end of the file";
            return false;
        }
        std::memcpy(at<std::uint8_t*>(base_ + section.VirtualAddress),
                    bytes.data() + section.PointerToRawData, size);
    }

    // Imports: every thunk goes to its own trap stub.
    const IMAGE_DATA_DIRECTORY imports =
        optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (imports.Size != 0) {
        auto* stubs = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT,
                         PAGE_EXECUTE_READWRITE));
        std::size_t stub = 0;
        for (auto* descriptor = at<IMAGE_IMPORT_DESCRIPTOR*>(base_ + imports.VirtualAddress);
             descriptor->Name != 0; ++descriptor) {
            const std::string dll = at<const char*>(base_ + descriptor->Name);
            auto* names = at<std::uint32_t*>(
                base_ + (descriptor->OriginalFirstThunk != 0 ? descriptor->OriginalFirstThunk
                                                             : descriptor->FirstThunk));
            auto* slots = at<std::uint32_t*>(base_ + descriptor->FirstThunk);
            for (; *names != 0; ++names, ++slots) {
                std::string name = dll + "!";
                if ((*names & IMAGE_ORDINAL_FLAG32) != 0) {
                    name += "#" + std::to_string(*names & 0xffff);
                } else {
                    name += at<const char*>(base_ + *names + 2);
                }
                const auto index = static_cast<std::uint32_t>(g_import_names.size());
                g_import_names.push_back(name);
                if (stub + 10 > 0x10000) {
                    error = "too many imports";
                    return false;
                }
                std::uint8_t* code = stubs + stub;
                code[0] = 0x68;  // push index
                std::memcpy(code + 1, &index, 4);
                code[5] = 0xe8;  // call import_trap
                const auto target = static_cast<std::int32_t>(
                    reinterpret_cast<std::uintptr_t>(&import_trap) -
                    reinterpret_cast<std::uintptr_t>(code + 10));
                std::memcpy(code + 6, &target, 4);
                *slots = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code));
                stub += 10;
            }
        }
    }
    FlushInstructionCache(GetCurrentProcess(), mapped, kHoleSize);
    return true;
}

} // namespace equivalence
