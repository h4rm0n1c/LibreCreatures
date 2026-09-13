#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::ui {

struct DebugConsoleState {
    std::string filter_text;
    std::string accumulated_log_text;
    bool filter_enabled = false;
    bool world_update_paused = false;
    bool always_on_top = true;
    bool log_dirty = true;
};

// MFC/USER32 controls, document scheduling, clipboard handling, and stream
// lifetime are supplied by this host. The dialog code below owns the C1
// policy and ordering, not the native control or CRT object layouts.
class DebugConsoleHost {
public:
    virtual ~DebugConsoleHost() = default;

    virtual void initialise_base_dialog() = 0;
    virtual void set_checkbox_checked(std::uint32_t control_id,
                                      bool checked) = 0;
    virtual void set_dialog_always_on_top(bool enabled) = 0;
    virtual void update_dialog_data(bool save_and_validate) = 0;

    // MFC DDX control binding and value exchange are supplied by the dialog
    // host.  The clean source retains the recovered control order and the two
    // application-owned value bindings without reproducing CDataExchange.
    virtual void bind_control(std::uint32_t control_id) = 0;
    virtual void exchange_log_output_text(std::string& value,
                                          bool save_and_validate) = 0;
    virtual void exchange_world_update_paused(bool& value,
                                              bool save_and_validate) = 0;

    virtual void set_log_output_text(std::string_view text) = 0;
    virtual void move_log_output_caret_to_end() = 0;
    virtual bool pause_checkbox_checked() const = 0;
    virtual void service_world_update_timer() = 0;
    virtual void set_world_update_control_enabled(bool enabled) = 0;

    virtual void arm_world_update_timer() = 0;
    virtual bool debug_log_category_enabled(std::uint32_t category) const = 0;
    virtual void disable_debug_log_category(std::uint32_t category) = 0;

    virtual std::string read_filter_control_text() const = 0;
    virtual void set_filter_control_enabled(bool enabled) = 0;
    virtual std::size_t first_visible_log_line() const = 0;
    virtual std::size_t log_line_start_offset(std::size_t line) const = 0;
    virtual void select_log_output(std::size_t start,
                                   std::size_t end) = 0;
    virtual void scroll_log_output_caret() = 0;
    virtual void copy_log_output_selection() = 0;

    virtual bool log_file_is_open() const = 0;
    virtual void open_log_file(std::string_view path) = 0;
    virtual void close_log_file() = 0;
    virtual void set_mirror_button_checked(bool checked) = 0;
    virtual void set_dialog_title(std::string_view title) = 0;
    virtual void start_log_flush_timer(std::uint32_t interval_ms) = 0;
    virtual void stop_log_flush_timer() = 0;
    virtual void flash_dialog() = 0;
};

void initialise_debug_console(DebugConsoleState& state,
                              DebugConsoleHost& host);
void exchange_debug_console_data(DebugConsoleState& state,
                                 DebugConsoleHost& host,
                                 bool save_and_validate);
void refresh_debug_log(DebugConsoleState& state,
                       DebugConsoleHost& host);
void clear_debug_log(DebugConsoleState& state,
                     DebugConsoleHost& host);
void pause_world_update(DebugConsoleHost& host);
void toggle_world_update_pause(DebugConsoleState& state,
                               DebugConsoleHost& host);
void copy_filter_control_text(DebugConsoleState& state,
                              DebugConsoleHost& host);
void toggle_filter_enabled(DebugConsoleState& state,
                           DebugConsoleHost& host);
void copy_all_debug_log(DebugConsoleHost& host);
void copy_visible_debug_log(DebugConsoleHost& host);
void toggle_log_file_mirroring(DebugConsoleHost& host);

} // namespace creatures1::ui
