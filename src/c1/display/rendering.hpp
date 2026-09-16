#pragma once

#include "../objects/entity.hpp"
#include "gallery.hpp"
#include "../world/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace creatures1::creatures {
class Creature;
}

namespace creatures1::display {

struct VisibleSpriteRecord {
    std::int32_t entity_registry_index = 0;
    std::int32_t render_plane = 0;
};

struct RendererPaletteEntry {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

struct RendererDibColour {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t reserved = 0;
};

// The concrete implementation owns CWnd/CDC/HDC/HBITMAP and the renderable
// object collection.  Renderer policy stays here in ordinary C++; all MFC,
// USER32, GDI, and native object layout remain behind this narrow boundary.
class WorldRendererHost {
public:
    virtual ~WorldRendererHost() = default;

    virtual void* game_palette() const = 0;
    virtual std::uint32_t realize_palette(void* owner_window,
                                           void* palette) = 0;
    virtual void invalidate_window(void* owner_window) = 0;
    virtual bool owner_is_minimized(void* owner_window) const = 0;

    // Scene access and image rasterisation are the recovered C1 render core's
    // only non-GDI dependencies.  The concrete owner supplies the registry,
    // gallery cache, and sprite-file cache; C1 retains tile traversal,
    // visibility, wrapping, ordering, and overlay policy below.
    virtual Gallery* background_gallery() const = 0;
    virtual std::size_t entity_count() const = 0;
    virtual const ::creatures1::objects::Entity* entity_at(
        std::size_t index) const = 0;
    virtual void report_invalid_render_registry_index() const = 0;
    virtual void blit_image_to_dib(
        Image& image,
        std::uint8_t* dib_pixels,
        int world_x,
        int world_y,
        const world::WorldRect& clip_rect,
        const world::WorldRect& view_rect,
        bool direct_copy) = 0;
    virtual Gallery* acquire_overlay_gallery(
        std::uint32_t gallery_identifier) = 0;

    virtual void* create_memory_dc() = 0;
    virtual void* create_indexed_dib(void* memory_dc,
                                     int width,
                                     int height,
                                     std::uint8_t*& pixels) = 0;
    virtual void* select_bitmap(void* memory_dc, void* bitmap) = 0;
    virtual void delete_object(void* object) = 0;
    virtual void delete_dc(void* memory_dc) = 0;
    virtual void bit_blt(void* target_context,
                         int destination_x,
                         int destination_y,
                         int width,
                         int height,
                         void* source_context,
                         int source_x,
                         int source_y,
                         std::uint32_t raster_operation) = 0;
    virtual void draw_dirty_world_outline(
        void* target_context,
        const world::WorldRect& dirty_world_rect,
        const world::WorldRect& viewport_rect) = 0;
    virtual std::array<RendererPaletteEntry, 0x100>
    read_palette_entries(void* palette) const = 0;
    virtual void set_dib_colour_table(
        void* memory_dc,
        const std::array<RendererDibColour, 0x100>& colours) = 0;

    // Fills the owner's client area black THROUGH the given context.
    // RedrawFullView @ 0x00412490 calls FillSolidRect on the same device
    // context it then presents with, so both obey that context's clip
    // region.  Clearing through a separately acquired DC instead wipes
    // the whole client while the present only repaints the update rect.
    virtual void fill_client_background_black(void* device_context) = 0;
    virtual void present_dirty_world_rect(
        void* owner_window,
        const world::WorldRect& world_rect,
        const world::WorldRect& viewport_rect) = 0;
    virtual void present_current_view(void* owner_window,
                                      const world::WorldRect& viewport_rect) = 0;
    virtual void move_renderable_objects(int delta_x, int delta_y) = 0;
    virtual void update_view_anchored_objects() = 0;

    virtual ::creatures1::creatures::Creature* selected_creature() const = 0;
    virtual bool selected_creature_is_edit_object() const = 0;
    virtual bool selected_creature_is_bounded() const = 0;
    virtual bool selected_creature_down_foot(int& world_x,
                                             int& world_y) const = 0;
    virtual world::WorldRect navigation_bounds() const = 0;
    virtual bool contains_point(const world::WorldRect& bounds,
                                int world_x,
                                int world_y) const = 0;
};

void merge_visible_sprite_record_runs_to_buffer(
    const VisibleSpriteRecord* source_begin,
    const VisibleSpriteRecord* source_end,
    VisibleSpriteRecord* destination_begin,
    int left_run_length,
    int record_count);

void reverse_visible_sprite_record_range(VisibleSpriteRecord* first,
                                         VisibleSpriteRecord* last);

VisibleSpriteRecord* insertion_sort_visible_sprite_records_by_plane(
    VisibleSpriteRecord* begin,
    VisibleSpriteRecord* end,
    int record_count);

void stable_sort_visible_sprite_records_by_plane(
    VisibleSpriteRecord* begin,
    VisibleSpriteRecord* end,
    int record_count,
    VisibleSpriteRecord* scratch_buffer);

void inplace_merge_visible_sprite_records_by_plane(
    VisibleSpriteRecord* first,
    VisibleSpriteRecord* middle,
    VisibleSpriteRecord* last,
    int left_count,
    int right_count,
    VisibleSpriteRecord* scratch_buffer,
    int scratch_capacity,
    int total_count);

void symmetric_merge_visible_sprite_record_ranges_by_plane(
    VisibleSpriteRecord* first,
    VisibleSpriteRecord* middle,
    VisibleSpriteRecord* last,
    int left_count,
    int right_count,
    VisibleSpriteRecord* scratch_buffer,
    int scratch_capacity,
    int total_count,
    VisibleSpriteRecord* left_split,
    VisibleSpriteRecord* right_split,
    int left_lower_count,
    int right_lower_count);

class WorldRenderer {
public:
    WorldRenderer(WorldRendererHost& host,
                  void* owner_window,
                  int initial_viewport_left,
                  int initial_viewport_top,
                  int initial_viewport_width,
                  int initial_viewport_height,
                  ::creatures1::creatures::Creature* followed_creature,
                  bool smooth_scrolling_enabled,
                  int overlay_gallery_identifier,
                  bool apply_world_scroll_side_effects = true);
    ~WorldRenderer();

    void redraw_full_view(void* device_context);
    bool full_redraw_pending() const { return full_redraw_pending_; }
    void set_full_redraw_pending(bool pending) {
        full_redraw_pending_ = pending;
    }
    void resize_back_buffer_for_viewport(void* viewport_window,
                                         int viewport_width,
                                         int viewport_height);
    std::uint32_t realize_palette();
    void set_smooth_scrolling_enabled(bool enabled);
    void present_world_rect(void* device_context,
                            const world::WorldRect& requested_rect);
    // SFCDoc::UpdateWorld @ 0x004324e0 turns deferred rendering on before it
    // ticks the object registry and flushes after the tick-phase dispatch, so
    // a whole world update presents one merged set of rectangles instead of
    // blitting each change as it happens.
    void begin_deferred_dirty_rectangles();
    void flush_deferred_dirty_rectangles();
    // CAOS `sys: edit left top right bottom`.  The rectangle is a debug
    // highlight that render_and_present_rect outlines in magenta; setting it
    // dirties the whole viewport so the outline is drawn immediately.  A
    // rectangle whose right edge is zero turns the highlight off, which is how
    // the executable itself tests for "no highlight".
    void set_debug_highlight_rect(int left, int top, int right, int bottom);
    void queue_dirty_world_rect(int world_left,
                                int world_top,
                                int world_right,
                                int world_bottom);
    void request_viewport_origin(int world_x, int world_y);
    void request_viewport_origin_for_selected_creature();
    void center_viewport_on_selected_creature_if_in_pan_region();
    void follow_selected_creature_viewport();
    bool is_selected_creature_within_safe_area() const;
    void scroll_viewport(int& in_out_delta_x, int& in_out_delta_y);
    bool advance_smooth_scroll();
    void reset_navigation();
    void set_viewport_origin(int world_x, int world_y);
    // CEyeView writes its viewport directly after following the selected
    // creature. That secondary view must not shift the document's renderable
    // objects as the main view does when it scrolls.
    void set_viewport_origin_without_world_shift(int world_x, int world_y);
    void* create_back_buffer_dib(int width, int height);
    void update_dib_palette();

    int viewport_left() const { return viewport_left_; }
    int viewport_top() const { return viewport_top_; }
    int viewport_right() const { return viewport_right_; }
    int viewport_bottom() const { return viewport_bottom_; }

private:
    static int choose_scroll_step(int remaining,
                                  int accumulated,
                                  int previous_step);
    static int wrap_world_x(int world_x);

    void render_world_rect_to_dib(const world::WorldRect& render_rect);
    void render_and_present_rect(void* target_context,
                                 const world::WorldRect& render_rect,
                                 const world::WorldRect& clip_rect);

    WorldRendererHost& host_;
    void* owner_window_ = nullptr;
    void* memory_dc_ = nullptr;
    void* dib_section_ = nullptr;
    void* previous_selected_object_ = nullptr;
    std::uint8_t* dib_pixels_ = nullptr;
    void* palette_ = nullptr;
    bool smooth_scrolling_enabled_ = false;
    // The main renderer owns the document's screen-space renderable set and
    // therefore applies native ScrollViewport/SetViewportOrigin movement to
    // it.  CEyeView has a second renderer over the same document and must
    // never apply those shared-world side effects.
    bool apply_world_scroll_side_effects_ = true;
    ::creatures1::creatures::Creature* followed_creature_ = nullptr;
    int viewport_left_ = 0;
    int viewport_top_ = 0;
    int viewport_right_ = 0;
    int viewport_bottom_ = 0;
    int viewport_width_ = 0;
    int viewport_height_ = 0;
    bool full_redraw_pending_ = false;
    int smooth_scroll_remaining_x_ = 0;
    int smooth_scroll_remaining_y_ = 0;
    int smooth_scroll_accumulated_x_ = 0;
    int smooth_scroll_accumulated_y_ = 0;
    int smooth_scroll_step_x_ = 0;
    int smooth_scroll_step_y_ = 0;
    int overlay_gallery_identifier_ = 0;
    Gallery* overlay_image_ = nullptr;
    std::array<VisibleSpriteRecord, 4000> visible_sprite_records_{};
    std::array<world::WorldRect, 64> queued_dirty_rects_{};
    int queued_dirty_rect_count_ = 0;
    bool deferred_dirty_rect_rendering_ = false;
    world::WorldRect dirty_world_rect_{};
};

} // namespace creatures1::display
