#pragma once

#include "../display/rendering.hpp"

struct HWND__;
using HWND = HWND__*;

namespace creatures1::platform {

// Lower-level GDI owner for WorldRenderer.  Scene lookup, image decoding,
// dirty-queue policy, and viewport policy stay in display/rendering.cpp; this
// class contains only the recovered CreateDIBSection/BitBlt/palette/DC and
// dirty-outline operations.
class WindowsWorldRendererGdiHost final {
public:
    explicit WindowsWorldRendererGdiHost(HWND owner_window);

    void* create_memory_dc() const;
    void* create_indexed_dib(void* memory_dc, int width, int height,
                             std::uint8_t*& pixels) const;
    void* select_bitmap(void* memory_dc, void* bitmap) const;
    void delete_object(void* object) const;
    void delete_dc(void* memory_dc) const;
    void bit_blt(void* target_context, int destination_x, int destination_y,
                 int width, int height, void* source_context, int source_x,
                 int source_y, std::uint32_t raster_operation) const;

    bool owner_is_minimized(void* owner_window) const;
    std::uint32_t realize_palette(void* owner_window, void* palette) const;
    void invalidate_window(void* owner_window) const;
    void fill_client_background_black(void* device_context) const;
    void draw_dirty_world_outline(
        void* target_context, const world::WorldRect& dirty_world_rect,
        const world::WorldRect& viewport_rect) const;

    std::array<display::RendererPaletteEntry, 0x100>
    read_palette_entries(void* palette) const;
    void set_dib_colour_table(
        void* memory_dc,
        const std::array<display::RendererDibColour, 0x100>& colours) const;

    void* acquire_client_context() const;
    void release_client_context(void* device_context) const;

private:
    HWND owner_window_ = nullptr;
};

} // namespace creatures1::platform
