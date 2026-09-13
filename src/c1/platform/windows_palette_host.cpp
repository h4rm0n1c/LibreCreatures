#include "windows_palette_host.hpp"

#include "windows_prelude.hpp"

#include <array>
#include <cstring>
#include <vector>

namespace creatures1::platform {
namespace {

class WindowsNativePalette final : public display::NativePalette {
public:
    explicit WindowsNativePalette(HPALETTE handle) : handle_(handle) {}

    ~WindowsNativePalette() override {
        if (handle_ != nullptr) {
            DeleteObject(handle_);
        }
    }

    HPALETTE handle() const { return handle_; }

private:
    HPALETTE handle_ = nullptr;
};

HDC acquire_palette_dc(HWND owner_window) {
    return GetDC(owner_window);
}

void release_palette_dc(HWND owner_window, HDC device_context) {
    if (device_context != nullptr) {
        ReleaseDC(owner_window, device_context);
    }
}

} // namespace

WindowsPalettePlatform::WindowsPalettePlatform(HWND owner_window)
    : owner_window_(owner_window) {}

namespace {

// The twenty static colours Windows keeps at the ends of a palettized system
// palette.  GetSystemPaletteEntries reports nothing on a display that is not
// palettized -- every modern one -- and the native simply left those entries
// as it found them, which was correct in 1996 and leaves them black now.
// Sprites use these slots for their whites and highlights, so without them a
// speech bubble's interior, the fishbowl, the projector light and every
// highlight drawn from the top of the palette render black.
struct StaticColour { std::uint8_t red, green, blue; };

constexpr StaticColour kLowStaticColours[10] = {
    {0, 0, 0},       {128, 0, 0},     {0, 128, 0},     {128, 128, 0},
    {0, 0, 128},     {128, 0, 128},   {0, 128, 128},   {192, 192, 192},
    {192, 220, 192}, {166, 202, 240},
};

constexpr StaticColour kHighStaticColours[10] = {
    {255, 251, 240}, {160, 160, 164}, {128, 128, 128}, {255, 0, 0},
    {0, 255, 0},     {255, 255, 0},   {0, 0, 255},     {255, 0, 255},
    {0, 255, 255},   {255, 255, 255},
};

} // namespace

void WindowsPalettePlatform::prime_system_palette(
    const display::LogicalPalette& probe) {
    HDC device_context = acquire_palette_dc(nullptr);
    if (device_context == nullptr) {
        return;
    }

    std::vector<std::uint8_t> storage(
        sizeof(LOGPALETTE) +
        (probe.entries.size() - 1) * sizeof(PALETTEENTRY));
    auto* native_palette = reinterpret_cast<LOGPALETTE*>(storage.data());
    native_palette->palVersion = probe.version;
    native_palette->palNumEntries = probe.entry_count;
    for (std::size_t index = 0; index < probe.entries.size(); ++index) {
        const display::LogicalPaletteEntry& source = probe.entries[index];
        PALETTEENTRY& destination = native_palette->palPalEntry[index];
        destination.peRed = source.colour.red;
        destination.peGreen = source.colour.green;
        destination.peBlue = source.colour.blue;
        destination.peFlags = source.flags;
    }

    HPALETTE probe_palette = CreatePalette(native_palette);
    if (probe_palette != nullptr) {
        HPALETTE previous_palette =
            SelectPalette(device_context, probe_palette, FALSE);
        RealizePalette(device_context);
        SelectPalette(device_context, previous_palette, FALSE);
        DeleteObject(probe_palette);
    }
    release_palette_dc(nullptr, device_context);
}

void WindowsPalettePlatform::fill_system_reserved_entries(
    display::LogicalPalette& palette) {
    HDC device_context = acquire_palette_dc(nullptr);
    if (device_context == nullptr) {
        return;
    }

    std::array<PALETTEENTRY, 10> low_entries{};
    std::array<PALETTEENTRY, 10> high_entries{};
    const UINT low_count = GetSystemPaletteEntries(
        device_context, 0, static_cast<UINT>(low_entries.size()),
        low_entries.data());
    const UINT high_count = GetSystemPaletteEntries(
        device_context, 0xf6, static_cast<UINT>(high_entries.size()),
        high_entries.data());

    for (std::size_t index = 0; index < 10; ++index) {
        palette.entries[index].colour =
            index < low_count
                ? display::PaletteColour{low_entries[index].peRed,
                                         low_entries[index].peGreen,
                                         low_entries[index].peBlue}
                : display::PaletteColour{kLowStaticColours[index].red,
                                         kLowStaticColours[index].green,
                                         kLowStaticColours[index].blue};
    }
    for (std::size_t index = 0; index < 10; ++index) {
        palette.entries[0xf6 + index].colour =
            index < high_count
                ? display::PaletteColour{high_entries[index].peRed,
                                         high_entries[index].peGreen,
                                         high_entries[index].peBlue}
                : display::PaletteColour{kHighStaticColours[index].red,
                                         kHighStaticColours[index].green,
                                         kHighStaticColours[index].blue};
    }
    release_palette_dc(nullptr, device_context);
}

std::unique_ptr<display::NativePalette>
WindowsPalettePlatform::create_palette(
    const display::LogicalPalette& palette) {
    std::vector<std::uint8_t> storage(
        sizeof(LOGPALETTE) +
        (palette.entries.size() - 1) * sizeof(PALETTEENTRY));
    auto* native_palette = reinterpret_cast<LOGPALETTE*>(storage.data());
    native_palette->palVersion = palette.version;
    native_palette->palNumEntries = palette.entry_count;
    for (std::size_t index = 0; index < palette.entries.size(); ++index) {
        const display::LogicalPaletteEntry& source = palette.entries[index];
        PALETTEENTRY& destination = native_palette->palPalEntry[index];
        destination.peRed = source.colour.red;
        destination.peGreen = source.colour.green;
        destination.peBlue = source.colour.blue;
        destination.peFlags = source.flags;
    }

    HPALETTE handle = CreatePalette(native_palette);
    if (handle == nullptr) {
        return nullptr;
    }
    return std::make_unique<WindowsNativePalette>(handle);
}

void* WindowsPalettePlatform::native_handle(
    const display::NativePalette* palette) {
    const auto* native = dynamic_cast<const WindowsNativePalette*>(palette);
    return native == nullptr ? nullptr : native->handle();
}

} // namespace creatures1::platform
