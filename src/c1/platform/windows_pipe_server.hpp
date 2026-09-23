#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "windows_prelude.hpp"

#include "../objects/object.hpp"
#include "../scripting/pipe_server.hpp"

namespace creatures1::platform {

// Native Win32 ownership for the recovered PipeServer boundary.  The pipe
// protocol and command policy remain in scripting::PipeServer; this class owns
// only events, the worker thread, overlapped I/O, and the UI-message bridge.
class WindowsPipeServerRuntime final : public scripting::PipeServerHost {
public:
    using MacroHolderFactory = std::function<
        std::unique_ptr<scripting::MacroHolder>(
            scripting::MacroExecutionMode)>;
    using SelectedCreatureQuery =
        std::function<objects::Object*()>;
    using KitShutdown = std::function<bool(std::uint32_t)>;

    WindowsPipeServerRuntime(HWND main_window,
                             MacroHolderFactory macro_holder_factory,
                             SelectedCreatureQuery selected_creature_query,
                             KitShutdown kit_shutdown);
    ~WindowsPipeServerRuntime() override;

    WindowsPipeServerRuntime(const WindowsPipeServerRuntime&) = delete;
    WindowsPipeServerRuntime& operator=(const WindowsPipeServerRuntime&) = delete;

    bool start();
    void stop();
    bool running() const;
    std::string dispatch_command(std::string_view command);
    void signal_command_complete();

    static bool running_under_wine();

    bool exchange_running_state(bool running) override;
    scripting::PipeHandle stop_event() const override;
    bool stop_requested() const override;
    scripting::PipeHandle command_done_event() const override;
    scripting::PipeHandle take_worker_thread_handle() override;
    void signal_event(scripting::PipeHandle event) override;
    scripting::WorkerWaitResult wait_for_worker_or_messages(
        scripting::PipeHandle worker,
        std::uint32_t timeout_milliseconds) override;
    void pump_window_messages() override;
    void discard_pipe_messages() override;

    scripting::PipeHandle create_named_pipe(
        const scripting::NamedPipeConfiguration& configuration) override;
    scripting::PipeHandle create_pipe_connect_event() override;
    scripting::PipeConnectResult connect_named_pipe(
        scripting::PipeHandle pipe,
        scripting::PipeHandle connect_event) override;
    scripting::PipeConnectWaitResult wait_for_pipe_or_stop(
        scripting::PipeHandle connect_event,
        scripting::PipeHandle stop_event) override;
    void cancel_pipe_io(scripting::PipeHandle pipe) override;
    void disconnect_named_pipe(scripting::PipeHandle pipe) override;

    bool main_frame_available() const override;
    bool main_window_available() const override;
    bool post_pipe_command(
        scripting::PipeServerCommandHandle* posted) override;
    scripting::CommandWaitResult wait_for_pipe_command(
        std::uint32_t timeout_milliseconds) override;

    bool debug_console_exists() const override;
    void log(std::string_view message) override;
    void close_handle(scripting::PipeHandle handle) override;

    std::unique_ptr<scripting::MacroHolder> create_macro_holder(
        scripting::MacroExecutionMode execution_mode) override;
    objects::Object* selected_creature() const override;
    bool shutdown_embedded_kit_tool(std::uint32_t tool_index) override;

    scripting::PipeHandle create_manual_reset_event() override;
    scripting::ClientWaitResult wait_for_client_or_stop(
        scripting::PipeHandle read_event,
        scripting::PipeHandle stop_event) override;
    scripting::PipeReadResult read_overlapped(
        scripting::PipeHandle pipe, char* destination, std::size_t capacity,
        scripting::PipeHandle read_event,
        std::uint32_t& bytes_read) override;
    void cancel_read(scripting::PipeHandle pipe) override;
    scripting::PipeReadResult complete_overlapped_read(
        scripting::PipeHandle pipe, scripting::PipeHandle read_event,
        std::uint32_t& bytes_read) override;
    scripting::PipeWriteResult write(
        scripting::PipeHandle pipe, const char* bytes,
        std::size_t byte_count) override;
    void flush(scripting::PipeHandle pipe) override;

private:
    static HANDLE native_handle(scripting::PipeHandle handle);
    static scripting::PipeHandle opaque_handle(HANDLE handle);
    static scripting::PipeReadResult read_error(DWORD error_code);

    HWND main_window_ = nullptr;
    MacroHolderFactory macro_holder_factory_;
    SelectedCreatureQuery selected_creature_query_;
    KitShutdown kit_shutdown_;
    std::atomic_bool running_{false};
    HANDLE stop_event_ = nullptr;
    HANDLE command_done_event_ = nullptr;
    HANDLE worker_thread_ = nullptr;
    std::unique_ptr<scripting::PipeServer> server_;
    scripting::PipeServerThreadContext thread_context_{};
    OVERLAPPED connect_overlapped_{};
    OVERLAPPED read_overlapped_{};
};

} // namespace creatures1::platform
