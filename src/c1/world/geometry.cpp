#include "geometry.hpp"

#include <algorithm>

namespace creatures1::world {

bool wrapped_world_rects_overlap(const WorldRect& first_rect,
                                 const WorldRect& second_rect) {
    if (second_rect.min_y >= first_rect.max_y ||
        first_rect.min_y >= second_rect.max_y) {
        return false;
    }

    if (first_rect.max_x < kWorldWidth) {
        if (second_rect.max_x < kWorldWidth) {
            return second_rect.max_x > first_rect.min_x &&
                   first_rect.max_x > second_rect.min_x;
        }
        if (first_rect.min_x < second_rect.max_x - kWorldWidth) {
            return true;
        }
        return first_rect.max_x > second_rect.min_x;
    }

    if (second_rect.max_x >= kWorldWidth) {
        return true;
    }
    if (second_rect.min_x < first_rect.max_x - kWorldWidth) {
        return true;
    }
    return second_rect.max_x > first_rect.min_x;
}

bool intersect_wrapped_world_rects(WorldRect& in_out_first_rect,
                                   WorldRect& second_rect) {
    if (in_out_first_rect.max_y <= second_rect.min_y ||
        second_rect.max_y <= in_out_first_rect.min_y) {
        return false;
    }

    const int first_max_x = in_out_first_rect.max_x;
    const int second_max_x = second_rect.max_x;

    if (first_max_x < kWorldWidth) {
        const int first_min_x = in_out_first_rect.min_x;
        if (second_max_x < kWorldWidth) {
            if (second_max_x <= first_min_x ||
                first_max_x <= second_rect.min_x) {
                return false;
            }
            in_out_first_rect.min_x =
                std::max(first_min_x, second_rect.min_x);
            in_out_first_rect.max_x =
                std::min(first_max_x, second_max_x);
        } else {
            int wrapped_second_max_x = second_max_x - kWorldWidth;
            if (first_min_x < wrapped_second_max_x &&
                first_max_x <= second_rect.min_x) {
                second_rect.min_x = first_min_x;
                wrapped_second_max_x =
                    std::min(first_max_x, wrapped_second_max_x);
                second_rect.max_x = wrapped_second_max_x;
                goto intersect_y;
            }
            if (first_max_x <= second_rect.min_x) {
                return false;
            }
            second_rect.max_x = first_max_x;
            second_rect.min_x =
                std::max(second_rect.min_x, in_out_first_rect.min_x);
        }
    } else {
        const int second_min_x = second_rect.min_x;
        if (second_max_x < kWorldWidth) {
            int wrapped_first_max_x = first_max_x - kWorldWidth;
            if (second_min_x < wrapped_first_max_x &&
                second_max_x <= in_out_first_rect.min_x) {
                in_out_first_rect.min_x = second_min_x;
                wrapped_first_max_x =
                    std::min(second_max_x, wrapped_first_max_x);
                in_out_first_rect.max_x = wrapped_first_max_x;
                goto intersect_y;
            }
            if (second_max_x <= in_out_first_rect.min_x) {
                return false;
            }
            in_out_first_rect.max_x = second_max_x;
            in_out_first_rect.min_x =
                std::max(in_out_first_rect.min_x, second_rect.min_x);
        } else {
            in_out_first_rect.min_x =
                std::max(in_out_first_rect.min_x, second_min_x);
            in_out_first_rect.max_x =
                std::min(in_out_first_rect.max_x, second_max_x);
        }
    }

intersect_y:
    in_out_first_rect.min_y =
        std::max(in_out_first_rect.min_y, second_rect.min_y);
    in_out_first_rect.max_y =
        std::min(in_out_first_rect.max_y, second_rect.max_y);
    return true;
}

WorldRect* union_wrapped_world_rects(WorldRect& out_rect,
                                     const WorldRect& first_rect,
                                     const WorldRect& second_rect) {
    out_rect = first_rect;
    out_rect.min_y = std::min(first_rect.min_y, second_rect.min_y);
    out_rect.max_y = std::max(first_rect.max_y, second_rect.max_y);

    const int first_max_x = first_rect.max_x;
    const int second_max_x = second_rect.max_x;
    if (first_max_x < kWorldWidth) {
        if (second_max_x >= kWorldWidth) {
            if (first_max_x <= second_rect.min_x) {
                out_rect.min_x = second_rect.min_x;
                out_rect.max_x = std::max(first_max_x + kWorldWidth,
                                          second_max_x);
                return &out_rect;
            }
            out_rect.max_x = second_max_x;
            out_rect.min_x = std::min(out_rect.min_x, second_rect.min_x);
            return &out_rect;
        }
    } else if (second_max_x < kWorldWidth) {
        if (second_max_x <= first_rect.min_x) {
            out_rect.max_x = std::max(out_rect.max_x,
                                      second_max_x + kWorldWidth);
            return &out_rect;
        }
        out_rect.min_x = std::min(out_rect.min_x, second_rect.min_x);
        return &out_rect;
    }

    out_rect.min_x = std::min(out_rect.min_x, second_rect.min_x);
    out_rect.max_x = std::max(out_rect.max_x, second_max_x);
    return &out_rect;
}

} // namespace creatures1::world
