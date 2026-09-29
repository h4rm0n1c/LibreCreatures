// display/s32_to_spr and application/s32_sprite_files: a .spr made from an
// .s32 for galleries that ship only as .s32.
// Build: c++ -std=c++17 -I src/c1 tests/c1_s32_to_spr_test.cpp
//            src/c1/display/s32_to_spr.cpp src/c1/display/s32.cpp
//            src/c1/display/png_decode.cpp
//            src/c1/application/s32_sprite_files.cpp
//            src/c1/application/resource_hosts.cpp
//            src/c1/display/sprite_cache.cpp src/c1/display/image.cpp
//            src/c1/display/blit.cpp

#include "application/s32_sprite_files.hpp"
#include "display/s32_to_spr.hpp"
#include "s32_test_files.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace display = creatures1::display;
namespace application = creatures1::application;
using s32_test_files::make_png;
using s32_test_files::make_s32;

namespace {

std::uint16_t u16_at(const Bytes& bytes, std::size_t at) {
    return static_cast<std::uint16_t>(bytes[at] | bytes[at + 1] << 8);
}

std::uint32_t u32_at(const Bytes& bytes, std::size_t at) {
    return bytes[at] | bytes[at + 1] << 8 | bytes[at + 2] << 16 |
           static_cast<std::uint32_t>(bytes[at + 3]) << 24;
}

void write_file(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

} // namespace

int main() {
    display::PaletteDtaBuffer colours{};
    for (auto& colour : colours.colours) {
        colour = {0, 0, 200};
    }
    colours.colours[0] = {255, 0, 0};  // index 10
    colours.colours[1] = {0, 255, 0};  // index 11
    colours.colours[5] = {0, 0, 0};    // index 15: opaque black

    // Frame 0: exact red.  Frame 1: nearly green.  Frame 2: transparent.
    // Frame 3: opaque black, which must not become index 0.
    const Bytes s32 = make_s32({make_png(3, 2, 0xff0000ffu),
                                make_png(2, 2, 0x0afa05ffu),
                                make_png(1, 1, 0x12345600u),
                                make_png(1, 1, 0x000000ffu)});
    Bytes spr;
    std::string error;
    assert(display::build_spr_from_s32(s32.data(), s32.size(), colours, spr,
                                       error));
    assert(u16_at(spr, 0) == 4);
    const std::uint32_t first = u32_at(spr, 2);
    assert(first == 2 + 8 * 4);
    assert(u16_at(spr, 6) == 3 && u16_at(spr, 8) == 2);
    for (std::size_t i = 0; i < 6; ++i) {
        assert(spr[first + i] == 10);
    }
    const std::uint32_t second = u32_at(spr, 10);
    assert(second == first + 6);
    assert(u16_at(spr, 14) == 2 && u16_at(spr, 16) == 2);
    assert(spr[second] == 11 && spr[second + 3] == 11);
    assert(spr[u32_at(spr, 18)] == 0);
    assert(spr[u32_at(spr, 26)] == 15);
    assert(spr.size() == first + 6 + 4 + 1 + 1);
    const std::size_t made_size = spr.size();

    // Not an S32: refused with a reason.
    const Bytes junk = {'R', 'I', 'F', 'F', 0, 0, 0, 0};
    assert(!display::build_spr_from_s32(junk.data(), junk.size(), colours,
                                        spr, error));
    assert(!error.empty() && spr.empty());

    // The file backend: a made .spr only where there is no real one.
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "c1_s32_to_spr_test";
    fs::remove_all(root);
    fs::create_directories(root / "world");
    fs::create_directories(root / "install");
    const std::string world = (root / "world").string() + "/";
    const std::string install = (root / "install").string() + "/";
    write_file(root / "world" / "Toys.s32", s32);

    application::StandardResourceFileBackend disk;
    application::S32SpriteFileBackend files(disk);
    files.set_sources(
        {[&colours] { return &colours; },
         [&] { return display::SpriteFileSearchPaths(world, install); }});

    assert(files.regular_file_exists(world + "Toys.spr"));
    auto file = files.open_for_read(world + "Toys.spr");
    assert(file != nullptr);
    std::uint8_t header[10] = {};
    assert(file->read_exact(header, sizeof header));
    assert(header[0] == 4 && header[6] == 3 && header[8] == 2);
    assert(file->seek_from_beginning(first));
    std::uint8_t pixel = 0;
    assert(file->read_exact(&pixel, 1) && pixel == 10);
    assert(!file->seek_from_beginning(static_cast<std::uint32_t>(made_size + 1)));
    const auto log = files.take_log();
    assert(log.size() == 1 && log[0].rfind("made Toys.spr from Toys.s32", 0) == 0);

    // No .s32 either: just missing.
    assert(files.open_for_read(world + "None.spr") == nullptr);
    assert(!files.regular_file_exists(world + "None.spr"));
    // Other files are untouched.
    assert(files.open_for_read(world + "Toys.s32") != nullptr);

    // A real Toys.spr in the install wins over the world's .s32.
    write_file(root / "install" / "Toys.spr", {1, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    assert(files.open_for_read(world + "Toys.spr") == nullptr);
    assert(files.open_for_read(install + "Toys.spr") != nullptr);

    // A broken .s32 is refused once, with a reason.
    write_file(root / "world" / "Bad.s32", junk);
    assert(files.open_for_read(world + "Bad.spr") == nullptr);
    assert(files.open_for_read(world + "Bad.spr") == nullptr);
    const auto refusals = files.take_log();
    assert(refusals.size() == 1 &&
           refusals[0].find("Bad.s32: not a readable S32 file") == 0);

    // No palette: nothing made, said why.
    files.set_sources({[] { return static_cast<const display::PaletteDtaBuffer*>(nullptr); },
                       [&] { return display::SpriteFileSearchPaths(world, install); }});
    files.clear();
    write_file(root / "world" / "Other.s32", s32);
    assert(files.open_for_read(world + "Other.spr") == nullptr);
    assert(files.take_log().size() == 1);

    fs::remove_all(root);
    std::puts("c1_s32_to_spr_test: ok");
    return 0;
}
