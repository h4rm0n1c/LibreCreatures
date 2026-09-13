#pragma once

#include "../display/palette.hpp"

struct HWND__;
using HWND = HWND__*;

namespace creatures1::platform {

// The palette policy is reconstructed in display/palette.cpp.  This adapter
// owns only the Win32 HDC/HPALETTE lifetime and the system-palette queries
// used by the original C1 initialization path.
class WindowsPalettePlatform final : public display::PalettePlatform {
public:
    explicit WindowsPalettePlatform(HWND owner_window = nullptr);
    ~WindowsPalettePlatform() override = default;

    void prime_system_palette(
        const display::LogicalPalette& probe) override;
    void fill_system_reserved_entries(
        display::LogicalPalette& palette) override;
    std::unique_ptr<display::NativePalette> create_palette(
        const display::LogicalPalette& palette) override;

    // WorldRendererHost receives the native palette as an opaque boundary
    // value.  This helper keeps the downcast and HPALETTE extraction here.
    static void* native_handle(const display::NativePalette* palette);

private:
    HWND owner_window_ = nullptr;
};

} // namespace creatures1::platform
