#include "windows_gdi_host.hpp"

#include "../world/viewport.hpp"

#include "windows_prelude.hpp"

#include <array>

namespace creatures1::platform {

WindowsWorldRendererGdiHost::WindowsWorldRendererGdiHost(HWND owner_window)
    : owner_window_(owner_window) {}

void* WindowsWorldRendererGdiHost::create_memory_dc() const {
    return CreateCompatibleDC(nullptr);
}

void* WindowsWorldRendererGdiHost::create_indexed_dib(
    void* memory_dc, int width, int height, std::uint8_t*& pixels) const {
    pixels = nullptr;
    if (memory_dc == nullptr || width <= 0 || height <= 0) {
        return nullptr;
    }

    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 8;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    void* pixel_storage = nullptr;
    HBITMAP bitmap = CreateDIBSection(
        static_cast<HDC>(memory_dc), &bitmap_info, DIB_RGB_COLORS,
        &pixel_storage, nullptr, 0);
    pixels = static_cast<std::uint8_t*>(pixel_storage);
    return bitmap;
}

void* WindowsWorldRendererGdiHost::select_bitmap(void* memory_dc,
                                                   void* bitmap) const {
    if (memory_dc == nullptr || bitmap == nullptr) {
        return nullptr;
    }
    return SelectObject(static_cast<HDC>(memory_dc), bitmap);
}

void WindowsWorldRendererGdiHost::delete_object(void* object) const {
    if (object != nullptr) {
        DeleteObject(object);
    }
}

void WindowsWorldRendererGdiHost::delete_dc(void* memory_dc) const {
    if (memory_dc != nullptr) {
        DeleteDC(static_cast<HDC>(memory_dc));
    }
}

void WindowsWorldRendererGdiHost::bit_blt(
    void* target_context, int destination_x, int destination_y, int width,
    int height, void* source_context, int source_x, int source_y,
    std::uint32_t raster_operation) const {
    if (target_context == nullptr || source_context == nullptr || width <= 0 ||
        height <= 0) {
        return;
    }
    BitBlt(static_cast<HDC>(target_context), destination_x, destination_y,
           width, height, static_cast<HDC>(source_context), source_x, source_y,
           raster_operation);
}

bool WindowsWorldRendererGdiHost::owner_is_minimized(void* owner_window) const {
    HWND window = owner_window == nullptr ? owner_window_ :
                  static_cast<HWND>(owner_window);
    return window != nullptr && IsIconic(window) != FALSE;
}

std::uint32_t WindowsWorldRendererGdiHost::realize_palette(
    void* owner_window, void* palette) const {
    HWND window = owner_window == nullptr ? owner_window_ :
                  static_cast<HWND>(owner_window);
    if (window == nullptr || palette == nullptr) {
        return 0;
    }
    HDC device_context = GetDC(window);
    if (device_context == nullptr) {
        return 0;
    }
    HPALETTE previous_palette = SelectPalette(
        device_context, static_cast<HPALETTE>(palette), FALSE);
    const UINT changed = RealizePalette(device_context);
    if (previous_palette != nullptr) {
        SelectPalette(device_context, previous_palette, FALSE);
    }
    ReleaseDC(window, device_context);
    return changed;
}

void WindowsWorldRendererGdiHost::invalidate_window(void* owner_window) const {
    HWND window = owner_window == nullptr ? owner_window_ :
                  static_cast<HWND>(owner_window);
    if (window != nullptr) {
        InvalidateRect(window, nullptr, FALSE);
    }
}

void WindowsWorldRendererGdiHost::fill_client_background_black(
    void* context) const {
    HDC device_context = static_cast<HDC>(context);
    if (device_context == nullptr) {
        return;
    }
    HWND window = WindowFromDC(device_context);
    if (window == nullptr) {
        window = owner_window_;
    }
    if (window == nullptr) {
        return;
    }
    RECT client_rect{};
    GetClientRect(window, &client_rect);
    FillRect(device_context, &client_rect,
             static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
}

void WindowsWorldRendererGdiHost::draw_dirty_world_outline(
    void* target_context, const world::WorldRect& dirty_world_rect,
    const world::WorldRect& viewport_rect) const {
    if (target_context == nullptr) {
        return;
    }

    world::WorldRect viewport_bounds{
        viewport_rect.min_x, viewport_rect.min_y, viewport_rect.max_x,
        viewport_rect.max_y};
    world::WorldRect dirty_viewport{};
    world::convert_world_rect_to_viewport_rect(
        {viewport_rect.min_x, viewport_rect.min_y, viewport_rect.max_x,
         viewport_rect.max_y},
        dirty_viewport, dirty_world_rect);

    RECT outline{
        dirty_viewport.min_x, dirty_viewport.min_y, dirty_viewport.max_x,
        dirty_viewport.max_y};
    RECT client{
        0, 0, viewport_bounds.max_x - viewport_bounds.min_x,
        viewport_bounds.max_y - viewport_bounds.min_y};
    RECT clipped{};
    if (IntersectRect(&clipped, &outline, &client) == FALSE) {
        return;
    }

    HDC device_context = static_cast<HDC>(target_context);
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0xff, 0x80, 0xff));
    if (pen == nullptr) {
        return;
    }
    HGDIOBJ previous = SelectObject(device_context, pen);
    MoveToEx(device_context, clipped.left, clipped.top, nullptr);
    LineTo(device_context, clipped.right, clipped.top);
    LineTo(device_context, clipped.right, clipped.bottom);
    LineTo(device_context, clipped.left, clipped.bottom);
    LineTo(device_context, clipped.left, clipped.top);
    if (previous != nullptr) {
        SelectObject(device_context, previous);
    }
    DeleteObject(pen);
}

std::array<display::RendererPaletteEntry, 0x100>
WindowsWorldRendererGdiHost::read_palette_entries(void* palette) const {
    std::array<display::RendererPaletteEntry, 0x100> result{};
    if (palette == nullptr) {
        return result;
    }
    std::array<PALETTEENTRY, 0x100> entries{};
    const UINT count = GetPaletteEntries(
        static_cast<HPALETTE>(palette), 0, static_cast<UINT>(entries.size()),
        entries.data());
    for (UINT index = 0; index < count && index < result.size(); ++index) {
        result[index] = {
            entries[index].peRed,
            entries[index].peGreen,
            entries[index].peBlue,
        };
    }
    return result;
}

void WindowsWorldRendererGdiHost::set_dib_colour_table(
    void* memory_dc,
    const std::array<display::RendererDibColour, 0x100>& colours) const {
    if (memory_dc == nullptr) {
        return;
    }
    std::array<RGBQUAD, 0x100> native_colours{};
    for (std::size_t index = 0; index < native_colours.size(); ++index) {
        native_colours[index].rgbRed = colours[index].red;
        native_colours[index].rgbGreen = colours[index].green;
        native_colours[index].rgbBlue = colours[index].blue;
        native_colours[index].rgbReserved = colours[index].reserved;
    }
    SetDIBColorTable(static_cast<HDC>(memory_dc), 0,
                     static_cast<UINT>(native_colours.size()),
                     native_colours.data());
}

void* WindowsWorldRendererGdiHost::acquire_client_context() const {
    return owner_window_ == nullptr ? nullptr : GetDC(owner_window_);
}

void WindowsWorldRendererGdiHost::release_client_context(
    void* device_context) const {
    if (owner_window_ != nullptr && device_context != nullptr) {
        ReleaseDC(owner_window_, static_cast<HDC>(device_context));
    }
}

} // namespace creatures1::platform
