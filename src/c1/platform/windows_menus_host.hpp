#pragma once

#include "windows_prelude.hpp"

#include "../ui/menus.hpp"

namespace creatures1::platform {

// Concrete Win32 menu handle behind the semantic ui::MenuHandle boundary.
class C1NativeMenuHandle final : public creatures1::ui::MenuHandle {
public:
    explicit C1NativeMenuHandle(HMENU menu) : menu_(menu) {}

    HMENU native_menu() const { return menu_; }

private:
    HMENU menu_ = nullptr;
};

} // namespace creatures1::platform
