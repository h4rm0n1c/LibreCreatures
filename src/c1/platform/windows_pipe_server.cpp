#include "windows_pipe_server.hpp"
#include "security.hpp"

#include <sddl.h>

#include <mutex>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace creatures1::platform {

namespace {

constexpr UINT kPipeServerMessage = 0x402;

bool is_disconnected_error(DWORD error_code) {
    return error_code == ERROR_BROKEN_PIPE || error_code == ERROR_NO_DATA ||
           error_code == ERROR_PIPE_NOT_CONNECTED;
}

// Win32BuildCurrentUserSecurityDescriptor @ 0x00445b30: once per process,
// grant GENERIC_ALL to SYSTEM, the built-in Administrators and the user
// running the game, and nobody else.  The pipe executes arbitrary CAOS, so
// the default DACL CreateNamedPipe otherwise applies is too open.  As in the
// native, any failure leaves the pipe on default security rather than
// refusing to start it.
class WindowsNamedPipeSecurity final : public NamedPipeSecurityApi {
public:
    bool ensure_current_user_security_attributes() override {
        std::call_once(once_, [] { build(); });
        return descriptor_ != nullptr;
    }
    static SECURITY_ATTRIBUTES* attributes() {
        return descriptor_ != nullptr ? &attributes_ : nullptr;
    }

private:
    static void build() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            return;
        }
        DWORD size = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::string buffer(size, '\0');
        LPSTR sid_text = nullptr;
        if (size != 0 &&
            GetTokenInformation(token, TokenUser, buffer.data(), size, &size) &&
            ConvertSidToStringSidA(
                reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid,
                &sid_text)) {
            char sddl[0x100] = {};
            _snprintf_s(sddl, sizeof(sddl), _TRUNCATE,
                        "D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;%s)", sid_text);
            PSECURITY_DESCRIPTOR descriptor = nullptr;
            if (ConvertStringSecurityDescriptorToSecurityDescriptorA(
                    sddl, SDDL_REVISION_1, &descriptor, nullptr)) {
                attributes_.nLength = sizeof(attributes_);
                attributes_.lpSecurityDescriptor = descriptor;
                attributes_.bInheritHandle = FALSE;
                descriptor_ = descriptor;
            }
            LocalFree(sid_text);
        }
        CloseHandle(token);
    }

    static inline std::once_flag once_;
    static inline SECURITY_ATTRIBUTES attributes_{};
    static inline PSECURITY_DESCRIPTOR descriptor_ = nullptr;
};

} // namespace

WindowsPipeServerRuntime::WindowsPipeServerRuntime(
    HWND main_window, MacroHolderFactory macro_holder_factory,
    SelectedCreatureQuery selected_creature_query, KitShutdown kit_shutdown)
    : main_window_(main_window),
      macro_holder_factory_(std::move(macro_holder_factory)),
      selected_creature_query_(std::move(selected_creature_query)),
      kit_shutdown_(std::move(kit_shutdown)) {}

WindowsPipeServerRuntime::~WindowsPipeServerRuntime() {
    stop();
    server_.reset();
}

bool WindowsPipeServerRuntime::start() {
    if (running()) {
        return true;
    }
    if (main_window_ == nullptr || !IsWindow(main_window_)) {
        return false;
    }

    stop_event_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (stop_event_ == nullptr) {
        return false;
    }
    command_done_event_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (command_done_event_ == nullptr) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
        return false;
    }

    server_ = std::make_unique<scripting::PipeServer>(*this);
    thread_context_.server = server_.get();
    worker_thread_ = CreateThread(
        nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(
            &scripting::pipe_server_thread_entry),
        &thread_context_, 0, nullptr);
    if (worker_thread_ == nullptr) {
        server_.reset();
        CloseHandle(command_done_event_);
        CloseHandle(stop_event_);
        command_done_event_ = nullptr;
        stop_event_ = nullptr;
        thread_context_.server = nullptr;
        return false;
    }

    // The native startup publishes this flag only after CreateThread succeeds.
    // The worker's stop test is event-based, so it cannot observe this short
    // publication window as an early shutdown.
    running_.store(true, std::memory_order_release);
    return true;
}

void WindowsPipeServerRuntime::stop() {
    if (server_ == nullptr || !running()) {
        return;
    }
    // PipeServer::stop owns the recovered ordering: publish the stopped
    // state, signal both events, join/pump the worker, close native handles,
    // and release pending macro holders.  Do not exchange the state here or
    // that policy would return before doing any of that work.
    server_->stop();
    thread_context_.server = nullptr;
    stop_event_ = nullptr;
    command_done_event_ = nullptr;
    worker_thread_ = nullptr;
    server_.reset();
}

bool WindowsPipeServerRuntime::running() const {
    return running_.load(std::memory_order_acquire);
}

std::string WindowsPipeServerRuntime::dispatch_command(
    std::string_view command) {
    return server_ == nullptr ? std::string("ERROR\x1eServer unavailable")
                               : server_->dispatch_command(command);
}

void WindowsPipeServerRuntime::signal_command_complete() {
    if (command_done_event_ != nullptr) {
        SetEvent(command_done_event_);
    }
}

bool WindowsPipeServerRuntime::running_under_wine() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    return ntdll != nullptr &&
           GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

bool WindowsPipeServerRuntime::exchange_running_state(bool running_value) {
    return running_.exchange(running_value, std::memory_order_acq_rel);
}

scripting::PipeHandle WindowsPipeServerRuntime::stop_event() const {
    return opaque_handle(stop_event_);
}

bool WindowsPipeServerRuntime::stop_requested() const {
    return stop_event_ != nullptr &&
           WaitForSingleObject(stop_event_, 0) == WAIT_OBJECT_0;
}

scripting::PipeHandle WindowsPipeServerRuntime::command_done_event() const {
    return opaque_handle(command_done_event_);
}

scripting::PipeHandle WindowsPipeServerRuntime::take_worker_thread_handle() {
    const HANDLE worker = worker_thread_;
    worker_thread_ = nullptr;
    return opaque_handle(worker);
}

void WindowsPipeServerRuntime::signal_event(scripting::PipeHandle event) {
    HANDLE native_event = native_handle(event);
    if (native_event != nullptr && native_event != INVALID_HANDLE_VALUE) {
        SetEvent(native_event);
    }
}

scripting::WorkerWaitResult WindowsPipeServerRuntime::wait_for_worker_or_messages(
    scripting::PipeHandle worker, std::uint32_t timeout_milliseconds) {
    HANDLE native_worker = native_handle(worker);
    if (native_worker == nullptr || native_worker == INVALID_HANDLE_VALUE) {
        return scripting::WorkerWaitResult::failed;
    }
    const DWORD result = MsgWaitForMultipleObjects(
        1, &native_worker, FALSE, timeout_milliseconds, QS_ALLINPUT);
    if (result == WAIT_OBJECT_0) {
        return scripting::WorkerWaitResult::worker_finished;
    }
    if (result == WAIT_OBJECT_0 + 1) {
        return scripting::WorkerWaitResult::messages_available;
    }
    if (result == WAIT_TIMEOUT) {
        return scripting::WorkerWaitResult::timed_out;
    }
    return scripting::WorkerWaitResult::failed;
}

void WindowsPipeServerRuntime::pump_window_messages() {
    MSG message{};
    while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

void WindowsPipeServerRuntime::discard_pipe_messages() {
    MSG message{};
    while (PeekMessageA(&message, nullptr, kPipeServerMessage,
                        kPipeServerMessage, PM_REMOVE) != FALSE) {
        delete reinterpret_cast<scripting::PipeServerCommandHandle*>(
            message.wParam);
    }
}

scripting::PipeHandle WindowsPipeServerRuntime::create_named_pipe(
    const scripting::NamedPipeConfiguration& configuration) {
    if (configuration.name == nullptr) {
        return 0;
    }
    WindowsNamedPipeSecurity security;
    build_current_user_security_descriptor(security);
    HANDLE pipe = CreateNamedPipeA(
        configuration.name, configuration.open_mode, configuration.pipe_mode,
        configuration.instance_count, configuration.output_buffer_size,
        configuration.input_buffer_size,
        configuration.default_timeout_milliseconds,
        WindowsNamedPipeSecurity::attributes());
    return opaque_handle(pipe);
}

scripting::PipeHandle WindowsPipeServerRuntime::create_pipe_connect_event() {
    return opaque_handle(CreateEventA(nullptr, TRUE, FALSE, nullptr));
}

scripting::PipeConnectResult WindowsPipeServerRuntime::connect_named_pipe(
    scripting::PipeHandle pipe, scripting::PipeHandle connect_event) {
    connect_overlapped_ = {};
    connect_overlapped_.hEvent = native_handle(connect_event);
    const BOOL connected = ConnectNamedPipe(native_handle(pipe),
                                             &connect_overlapped_);
    if (connected != FALSE) {
        return {true, false, 0};
    }
    const DWORD error_code = GetLastError();
    if (error_code == ERROR_IO_PENDING) {
        return {false, true, error_code};
    }
    return {false, false, error_code};
}

scripting::PipeConnectWaitResult WindowsPipeServerRuntime::wait_for_pipe_or_stop(
    scripting::PipeHandle connect_event, scripting::PipeHandle stop_event) {
    HANDLE handles[2] = {native_handle(connect_event), native_handle(stop_event)};
    const DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    if (result == WAIT_OBJECT_0) {
        return scripting::PipeConnectWaitResult::client_connected;
    }
    if (result == WAIT_OBJECT_0 + 1) {
        return scripting::PipeConnectWaitResult::stop_requested;
    }
    return scripting::PipeConnectWaitResult::failed;
}

void WindowsPipeServerRuntime::cancel_pipe_io(scripting::PipeHandle pipe) {
    CancelIoEx(native_handle(pipe), nullptr);
}

void WindowsPipeServerRuntime::disconnect_named_pipe(scripting::PipeHandle pipe) {
    DisconnectNamedPipe(native_handle(pipe));
}

bool WindowsPipeServerRuntime::main_frame_available() const {
    return main_window_ != nullptr;
}

bool WindowsPipeServerRuntime::main_window_available() const {
    return main_window_ != nullptr && IsWindow(main_window_) != FALSE;
}

bool WindowsPipeServerRuntime::post_pipe_command(
    scripting::PipeServerCommandHandle* posted) {
    return main_window_available() &&
           PostMessageA(main_window_, kPipeServerMessage,
                        reinterpret_cast<WPARAM>(posted), 0) != FALSE;
}

scripting::CommandWaitResult WindowsPipeServerRuntime::wait_for_pipe_command(
    std::uint32_t timeout_milliseconds) {
    HANDLE handles[2] = {stop_event_, command_done_event_};
    const DWORD result = WaitForMultipleObjects(
        2, handles, FALSE, timeout_milliseconds);
    if (result == WAIT_OBJECT_0) {
        return scripting::CommandWaitResult::stop_requested;
    }
    if (result == WAIT_OBJECT_0 + 1) {
        return scripting::CommandWaitResult::completed;
    }
    if (result == WAIT_TIMEOUT) {
        return scripting::CommandWaitResult::timed_out;
    }
    return scripting::CommandWaitResult::failed;
}

bool WindowsPipeServerRuntime::debug_console_exists() const {
    return GetConsoleWindow() != nullptr;
}

void WindowsPipeServerRuntime::log(std::string_view message) {
    const std::string text(message);
    OutputDebugStringA(text.c_str());
}

void WindowsPipeServerRuntime::close_handle(scripting::PipeHandle handle) {
    HANDLE native = native_handle(handle);
    if (native != nullptr && native != INVALID_HANDLE_VALUE) {
        CloseHandle(native);
    }
}

std::unique_ptr<scripting::MacroHolder>
WindowsPipeServerRuntime::create_macro_holder(
    scripting::MacroExecutionMode execution_mode) {
    // MacroHolder needs the already-existing interpreter host.  Returning the
    // callback result keeps this transport honest until that host is composed;
    // it does not create a second or guessed interpreter implementation.
    return macro_holder_factory_ == nullptr
               ? nullptr
               : macro_holder_factory_(execution_mode);
}

objects::Object* WindowsPipeServerRuntime::selected_creature() const {
    return selected_creature_query_ == nullptr ? nullptr
                                                : selected_creature_query_();
}

bool WindowsPipeServerRuntime::shutdown_embedded_kit_tool(
    std::uint32_t tool_index) {
    return kit_shutdown_ != nullptr && kit_shutdown_(tool_index);
}

scripting::PipeHandle WindowsPipeServerRuntime::create_manual_reset_event() {
    return opaque_handle(CreateEventA(nullptr, TRUE, FALSE, nullptr));
}

scripting::ClientWaitResult WindowsPipeServerRuntime::wait_for_client_or_stop(
    scripting::PipeHandle read_event, scripting::PipeHandle stop_event) {
    HANDLE handles[2] = {native_handle(read_event), native_handle(stop_event)};
    const DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    return result == WAIT_OBJECT_0 + 1
               ? scripting::ClientWaitResult::stop_requested
               : scripting::ClientWaitResult::read_completed;
}

scripting::PipeReadResult WindowsPipeServerRuntime::read_overlapped(
    scripting::PipeHandle pipe, char* destination, std::size_t capacity,
    scripting::PipeHandle read_event, std::uint32_t& bytes_read) {
    bytes_read = 0;
    read_overlapped_ = {};
    read_overlapped_.hEvent = native_handle(read_event);
    // Manual-reset event: clear it here, immediately before the read, or a
    // completion signalled by the previous command would satisfy the next
    // pending-read wait straight away.
    ResetEvent(read_overlapped_.hEvent);
    const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(
        capacity, std::numeric_limits<DWORD>::max()));
    if (ReadFile(native_handle(pipe), destination, requested,
                 reinterpret_cast<LPDWORD>(&bytes_read), &read_overlapped_) !=
        FALSE) {
        return {true, false, false, 0};
    }
    return read_error(GetLastError());
}

void WindowsPipeServerRuntime::cancel_read(scripting::PipeHandle pipe) {
    CancelIoEx(native_handle(pipe), &read_overlapped_);
}

scripting::PipeReadResult
WindowsPipeServerRuntime::complete_overlapped_read(
    scripting::PipeHandle pipe, scripting::PipeHandle /*read_event*/,
    std::uint32_t& bytes_read) {
    bytes_read = 0;
    DWORD completed = 0;
    if (GetOverlappedResult(native_handle(pipe), &read_overlapped_, &completed,
                            FALSE) != FALSE) {
        bytes_read = completed;
        return {true, false, false, 0};
    }
    return read_error(GetLastError());
}

scripting::PipeWriteResult WindowsPipeServerRuntime::write(
    scripting::PipeHandle pipe, const char* bytes, std::size_t byte_count) {
    if (byte_count > std::numeric_limits<DWORD>::max()) {
        return {false, ERROR_NOT_ENOUGH_MEMORY};
    }
    DWORD bytes_written = 0;
    const BOOL succeeded = WriteFile(
        native_handle(pipe), bytes, static_cast<DWORD>(byte_count),
        &bytes_written, nullptr);
    return {succeeded != FALSE && bytes_written == byte_count,
            succeeded != FALSE ? ERROR_SUCCESS : GetLastError()};
}

void WindowsPipeServerRuntime::flush(scripting::PipeHandle pipe) {
    FlushFileBuffers(native_handle(pipe));
}

HANDLE WindowsPipeServerRuntime::native_handle(scripting::PipeHandle handle) {
    return reinterpret_cast<HANDLE>(handle);
}

scripting::PipeHandle WindowsPipeServerRuntime::opaque_handle(HANDLE handle) {
    return reinterpret_cast<scripting::PipeHandle>(handle);
}

scripting::PipeReadResult WindowsPipeServerRuntime::read_error(DWORD error_code) {
    return {false, error_code == ERROR_IO_PENDING,
            is_disconnected_error(error_code), error_code};
}

} // namespace creatures1::platform
