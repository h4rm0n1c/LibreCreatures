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

// The field dump of the load equivalence test (run_world.sh --dump): the
// same lines, in the same order, as LibreCreatures writes after its load
// (C1_WORLD_FIELD_DUMP in windows_document_host.cpp).  The offsets are the
// ones the function-level tests already prove by running the original's own
// code on them, not names from a struct guess:
//   creature list 0x4704bc, count 0x4704c0 (fanout_equivalence);
//   classifier +0x4, learned-word records +0xb64, 0x38 each: recognized
//   word +0, response word +0x19, reinforcement +0x34; drives +0x2c56
//   (words_equivalence, heard_equivalence).
std::string dump_text(const char* text, std::size_t capacity) {
    std::string out;
    for (std::size_t i = 0; i < capacity && text[i] != '\0'; ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte < 0x21 || byte > 0x7e || byte == '\\') {
            char escaped[5];
            std::snprintf(escaped, sizeof escaped, "\\x%02x", byte);
            out += escaped;
        } else {
            out += static_cast<char>(byte);
        }
    }
    return out;
}

void write_field_dump(equivalence::NativeImage& image, const char* path) {
    constexpr std::uint32_t kCreatureList = 0x004704bc, kCreatureCount = 0x004704c0;
    constexpr std::size_t kClassifier = 0x4, kRecords = 0xb64, kRecordSize = 0x38,
                          kRecordCount = 80, kResponse = 0x19, kReinforcement = 0x34,
                          kDrives = 0x2c56, kTextCapacity = 25;
    std::FILE* file = std::fopen(path, "w");
    if (file == nullptr) {
        std::fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    const int count = *image.at<int*>(kCreatureCount);
    auto* const* list = *image.at<std::uint8_t***>(kCreatureList);
    std::fprintf(file, "creatures %u\n", static_cast<unsigned>(count));
    for (int c = 0; c < count; ++c) {
        const std::uint8_t* creature = list[c];
        std::fprintf(file, "creature %u classifier %08x\n", static_cast<unsigned>(c),
                     *reinterpret_cast<const std::uint32_t*>(creature + kClassifier));
        for (std::size_t k = 0; k < kRecordCount; ++k) {
            const auto* record = reinterpret_cast<const char*>(creature + kRecords + k * kRecordSize);
            std::fprintf(file, "creature %u word %u recognized %s\n", static_cast<unsigned>(c),
                         static_cast<unsigned>(k), dump_text(record, kTextCapacity).c_str());
            std::fprintf(file, "creature %u word %u response %s\n", static_cast<unsigned>(c),
                         static_cast<unsigned>(k), dump_text(record + kResponse, kTextCapacity).c_str());
            std::fprintf(file, "creature %u word %u reinforcement %u\n", static_cast<unsigned>(c),
                         static_cast<unsigned>(k),
                         *reinterpret_cast<const std::uint32_t*>(record + kReinforcement));
        }
        for (std::size_t d = 0; d < 16; ++d) {
            std::fprintf(file, "creature %u drive %u %u\n", static_cast<unsigned>(c),
                         static_cast<unsigned>(d), creature[kDrives + d]);
        }
    }
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
    if (!serialize_world(image, doc, root + argv[4], true)) {
        return 1;
    }
    if (const char* dump = std::getenv("C1_WORLD_FIELD_DUMP")) {
        write_field_dump(image, dump);
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
