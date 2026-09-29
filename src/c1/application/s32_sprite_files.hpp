#pragma once

#include "resource_hosts.hpp"
#include "../display/palette.hpp"
#include "../display/sprite_cache.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace creatures1::application {

// Not native (neorender): the game's files, plus a .spr for any gallery
// that ships only as an .s32.  Asked for NAME.spr where there is none, and
// with no NAME.spr in any image directory, it answers with one made from
// NAME.s32 in the same directory (display/s32_to_spr), so everything that
// reads sprite files -- gallery indexes, pixels, creature body images --
// works unchanged.  A real NAME.spr anywhere always wins: game logic uses
// the .spr's frames whenever there is one.  Each made .spr is kept until
// clear().
class S32SpriteFileBackend final : public ResourceFileBackend {
public:
    struct Sources {
        // The game's own colours (PALETTE.DTA), or null if unreadable.
        std::function<const display::PaletteDtaBuffer*()> game_colours;
        // The directories .spr files are looked for in.
        std::function<display::SpriteFileSearchPaths()> image_directories;
    };

    explicit S32SpriteFileBackend(ResourceFileBackend& files);

    void set_sources(Sources sources) { sources_ = std::move(sources); }
    // Forgets every made .spr (a new world, a new palette).
    void clear() { made_.clear(); }
    // One line per .spr made or refused since the last call.
    std::vector<std::string> take_log();

    std::unique_ptr<ResourceReadFile> open_for_read(
        std::string_view path) override;
    std::unique_ptr<ResourceWriteFile> open_for_create(
        std::string_view path) override;
    bool regular_file_exists(std::string_view path) const override;
    bool read_binary_file(std::string_view path,
                          std::vector<std::uint8_t>& bytes) const override;
    bool read_text_file(std::string_view path,
                        std::string& text) const override;

private:
    using Made = std::shared_ptr<const std::vector<std::uint8_t>>;
    // The .spr made for `path`, or null when there is nothing to make.
    Made made_spr(std::string_view path) const;

    ResourceFileBackend& files_;
    Sources sources_;
    // Refusals are kept too (as null), so a bad .s32 is tried once.
    mutable std::map<std::string, Made> made_;
    mutable std::vector<std::string> log_;
};

} // namespace creatures1::application
