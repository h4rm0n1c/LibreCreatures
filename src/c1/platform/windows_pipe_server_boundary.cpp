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

std::string WindowsPipeServerBoundary::dispatch_command(
    std::string_view command) {
    return runtime_ == nullptr
               ? std::string("ERROR\x1eServer unavailable")
               : runtime_->dispatch_command(command);
}

void WindowsPipeServerBoundary::signal_command_complete() {
    if (runtime_ != nullptr) {
        runtime_->signal_command_complete();
    }
}

} // namespace creatures1::platform
