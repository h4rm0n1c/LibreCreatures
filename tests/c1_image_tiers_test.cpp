// display/image_tiers: finding NAME.s32 / NAME@2x.s32 beside a gallery,
// checking them against its 1x frames, choosing a tier under a cap.
// Build: c++ -std=c++17 -I src/c1 tests/c1_image_tiers_test.cpp
//            src/c1/display/image_tiers.cpp src/c1/display/image.cpp
//            src/c1/display/sprite_cache.cpp src/c1/display/s32.cpp
//            src/c1/display/png_decode.cpp src/c1/display/blit.cpp

#include "display/image_tiers.hpp"
#include "s32_test_files.hpp"

#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

namespace display = creatures1::display;
using namespace s32_test_files;
using display::ImageTier;

namespace {

class MemoryFiles final : public display::TierFileSource {
public:
    std::map<std::string, Bytes> files;
    int reads = 0;
    bool read_whole_file(std::string_view path, Bytes& out) override {
        ++reads;
        const auto found = files.find(std::string(path));
        if (found == files.end()) {
            return false;
        }
        out = found->second;
        return true;
    }
};

constexpr display::SpriteFileId kBack =
    'B' | ('a' << 8) | ('c' << 16) | ('k' << 24);

// A gallery over frames [first, first + sizes.size()) of the file.
std::unique_ptr<display::Gallery> make_gallery(
    std::int32_t first, const std::vector<std::pair<int, int>>& sizes) {
    auto gallery = std::make_unique<display::Gallery>();
    gallery->sprite_file_id = kBack;
    gallery->header_record_index = first;
    gallery->image_count = static_cast<std::uint32_t>(sizes.size());
    gallery->images = std::make_unique<display::Image[]>(sizes.size());
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        gallery->images[i].configure(gallery.get(), sizes[i].first,
                                     sizes[i].second, 0);
    }
    return gallery;
}

Bytes s32_of(const std::vector<std::pair<int, int>>& sizes, int scale,
             std::uint32_t colour) {
    std::vector<Bytes> pngs;
    for (const auto& [w, h] : sizes) {
        pngs.push_back(make_png(static_cast<std::uint32_t>(w * scale),
                                static_cast<std::uint32_t>(h * scale), colour));
    }
    return make_s32(pngs);
}

} // namespace

int main() {
    assert(display::tier_file_name(kBack, ImageTier::spr) == "Back.spr");
    assert(display::tier_file_name(kBack, ImageTier::s32) == "Back.s32");
    assert(display::tier_file_name(kBack, ImageTier::s32_2x) == "Back@2x.s32");

    const std::vector<std::pair<int, int>> sizes = {{4, 3}, {2, 5}, {6, 1}};
    const display::SpriteFileSearchPaths paths("C:\\world\\Images",
                                               "C:\\game\\Images\\");

    // Nothing but the .spr.
    {
        MemoryFiles files;
        display::ImageTierStore store;
        auto gallery = make_gallery(0, sizes);
        assert(store.tiers_for(*gallery, paths, files) == 0b001);
        assert(store.best_tier(*gallery, 0, ImageTier::s32_2x, paths, files) ==
               ImageTier::spr);
        display::RgbaImage out;
        assert(!store.decode(*gallery, 0, ImageTier::s32, paths, files, out));
    }

    // Both tiers in the install's images; the cap chooses.
    {
        MemoryFiles files;
        files.files["C:\\game\\Images\\Back.s32"] = s32_of(sizes, 1, 0x112233ffu);
        files.files["C:\\game\\Images\\Back@2x.s32"] =
            s32_of(sizes, 2, 0x445566ffu);
        display::ImageTierStore store;
        auto gallery = make_gallery(0, sizes);
        assert(store.tiers_for(*gallery, paths, files) == 0b111);
        assert(store.best_tier(*gallery, 1, ImageTier::s32_2x, paths, files) ==
               ImageTier::s32_2x);
        assert(store.best_tier(*gallery, 1, ImageTier::s32, paths, files) ==
               ImageTier::s32);
        assert(store.best_tier(*gallery, 1, ImageTier::spr, paths, files) ==
               ImageTier::spr);

        display::RgbaImage out;
        assert(store.decode(*gallery, 1, ImageTier::s32_2x, paths, files, out));
        assert(out.width == 4 && out.height == 10 && out.rgba[0] == 0x44);
        assert(store.decode(*gallery, 2, ImageTier::s32, paths, files, out));
        assert(out.width == 6 && out.height == 1 && out.rgba[0] == 0x11);

        // Checked once: asking again reads nothing.
        const int reads = files.reads;
        store.tiers_for(*gallery, paths, files);
        assert(files.reads == reads);

        // An image the game drew into falls back to its .spr pixels.
        gallery->images[1].mark_pixels_changed();
        assert(gallery->images[1].runtime_drawn());
        assert(gallery->images[1].pixel_version() == 1);
        assert(store.best_tier(*gallery, 1, ImageTier::s32_2x, paths, files) ==
               ImageTier::spr);
        assert(store.best_tier(*gallery, 0, ImageTier::s32_2x, paths, files) ==
               ImageTier::s32_2x);
    }

    // A gallery over a later range of the file's frames.
    {
        MemoryFiles files;
        std::vector<std::pair<int, int>> file_sizes = {{9, 9}, {4, 3}, {2, 5}};
        files.files["C:\\game\\Images\\Back.s32"] =
            s32_of(file_sizes, 1, 0x010203ffu);
        display::ImageTierStore store;
        auto gallery = make_gallery(1, {{4, 3}, {2, 5}});
        assert(store.tiers_for(*gallery, paths, files) == 0b011);
        display::RgbaImage out;
        assert(store.decode(*gallery, 1, ImageTier::s32, paths, files, out));
        assert(out.width == 2 && out.height == 5);
    }

    // Wrong sizes, too few frames, or a damaged file: refused and logged.
    {
        MemoryFiles files;
        std::vector<std::pair<int, int>> wrong = sizes;
        wrong[2] = {7, 1};
        files.files["C:\\game\\Images\\Back.s32"] = s32_of(wrong, 1, 0xffu);
        files.files["C:\\game\\Images\\Back@2x.s32"] =
            s32_of({{4, 3}}, 2, 0xffu);  // one frame of three
        display::ImageTierStore store;
        auto gallery = make_gallery(0, sizes);
        assert(store.tiers_for(*gallery, paths, files) == 0b001);
        const auto log = store.take_log();
        assert(log.size() == 2);
        assert(log[0].find("expected 6x1") != std::string::npos);
        assert(log[1].find("has no frame 1") != std::string::npos);
        assert(store.take_log().empty());

        // @2x must be exactly twice: a 1x-sized @2x is refused.
        MemoryFiles same;
        same.files["C:\\game\\Images\\Back@2x.s32"] = s32_of(sizes, 1, 0xffu);
        display::ImageTierStore store2;
        assert(store2.tiers_for(*make_gallery(0, sizes), paths, same) == 0b001);

        MemoryFiles junk;
        junk.files["C:\\game\\Images\\Back.s32"] = Bytes(200, 0x5a);
        display::ImageTierStore store3;
        assert(store3.tiers_for(*make_gallery(0, sizes), paths, junk) == 0b001);
        assert(store3.take_log().size() == 1);
    }

    // The world's own images win over the install's, as for a .spr.
    {
        MemoryFiles files;
        files.files["C:\\world\\Images\\Back.s32"] = s32_of(sizes, 1, 0xaa0000ffu);
        files.files["C:\\game\\Images\\Back.s32"] = s32_of(sizes, 1, 0x00bb00ffu);
        display::ImageTierStore store;
        auto gallery = make_gallery(0, sizes);
        display::RgbaImage out;
        assert(store.decode(*gallery, 0, ImageTier::s32, paths, files, out));
        assert(out.rgba[0] == 0xaa);

        // clear() forgets it all.
        store.clear();
        files.files.erase("C:\\world\\Images\\Back.s32");
        assert(store.decode(*gallery, 0, ImageTier::s32, paths, files, out));
        assert(out.rgba[1] == 0xbb);
    }

    std::puts("c1_image_tiers_test: ok");
    return 0;
}
