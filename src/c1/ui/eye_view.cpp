#include "eye_view.hpp"

#include <cstdlib>

namespace creatures1::ui {
namespace {

constexpr std::uint32_t kEyeViewWindowStyle = 0x90c00000;

}  // namespace

void update_eye_view_title(EyeViewHost& host) {
    std::string title = host.localized_eye_view_title();
    title += " - ";
    title += host.selected_creature_name();
    host.set_window_title(title);
}

void persist_eye_view_window_position(EyeViewHost& host) {
    host.persist_eye_view_position(host.window_position());
    host.forward_default_window_operation();
}

void resize_eye_view(EyeViewHost& host, unsigned size_type,
                     int client_width, int client_height) {
    host.forward_default_size(size_type, client_width, client_height);
    host.resize_world_renderer(client_width, client_height);
}

void on_eye_view_palette_changed(EyeViewHost& host,
                                 const void* palette_focus_window) {
    host.forward_default_window_operation();
    if (!host.palette_focus_is_this_window(palette_focus_window) &&
        !host.palette_focus_owns_renderer(palette_focus_window)) {
        static_cast<void>(host.realize_world_renderer_palette());
    }
}

std::uint32_t on_eye_view_query_new_palette(EyeViewHost& host) {
    host.forward_default_window_operation();
    return host.realize_world_renderer_palette();
}

bool create_eye_view_window(EyeViewHost& host, std::string_view title) {
    const bool created =
        host.create_native_eye_window(title, kEyeViewWindowStyle);

    const EyeViewRect default_rect = host.default_eye_view_rect();
    EyeViewPosition position{default_rect.left, default_rect.top};
    if (!host.read_saved_eye_view_position(position)) {
        host.persist_eye_view_position(position);
    }

    const EyeViewPosition limits = host.eye_view_position_limits();
    if (position.x >= limits.x || position.y >= limits.y) {
        position = {};
    }

    host.move_eye_view_window(
        position.x, position.y,
        default_rect.right - default_rect.left,
        default_rect.bottom - default_rect.top,
        true);
    update_eye_view_title(host);
    return created;
}

void update_selected_creature_follow_viewport(FollowViewportHost& host) {
    FollowViewportTarget target;
    if (!host.selected_creature(target)) {
        return;
    }

    int target_x = 0;
    int target_y = 0;
    if (!target.has_motion_target || target.sleep_indicator_active ||
        target.motion_target_y > ::creatures1::world::kWorldHeight) {
        target_x = target.sound_source_x + target.visual_width / 2;
        if (target_x < ::creatures1::world::kWorldWidth) {
            if (target_x < 0) {
                target_x += ::creatures1::world::kWorldWidth;
            }
        } else {
            target_x -= ::creatures1::world::kWorldWidth;
        }
        target_y = target.sound_source_y + target.visual_height / 2;
    } else {
        target_x = target.motion_target_x;
        target_y = target.motion_target_y;
    }

    int center_x = host.follow_center_x();
    int center_y = host.follow_center_y();
    int delta_x = target_x - center_x;
    if (delta_x < -::creatures1::world::kWorldWidth / 2) {
        delta_x += ::creatures1::world::kWorldWidth;
    } else if (delta_x > ::creatures1::world::kWorldWidth / 2) {
        delta_x -= ::creatures1::world::kWorldWidth;
    }

    if (!host.follow_position_valid() || std::abs(delta_x) > 0x280) {
        center_x = target_x;
        center_y = target_y;
    } else {
        if (delta_x < 0) {
            center_x -= 0x10;
            if (center_x < target_x) {
                center_x = target_x;
            }
        } else if (delta_x > 0) {
            center_x += 0x10;
            if (center_x > target_x) {
                center_x = target_x;
            }
        }

        if (center_y < target_y) {
            center_y += 0x10;
            if (center_y > target_y) {
                center_y = target_y;
            }
        } else if (center_y > target_y) {
            center_y -= 0x10;
            if (center_y < target_y) {
                center_y = target_y;
            }
        }
    }

    host.set_follow_position(center_x, center_y, true);
    const ::creatures1::world::ViewportBounds viewport{
        center_x - 0x40, center_y - 0x30,
        center_x + 0x40, center_y + 0x30,
    };
    host.publish_follow_viewport(viewport);
    host.queue_dirty_world_rect(::creatures1::world::WorldRect{
        viewport.left, viewport.top, viewport.right, viewport.bottom});
}

}  // namespace creatures1::ui
