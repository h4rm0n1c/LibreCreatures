#include "debug_console.hpp"

namespace creatures1::ui {
namespace {

constexpr std::uint32_t kPauseCheckBoxId = 0x3fe;
constexpr std::uint32_t kFilterCheckBoxId = 0x405;
constexpr std::uint32_t kTopmostCheckBoxId = 0x404;
constexpr std::uint32_t kDebugLogCategory = 1;
constexpr std::uint32_t kLogFlushIntervalMs = 500;

void publish_log_output(DebugConsoleState& state,
                        DebugConsoleHost& host) {
    host.set_log_output_text(state.accumulated_log_text);
    host.move_log_output_caret_to_end();
    if (host.pause_checkbox_checked()) {
        host.service_world_update_timer();
        host.set_world_update_control_enabled(true);
    }
}

} // namespace

void initialise_debug_console(DebugConsoleState& state,
                              DebugConsoleHost& host) {
    host.initialise_base_dialog();
    state.world_update_paused = false;
    host.set_checkbox_checked(kPauseCheckBoxId, false);

    state.filter_text.clear();
    state.log_dirty = true;
    state.filter_enabled = false;
    host.set_checkbox_checked(kFilterCheckBoxId, false);

    state.always_on_top = true;
    host.set_checkbox_checked(kTopmostCheckBoxId, true);
    host.set_dialog_always_on_top(state.always_on_top);
    host.update_dialog_data(false);
}

void exchange_debug_console_data(DebugConsoleState& state,
                                 DebugConsoleHost& host,
                                 bool save_and_validate) {
    host.bind_control(0x409);
    host.bind_control(0x404);
    host.bind_control(0x405);
    host.bind_control(0x401);
    host.bind_control(0x3fe);
    host.bind_control(0x3ff);
    host.bind_control(0x3fc);
    host.bind_control(0x3f7);
    host.exchange_log_output_text(state.accumulated_log_text,
                                  save_and_validate);
    host.exchange_world_update_paused(state.world_update_paused,
                                      save_and_validate);
}

void refresh_debug_log(DebugConsoleState& state,
                       DebugConsoleHost& host) {
    if (!state.log_dirty) {
        return;
    }

    publish_log_output(state, host);
    state.log_dirty = false;
}

void clear_debug_log(DebugConsoleState& state,
                     DebugConsoleHost& host) {
    state.accumulated_log_text.clear();
    state.log_dirty = true;
    publish_log_output(state, host);
    state.log_dirty = false;
}

void pause_world_update(DebugConsoleHost& host) {
    host.set_world_update_control_enabled(false);
    host.arm_world_update_timer();
}

void toggle_world_update_pause(DebugConsoleState& state,
                               DebugConsoleHost& host) {
    const bool was_paused = state.world_update_paused;
    state.world_update_paused = !was_paused;
    if (was_paused) {
        host.set_world_update_control_enabled(false);
        host.arm_world_update_timer();
        return;
    }

    if (host.debug_log_category_enabled(kDebugLogCategory)) {
        host.disable_debug_log_category(kDebugLogCategory);
        return;
    }

    host.set_world_update_control_enabled(true);
    host.service_world_update_timer();
}

void copy_filter_control_text(DebugConsoleState& state,
                              DebugConsoleHost& host) {
    state.filter_text = host.read_filter_control_text();
}

void toggle_filter_enabled(DebugConsoleState& state,
                           DebugConsoleHost& host) {
    state.filter_enabled = !state.filter_enabled;
    host.set_filter_control_enabled(state.filter_enabled);
}

void copy_all_debug_log(DebugConsoleHost& host) {
    host.select_log_output(0, static_cast<std::size_t>(-1));
    host.scroll_log_output_caret();
    host.copy_log_output_selection();
}

void copy_visible_debug_log(DebugConsoleHost& host) {
    const auto first_line = host.first_visible_log_line();
    const auto start = host.log_line_start_offset(first_line);
    host.select_log_output(start, static_cast<std::size_t>(-1));
    host.scroll_log_output_caret();
    host.copy_log_output_selection();
}

void toggle_log_file_mirroring(DebugConsoleHost& host) {
    if (!host.log_file_is_open()) {
        host.open_log_file("Log.txt");
        host.set_mirror_button_checked(true);
        host.set_dialog_title("Log information - saving to disk");
        host.start_log_flush_timer(kLogFlushIntervalMs);
        return;
    }

    host.close_log_file();
    host.set_mirror_button_checked(false);
    host.set_dialog_title("Log information");
    host.stop_log_flush_timer();
    host.flash_dialog();
}

} // namespace creatures1::ui
