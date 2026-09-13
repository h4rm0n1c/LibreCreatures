#include "viewport.hpp"

#include "../display/rendering.hpp"

namespace creatures1::world {

void convert_world_rect_to_viewport_rect(const ViewportBounds& viewport,
                                         WorldRect& viewport_rect_out,
                                         const WorldRect& world_rect) {
    int left = world_rect.min_x - viewport.left;
    if (viewport.left - viewport.right + kWorldWidth < left) {
        left -= kWorldWidth;
    } else if (left < viewport.right - viewport.left - kWorldWidth) {
        left += kWorldWidth;
    }

    viewport_rect_out.min_x = left;
    viewport_rect_out.min_y = world_rect.min_y - viewport.top;
    viewport_rect_out.max_x = left + world_rect.max_x - world_rect.min_x;
    viewport_rect_out.max_y = world_rect.max_y - viewport.top;
}

void scroll_viewport_if_non_zero(ViewportScroller& renderer,
                                 int delta_x,
                                 int delta_y) {
    if (delta_x != 0 || delta_y != 0) {
        renderer.scroll_viewport(delta_x, delta_y);
    }
}

void center_viewport_on_selected_creature_if_in_pan_region(
    ::creatures1::display::WorldRenderer& renderer) {
    renderer.center_viewport_on_selected_creature_if_in_pan_region();
}

void follow_selected_creature_viewport(
    ::creatures1::display::WorldRenderer& renderer) {
    renderer.follow_selected_creature_viewport();
}

} // namespace creatures1::world
