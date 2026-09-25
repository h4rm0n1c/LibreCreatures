#include "windows_pipe_server_boundary.hpp"

#include <utility>

namespace creatures1::platform {

WindowsPipeServerBoundary::WindowsPipeServerBoundary(
    HWND main_window, MacroHolderFactory macro_holder_factory,
    SelectedCreatureQuery selected_creature_query, KitShutdown kit_shutdown)
    : main_window_(main_window),
      macro_holder_factory_(std::move(macro_holder_factory)),
      selected_creature_query_(std::move(selected_creature_query)),
      kit_shutdown_(std::move(kit_shutdown)) {}

WindowsPipeServerBoundary::~WindowsPipeServerBoundary() {
    stop();
}

bool WindowsPipeServerBoundary::start() {
    if (runtime_ != nullptr) {
        return runtime_->running();
    }
    if (!WindowsPipeServerRuntime::running_under_wine()) {
        return true;
    }

    runtime_ = std::make_unique<WindowsPipeServerRuntime>(
        main_window_, std::move(macro_holder_factory_),
        std::move(selected_creature_query_), std::move(kit_shutdown_));
    if (!runtime_->start()) {
        runtime_.reset();
        return false;
    }
    return true;
}

void WindowsPipeServerBoundary::stop() {
    if (runtime_ == nullptr) {
        return;
    }
    runtime_->stop();
    runtime_.reset();
}

bool WindowsPipeServerBoundary::running() const {
    return runtime_ != nullptr && runtime_->running();
}

namespace {

// Opt-in record of every kit request and the game's answer, for comparing
// kits against each other: set C1_KIT_TRAFFIC_LOG to a file path.  Field
// separators (0x1E) are written as '|'.
void log_kit_traffic(std::string_view request, const std::string& response) {
    char path[MAX_PATH] = {};
    if (GetEnvironmentVariableA("C1_KIT_TRAFFIC_LOG", path, MAX_PATH) == 0) {
        return;
    }
    const auto printable = [](std::string_view text) {
        std::string out;
        for (const char c : text) {
            if (c == '\x1e') {
                out.push_back('|');
            } else if (c == '\0') {
                break;
            } else if (c == '\r' || c == '\n') {
                out.append("\\n");
            } else {
                out.push_back(c);
            }
        }
        return out;
    };
    const std::string line = printable(request) + "  ->  " +
                             printable(response) + "\n";
    const HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ,
                                    nullptr, OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written,
              nullptr);
    CloseHandle(file);
}

} // namespace

std::string WindowsPipeServerBoundary::dispatch_command(
    std::string_view command) {
    std::string response = runtime_ == nullptr
                               ? std::string("ERROR\x1eServer unavailable")
                               : runtime_->dispatch_command(command);
    log_kit_traffic(command, response);
    return response;
}

void WindowsPipeServerBoundary::signal_command_complete() {
    if (runtime_ != nullptr) {
        runtime_->signal_command_complete();
    }
}

} // namespace creatures1::platform
