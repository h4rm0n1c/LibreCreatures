#pragma once

namespace creatures1::world {

struct WorldRect {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
};

constexpr int kWorldWidth = 0x20a0;
constexpr int kWorldHeight = 0x4b0;

bool wrapped_world_rects_overlap(const WorldRect& first_rect,
                                 const WorldRect& second_rect);

bool intersect_wrapped_world_rects(WorldRect& in_out_first_rect,
                                   WorldRect& second_rect);

WorldRect* union_wrapped_world_rects(WorldRect& out_rect,
                                     const WorldRect& first_rect,
                                     const WorldRect& second_rect);

} // namespace creatures1::world
