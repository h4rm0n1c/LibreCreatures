#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace creatures1::ui {

// A non-owning menu wrapper supplied by the MFC/USER32 integration layer.
// C1 owns the lookup policy; the native handle and its lifetime remain
// platform-owned.
class MenuHandle {
public:
    virtual ~MenuHandle() = default;
};

class MenuPlatform {
public:
    virtual ~MenuPlatform() = default;

    virtual MenuHandle* main_window_menu() const = 0;
    virtual std::string load_string(std::uint32_t resource_id) const = 0;
    virtual std::size_t item_count(const MenuHandle& menu) const = 0;
    virtual bool item_text(const MenuHandle& menu,
                           std::size_t item_index,
                           char* text,
                           std::size_t text_capacity) const = 0;
    virtual MenuHandle* submenu(const MenuHandle& menu,
                                std::size_t item_index) const = 0;
};

// Locate the top-level Camera menu used by the favourite-place commands.
// The returned wrapper is borrowed from MenuPlatform.
MenuHandle* find_camera_submenu(const MenuPlatform& platform);

} // namespace creatures1::ui
