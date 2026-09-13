#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "../world/viewport.hpp"

namespace creatures1::ui {

struct EyeViewRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct EyeViewPosition {
    int x = 0;
    int y = 0;
};

// The concrete CEyeView owns the MFC window and its WorldRenderer.  The
// policy recovered from C1 is kept here; window-class registration, registry
// handles, USER32 metrics, and MFC message dispatch remain host operations.
class EyeViewHost {
public:
    virtual ~EyeViewHost() = default;

    virtual std::string localized_eye_view_title() const = 0;
    virtual std::string selected_creature_name() const = 0;
    virtual void set_window_title(std::string_view title) = 0;

    virtual EyeViewPosition window_position() const = 0;
    virtual void persist_eye_view_position(const EyeViewPosition& position) = 0;
    virtual void forward_default_window_operation() = 0;

    virtual void forward_default_size(unsigned size_type,
                                      int client_width,
                                      int client_height) = 0;
    // The palette-focus argument is an MFC CWnd* at the binary boundary.  It
    // remains an identity token here; the host performs the CWnd/HWND lookup
    // instead of leaking MFC layout into the C1 policy layer.
    virtual bool palette_focus_is_this_window(const void* palette_focus_window) const = 0;
    virtual bool palette_focus_owns_renderer(const void* palette_focus_window) const = 0;
    virtual std::uint32_t realize_world_renderer_palette() = 0;

    virtual void resize_world_renderer(int client_width,
                                       int client_height) = 0;

    virtual bool create_native_eye_window(std::string_view title,
                                          std::uint32_t style) = 0;
    virtual EyeViewRect default_eye_view_rect() const = 0;
    virtual bool read_saved_eye_view_position(EyeViewPosition& position) const = 0;
    virtual EyeViewPosition eye_view_position_limits() const = 0;
    virtual void move_eye_view_window(int x, int y, int width, int height,
                                      bool repaint) = 0;
};

struct FollowViewportTarget {
    bool has_motion_target = false;
    bool sleep_indicator_active = false;
    int motion_target_x = 0;
    int motion_target_y = 0;
    int sound_source_x = 0;
    int sound_source_y = 0;
    int visual_width = 0;
    int visual_height = 0;
};

class FollowViewportHost {
public:
    virtual ~FollowViewportHost() = default;
    virtual bool selected_creature(FollowViewportTarget& target) const = 0;
    virtual int follow_center_x() const = 0;
    virtual int follow_center_y() const = 0;
    virtual bool follow_position_valid() const = 0;
    virtual void set_follow_position(int center_x, int center_y,
                                     bool valid) = 0;
    virtual void publish_follow_viewport(
        const ::creatures1::world::ViewportBounds& bounds) = 0;
    virtual void queue_dirty_world_rect(
        const ::creatures1::world::WorldRect& rect) = 0;
};

void update_eye_view_title(EyeViewHost& host);
void persist_eye_view_window_position(EyeViewHost& host);
void resize_eye_view(EyeViewHost& host, unsigned size_type,
                     int client_width, int client_height);
void on_eye_view_palette_changed(EyeViewHost& host,
                                 const void* palette_focus_window);
std::uint32_t on_eye_view_query_new_palette(EyeViewHost& host);
bool create_eye_view_window(EyeViewHost& host, std::string_view title);
void update_selected_creature_follow_viewport(FollowViewportHost& host);

}  // namespace creatures1::ui
