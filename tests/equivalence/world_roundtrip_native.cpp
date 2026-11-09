// The original's own world load and save: SFCDoc::Serialize @004311a0
// loads a world through MFC's real CArchive, SFCDoc::UpdateWorld @004324e0
// runs <ticks> world ticks (0 by default), and the world is saved again.  The LC side of the world test is the game itself
// (C1_WORLD_ROUNDTRIP, see run_world.sh), because LC's world archive is
// MFC's CArchive too.
//
//   world_roundtrip_native.exe <Creatures.exe> <install-dir> <root> <in.sfc> <out.sfc> [<ticks>]
//
// Every import is bound to the real DLL: MFC and the C++ runtime from the
// install folder, the rest from Wine.  <root> holds the game's data folders
// (Images, Body Data, Palettes, Genetics, Sounds) as copies; the world
// names are relative to it.  Nothing is drawn: the view, the renderer and
// its window are zeroed fakes, and the few calls that would reach a window
// are stubbed (below).
#include "native_image.hpp"
#include "creatures/load_dump.hpp"
#include "load_dump_native.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void __attribute__((thiscall)) ignore_dirty_rect(void*, int, int, int, int) {}

// Turns the function at `at` into `ret pop`.
void make_return_pop(equivalence::NativeImage& image, std::uint32_t at,
                     std::uint16_t pop) {
    auto* code = image.at<std::uint8_t*>(at);
    code[0] = 0xc2;
    std::memcpy(code + 1, &pop, 2);
}

using FileCtor = void(__attribute__((thiscall))*)(void*);
using FileOpen = int(__attribute__((thiscall))*)(void*, const char*, unsigned, void*);
using ArchiveCtor = void(__attribute__((thiscall))*)(void*, void*, unsigned, int, void*);
using Serialize = void(__attribute__((thiscall))*)(void*, void*);
using Thiscall0 = void(__attribute__((thiscall))*)(void*);

// One CFile + CArchive pass of SFCDoc::Serialize.  mode: 1 load, 0 store.
bool serialize_world(equivalence::NativeImage& image, void* doc,
                     const std::string& path, bool load) {
    static std::uint8_t file[2][0x100], archive[2][0x100];
    const int slot = load ? 0 : 1;
    (*image.at<FileCtor*>(0x450574))(file[slot]);                  // CFile::CFile
    const unsigned open_mode = load ? 0x20 : 0x1001;   // shareDenyWrite / modeCreate|modeWrite
    if (!(*image.at<FileOpen*>(0x450578))(file[slot], path.c_str(), open_mode, nullptr)) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return false;
    }
    (*image.at<ArchiveCtor*>(0x45056c))(archive[slot], file[slot], load ? 1 : 0, 0x1000, nullptr);
    image.at<Serialize>(0x004311a0)(doc, archive[slot]);
    (*image.at<Thiscall0*>(0x450570))(archive[slot]);   // ~CArchive: flushes
    (*image.at<Thiscall0*>(0x450c4c))(file[slot]);      // CFile::Close
    return true;
}

// The field dump of the load equivalence test (run_world.sh --lc-dump): every
// field in src/c1/creatures/load_dump_fields.inc, read from the original's
// Creature objects at their offsets, through the formatters LibreCreatures
// uses for its own dump (load_dump.hpp).  The creature list is 0x4704bc,
// its count 0x4704c0 (fanout_equivalence).
namespace dump = creatures1::load_dump;
using equivalence::native_text;
using equivalence::value_at;


// The coverage half of the load test (run_world.sh --dump): the original's
// objects come from operator new through the import slot 0x450aec (the
// Creature, 0x2cdc bytes, from Creature::CreateObject 00406f64: push 0x2cdc,
// call [0x450aec]).  With C1_WORLD_FILL=<byte>, that slot fills every new
// block with the byte before its constructor runs; with
// C1_WORLD_RAW_DUMP=<file>, the creatures' bytes are written after the load.
// Two runs with different fills show which bytes the constructor and the
// load set (tests/equivalence/load_coverage.py).
constexpr std::uint32_t kOperatorNewSlot = 0x00450aec;
constexpr std::size_t kCreatureObjectSize = 0x2cdc;
using OperatorNew = void*(__cdecl*)(std::size_t);
OperatorNew g_real_new = nullptr;
int g_fill = -1;

void* __cdecl filling_new(std::size_t size) {
    void* block = g_real_new(size);
    if (block != nullptr && g_fill >= 0) {
        std::memset(block, g_fill, size);
    }
    return block;
}

void write_raw_dump(equivalence::NativeImage& image, const char* path) {
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    const int count = *image.at<int*>(0x004704c0);
    auto* const* list = *image.at<std::uint8_t***>(0x004704bc);
    for (int c = 0; c < count; ++c) {
        std::fwrite(list[c], 1, kCreatureObjectSize, file);
    }
    std::fclose(file);
}

// The original's object registry (array 0x4704a8, count 0x4704ac; the
// heard and vehicle tests) and each object's MFC class: vftable slot 0 is
// GetRuntimeClass; with _AFXDLL a CRuntimeClass is {name +0, object size
// +4, schema +8, CreateObject +0xc, GetBaseClass +0x10}.
using GetRuntimeClass = const std::uint8_t*(__attribute__((thiscall))*)(const void*);
using GetBaseClass = const std::uint8_t*(__stdcall*)();

const std::uint8_t* runtime_class(const std::uint8_t* object) {
    const auto* vftable = *reinterpret_cast<GetRuntimeClass* const*>(object);
    return vftable[0](object);
}

std::vector<std::string> class_chain(const std::uint8_t* object) {
    std::vector<std::string> names;
    for (const std::uint8_t* rc = runtime_class(object); rc != nullptr;) {
        names.emplace_back(*reinterpret_cast<const char* const*>(rc));
        const auto base = *reinterpret_cast<const GetBaseClass*>(rc + 0x10);
        rc = base == nullptr ? nullptr : base();
    }
    return names;
}

bool is_kind(const std::vector<std::string>& chain, const char* name) {
    for (const std::string& n : chain) {
        if (n == name) return true;
    }
    return false;
}

void write_object_dump(equivalence::NativeImage& image, std::FILE* file) {
    using dump::U8; using dump::Bool; using dump::U32; using dump::I32; using dump::Ptr;
    using dump::Rect; using dump::Text; using dump::Bytes; using dump::CursorFrom; using dump::CStr;
    const int count = *image.at<int*>(0x004704ac);
    auto* const* list = *image.at<std::uint8_t***>(0x004704a8);
    std::fprintf(file, "objects %u\n", static_cast<unsigned>(count));
    for (int n = 0; n < count; ++n) {
        const std::uint8_t* object = list[n];
        const std::vector<std::string> chain = class_chain(object);
#define LOAD_DUMP_OBJECT_FIELD(cls, name, offset, kind, count_, stride, proven_by, lc)            \
        if (is_kind(chain, #cls)) {                                                               \
            for (std::size_t i = 0; i < (count_); ++i) {                                          \
                dump::write_object_line(file, static_cast<std::size_t>(n), #name, (count_), i,    \
                                        native_text(kind, object, (offset) + i * (stride)));      \
            }                                                                                     \
        }
#include "creatures/load_dump_objects.inc"
#undef LOAD_DUMP_OBJECT_FIELD
    }
}

// The objects' raw bytes for load_coverage.py: per object, its MFC class
// chain, most derived first, joined with '/' (128 bytes, NUL-padded), its
// size (4 bytes, CRuntimeClass +4) and its bytes.
void write_object_raw_dump(equivalence::NativeImage& image, const char* path) {
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    const int count = *image.at<int*>(0x004704ac);
    auto* const* list = *image.at<std::uint8_t***>(0x004704a8);
    for (int n = 0; n < count; ++n) {
        const std::uint8_t* object = list[n];
        const std::uint8_t* rc = runtime_class(object);
        std::string joined;
        for (const std::string& n : class_chain(object)) {
            joined += (joined.empty() ? "" : "/") + n;
        }
        char name[128] = {};
        std::strncpy(name, joined.c_str(), sizeof name - 1);
        std::uint32_t size = 0;
        std::memcpy(&size, rc + 4, 4);
        std::fwrite(name, 1, sizeof name, file);
        std::fwrite(&size, 1, 4, file);
        std::fwrite(object, 1, size, file);
    }
    std::fclose(file);
}

void write_field_dump(equivalence::NativeImage& image, const char* path) {
    std::FILE* file = std::fopen(path, "w");
    if (file == nullptr) {
        std::fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    using dump::U8; using dump::Bool; using dump::U32; using dump::I32; using dump::Ptr;
    using dump::Rect; using dump::Text; using dump::Bytes; using dump::CursorFrom; using dump::CStr;
    const int count = *image.at<int*>(0x004704c0);
    auto* const* list = *image.at<std::uint8_t***>(0x004704bc);
    std::fprintf(file, "creatures %u\n", static_cast<unsigned>(count));
    for (int c = 0; c < count; ++c) {
        const std::uint8_t* creature = list[c];
#define LOAD_DUMP_FIELD(name, offset, kind, count_, stride, proven_by, lc)                  \
        for (std::size_t i = 0; i < (count_); ++i) {                                        \
            dump::write_line(file, static_cast<std::size_t>(c), #name, (count_), i,         \
                             native_text(kind, creature, (offset) + i * (stride)));         \
        }
#include "creatures/load_dump_fields.inc"
#undef LOAD_DUMP_FIELD
        // The brain (Creature +0x2c18; brain_native.hpp): lobe n at
        // brain + 8 + 200 * n, the count at +6408; the neurons at the
        // pointer at lobe +156, 16 bytes each, their count at lobe +164.
        std::uint32_t brain_address;
        std::memcpy(&brain_address, creature + 0x2c18, 4);
        if (brain_address == 0) {
            std::fprintf(file, "creature %u brain null\n", static_cast<unsigned>(c));
            continue;
        }
        const auto* brain = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(brain_address));
        const std::uint32_t lobes = value_at<std::uint32_t>(brain + 6408);
        std::fprintf(file, "creature %u lobes %u\n", static_cast<unsigned>(c), lobes);
        for (std::uint32_t l = 0; l < lobes; ++l) {
            const std::uint8_t* lobe = brain + 8 + 200 * l;
#define LOAD_DUMP_LOBE_FIELD(name, offset, kind, count_, stride, proven_by, lc)                    \
            for (std::size_t i = 0; i < (count_); ++i) {                                         \
                dump::write_brain_line(file, static_cast<std::size_t>(c), l, -1, #name, (count_), i, \
                                       native_text(kind, lobe, (offset) + i * (stride)));         \
            }
#define LOAD_DUMP_NEURON_FIELD(name, offset, kind, count_, stride, proven_by, lc)
#include "creatures/load_dump_brain.inc"
#undef LOAD_DUMP_LOBE_FIELD
#undef LOAD_DUMP_NEURON_FIELD
            const std::uint32_t neurons = value_at<std::uint32_t>(lobe + 164);
            const auto* neuron_base = reinterpret_cast<const std::uint8_t*>(
                static_cast<std::uintptr_t>(value_at<std::uint32_t>(lobe + 156)));
            for (std::uint32_t n = 0; n < neurons; ++n) {
                const std::uint8_t* neuron = neuron_base + 16 * n;
#define LOAD_DUMP_LOBE_FIELD(name, offset, kind, count_, stride, proven_by, lc)
#define LOAD_DUMP_NEURON_FIELD(name, offset, kind, count_, stride, proven_by, lc)                  \
                for (std::size_t i = 0; i < (count_); ++i) {                                     \
                    dump::write_brain_line(file, static_cast<std::size_t>(c), l, static_cast<long>(n), \
                                           #name, (count_), i,                                    \
                                           native_text(kind, neuron, (offset) + i * (stride)));   \
                }
#include "creatures/load_dump_brain.inc"
#undef LOAD_DUMP_LOBE_FIELD
#undef LOAD_DUMP_NEURON_FIELD
            }
        }
    }
    write_object_dump(image, file);
    std::fclose(file);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 6 && argc != 7) {
        std::fprintf(stderr,
                     "usage: world_roundtrip_native <Creatures.exe> <install-dir> <root> <in.sfc> <out.sfc> [<ticks>]\n");
        return 2;
    }
    const long ticks = argc == 7 ? std::strtol(argv[6], nullptr, 10) : 0;
    const std::string root = std::string(argv[3]) + "\\";
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    std::vector<std::string> missing;
    image.bind_real_imports(argv[2], missing);
    if (!missing.empty()) {
        for (const std::string& name : missing) {
            std::fprintf(stderr, "missing import %s\n", name.c_str());
        }
        return 2;
    }
    // The exe's C++ initializers (its global constructors).
    using Init = void(__cdecl*)();
    for (std::uint32_t at = 0x450df0; at < 0x450ee8; at += 4) {
        if (Init f = *image.at<Init*>(at)) {
            f();
        }
    }

    // g_SFCApp: the primary and secondary directory tables (8 slots of 260
    // bytes at +304 and +2384) all point into <root>.
    auto* app = *image.at<std::uint8_t**>(0x468d80);
    if (app == nullptr) {
        std::fprintf(stderr, "no g_SFCApp\n");
        return 1;
    }
    const char* folders[8] = {"", "Sounds\\", "", "Palettes\\", "Images\\",
                              "Genetics\\", "Body Data\\", ""};
    for (int slot = 0; slot < 8; ++slot) {
        const std::string path = root + folders[slot];
        std::strcpy(reinterpret_cast<char*>(app + 304 + 260 * slot), path.c_str());
        std::strcpy(reinterpret_cast<char*>(app + 2384 + 260 * slot), path.c_str());
    }

    image.make_return(0x00422250);          // RebuildCreatureSelectionMenu: a menu
    make_return_pop(image, 0x004131a0, 8);  // CWorldRenderer::RequestViewportOrigin
    image.make_return(0x00418000);          // DebugLog
    image.make_return(0x004168b0);          // CEventBar::UpdateStatusPanes: the panes' text
    // No autosave in the tick: the interval (g_SFCApp+0xd8) is the longest.
    *reinterpret_cast<std::uint32_t*>(app + 0xd8) = 0xffffffffu;
    // A zeroed view and renderer, so the hand is placed at mouse (0, 0); the
    // renderer's window has a null HWND and no redraw pending.
    static std::uint8_t view[0x1000], renderer[0x4000], window[0x100];
    *reinterpret_cast<void**>(view + 0x14c) = renderer;
    *reinterpret_cast<void**>(renderer + 0x2c) = window;
    *image.at<void**>(0x468d84) = view;
    image.redirect(0x00412610, reinterpret_cast<const void*>(&ignore_dirty_rect));
    // MoveToAndRedraw -> MoveTo: the move without the redraw.
    image.redirect(0x00426f90, image.at<const void*>(0x004267c0));

    // The main frame (it makes the event bar and sets the toolbar global
    // 0x468d9c) and the document, by their MFC CreateObject functions.
    using Create = void*(__cdecl*)();
    image.at<Create>(0x00420f10)();
    void* doc = image.at<Create>(0x004301f0)();
    if (doc == nullptr) {
        std::fprintf(stderr, "no document\n");
        return 1;
    }
    if (const char* fill = std::getenv("C1_WORLD_FILL")) {
        g_fill = static_cast<int>(std::strtol(fill, nullptr, 0)) & 0xff;
        g_real_new = *image.at<OperatorNew*>(kOperatorNewSlot);
        *image.at<OperatorNew*>(kOperatorNewSlot) = &filling_new;
    }
    if (!serialize_world(image, doc, root + argv[4], true)) {
        return 1;
    }
    if (const char* dump = std::getenv("C1_WORLD_FIELD_DUMP")) {
        write_field_dump(image, dump);
    }
    if (const char* raw = std::getenv("C1_WORLD_RAW_DUMP")) {
        write_raw_dump(image, raw);
    }
    if (const char* raw = std::getenv("C1_WORLD_OBJECT_RAW_DUMP")) {
        write_object_raw_dump(image, raw);
    }
    using UpdateWorld = void(__attribute__((thiscall))*)(void*);
    for (long tick = 0; tick < ticks; ++tick) {
        image.at<UpdateWorld>(0x004324e0)(doc);
    }
    if (!serialize_world(image, doc, root + argv[5], false)) {
        return 1;
    }
    std::printf("objects %d, creatures %d\n", *image.at<int*>(0x4704ac),
                *image.at<int*>(0x4704c0));
    return 0;
}
