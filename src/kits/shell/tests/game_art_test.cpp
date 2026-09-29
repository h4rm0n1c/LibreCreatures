// Portable tests for c1kitshell/game_art: which of .spr, .s32 and @2x.s32
// the kits draw, and from which directory.
//   g++ -std=c++17 -Iinclude -I../c1kitlib/include tests/game_art_test.cpp
//       src/game_art.cpp src/game_art_decoders.cpp

#include "c1kitshell/game_art.hpp"

#include "../../../../tests/s32_test_files.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

using c1kitshell::GameArt;
using c1kitshell::GameArtTier;
using c1kitshell::load_game_art;
using s32_test_files::make_png;
using s32_test_files::make_s32;

namespace {

void write_file(const std::filesystem::path& path, const Bytes& bytes) {
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

// Two frames, 2x1 and 1x1: indices {5, 0} and {7}.
Bytes make_spr() {
    Bytes spr = {2, 0};
    s32_test_files::put_le32(spr, 18);
    s32_test_files::put_le16(spr, 2);
    s32_test_files::put_le16(spr, 1);
    s32_test_files::put_le32(spr, 20);
    s32_test_files::put_le16(spr, 1);
    s32_test_files::put_le16(spr, 1);
    spr.insert(spr.end(), {5, 0, 7});
    return spr;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "c1kit_game_art_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const std::vector<std::string> dirs = {(root / "world").string() + "/",
                                           (root / "install").string() + "/"};
    fs::create_directories(root / "world");
    fs::create_directories(root / "install");
    const fs::path install = root / "install";

    c1kit::GamePalette palette{};
    palette[5] = {10, 20, 30};
    palette[7] = {40, 50, 60};

    GameArt art;
    // Nothing there.
    assert(!load_game_art(dirs, "eggs", palette, true, art));

    // .spr only: through the palette, index 0 transparent.
    write_file(install / "eggs.spr", make_spr());
    assert(load_game_art(dirs, "eggs", palette, true, art));
    assert(art.tier == GameArtTier::spr && art.frames.size() == 2);
    assert(art.frames[0].pixels[0] == 0xff0a141eu && art.frames[0].pixels[1] == 0);
    assert(art.frames[1].pixels[0] == 0xff28323cu);

    // A matching .s32 is drawn instead, in true colour.
    write_file(install / "eggs.s32",
               make_s32({make_png(2, 1, 0x112233ffu), make_png(1, 1, 0x445566ffu)}));
    assert(load_game_art(dirs, "eggs", palette, false, art));
    assert(art.tier == GameArtTier::s32 && art.scale() == 1);
    assert(art.frames[0].pixels[1] == 0xff112233u);

    // @2x only when asked for, and only at exactly twice the size.
    write_file(install / "eggs@2x.s32",
               make_s32({make_png(4, 2, 0x778899ffu), make_png(2, 2, 0x778899ffu)}));
    assert(load_game_art(dirs, "eggs", palette, true, art));
    assert(art.tier == GameArtTier::s32_2x && art.scale() == 2);
    assert(art.frames[0].width == 4 && art.frames[1].height == 2);
    write_file(install / "eggs@2x.s32",
               make_s32({make_png(4, 2, 0x778899ffu), make_png(3, 2, 0x778899ffu)}));
    assert(load_game_art(dirs, "eggs", palette, true, art) && art.tier == GameArtTier::s32);

    // A .s32 whose sizes differ from the .spr's is passed over.
    write_file(install / "eggs.s32", make_s32({make_png(3, 1, 0x112233ffu), make_png(1, 1, 0)}));
    assert(load_game_art(dirs, "eggs", palette, false, art) && art.tier == GameArtTier::spr);

    // No .spr: the .s32 alone, at its own sizes, with @2x checked against it.
    fs::remove(install / "eggs.spr");
    write_file(install / "eggs@2x.s32", make_s32({make_png(6, 2, 0x778899ffu),
                                               make_png(2, 2, 0x778899ffu)}));
    assert(load_game_art(dirs, "eggs", palette, false, art));
    assert(art.tier == GameArtTier::s32 && art.frames.size() == 2 && art.frames[0].width == 3);
    assert(load_game_art(dirs, "eggs", palette, true, art) && art.tier == GameArtTier::s32_2x);

    // A damaged .s32 on its own is nothing to draw.
    write_file(install / "eggs.s32", {'j', 'u', 'n', 'k'});
    fs::remove(install / "eggs@2x.s32");
    assert(!load_game_art(dirs, "eggs", palette, true, art));

    // The world's files come first, as the game looks there first.
    write_file(install / "eggs.spr", make_spr());
    write_file(install / "eggs.s32", make_s32({make_png(2, 1, 0x112233ffu), make_png(1, 1, 0)}));
    write_file(root / "world" / "eggs.s32",
               make_s32({make_png(2, 1, 0xabcdefffu), make_png(1, 1, 0)}));
    assert(load_game_art(dirs, "eggs", palette, false, art) && art.tier == GameArtTier::s32);
    assert(art.frames[0].pixels[0] == 0xffabcdefu);
    // A world .s32 that doesn't fit is passed over for the installation's.
    write_file(root / "world" / "eggs.s32", make_s32({make_png(5, 1, 0xabcdefffu)}));
    assert(load_game_art(dirs, "eggs", palette, false, art) && art.tier == GameArtTier::s32);
    assert(art.frames[0].pixels[0] == 0xff112233u);

    fs::remove_all(root);
    std::puts("game_art_test: ok");
    return 0;
}
