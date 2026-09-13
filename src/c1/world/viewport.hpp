#pragma once

#include "geometry.hpp"

namespace creatures1::display {
class WorldRenderer;
}

namespace creatures1::world {

struct ViewportBounds {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

void convert_world_rect_to_viewport_rect(const ViewportBounds& viewport,
                                         WorldRect& viewport_rect_out,
                                         const WorldRect& world_rect);

class ViewportScroller {
public:
    virtual ~ViewportScroller() = default;
    virtual void scroll_viewport(int& delta_x, int& delta_y) = 0;
};

void scroll_viewport_if_non_zero(ViewportScroller& renderer,
                                 int delta_x,
                                 int delta_y);

void center_viewport_on_selected_creature_if_in_pan_region(
    ::creatures1::display::WorldRenderer& renderer);

void follow_selected_creature_viewport(
    ::creatures1::display::WorldRenderer& renderer);

} // namespace creatures1::world
