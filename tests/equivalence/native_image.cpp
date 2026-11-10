#include "native_image.hpp"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace equivalence {

namespace {

std::vector<std::string> g_import_names;
std::vector<std::uint32_t*> g_import_slots;

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

// Function coverage (tests/equivalence/function_coverage.py), only when
// EQUIVALENCE_COVERAGE names a file of function addresses: each function's
// first byte becomes an int3; its first hit appends "<address> <program>"
// to EQUIVALENCE_COVERAGE_OUT, puts the byte back and goes on.  No test's
// result changes: the function then runs as it would have.
std::unordered_map<std::uint32_t, std::uint8_t> g_coverage_bytes;
std::string g_coverage_out, g_coverage_program;

LONG CALLBACK coverage_handler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto address = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress));
    const auto found = g_coverage_bytes.find(address);
    if (found == g_coverage_bytes.end()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    auto* code = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
    *code = found->second;
    g_coverage_bytes.erase(found);
    FlushInstructionCache(GetCurrentProcess(), code, 1);
    if (std::FILE* out = std::fopen(g_coverage_out.c_str(), "a")) {
        std::fprintf(out, "%08x %s\n", static_cast<unsigned>(address), g_coverage_program.c_str());
        std::fclose(out);
    }
    info->ContextRecord->Eip = address;
    return EXCEPTION_CONTINUE_EXECUTION;
}

void arm_coverage(std::uint32_t image_start, std::uint32_t image_end) {
    const char* starts = std::getenv("EQUIVALENCE_COVERAGE");
    const char* out = std::getenv("EQUIVALENCE_COVERAGE_OUT");
    if (starts == nullptr || out == nullptr) {
        return;
    }
    g_coverage_out = out;
    char program[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, program, MAX_PATH);
    g_coverage_program = program;
    const std::size_t slash = g_coverage_program.find_last_of("\\/");
    if (slash != std::string::npos) g_coverage_program.erase(0, slash + 1);
    std::ifstream list(starts);
    std::string line;
    while (std::getline(list, line)) {
        const auto address = static_cast<std::uint32_t>(std::strtoul(line.c_str(), nullptr, 16));
        if (address < image_start || address >= image_end || g_coverage_bytes.count(address) != 0) {
            continue;
        }
        auto* code = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
        g_coverage_bytes[address] = *code;
        *code = 0xcc;
    }
    AddVectoredExceptionHandler(1, coverage_handler);
}

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
                g_import_slots.push_back(slots);
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
    arm_coverage(base_ + 0x1000, base_ + optional.SizeOfImage);
    FlushInstructionCache(GetCurrentProcess(), mapped, kHoleSize);
    return true;
}

bool NativeImage::bind_import(const std::string& name, void* function) {
    bool bound = false;
    for (std::size_t i = 0; i < g_import_names.size(); ++i) {
        const std::string& import = g_import_names[i];
        if (import.size() >= name.size() &&
            import.compare(import.size() - name.size(), name.size(), name) == 0) {
            *g_import_slots[i] =
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(function));
            bound = true;
        }
    }
    return bound;
}

int NativeImage::bind_real_imports(const std::string& dll_directory, std::vector<std::string>& missing) {
    int bound = 0;
    std::string loaded_dll;
    HMODULE module = nullptr;
    for (std::size_t i = 0; i < g_import_names.size(); ++i) {
        const std::string& import = g_import_names[i];
        const std::size_t bang = import.find('!');
        const std::string dll = import.substr(0, bang);
        const std::string symbol = import.substr(bang + 1);
        if (dll != loaded_dll) {
            loaded_dll = dll;
            const std::string local = dll_directory + "\\" + dll;
            module = GetFileAttributesA(local.c_str()) != INVALID_FILE_ATTRIBUTES
                         ? LoadLibraryExA(local.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)
                         : LoadLibraryA(dll.c_str());
        }
        FARPROC function = nullptr;
        if (module != nullptr) {
            function = symbol[0] == '#'
                           ? GetProcAddress(module, MAKEINTRESOURCEA(std::atoi(symbol.c_str() + 1)))
                           : GetProcAddress(module, symbol.c_str());
        }
        if (function == nullptr) {
            missing.push_back(import);
            continue;
        }
        *g_import_slots[i] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(function));
        ++bound;
    }
    return bound;
}

void NativeImage::make_return(std::uint32_t address) {
    *at<std::uint8_t*>(address) = 0xc3;
    FlushInstructionCache(GetCurrentProcess(), at<void*>(address), 1);
}

void NativeImage::redirect(std::uint32_t address, const void* function) {
    std::uint8_t* code = at<std::uint8_t*>(address);
    const auto target = reinterpret_cast<std::uintptr_t>(function);
    const std::int32_t displacement =
        static_cast<std::int32_t>(target - (reinterpret_cast<std::uintptr_t>(code) + 5));
    code[0] = 0xe9;  // jmp rel32
    std::memcpy(code + 1, &displacement, 4);
    FlushInstructionCache(GetCurrentProcess(), code, 5);
}

} // namespace equivalence
