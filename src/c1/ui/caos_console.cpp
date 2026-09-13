#include "caos_console.hpp"

#include <algorithm>

namespace creatures1::ui {
namespace {

constexpr unsigned kMoveWindowNoActivate = 4;
constexpr unsigned kMoveWindowNoSize = 5;
constexpr unsigned kMinimumWidth = 200;
constexpr unsigned kMinimumHeight = 100;
constexpr std::size_t kMaximumOutputCharacters = 100000;
constexpr std::size_t kRetainedOutputCharacters = 50000;
constexpr std::uint32_t kKeyDownMessage = 0x100;
constexpr std::uint32_t kCopyShortcut = 0x43;
constexpr std::uint32_t kUpKey = 0x26;
constexpr std::uint32_t kDownKey = 0x28;
constexpr unsigned kMinimizedSizeType = 1;
constexpr std::size_t kMaximumCommandHistory = 50;

long width(const CaosRect& rect) {
    return rect.right - rect.left;
}

long height(const CaosRect& rect) {
    return rect.bottom - rect.top;
}

void trim_ascii_whitespace(std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string::npos) {
        text.clear();
        return;
    }
    const auto last = text.find_last_not_of(" \t\r\n\f\v");
    text.erase(last + 1);
    text.erase(0, first);
}

std::string normalize_caos_output(std::string output) {
    std::string normalized;
    normalized.reserve(output.size() + 2);
    for (std::size_t index = 0; index < output.size(); ++index) {
        if (output[index] == '\n' &&
            (index == 0 || output[index - 1] != '\r')) {
            normalized += '\r';
        }
        normalized += output[index];
    }
    for (std::size_t index = 0; index + 2 < normalized.size();) {
        if (normalized.compare(index, 3, "\r\r\n") == 0) {
            normalized.erase(index, 1);
            continue;
        }
        ++index;
    }
    return normalized;
}

}  // namespace

void initialise_caos_console(CaosConsoleState& state, CaosConsoleHost& host) {
    host.initialise_base_dialog(state);
    state.client_rect = host.client_rect();

    if (host.control_exists(CaosControl::send)) {
        const CaosRect rect = host.control_screen_rect(CaosControl::send);
        state.send_button_width = width(rect);
        state.send_button_height = height(rect);
    }
    if (host.control_exists(CaosControl::load)) {
        state.load_button_width =
            width(host.control_screen_rect(CaosControl::load));
    }

    const CaosRect topmost_rect =
        host.control_screen_rect(CaosControl::always_on_top);
    state.always_on_top_width = width(topmost_rect);
    state.always_on_top_height = height(topmost_rect);

    CaosRect input_rect =
        host.screen_to_client(host.control_screen_rect(CaosControl::command_input));
    state.command_input_height = height(input_rect);
    state.input_top_offset = state.client_rect.bottom - input_rect.top;

    CaosRect topmost_client_rect = host.screen_to_client(topmost_rect);
    state.always_on_top_top_offset =
        state.client_rect.bottom - topmost_client_rect.top;

    const CaosRect output_rect =
        host.screen_to_client(host.control_screen_rect(CaosControl::output));
    state.dialog_margin = output_rect.left;
    state.layout_mode = 4;

    CaosRect main_window_rect;
    if (host.main_window_client_rect_in_screen(main_window_rect)) {
        const CaosRect dialog_rect = host.dialog_screen_rect();
        host.position_dialog(main_window_rect.left,
                             dialog_rect.top - dialog_rect.bottom +
                                 main_window_rect.bottom);
    }

    state.always_on_top = false;
    host.set_always_on_top_checked(false);
    host.focus_command_input();
}

void clear_caos_command_input(CaosConsoleHost& host) {
    host.set_control_text(CaosControl::command_input, {});
}

void clear_caos_output(CaosConsoleHost& host) {
    host.set_control_text(CaosControl::output, {});
}

void toggle_caos_always_on_top(CaosConsoleState& state, CaosConsoleHost& host) {
    state.always_on_top = !state.always_on_top;
    host.set_always_on_top_checked(state.always_on_top);
    host.apply_always_on_top(state.always_on_top);
}

void resize_caos_console(CaosConsoleState& state, CaosConsoleHost& host,
                         unsigned size_type, long client_width,
                         long client_height) {
    host.default_window_message();
    if (size_type == kMinimizedSizeType ||
        !host.control_exists(CaosControl::output)) {
        return;
    }

    const long topmost_y = client_height - state.always_on_top_top_offset;
    const long input_y = client_height - state.input_top_offset;
    const long send_x = client_width - state.send_button_width - state.dialog_margin;

    host.set_control_position(
        CaosControl::output, state.dialog_margin, state.dialog_margin,
        client_width - state.dialog_margin * 2,
        input_y - state.dialog_margin * 2, kMoveWindowNoActivate);
    host.set_control_position(
        CaosControl::command_input, state.dialog_margin, input_y,
        client_width - state.dialog_margin * 3 - state.send_button_width,
        state.command_input_height, kMoveWindowNoActivate);
    if (host.control_exists(CaosControl::send)) {
        host.set_control_position(CaosControl::send, send_x, input_y,
                                  state.send_button_width,
                                  state.send_button_height,
                                  kMoveWindowNoActivate);
    }
    host.set_control_position(CaosControl::always_on_top, state.dialog_margin,
                              topmost_y, state.always_on_top_width,
                              state.always_on_top_height, kMoveWindowNoActivate);
    if (host.control_exists(CaosControl::clear)) {
        host.set_control_position(CaosControl::clear, send_x, topmost_y,
                                  state.send_button_width,
                                  state.send_button_height,
                                  kMoveWindowNoActivate);
    }
    if (host.control_exists(CaosControl::load)) {
        const long load_x = client_width - state.layout_mode -
                            state.load_button_width - state.send_button_width -
                            state.dialog_margin;
        host.set_control_position(CaosControl::load, load_x, topmost_y,
                                  state.load_button_width,
                                  state.send_button_height,
                                  kMoveWindowNoActivate);
    }
    host.invalidate_dialog();
}

void apply_caos_minimum_size(CaosConsoleHost& host) {
    host.set_minimum_tracking_size(kMinimumWidth, kMinimumHeight);
    host.default_window_message();
}

void append_caos_output(CaosConsoleHost& host, std::string_view text) {
    const std::size_t current_length = host.output_text_length();
    host.select_output(current_length, current_length);
    host.replace_output_selection(text);

    if (host.output_text_length() > kMaximumOutputCharacters) {
        std::string output = host.output_text();
        if (output.size() > kRetainedOutputCharacters) {
            output.erase(0, output.size() - kRetainedOutputCharacters);
        }
        host.set_control_text(CaosControl::output, output);
    }
    host.move_output_caret_to_end();
}

void execute_caos_command(CaosConsoleHost& host, std::string_view command) {
    const CaosExecutionResult result = host.execute_command(command);
    if (!result.succeeded) {
        append_caos_output(host, "Error: Failed to execute command\r\n");
        return;
    }

    if (result.output.empty()) {
        append_caos_output(host, "OK\r\n");
        return;
    }

    std::string output = normalize_caos_output(result.output);
    append_caos_output(host, output);
    if (output.size() < 2 || output.compare(output.size() - 2, 2, "\r\n") != 0) {
        append_caos_output(host, "\r\n");
    }
}

void load_caos_script_from_file(CaosScriptLoadHost& host) {
    std::string selected_path;
    if (!host.choose_script_file(selected_path)) {
        return;
    }

    std::vector<std::string> lines;
    if (!host.read_script_lines(selected_path, lines)) {
        host.report_script_open_failure();
        return;
    }

    std::string script;
    for (std::string& line : lines) {
        trim_ascii_whitespace(line);
        if (line.empty() || line.front() == '*') {
            continue;
        }
        if (!script.empty()) {
            script.push_back(' ');
        }
        script += line;
    }

    host.set_script_editor_text(script);
    host.focus_script_editor();
    host.select_all_script_editor_text();
}

void submit_caos_command(CaosConsoleState& state, CaosConsoleHost& host) {
    state.command_text = host.command_input_text();
    trim_ascii_whitespace(state.command_text);
    if (state.command_text.empty()) {
        return;
    }

    if (state.command_history.empty() ||
        state.command_history.back() != state.command_text) {
        state.command_history.push_back(state.command_text);
        while (state.command_history.size() > kMaximumCommandHistory) {
            state.command_history.erase(state.command_history.begin());
        }
    }
    state.history_selected_index = state.command_history.size();
    host.echo_submitted_command(state.command_text);
    execute_caos_command(host, state.command_text);
    state.command_text.clear();
    host.set_control_text(CaosControl::command_input, {});
    host.focus_command_input();
}

bool translate_caos_key(CaosConsoleState& state, CaosConsoleHost& host,
                        const CaosKeyMessage& message) {
    if (message.message == kKeyDownMessage && message.key == kCopyShortcut &&
        message.control_down && message.shift_down) {
        host.handle_console_shortcut();
        return true;
    }

    if (!message.targets_command_input) {
        return host.default_pre_translate(message);
    }

    if (message.key == kUpKey && !state.command_history.empty() &&
        state.history_selected_index > 0) {
        --state.history_selected_index;
        host.set_control_text(
            CaosControl::command_input,
            state.command_history[state.history_selected_index]);
        return true;
    }

    if (message.key == kDownKey) {
        const std::size_t history_count = state.command_history.size();
        if (state.history_selected_index + 1 < history_count) {
            ++state.history_selected_index;
            host.set_control_text(
                CaosControl::command_input,
                state.command_history[state.history_selected_index]);
        } else if (state.history_selected_index + 1 == history_count) {
            state.history_selected_index = history_count;
            host.set_control_text(CaosControl::command_input, {});
        }
        return true;
    }

    return host.default_pre_translate(message);
}

}  // namespace creatures1::ui
