#pragma once

#include "../application/application.hpp"
#include "../brain/classifiers.hpp"
#include "../world/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::objects {
class Object;
}

namespace creatures1::ui {

enum class SfcViewPendingInputFlag : std::uint32_t {
    left_button = 0x1,
    right_button = 0x2,
    left_with_shift = 0x4,
    right_with_shift = 0x8,
};

enum class ViewportNavigationMode : std::uint32_t {
    disabled = 0,
    manual = 1,
    follow_selected_creature = 2,
};

struct SfcViewState {
    std::uint32_t pending_input_flags = 0;
    // PointerTool::ProcessPendingInput copies pending_input_flags here before
    // clearing them. Creature::HandlePickupEvent consumes this previous
    // snapshot, not the now-cleared pending flags.
    std::uint32_t previous_input_flags = 0;
    bool viewport_navigation_active = false;
    ViewportNavigationMode viewport_navigation_mode =
        ViewportNavigationMode::follow_selected_creature;
    bool sound_enabled = true;
    bool sound_plays_when_unfocused = true;
    bool has_focus = true;
    bool show_coordinates = false;
    bool show_classifiers = false;
    std::uint32_t manual_navigation_safe_frame_count = 0;
    int drag_start_x = 0;
    int drag_start_y = 0;
    int keyboard_scroll_velocity_x = 0;
    int keyboard_scroll_velocity_y = 0;
    int mouse_client_x = 0;
    int mouse_client_y = 0;
    int viewport_navigation_target_client_x = 0;
    int viewport_navigation_target_client_y = 0;
};

struct WorldViewSettings {
    int half_width = 0;
    int half_height = 0;
    std::uint32_t application_privilege_level = 0;
    bool world_update_timer_is_running = false;
    bool smooth_scrolling_enabled = false;
};

// MFC, USER32, GDI, and the concrete WorldRenderer stay behind this host.
// The functions below contain the recovered C1 view policy and do not expose
// CWnd/CDC/HWND or their compiler-generated layouts.
class SfcViewHost {
public:
    virtual ~SfcViewHost() = default;

    // These operations are the MFC/USER32/registry/renderer boundary of the
    // SFCView owner.  The view policy below remains ordinary C++; the host
    // supplies concrete window handles and WorldRenderer ownership.
    virtual void initialize_view_base() = 0;
    virtual void load_view_settings(WorldViewSettings& settings,
                                    SfcViewState& state) = 0;
    virtual void persist_view_settings(const WorldViewSettings& settings,
                                       const SfcViewState& state) = 0;
    virtual bool read_dword_setting(std::string_view name,
                                    std::uint32_t& value,
                                    std::uint32_t default_value) = 0;
    virtual void write_dword_setting(std::string_view name,
                                     std::uint32_t value) = 0;
    virtual void create_world_renderer(int viewport_width,
                                       int viewport_height,
                                       bool smooth_scrolling_enabled) = 0;
    virtual void destroy_world_renderer() = 0;
    virtual void initialize_classifier_tip() = 0;
    virtual void destroy_classifier_tip() = 0;
    virtual void forward_set_focus_message() = 0;
    virtual void forward_kill_focus_message() = 0;
    virtual void restore_sound_mixer() = 0;
    virtual void suspend_sound_mixer() = 0;
    virtual int scroll_position(unsigned axis) const = 0;
    virtual bool main_frame_is_active() const = 0;
    virtual bool key_is_down(std::uint32_t virtual_key) const = 0;
    virtual void scroll_viewport(int& delta_x, int& delta_y) = 0;
    virtual void reset_renderer_navigation() = 0;
    virtual bool selected_creature_exists() const = 0;
    virtual void set_view_window_class() = 0;
    virtual void create_classifier_tip_window() = 0;

    virtual void forward_default_mouse_message(std::uint32_t n_flags,
                                               int client_x,
                                               int client_y) = 0;
    virtual void forward_default_size(unsigned resize_type,
                                      int client_width,
                                      int client_height) = 0;
    virtual void release_mouse_capture() = 0;
    virtual void capture_mouse_for_drag() = 0;
    virtual void materialize_previous_capture_window() = 0;

    virtual void set_scroll_range(unsigned axis, int minimum, int maximum,
                                  bool redraw) = 0;
    virtual void set_scroll_position(unsigned axis, int position,
                                     bool redraw) = 0;

    // OnMouseMove policy services.  Object storage, classifier-name data,
    // tooltip windows, and USER/GDI cursor/status operations remain owned by
    // their application/platform owners; the recovered ordering and policy
    // stay in on_mouse_move below.
    virtual void set_default_arrow_cursor() = 0;
    virtual void set_coordinate_status(std::string_view text) = 0;
    virtual int viewport_left() const = 0;
    virtual int viewport_top() const = 0;
    virtual std::size_t non_scenery_object_count() const = 0;
    virtual objects::Object* non_scenery_object_at(
        std::size_t index) const = 0;
    virtual void report_invalid_non_scenery_object_index() const = 0;
    virtual std::size_t scenery_object_count() const = 0;
    virtual objects::Object* scenery_object_at(std::size_t index) const = 0;
    virtual void report_invalid_scenery_object_index() const = 0;
    virtual objects::Object* pointer_tool() const = 0;
    virtual bool point_in_world_rect(const world::WorldRect& bounds,
                                     int world_x, int world_y) const = 0;
    virtual std::string classifier_name(
        const brain::ClassifierId& classifier) const = 0;
    virtual void update_classifier_tip(std::string_view text,
                                       int client_x, int client_y) = 0;
    virtual void clear_classifier_tip_text() = 0;
    virtual void update_pointer_tool_unbounded_position_and_redraw() = 0;

    virtual void fill_client_background_black(void* device_context) = 0;
    virtual void mark_renderer_full_redraw() = 0;
    virtual void present_renderer_view(void* device_context) = 0;
    virtual void resize_renderer_for_viewport(int client_width,
                                              int client_height) = 0;
    virtual void request_renderer_origin_for_selected_creature() = 0;
    virtual void invalidate_main_toolbar() = 0;

    // SFCView::OnKeyDown policy services.  The host owns the MFC windows,
    // timer, profiler, status bar, and favourite-place document storage.
    virtual void open_or_activate_system_information() = 0;
    virtual void configure_world_update_timer(
        std::uint32_t interval_or_adjustment_code) = 0;
    virtual bool has_favourite_place(std::size_t index) const = 0;
    virtual void request_renderer_origin_for_favourite_place(
        std::size_t index) = 0;
    virtual void generate_profiler_report() = 0;
    virtual void hide_classifier_tip() = 0;
    virtual void clear_coordinate_status() = 0;
    virtual void forward_default_key_down(std::uint32_t virtual_key,
                                          std::uint32_t repeat_count,
                                          std::uint32_t key_flags) = 0;
};

void initialize_view(SfcViewState& state, WorldViewSettings& settings,
                     SfcViewHost& host, std::uint32_t application_privilege_level);

void shutdown_view(const SfcViewState& state, const WorldViewSettings& settings,
                   SfcViewHost& host);

void configure_pre_create_window(SfcViewHost& host);

void initialize_view_window(SfcViewHost& host, int world_half_width,
                            int world_half_height);

void on_set_focus(SfcViewState& state, SfcViewHost& host);

void on_kill_focus(SfcViewState& state, SfcViewHost& host);

// Selects one of the four `sndf` policy words at the platform boundary.
enum class SfcViewSoundPolicy {
    foreground_only,
    plays_unfocused,
    enable,
    disable,
};

// The four `sndf` policy words. SoundManager::suspend_mixer/restore_mixer
// already carry the suspended/backend-ready guards and the stop-all-sounds
// step, so these own only the view-state transition and when to ask.
void set_sound_foreground_policy(SfcViewState& state, SfcViewHost& host);
void set_sound_conservative_policy(SfcViewState& state, SfcViewHost& host);
void enable_sound_and_restore_if_ready(SfcViewState& state, SfcViewHost& host);
void disable_sound_and_suspend_if_ready(SfcViewState& state,
                                        SfcViewHost& host);

void set_viewport_navigation_mode(SfcViewState& state, SfcViewHost& host,
                                  bool manual_mode_requested);

void toggle_follow_selected_creature_mode(SfcViewState& state,
                                          WorldViewSettings& settings,
                                          SfcViewHost& host);

void update_follow_selected_creature_command(
    const SfcViewState& state, const WorldViewSettings& settings,
    SfcViewHost& host, application::CommandUi& command_ui);

void on_horizontal_scroll(SfcViewState& state, WorldViewSettings& settings,
                          SfcViewHost& host, std::uint32_t scroll_command,
                          int thumb_position);

void on_vertical_scroll(SfcViewState& state, WorldViewSettings& settings,
                        SfcViewHost& host, std::uint32_t scroll_command,
                        int thumb_position);

void update_keyboard_scroll(SfcViewState& state,
                            const WorldViewSettings& settings,
                            SfcViewHost& host);

void on_key_down(SfcViewState& state, SfcViewHost& host,
                 std::uint32_t virtual_key, std::uint32_t repeat_count,
                 std::uint32_t key_flags);

void on_mouse_move(SfcViewState& state, const WorldViewSettings& settings,
                   SfcViewHost& host, std::uint32_t n_flags, int client_x,
                   int client_y);

class TextInputQueue {
public:
    virtual ~TextInputQueue() = default;

    virtual bool control_key_is_down() const = 0;
    virtual void forward_default_character_message(
        std::uint32_t character_code) = 0;
    virtual bool try_enqueue(char character) = 0;
};

void on_draw(SfcViewHost& host, void* device_context);

void on_size(SfcViewHost& host, unsigned resize_type, int client_width,
             int client_height);

void begin_drag_resize(SfcViewState& state, SfcViewHost& host,
                       std::uint32_t message_flags, int client_x,
                       int client_y);

void on_mouse_button_release(SfcViewState& state, SfcViewHost& host,
                             std::uint32_t n_flags, int client_x,
                             int client_y);

void on_left_button_down(SfcViewState& state, SfcViewHost& host,
                         std::uint32_t n_flags,
                         int client_x, int client_y,
                         std::uint32_t application_privilege_level);

void on_right_button_down(SfcViewState& state, SfcViewHost& host,
                          std::uint32_t n_flags, int client_x, int client_y);

void queue_text_input(TextInputQueue& queue, std::uint32_t character_code);

void reset_world_scrollbars(SfcViewHost& host, int world_half_width,
                            int world_half_height);

void toggle_infinite_scroll_world_size(SfcViewHost& host,
                                       WorldViewSettings& settings);

void return_viewport_navigation_to_selected_creature(
    SfcViewHost& host,
    WorldViewSettings& settings,
    ViewportNavigationMode& navigation_mode);

void update_world_half_height_command(
    application::CommandUi& command_ui, const WorldViewSettings& settings);

}  // namespace creatures1::ui
