#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace creatures1::ui {

enum class CaosControl {
    output,
    command_input,
    always_on_top,
    send,
    clear,
    load,
};

struct CaosRect {
    long left = 0;
    long top = 0;
    long right = 0;
    long bottom = 0;
};

struct CaosConsoleState {
    std::string command_text;
    std::vector<std::string> command_history;
    std::size_t history_selected_index = 0;
    bool always_on_top = false;

    CaosRect client_rect;
    long send_button_width = 0;
    long send_button_height = 0;
    long load_button_width = 0;
    long always_on_top_width = 0;
    long always_on_top_height = 0;
    long command_input_height = 0;
    long dialog_margin = 0;
    long input_top_offset = 0;
    long always_on_top_top_offset = 0;
    unsigned layout_mode = 4;
};

struct CaosKeyMessage {
    std::uint32_t message = 0;
    std::uint32_t key = 0;
    bool control_down = false;
    bool shift_down = false;
    bool targets_command_input = false;
};

struct CaosExecutionResult {
    bool succeeded = false;
    std::string output;
};

class CaosScriptLoadHost {
public:
    virtual ~CaosScriptLoadHost() = default;

    virtual bool choose_script_file(std::string& selected_path) = 0;
    virtual bool read_script_lines(std::string_view selected_path,
                                   std::vector<std::string>& lines) = 0;
    virtual void report_script_open_failure() = 0;
    virtual void set_script_editor_text(std::string_view text) = 0;
    virtual void focus_script_editor() = 0;
    virtual void select_all_script_editor_text() = 0;
};

class CaosConsoleHost {
public:
    virtual ~CaosConsoleHost() = default;

    virtual void initialise_base_dialog(CaosConsoleState& state) = 0;
    virtual CaosRect client_rect() const = 0;
    virtual bool control_exists(CaosControl control) const = 0;
    virtual CaosRect control_screen_rect(CaosControl control) const = 0;
    virtual CaosRect screen_to_client(CaosRect rect) const = 0;
    virtual CaosRect dialog_screen_rect() const = 0;
    virtual bool main_window_client_rect_in_screen(CaosRect& rect) const = 0;
    virtual void position_dialog(long x, long y) = 0;
    virtual void set_always_on_top_checked(bool checked) = 0;
    virtual void apply_always_on_top(bool enabled) = 0;
    virtual void focus_command_input() = 0;

    virtual std::string command_input_text() const = 0;
    virtual void echo_submitted_command(std::string_view command) = 0;
    virtual CaosExecutionResult execute_command(std::string_view command) = 0;

    virtual void set_control_text(CaosControl control, std::string_view text) = 0;
    virtual std::size_t output_text_length() const = 0;
    virtual std::string output_text() const = 0;
    virtual void select_output(std::size_t start, std::size_t end) = 0;
    virtual void replace_output_selection(std::string_view text) = 0;
    virtual void move_output_caret_to_end() = 0;

    virtual void set_control_position(CaosControl control, long x, long y,
                                      long width, long height,
                                      unsigned flags) = 0;
    virtual void invalidate_dialog() = 0;
    virtual void set_minimum_tracking_size(long width, long height) = 0;
    virtual void default_window_message() = 0;
    virtual void handle_console_shortcut() = 0;
    virtual bool default_pre_translate(const CaosKeyMessage& message) = 0;
};

void initialise_caos_console(CaosConsoleState& state, CaosConsoleHost& host);
void clear_caos_command_input(CaosConsoleHost& host);
void clear_caos_output(CaosConsoleHost& host);
void toggle_caos_always_on_top(CaosConsoleState& state, CaosConsoleHost& host);
void resize_caos_console(CaosConsoleState& state, CaosConsoleHost& host,
                         unsigned size_type, long client_width,
                         long client_height);
void apply_caos_minimum_size(CaosConsoleHost& host);
void append_caos_output(CaosConsoleHost& host, std::string_view text);
void submit_caos_command(CaosConsoleState& state, CaosConsoleHost& host);
void execute_caos_command(CaosConsoleHost& host, std::string_view command);
bool translate_caos_key(CaosConsoleState& state, CaosConsoleHost& host,
                        const CaosKeyMessage& message);
void load_caos_script_from_file(CaosScriptLoadHost& host);

}  // namespace creatures1::ui
