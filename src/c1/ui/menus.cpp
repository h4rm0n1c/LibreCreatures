#include "menus.hpp"

#include <array>
#include <string_view>

namespace creatures1::ui {
namespace {

constexpr std::uint32_t kCameraMenuResourceId = 0xef27;
constexpr std::size_t kMenuTextCapacity = 64;

} // namespace

MenuHandle* find_camera_submenu(const MenuPlatform& platform) {
    MenuHandle* main_menu = platform.main_window_menu();
    if (main_menu == nullptr) {
        return nullptr;
    }

    const std::string camera_label =
        platform.load_string(kCameraMenuResourceId);
    std::size_t item_index = 0;
    std::size_t item_count = platform.item_count(*main_menu);
    while (item_index < item_count) {
        std::array<char, kMenuTextCapacity> item_text{};
        platform.item_text(*main_menu,
                           item_index,
                           item_text.data(),
                           item_text.size());
        if (std::string_view(item_text.data()) == camera_label) {
            return platform.submenu(*main_menu, item_index);
        }

        ++item_index;
        // The original menu loop asks USER32 for the count again after each
        // item, so a platform implementation may reflect menu mutations.
        item_count = platform.item_count(*main_menu);
    }

    return nullptr;
}

} // namespace creatures1::ui
