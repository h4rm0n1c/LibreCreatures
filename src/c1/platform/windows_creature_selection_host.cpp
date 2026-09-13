#include "windows_shell.hpp"

namespace creatures1::platform {

C1NativeCreatureSelectionMenuPlatform::C1NativeCreatureSelectionMenuPlatform(C1MainFrame& frame) : frame_(frame), root_(frame.GetMenu() == nullptr ? nullptr : frame.GetMenu()->GetSafeHmenu()) {}


creatures1::ui::MenuHandle* C1NativeCreatureSelectionMenuPlatform::main_window_menu() const {
    return root_.native_menu() == nullptr ? nullptr : &root_;
}

std::string C1NativeCreatureSelectionMenuPlatform::load_string(std::uint32_t resource_id) const {
    CStringA value;
    value.LoadStringA(static_cast<UINT>(resource_id));
    return value.GetString();
}

std::size_t C1NativeCreatureSelectionMenuPlatform::item_count( const creatures1::ui::MenuHandle& menu) const {
    const HMENU native = native_handle(menu);
    if (native == nullptr) {
        return 0;
    }
    const int count = ::GetMenuItemCount(native);
    return count < 0 ? 0 : static_cast<std::size_t>(count);
}

bool C1NativeCreatureSelectionMenuPlatform::item_text(const creatures1::ui::MenuHandle& menu, std::size_t item_index, char* text, std::size_t text_capacity) const {
    const HMENU native = native_handle(menu);
    if (native == nullptr || text == nullptr || text_capacity == 0 ||
        item_index > static_cast<std::size_t>(UINT_MAX)) {
        return false;
    }
    const UINT capacity = static_cast<UINT>(
        (std::min)(text_capacity - 1,
                   static_cast<std::size_t>(UINT_MAX - 1)));
    text[0] = '\0';
    const int length = ::GetMenuStringA(
        native, static_cast<UINT>(item_index), text, capacity,
        MF_BYPOSITION);
    text[length < 0 ? 0 : length] = '\0';
    return length > 0;
}

creatures1::ui::MenuHandle* C1NativeCreatureSelectionMenuPlatform::submenu( const creatures1::ui::MenuHandle& menu, std::size_t item_index) const {
    const HMENU native = native_handle(menu);
    if (native == nullptr || item_index > static_cast<std::size_t>(UINT_MAX)) {
        return nullptr;
    }
    const HMENU child = ::GetSubMenu(native, static_cast<int>(item_index));
    if (child == nullptr) {
        return nullptr;
    }
    for (const std::unique_ptr<C1NativeMenuHandle>& handle : submenus_) {
        if (handle->native_menu() == child) {
            return handle.get();
        }
    }
    submenus_.push_back(std::make_unique<C1NativeMenuHandle>(child));
    return submenus_.back().get();
}

bool C1NativeCreatureSelectionMenuPlatform::delete_item_by_position(creatures1::ui::MenuHandle& menu, std::size_t item_index) {
    const HMENU native = native_handle(menu);
    return native != nullptr &&
           item_index <= static_cast<std::size_t>(UINT_MAX) &&
           ::DeleteMenu(native, static_cast<UINT>(item_index),
                        MF_BYPOSITION) != FALSE;
}

bool C1NativeCreatureSelectionMenuPlatform::replace_item_by_position(creatures1::ui::MenuHandle& menu, std::size_t item_index, std::uint32_t command_id, std::string_view caption) {
    const HMENU native = native_handle(menu);
    return native != nullptr &&
           item_index <= static_cast<std::size_t>(UINT_MAX) &&
           ::ModifyMenuA(native, static_cast<UINT>(item_index),
                         MF_BYPOSITION | MF_STRING,
                         static_cast<UINT>(command_id),
                         std::string(caption).c_str()) != FALSE;
}

bool C1NativeCreatureSelectionMenuPlatform::append_item(creatures1::ui::MenuHandle& menu, std::uint32_t command_id, std::string_view caption) {
    const HMENU native = native_handle(menu);
    return native != nullptr &&
           ::AppendMenuA(native, MF_STRING, static_cast<UINT>(command_id),
                         std::string(caption).c_str()) != FALSE;
}

void C1NativeCreatureSelectionMenuPlatform::invalidate_main_toolbar() {
    frame_.invalidate_main_toolbar();
}

void C1NativeCreatureSelectionMenuPlatform::draw_main_menu_bar() { frame_.DrawMenuBar(); }


HMENU C1NativeCreatureSelectionMenuPlatform::native_handle(const creatures1::ui::MenuHandle& menu) {
    const auto* native = dynamic_cast<const C1NativeMenuHandle*>(&menu);
    return native == nullptr ? nullptr : native->native_menu();
}

} // namespace creatures1::platform
