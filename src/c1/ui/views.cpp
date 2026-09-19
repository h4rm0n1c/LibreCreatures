#include "views.hpp"

#include "../objects/object.hpp"

#include <algorithm>
#include <string>

namespace creatures1::ui {
namespace {

constexpr std::uint32_t kMouseKeyStateShift = 0x4u;
constexpr int kSmallWorldHalfWidth = 400;
constexpr int kSmallWorldHalfHeight = 0xfa;
constexpr int kInfiniteWorldHalfWidth = 0x1050;
constexpr int kInfiniteWorldHalfHeight = 0x4b0;
constexpr std::uint32_t kVirtualKeyPeriod = 0x2e;
constexpr std::uint32_t kVirtualKeyZero = 0x30;
constexpr std::uint32_t kVirtualKeyOne = 0x31;
constexpr std::uint32_t kVirtualKeySix = 0x36;
constexpr std::uint32_t kVirtualKeyM = 0x4d;
constexpr std::uint32_t kVirtualKeyT = 0x54;
constexpr std::uint32_t kVirtualKeyX = 0x58;
constexpr std::uint32_t kVirtualKeyAdd = 0x6b;
constexpr std::uint32_t kVirtualKeySubtract = 0x6d;
constexpr std::uint32_t kVirtualKeyEqual = 0xbb;
constexpr std::uint32_t kVirtualKeyMinus = 0xbd;
constexpr std::uint32_t kVirtualKeyControl = 0x11;
constexpr std::uint32_t kVirtualKeyShift = 0x10;
constexpr std::uint32_t kSlowTimerCommand = 900000;
constexpr std::uint32_t kIncreaseTimerCommand = 500000;
constexpr std::uint32_t kDecreaseTimerCommand = 500001;

std::uint32_t flag_value(SfcViewPendingInputFlag flag) {
    return static_cast<std::uint32_t>(flag);
}

}  // namespace

void initialize_view(SfcViewState& state, WorldViewSettings& settings,
                     SfcViewHost& host,
                     std::uint32_t application_privilege_level) {
    host.initialize_view_base();

    state = {};
    state.viewport_navigation_mode =
        ViewportNavigationMode::follow_selected_creature;
    state.sound_enabled = true;
    state.sound_plays_when_unfocused = true;
    state.has_focus = true;

    settings.half_width = kInfiniteWorldHalfWidth;
    settings.half_height = 0x15e;
    settings.application_privilege_level = application_privilege_level;
    settings.smooth_scrolling_enabled = false;
    host.load_view_settings(settings, state);

    if (application_privilege_level > 0) {
        settings.half_width = kInfiniteWorldHalfWidth;
        settings.half_height = kInfiniteWorldHalfHeight;
    }
    host.create_world_renderer(settings.max_view_width,
                               settings.max_view_height,
                               settings.smooth_scrolling_enabled);
}

void shutdown_view(const SfcViewState& state,
                   const WorldViewSettings& settings, SfcViewHost& host) {
    host.persist_view_settings(settings, state);
    host.destroy_world_renderer();
    host.destroy_classifier_tip();
}

void configure_pre_create_window(SfcViewHost& host) {
    host.set_view_window_class();
}

void initialize_view_window(SfcViewHost& host, int world_half_width,
                            int world_half_height) {
    reset_world_scrollbars(host, world_half_width, world_half_height);
    host.create_classifier_tip_window();
}

void on_set_focus(SfcViewState& state, SfcViewHost& host) {
    host.forward_set_focus_message();
    state.has_focus = true;
    if (!state.sound_plays_when_unfocused && state.sound_enabled) {
        host.restore_sound_mixer();
    }
}

void on_kill_focus(SfcViewState& state, SfcViewHost& host) {
    host.forward_kill_focus_message();
    state.has_focus = false;
    if (!state.sound_plays_when_unfocused && state.sound_enabled) {
        host.suspend_sound_mixer();
    }
}

// `sndf fore`: sound follows the window. Dropping the unfocused permission
// while the window is already unfocused silences it now.
void set_sound_foreground_policy(SfcViewState& state, SfcViewHost& host) {
    if (!state.sound_plays_when_unfocused) {
        return;
    }
    state.sound_plays_when_unfocused = false;
    if (state.sound_enabled && !state.has_focus) {
        host.suspend_sound_mixer();
    }
}

// `sndf cons`: sound keeps playing unfocused, so an already-unfocused window
// gets it back.
void set_sound_conservative_policy(SfcViewState& state, SfcViewHost& host) {
    if (state.sound_plays_when_unfocused) {
        return;
    }
    state.sound_plays_when_unfocused = true;
    if (state.sound_enabled && !state.has_focus) {
        host.restore_sound_mixer();
    }
}

void enable_sound_and_restore_if_ready(SfcViewState& state,
                                       SfcViewHost& host) {
    state.sound_enabled = true;
    if (state.sound_plays_when_unfocused || state.has_focus) {
        host.restore_sound_mixer();
    }
}

void disable_sound_and_suspend_if_ready(SfcViewState& state,
                                        SfcViewHost& host) {
    state.sound_enabled = false;
    host.suspend_sound_mixer();
}

void set_viewport_navigation_mode(SfcViewState& state, SfcViewHost& host,
                                  bool manual_mode_requested) {
    if (state.viewport_navigation_mode == ViewportNavigationMode::disabled) {
        return;
    }
    if (manual_mode_requested) {
        if (state.viewport_navigation_mode ==
            ViewportNavigationMode::manual) {
            return;
        }
        state.viewport_navigation_mode = ViewportNavigationMode::manual;
    } else {
        state.viewport_navigation_mode = ViewportNavigationMode::disabled;
    }

    host.reset_renderer_navigation();
    state.manual_navigation_safe_frame_count = 0;
    host.invalidate_main_toolbar();
}

void toggle_follow_selected_creature_mode(SfcViewState& state,
                                          WorldViewSettings& settings,
                                          SfcViewHost& host) {
    if (state.viewport_navigation_mode !=
        ViewportNavigationMode::follow_selected_creature) {
        reset_world_scrollbars(host, settings.half_width, settings.half_height);
        state.viewport_navigation_mode =
            ViewportNavigationMode::follow_selected_creature;
        host.request_renderer_origin_for_selected_creature();
    } else {
        state.viewport_navigation_mode = ViewportNavigationMode::disabled;
        host.reset_renderer_navigation();
        state.manual_navigation_safe_frame_count = 0;
    }
    host.invalidate_main_toolbar();
}

void update_follow_selected_creature_command(
    const SfcViewState& state, const WorldViewSettings& settings,
    SfcViewHost& host, application::CommandUi& command_ui) {
    command_ui.set_checked(
        state.viewport_navigation_mode ==
        ViewportNavigationMode::follow_selected_creature);
    command_ui.set_enabled(host.selected_creature_exists() &&
                           settings.world_update_timer_is_running);
}

namespace {

int requested_scroll_position(std::uint32_t scroll_command,
                              int current_position, int thumb_position,
                              int world_half_extent) {
    switch (scroll_command) {
    case 0:  // SB_LINEUP
        return current_position - 0x10;
    case 1:  // SB_LINEDOWN
        return current_position + 0x10;
    case 2:  // SB_PAGEUP
        return current_position - world_half_extent / 2;
    case 3:  // SB_PAGEDOWN
        return current_position + world_half_extent / 2;
    case 4:  // SB_THUMBPOSITION
    case 5:  // SB_THUMBTRACK
        return thumb_position;
    case 6:  // SB_TOP
        return 0;
    case 7:  // SB_BOTTOM
        return world_half_extent * 2;
    default:
        return current_position;
    }
}

int clamp_scroll_position(int requested, int world_half_extent) {
    return std::clamp(requested, 0, world_half_extent * 2);
}

}  // namespace

void on_horizontal_scroll(SfcViewState& state, WorldViewSettings& settings,
                          SfcViewHost& host, std::uint32_t scroll_command,
                          int thumb_position) {
    const int current_position = host.scroll_position(0);
    set_viewport_navigation_mode(state, host, true);
    const int requested = requested_scroll_position(
        scroll_command, current_position, thumb_position, settings.half_width);
    const bool infinite = settings.half_width != kSmallWorldHalfWidth;
    const int clamped = infinite
                            ? requested
                            : clamp_scroll_position(requested, settings.half_width);
    if (clamped == current_position) {
        return;
    }
    int delta_x = clamped - current_position;
    int delta_y = 0;
    host.scroll_viewport(delta_x, delta_y);
    int position = current_position + delta_x;
    if (infinite) {
        const int range = settings.half_width * 2;
        position = ((position % range) + range) % range;
    }
    host.set_scroll_position(0, position, true);
}

void on_vertical_scroll(SfcViewState& state, WorldViewSettings& settings,
                        SfcViewHost& host, std::uint32_t scroll_command,
                        int thumb_position) {
    const int current_position = host.scroll_position(1);
    set_viewport_navigation_mode(state, host, true);
    const int requested = requested_scroll_position(
        scroll_command, current_position, thumb_position, settings.half_height);
    const int clamped = clamp_scroll_position(requested, settings.half_height);
    if (clamped == current_position) {
        return;
    }
    int delta_x = 0;
    int delta_y = clamped - current_position;
    host.scroll_viewport(delta_x, delta_y);
    host.set_scroll_position(1, current_position + delta_y, true);
}

void update_keyboard_scroll(SfcViewState& state,
                            const WorldViewSettings& settings,
                            SfcViewHost& host) {
    if (!host.main_frame_is_active()) {
        return;
    }

    int acceleration = 0x28;
    int velocity_limit = 100;
    int key_step = 0x14;
    if (host.key_is_down(0x10)) {  // VK_SHIFT
        acceleration = 0x50;
        velocity_limit = 200;
        key_step = 0x3c;
    }

    auto accelerate_axis = [](int& velocity, bool negative_key,
                              bool positive_key, int step, int acceleration) {
        if (negative_key) {
            velocity -= step;
        } else if (positive_key) {
            velocity += step;
        } else if (velocity < 0) {
            velocity += acceleration;
            if (velocity > 0) {
                velocity = 0;
            }
        } else if (velocity > 0) {
            velocity -= acceleration;
            if (velocity < 0) {
                velocity = 0;
            }
        }
    };

    accelerate_axis(state.keyboard_scroll_velocity_x,
                    host.key_is_down(0x25), host.key_is_down(0x27),
                    key_step, acceleration);
    accelerate_axis(state.keyboard_scroll_velocity_y,
                    host.key_is_down(0x26), host.key_is_down(0x28),
                    key_step, acceleration);

    state.keyboard_scroll_velocity_x = std::clamp(
        state.keyboard_scroll_velocity_x, -velocity_limit, velocity_limit);
    state.keyboard_scroll_velocity_y = std::clamp(
        state.keyboard_scroll_velocity_y, -velocity_limit, velocity_limit);

    int horizontal_position = host.scroll_position(0);
    const int vertical_position = host.scroll_position(1);
    int delta_x = state.keyboard_scroll_velocity_x;
    int delta_y = state.keyboard_scroll_velocity_y;

    if (settings.half_width == kSmallWorldHalfWidth) {
        const int proposed = horizontal_position + delta_x;
        if (proposed < 0) {
            delta_x = -horizontal_position;
        } else if (proposed > settings.half_width * 2 - 1) {
            delta_x = settings.half_width * 2 - horizontal_position;
        }
        if (delta_x != state.keyboard_scroll_velocity_x) {
            state.keyboard_scroll_velocity_x = 0;
        }
    }

    const int proposed_y = vertical_position + delta_y;
    if (proposed_y < 0) {
        delta_y = -vertical_position;
        state.keyboard_scroll_velocity_y = 0;
    } else if (proposed_y > settings.half_height * 2) {
        delta_y = settings.half_height * 2 - vertical_position;
        state.keyboard_scroll_velocity_y = 0;
    }

    if (delta_x == 0 && delta_y == 0) {
        return;
    }

    set_viewport_navigation_mode(state, host, true);
    host.scroll_viewport(delta_x, delta_y);
    horizontal_position += delta_x;
    int new_vertical_position = vertical_position + delta_y;

    if (settings.half_width != kSmallWorldHalfWidth) {
        const int world_width = settings.half_width * 2;
        if (horizontal_position > world_width) {
            horizontal_position -= world_width;
        } else if (horizontal_position < 0) {
            horizontal_position += world_width;
        }
    }
    if (delta_x == 0) {
        state.keyboard_scroll_velocity_x = 0;
    }
    if (delta_y == 0) {
        state.keyboard_scroll_velocity_y = 0;
    }
    host.set_scroll_position(0, horizontal_position, true);
    host.set_scroll_position(1, new_vertical_position, true);
}

void on_key_down(SfcViewState& state, SfcViewHost& host,
                 std::uint32_t virtual_key, std::uint32_t repeat_count,
                 std::uint32_t key_flags) {
    const bool control_down = host.key_is_down(kVirtualKeyControl);
    const bool shift_down = host.key_is_down(kVirtualKeyShift);

    switch (virtual_key) {
    case kVirtualKeyPeriod:
        if (control_down) {
            host.open_or_activate_system_information();
            return;
        }
        break;

    case kVirtualKeyZero:
        if (control_down) {
            host.configure_world_update_timer(kSlowTimerCommand);
            return;
        }
        break;

    case kVirtualKeyOne:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case kVirtualKeySix:
        if (control_down) {
            // The native handler passes the key-derived slot directly.  The
            // document's favourite-place storage uses this same one-based
            // command slot; the host owns its concrete record layout.
            const std::size_t favourite_slot =
                static_cast<std::size_t>(virtual_key - kVirtualKeyZero);
            if (!host.has_favourite_place(favourite_slot)) {
                return;
            }
            set_viewport_navigation_mode(state, host, true);
            host.request_renderer_origin_for_favourite_place(favourite_slot);
            return;
        }
        break;

    case kVirtualKeyM:
        if (control_down && shift_down) {
            host.generate_profiler_report();
            return;
        }
        break;

    case kVirtualKeyT:
        if (control_down && shift_down) {
            const bool was_visible = state.show_classifiers;
            state.show_classifiers = !was_visible;
            if (was_visible) {
                host.hide_classifier_tip();
            }
            return;
        }
        break;

    case kVirtualKeyX:
        if (control_down && shift_down) {
            const bool was_visible = state.show_coordinates;
            state.show_coordinates = !was_visible;
            if (was_visible) {
                host.clear_coordinate_status();
            }
            return;
        }
        break;

    case kVirtualKeyAdd:
    case kVirtualKeyEqual:
        if (control_down) {
            host.configure_world_update_timer(kDecreaseTimerCommand);
            return;
        }
        break;

    case kVirtualKeySubtract:
    case kVirtualKeyMinus:
        if (control_down) {
            host.configure_world_update_timer(kIncreaseTimerCommand);
            return;
        }
        break;
    }

    host.forward_default_key_down(virtual_key, repeat_count, key_flags);
}

namespace {

objects::Object* find_classifier_target(SfcViewHost& host, int world_x,
                                        int world_y) {
    const std::size_t non_scenery_count = host.non_scenery_object_count();
    for (std::size_t index = 0; index < non_scenery_count; ++index) {
        objects::Object* object = host.non_scenery_object_at(index);
        if (object == nullptr) {
            host.report_invalid_non_scenery_object_index();
            continue;
        }
        if (object == host.pointer_tool()) {
            continue;
        }
        world::WorldRect bounds{};
        object->get_bounds(&bounds);
        if (host.point_in_world_rect(bounds, world_x, world_y)) {
            return object;
        }
    }

    const std::size_t scenery_count = host.scenery_object_count();
    for (std::size_t index = 0; index < scenery_count; ++index) {
        objects::Object* object = host.scenery_object_at(index);
        if (object == nullptr) {
            host.report_invalid_scenery_object_index();
            continue;
        }
        world::WorldRect bounds{};
        object->get_bounds(&bounds);
        if (host.point_in_world_rect(bounds, world_x, world_y)) {
            return object;
        }
    }
    return nullptr;
}

}  // namespace

void on_mouse_move(SfcViewState& state, const WorldViewSettings& settings,
                   SfcViewHost& host, std::uint32_t n_flags, int client_x,
                   int client_y) {
    (void)n_flags;
    if (!settings.world_update_timer_is_running) {
        host.set_default_arrow_cursor();
    }

    state.mouse_client_x = client_x;
    state.mouse_client_y = client_y;

    if (state.show_coordinates) {
        host.set_coordinate_status("X: " + std::to_string(client_x) +
                                   "  Y: " + std::to_string(client_y));
    }

    if (state.show_classifiers) {
        int world_x = host.viewport_left() + client_x;
        const int world_y = host.viewport_top() + client_y;
        if (world_x >= world::kWorldWidth) {
            world_x -= world::kWorldWidth;
        }

        objects::Object* object =
            find_classifier_target(host, world_x, world_y);
        if (object == nullptr) {
            host.hide_classifier_tip();
            host.clear_classifier_tip_text();
        } else {
            const std::uint32_t packed_classifier = object->classifier_base();
            const brain::ClassifierId classifier{
                static_cast<int>((packed_classifier >> 24) & 0xffu),
                static_cast<int>((packed_classifier >> 16) & 0xffu),
                static_cast<int>((packed_classifier >> 8) & 0xffu)};
            std::string tip_text =
                std::to_string(classifier.family) + ", " +
                std::to_string(classifier.genus) + ", " +
                std::to_string(classifier.species);
            const std::string name = host.classifier_name(classifier);
            if (!name.empty()) {
                tip_text += " - ";
                tip_text += name;
            }
            host.update_classifier_tip(tip_text, client_x, client_y);
        }
    }

    if (state.viewport_navigation_active) {
        const int current_x = host.scroll_position(0);
        const int current_y = host.scroll_position(1);
        int delta_x = state.viewport_navigation_target_client_x - client_x;
        int delta_y = state.viewport_navigation_target_client_y - client_y;

        if (settings.half_width == kSmallWorldHalfWidth) {
            const int proposed_x = current_x + delta_x;
            if (proposed_x < 0) {
                delta_x = -current_x;
            } else if (proposed_x > settings.half_width * 2) {
                delta_x = settings.half_width * 2 - current_x;
            }
        }
        if (settings.half_height == kSmallWorldHalfHeight) {
            const int proposed_y = current_y + delta_y;
            if (proposed_y < 0) {
                delta_y = -current_y;
            } else if (proposed_y > settings.half_height * 2) {
                delta_y = settings.half_height * 2 - current_y;
            }
        }

        if (delta_x != 0 || delta_y != 0) {
            set_viewport_navigation_mode(state, host, true);
            host.scroll_viewport(delta_x, delta_y);

            int new_x = current_x + delta_x;
            const int new_y = current_y + delta_y;
            if (settings.half_width != kSmallWorldHalfWidth) {
                const int world_width = settings.half_width * 2;
                if (new_x > world_width) {
                    new_x -= world_width;
                } else if (new_x < 0) {
                    new_x += world_width;
                }
            }
            host.set_scroll_position(0, new_x, true);
            host.set_scroll_position(1, new_y, true);
        }

        host.update_pointer_tool_unbounded_position_and_redraw();
        state.viewport_navigation_target_client_x = client_x;
        state.viewport_navigation_target_client_y = client_y;
    }
}

void on_draw(SfcViewHost& host, void* device_context) {
    host.fill_client_background_black(device_context);
    host.mark_renderer_full_redraw();
    host.present_renderer_view(device_context);
}

void on_size(SfcViewHost& host, unsigned resize_type, int client_width,
             int client_height) {
    host.forward_default_size(resize_type, client_width, client_height);
    host.resize_renderer_for_viewport(client_width, client_height);
}

void begin_drag_resize(SfcViewState& state, SfcViewHost& host,
                       std::uint32_t message_flags, int client_x,
                       int client_y) {
    state.viewport_navigation_active = true;
    state.drag_start_x = client_x;
    state.drag_start_y = client_y;
    host.capture_mouse_for_drag();
    host.materialize_previous_capture_window();
    host.forward_default_mouse_message(message_flags, client_x, client_y);
}

void on_mouse_button_release(SfcViewState& state, SfcViewHost& host,
                             std::uint32_t n_flags, int client_x,
                             int client_y) {
    state.viewport_navigation_active = false;
    host.release_mouse_capture();
    host.forward_default_mouse_message(n_flags, client_x, client_y);
}

void on_left_button_down(SfcViewState& state, SfcViewHost& host,
                         std::uint32_t n_flags,
                         int client_x, int client_y,
                         std::uint32_t application_privilege_level) {
    host.forward_default_mouse_message(n_flags, client_x, client_y);
    state.pending_input_flags |= flag_value(SfcViewPendingInputFlag::left_button);
    if ((n_flags & kMouseKeyStateShift) != 0 &&
        application_privilege_level > 1) {
        state.pending_input_flags |=
            flag_value(SfcViewPendingInputFlag::left_with_shift);
    }
}

void on_right_button_down(SfcViewState& state, SfcViewHost& host,
                          std::uint32_t n_flags, int client_x,
                          int client_y) {
    host.forward_default_mouse_message(n_flags, client_x, client_y);
    state.pending_input_flags |=
        flag_value(SfcViewPendingInputFlag::right_button);
    if ((n_flags & kMouseKeyStateShift) != 0) {
        state.pending_input_flags |=
            flag_value(SfcViewPendingInputFlag::right_with_shift);
    }
}

void queue_text_input(TextInputQueue& queue, std::uint32_t character_code) {
    if (queue.control_key_is_down()) {
        return;
    }
    queue.forward_default_character_message(character_code);
    (void)queue.try_enqueue(static_cast<char>(character_code));
}

void reset_world_scrollbars(SfcViewHost& host, int world_half_width,
                            int world_half_height) {
    host.set_scroll_range(0, 0, world_half_width * 2, false);
    host.set_scroll_range(1, 0, world_half_height * 2, false);
    host.set_scroll_position(0, world_half_width, true);
    host.set_scroll_position(1, world_half_height, true);
}

void toggle_infinite_scroll_world_size(SfcViewHost& host,
                                       WorldViewSettings& settings) {
    if (settings.half_height == kSmallWorldHalfHeight) {
        settings.half_width = kInfiniteWorldHalfWidth;
        settings.half_height = kInfiniteWorldHalfHeight;
    } else {
        settings.half_width = kSmallWorldHalfWidth;
        settings.half_height = kSmallWorldHalfHeight;
    }
    reset_world_scrollbars(host, settings.half_width, settings.half_height);
}

void return_viewport_navigation_to_selected_creature(
    SfcViewHost& host,
    WorldViewSettings& settings,
    ViewportNavigationMode& navigation_mode) {
    if (navigation_mode != ViewportNavigationMode::disabled) {
        reset_world_scrollbars(host, settings.half_width,
                               settings.half_height);
        navigation_mode = ViewportNavigationMode::follow_selected_creature;
        host.request_renderer_origin_for_selected_creature();
    }
    host.invalidate_main_toolbar();
}

void update_world_half_height_command(
    application::CommandUi& command_ui, const WorldViewSettings& settings) {
    command_ui.set_checked(settings.half_height == kInfiniteWorldHalfHeight);
    command_ui.set_enabled(settings.world_update_timer_is_running);
}

}  // namespace creatures1::ui
