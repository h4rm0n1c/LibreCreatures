#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "windows_prelude.hpp"

#include "windows_pipe_server.hpp"

namespace creatures1::platform {

// Application-facing owner for the recovered pipe boundary. The Win32
// runtime owns handles and overlapped I/O; this object owns when that runtime
// exists and keeps startup/shutdown composition out of the MFC entry unit.
class WindowsPipeServerBoundary final {
public:
    using MacroHolderFactory = WindowsPipeServerRuntime::MacroHolderFactory;
    using SelectedCreatureQuery =
        WindowsPipeServerRuntime::SelectedCreatureQuery;
    using KitShutdown = WindowsPipeServerRuntime::KitShutdown;

    WindowsPipeServerBoundary(HWND main_window,
                               MacroHolderFactory macro_holder_factory,
                               SelectedCreatureQuery selected_creature_query,
                               KitShutdown kit_shutdown);
    ~WindowsPipeServerBoundary();

    WindowsPipeServerBoundary(const WindowsPipeServerBoundary&) = delete;
    WindowsPipeServerBoundary& operator=(const WindowsPipeServerBoundary&) =
        delete;

    bool start();
    void stop();
    bool running() const;
    std::string dispatch_command(std::string_view command);
    void signal_command_complete();

private:
    HWND main_window_ = nullptr;
    MacroHolderFactory macro_holder_factory_;
    SelectedCreatureQuery selected_creature_query_;
    KitShutdown kit_shutdown_;
    std::unique_ptr<WindowsPipeServerRuntime> runtime_;
};

// Appends a line to the opt-in kit traffic log (C1_KIT_TRAFFIC_LOG).
void log_kit_traffic_line(std::string_view line);

} // namespace creatures1::platform
