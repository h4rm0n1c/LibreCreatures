#include "toolbars.hpp"

#include <array>
#include <string>

namespace creatures1::ui {
namespace {

constexpr std::uint32_t kToolbarStyle = 0x50002830;
constexpr std::uint32_t kToolbarControlId = 0xe800;
constexpr std::uint32_t kToolbarBitmapResource = 0x80;
constexpr std::uint32_t kToolbarButtonCount = 0x23;
constexpr std::uint32_t kSelectorControlId = 0xe803;
constexpr std::uint32_t kSelectorWindowStyle = 0x10200002;

struct ButtonOverride {
    std::uint32_t index;
    std::uint32_t command_id;
    std::uint32_t style;
    std::uint32_t image_index;
};

constexpr std::array<ButtonOverride, 9> kButtonOverrides{{
    {0, 0xe803, 1, 0x96},
    {2, 0xe802, 0, 0},
    {4, 0xe145, 0, 1},
    {6, 0x8040, 0, 0x13},
    {8, 0x8045, 0, 0x14},
    {9, 0x8046, 0, 0x15},
    {0xb, 0x800f, 0, 3},
    {0xc, 0x71, 0, 5},
    {0xd, 0x8003, 2, 4},
}};

void configure_default_buttons(ToolbarPlatform& platform) {
    for (std::uint32_t index = 0; index < kToolbarButtonCount; ++index) {
        platform.set_button(index, 0, 1, 10);
    }
    for (const auto& button : kButtonOverrides) {
        platform.set_button(button.index,
                            button.command_id,
                            button.style,
                            button.image_index);
    }
}

void remove_trailing_empty_buttons(ToolbarPlatform& platform) {
    // Native initializes this to 0, not "not found" (-1): if the backward
    // scan never finds a nonempty button, it leaves button 0 alone rather
    // than also removing it. Unreachable given configure_default_buttons
    // always assigns button 0 a nonzero command, but matching it costs
    // nothing and an all-empty toolbar is exactly the case worth being
    // exact about.
    std::int32_t last_nonempty = 0;
    for (std::int32_t index = static_cast<std::int32_t>(kToolbarButtonCount) - 1;
         index >= 0;
         --index) {
        if (platform.button_command(static_cast<std::uint32_t>(index)) != 0) {
            last_nonempty = index;
            break;
        }
    }

    for (std::int32_t index = static_cast<std::int32_t>(kToolbarButtonCount) - 1;
         index > last_nonempty;
         --index) {
        platform.remove_button(static_cast<std::uint32_t>(index));
    }
}

} // namespace

bool MainToolbar::create(std::uintptr_t parent_window) {
    if (!platform_.create_toolbar(parent_window,
                                  kToolbarStyle,
                                  kToolbarControlId) ||
        !platform_.load_toolbar_bitmap(kToolbarBitmapResource) ||
        !platform_.set_button_count(kToolbarButtonCount)) {
        return false;
    }

    configure_default_buttons(platform_);
    platform_.populate_embedded_kit_menu_and_toolbar();
    remove_trailing_empty_buttons(platform_);

    // MyToolBar::Create @ 00421c00: button 0's item rect supplies left/right
    // (it is the 0x96-wide placeholder), then top = 0 and bottom = 100 give
    // the dropped-down list its height.
    ToolbarRect selector_rect = platform_.item_rect(0);
    selector_rect.top = 0;
    selector_rect.bottom = 100;
    if (!platform_.create_creature_selector(selector_rect,
                                             parent_window,
                                             kSelectorControlId)) {
        return false;
    }

    if (platform_.uses_system_gui_font()) {
        platform_.use_default_selector_font();
    } else {
        const ToolbarFontSpec font{-0xc, 700, "Arial"};
        // The original continues with successful creation even if the
        // custom font cannot be attached.
        if (!platform_.set_selector_font(font)) {
            return true;
        }
    }
    platform_.apply_selector_font();
    return true;
}

void MainToolbar::serialize(ToolbarArchiveApi& archive,
                            CreatureSelectorApi& selector) const {
    serialize_main_toolbar(archive, selector);
}

void serialize_main_toolbar(ToolbarArchiveApi& archive,
                            CreatureSelectorApi& selector) {
    if (archive.loading()) {
        selector.clear();
        const auto item_count = archive.read_selector_count();
        for (std::int32_t index = 0; index < item_count; ++index) {
            selector.add(archive.read_string());
        }
        return;
    }

    const auto item_count = selector.count();
    archive.write_selector_count(static_cast<std::int16_t>(item_count));
    for (std::size_t index = 0; index < item_count; ++index) {
        archive.write_string(selector.text_at(index));
    }
}

std::int32_t add_kit_toolbar_bitmap(ToolbarPlatform& platform,
                                    platform::ComLocalServerApi& com,
                                    std::string_view kit_prog_id) {
    std::string server_path;
    if (!com.resolve_local_server_path(kit_prog_id, server_path)) {
        return -1;
    }

    const std::size_t extension = server_path.find_last_of('.');
    if (extension == std::string::npos) {
        return -1;
    }
    server_path.replace(extension, std::string::npos, ".bmp");
    if (!platform.file_exists(server_path)) {
        return -1;
    }

    const BitmapHandle bitmap =
        platform.load_bitmap_file(server_path, 0x10, 0x10);
    if (bitmap == 0) {
        return -1;
    }

    const std::int32_t image_index = platform.add_bitmap_to_toolbar(bitmap);
    if (image_index < 0) {
        platform.destroy_bitmap(bitmap);
    }
    return image_index;
}

} // namespace creatures1::ui
