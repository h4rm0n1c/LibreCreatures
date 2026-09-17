#include "pipe_server.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <cstdio>
#include <cwchar>
#include <limits>

#include <oleauto.h>

namespace creatures1::scripting {

namespace {

constexpr std::size_t kPipeReceiveBufferSize = 0x100000;
constexpr std::size_t kFireCommandOutputBufferSize = 0x10000;
constexpr std::uint16_t kSupportedMacroTypeCount = 5;
constexpr char kRecordSeparator = '\x1e';
constexpr DISPID kCommunicateDispatchId = 1;
constexpr WORD kRequiredInvokeFlags = DISPATCH_METHOD;
constexpr UINT kCommunicateArgumentCount = 2;
constexpr DWORD kPipeConnectAttempts = 3;
constexpr DWORD kPipeRetryDelayMilliseconds = 50;
constexpr DWORD kWorkerCreatePipeAttempts = 10;
constexpr DWORD kWorkerCreatePipeRetryDelayMilliseconds = 1000;

std::string error_response(std::string_view detail) {
    std::string response = "ERROR";
    response += kRecordSeparator;
    response += detail;
    return response;
}

struct CommandParts {
    std::string_view name;
    std::string_view first_argument;
    std::string_view second_argument;
};

bool split_command(std::string_view command, CommandParts& parts) {
    const std::size_t first_separator = command.find(kRecordSeparator);
    if (first_separator == std::string_view::npos) {
        return false;
    }

    parts.name = command.substr(0, first_separator);
    const std::string_view arguments = command.substr(first_separator + 1);
    const std::size_t second_separator = arguments.find(kRecordSeparator);
    if (second_separator == std::string_view::npos) {
        parts.first_argument = arguments;
        parts.second_argument = {};
    } else {
        parts.first_argument = arguments.substr(0, second_separator);
        parts.second_argument = arguments.substr(second_separator + 1);
    }
    return !parts.name.empty();
}

template <typename Integer>
bool parse_decimal(std::string_view text, Integer& value) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, value, 10);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

std::string success_response(std::string_view detail = {}) {
    std::string response = "OK";
    if (!detail.empty()) {
        response.push_back(kRecordSeparator);
        response.append(detail);
    }
    return response;
}

} // namespace

CPipeDispatchProxy::CPipeDispatchProxy(std::uint32_t tool_index)
    : tool_index_(tool_index) {
    std::snprintf(pipe_name_, sizeof(pipe_name_),
                  "\\\\.\\pipe\\Creatures1_Kit_Tool%u", tool_index_);
}

CPipeDispatchProxy::~CPipeDispatchProxy() = default;

HRESULT STDMETHODCALLTYPE CPipeDispatchProxy::QueryInterface(
    REFIID interface_id, void** object_out) {
    if (object_out == nullptr) {
        return E_POINTER;
    }
    *object_out = nullptr;
    if (!InlineIsEqualGUID(interface_id, IID_IUnknown) &&
        !InlineIsEqualGUID(interface_id, IID_IDispatch)) {
        return E_NOINTERFACE;
    }

    *object_out = static_cast<IDispatch*>(this);
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE CPipeDispatchProxy::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&reference_count_));
}

ULONG STDMETHODCALLTYPE CPipeDispatchProxy::Release() {
    const LONG remaining = InterlockedDecrement(&reference_count_);
    if (remaining == 0) {
        delete this;
    }
    return static_cast<ULONG>(remaining);
}

HRESULT STDMETHODCALLTYPE CPipeDispatchProxy::GetTypeInfoCount(UINT* count_out) {
    if (count_out == nullptr) {
        return E_POINTER;
    }
    *count_out = 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPipeDispatchProxy::GetTypeInfo(
    UINT /*type_info_index*/, LCID /*locale_id*/, ITypeInfo** type_info_out) {
    if (type_info_out == nullptr) {
        return E_POINTER;
    }
    *type_info_out = nullptr;
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CPipeDispatchProxy::GetIDsOfNames(
    REFIID /*interface_id*/, LPOLESTR* names, UINT name_count,
    LCID /*locale_id*/, DISPID* dispatch_ids) {
    if ((name_count != 0 && names == nullptr) || dispatch_ids == nullptr) {
        return E_POINTER;
    }

    for (UINT index = 0; index < name_count; ++index) {
        if (names[index] == nullptr ||
            _wcsicmp(names[index], L"Communicate") != 0) {
            dispatch_ids[index] = DISPID_UNKNOWN;
            return DISP_E_UNKNOWNNAME;
        }
        dispatch_ids[index] = kCommunicateDispatchId;
    }
    return S_OK;
}

void CPipeDispatchProxy::initialize_exception(EXCEPINFO* exception_info) {
    if (exception_info == nullptr) {
        return;
    }

    std::memset(exception_info, 0, sizeof(*exception_info));
    exception_info->wCode = 0x3e9;
    exception_info->bstrSource =
        SysAllocString(L"CPipeDispatchProxy");
    exception_info->bstrDescription =
        SysAllocString(L"Failed to communicate with kit via pipe");
    exception_info->scode = E_FAIL;
}

HRESULT CPipeDispatchProxy::send_arguments(
    const VARIANTARG& first_argument, const VARIANTARG& second_argument,
    VARIANT* result, EXCEPINFO* exception_info) {
    const LONG first_payload = first_argument.lVal;
    const LONG second_payload = second_argument.lVal;
    const std::uint32_t wire_payload[2] = {
        static_cast<std::uint32_t>(second_payload),
        static_cast<std::uint32_t>(first_payload),
    };

    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (DWORD attempt = 0; attempt < kPipeConnectAttempts; ++attempt) {
        pipe = CreateFileA(pipe_name_, GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            break;
        }

        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND ||
            attempt + 1 >= kPipeConnectAttempts) {
            break;
        }
        Sleep(kPipeRetryDelayMilliseconds);
    }

    bool succeeded = false;
    if (pipe != INVALID_HANDLE_VALUE) {
        DWORD bytes_written = 0;
        succeeded = WriteFile(pipe, wire_payload, sizeof(wire_payload),
                              &bytes_written, nullptr) != FALSE &&
                    bytes_written == sizeof(wire_payload);
        if (succeeded) {
            FlushFileBuffers(pipe);
        }
        CloseHandle(pipe);
    }

    if (result != nullptr) {
        VariantInit(result);
        result->vt = VT_BOOL;
        result->boolVal = succeeded ? VARIANT_TRUE : VARIANT_FALSE;
    }
    if (succeeded) {
        return S_OK;
    }

    initialize_exception(exception_info);
    return DISP_E_EXCEPTION;
}

HRESULT STDMETHODCALLTYPE CPipeDispatchProxy::Invoke(
    DISPID dispatch_id, REFIID /*interface_id*/, LCID /*locale_id*/,
    WORD invoke_flags, DISPPARAMS* parameters, VARIANT* result,
    EXCEPINFO* exception_info, UINT* /*argument_error*/) {
    if (dispatch_id != kCommunicateDispatchId ||
        (invoke_flags & kRequiredInvokeFlags) == 0) {
        return DISP_E_MEMBERNOTFOUND;
    }
    if (parameters == nullptr ||
        parameters->cArgs != kCommunicateArgumentCount ||
        parameters->rgvarg == nullptr) {
        return DISP_E_BADPARAMCOUNT;
    }

    // COM stores arguments in reverse order.  The binary accepts either two
    // direct VT_I4 values or two VARIANT|VT_BYREF wrappers, but rejects a
    // mixed pair.
    VARIANTARG* first_argument = &parameters->rgvarg[1];
    VARIANTARG* second_argument = &parameters->rgvarg[0];
    constexpr VARTYPE kVariantByReference = VT_VARIANT | VT_BYREF;
    if (first_argument->vt == kVariantByReference) {
        if (second_argument->vt != kVariantByReference ||
            first_argument->pvarVal == nullptr ||
            second_argument->pvarVal == nullptr) {
            return DISP_E_TYPEMISMATCH;
        }
        first_argument = first_argument->pvarVal;
        second_argument = second_argument->pvarVal;
    }
    if (first_argument->vt != VT_I4 || second_argument->vt != VT_I4) {
        return DISP_E_TYPEMISMATCH;
    }

    return send_arguments(*first_argument, *second_argument, result,
                          exception_info);
}

PipeServer::PipeServer(PipeServerHost& host) : host_(host) {
    receive_buffer_.reserve(kPipeReceiveBufferSize);
    receive_buffer_.resize(kPipeReceiveBufferSize);
}

void PipeServer::run_worker() {
    const PipeHandle stop_event = host_.stop_event();
    if (host_.stop_requested()) {
        return;
    }

    constexpr NamedPipeConfiguration kConfiguration = {
        "\\\\.\\pipe\\SFC_OLE", 0x40000003, 6, 0xff, 0x100000,
        0x100000, 1000};
    std::uint32_t create_failures = 0;

    for (;;) {
        const PipeHandle pipe = host_.create_named_pipe(kConfiguration);
        if (pipe == 0 || pipe == static_cast<PipeHandle>(-1)) {
            ++create_failures;
            if (host_.debug_console_exists()) {
                host_.log("PipeServer: CreateNamedPipe failed\n");
            }
            if (create_failures >= kWorkerCreatePipeAttempts) {
                if (host_.debug_console_exists()) {
                    host_.log("PipeServer: Max retries exceeded, shutting down\n");
                }
                return;
            }
            Sleep(kWorkerCreatePipeRetryDelayMilliseconds);
            continue;
        }
        create_failures = 0;

        const PipeHandle connect_event = host_.create_pipe_connect_event();
        if (connect_event == 0) {
            host_.close_handle(pipe);
            return;
        }

        const PipeConnectResult connect =
            host_.connect_named_pipe(pipe, connect_event);
        bool should_serve_client = connect.connected;
        if (!connect.connected && connect.pending) {
            should_serve_client =
                host_.wait_for_pipe_or_stop(connect_event, stop_event) ==
                PipeConnectWaitResult::client_connected;
            if (!should_serve_client) {
                host_.cancel_pipe_io(pipe);
            }
        } else if (!connect.connected && connect.error_code == 0x217) {
            // ERROR_PIPE_CONNECTED: the client won the race with ConnectNamedPipe.
            should_serve_client = true;
        } else if (!connect.connected && host_.debug_console_exists()) {
            host_.log("PipeServer: ConnectNamedPipe failed\n");
        }

        host_.close_handle(connect_event);
        if (should_serve_client) {
            log_if_enabled("PipeServer: Client connected\n");
            this->serve_client(pipe);
            log_if_enabled("PipeServer: Client disconnected\n");
        }

        host_.disconnect_named_pipe(pipe);
        host_.close_handle(pipe);
        if (host_.stop_requested()) {
            return;
        }
    }
}

#if defined(_MSC_VER) && defined(_M_IX86)
void __fastcall pipe_server_thread_run(void* thread_parameter) {
#else
void pipe_server_thread_run(void* thread_parameter) {
#endif
    auto* context = static_cast<PipeServerThreadContext*>(thread_parameter);
    if (context == nullptr || context->server == nullptr) {
        return;
    }
    context->server->run_worker();
}

#if defined(_MSC_VER) && defined(_M_IX86)
std::uint32_t __stdcall pipe_server_thread_entry(void* thread_parameter) {
#else
std::uint32_t pipe_server_thread_entry(void* thread_parameter) {
#endif
    pipe_server_thread_run(thread_parameter);
    return 0;
}

void PipeServer::log_if_enabled(std::string_view message) {
    if (host_.debug_console_exists()) {
        host_.log(message);
    }
}

void PipeServer::ensure_receive_buffer_capacity() {
    if (receive_buffer_.size() >= kPipeReceiveBufferSize) {
        return;
    }

    const std::size_t current_capacity = receive_buffer_.capacity();
    std::size_t new_capacity = current_capacity + current_capacity / 2;
    new_capacity = std::max(new_capacity, kPipeReceiveBufferSize);
    receive_buffer_.reserve(new_capacity);
    receive_buffer_.resize(kPipeReceiveBufferSize, '\0');
}

void PipeServer::stop() {
    if (!host_.exchange_running_state(false)) {
        return;
    }

    log_if_enabled("PipeServer: Stopping...\n");

    const PipeHandle stop_event = host_.stop_event();
    const PipeHandle command_done_event = host_.command_done_event();
    if (stop_event != 0) {
        host_.signal_event(stop_event);
    }
    if (command_done_event != 0) {
        host_.signal_event(command_done_event);
    }

    const PipeHandle worker_thread = host_.take_worker_thread_handle();
    if (worker_thread != 0) {
        constexpr std::uint32_t kWorkerShutdownTimeoutMilliseconds = 10000;
        std::uint32_t remaining_milliseconds =
            kWorkerShutdownTimeoutMilliseconds;
        for (;;) {
            const WorkerWaitResult wait_result =
                host_.wait_for_worker_or_messages(
                    worker_thread, remaining_milliseconds);
            if (wait_result == WorkerWaitResult::worker_finished) {
                break;
            }
            if (wait_result == WorkerWaitResult::messages_available) {
                host_.pump_window_messages();
                continue;
            }

            log_if_enabled(
                "PipeServer: Thread did not exit in time - abandoning "
                "(will be cleaned up at process exit)\n");
            break;
        }
        host_.close_handle(worker_thread);
    }

    host_.discard_pipe_messages();
    if (stop_event != 0) {
        host_.close_handle(stop_event);
    }
    if (command_done_event != 0) {
        host_.close_handle(command_done_event);
    }

    // The native implementation walks its pending-command tree and invokes
    // each holder's deleting destructor before releasing the tree nodes. The
    // map owns the same holder lifetime at source level; clearing it performs
    // that destruction before the container is reset.
    pending_commands_.clear();
    log_if_enabled("PipeServer: Stopped\n");
}

void PipeServer::serve_client(PipeHandle pipe) {
    ensure_receive_buffer_capacity();

    const PipeHandle read_event = host_.create_manual_reset_event();
    if (read_event == 0) {
        log_if_enabled("PipeServer: Failed to create overlapped event\n");
        return;
    }

    // The read has to be issued before anything waits on its event: the loop
    // is gated on a zero-timeout stop poll, and the read event is only waited
    // on once ReadFile reports the operation pending.  Waiting on the read
    // event up front deadlocks, because nothing has armed it yet.
    const PipeHandle stop_event = host_.stop_event();
    while (!host_.stop_requested()) {
        std::uint32_t bytes_read = 0;
        PipeReadResult read_result = host_.read_overlapped(
            pipe, receive_buffer_.data(), receive_buffer_.size() - 1,
            read_event, bytes_read);

        if (!read_result.succeeded && read_result.pending) {
            if (host_.wait_for_client_or_stop(read_event, stop_event) ==
                ClientWaitResult::stop_requested) {
                host_.cancel_read(pipe);
                break;
            }
            read_result = host_.complete_overlapped_read(
                pipe, read_event, bytes_read);
        }

        if (!read_result.succeeded) {
            if (!read_result.disconnected) {
                log_if_enabled("PipeServer: ReadFile failed\n");
            } else {
                log_if_enabled("PipeServer: Client disconnected\n");
            }
            break;
        }
        if (bytes_read == 0) {
            break;
        }

        receive_buffer_[bytes_read] = '\0';
        // A message on this pipe is a C string: the server writes its own
        // replies with the terminator included, and OLEKitProxy -- the shim
        // every kit talks through under Wine -- writes its requests the same
        // way.  Counting that terminator as message content made every
        // numeric argument fail to parse, so a kit's CREATEMACRO was answered
        // "Type out of range" while the identical request without the
        // terminator succeeded.
        std::size_t command_length = bytes_read;
        while (command_length > 0 &&
               receive_buffer_[command_length - 1] == '\0') {
            --command_length;
        }
        const std::string_view command(receive_buffer_.data(), command_length);
        log_if_enabled("PipeServer: Received command\n");
        const std::string response = marshal_command_to_main_thread(command);
        log_if_enabled("PipeServer: Sending response\n");

        const PipeWriteResult write_result =
            host_.write(pipe, response.c_str(), response.size() + 1);
        if (!write_result.succeeded) {
            log_if_enabled("PipeServer: WriteFile failed\n");
            break;
        }
        host_.flush(pipe);
    }

    host_.close_handle(read_event);
}

std::string PipeServer::marshal_command_to_main_thread(
    std::string_view command) {
    if (host_.stop_requested()) {
        return error_response("Server is shutting down");
    }
    if (!host_.main_frame_available()) {
        log_if_enabled("PipeServer: Cannot marshal command - pFrame is NULL\n");
        return error_response("Main frame not available");
    }
    if (!host_.main_window_available()) {
        log_if_enabled(
            "PipeServer: Cannot marshal command - window handle invalid\n");
        return error_response("Main window not available");
    }

    PipeServerCommandContext context;
    context.command.assign(command.data(), command.size());
    if (!host_.post_pipe_command(context)) {
        return error_response("Failed to post command to main thread");
    }

    switch (host_.wait_for_pipe_command(30000)) {
    case CommandWaitResult::completed:
        return context.response;
    case CommandWaitResult::stop_requested:
        return error_response("Server is shutting down");
    case CommandWaitResult::timed_out:
    case CommandWaitResult::failed:
        log_if_enabled(
            "PipeServer: MarshalCommand timed out or failed\n");
        return error_response("Command timed out");
    }
    return error_response("Command timed out");
}

std::string PipeServer::dispatch_command(std::string_view command) {
    CommandParts parts;
    if (!split_command(command, parts)) {
        return error_response("Invalid command format");
    }

    if (parts.name == "CREATEMACRO") {
        std::uint32_t mode_value = 0;
        if (!parse_decimal(parts.first_argument, mode_value) ||
            mode_value >= kSupportedMacroTypeCount) {
            return error_response("Type out of range");
        }
        auto holder = host_.create_macro_holder(
            static_cast<MacroExecutionMode>(mode_value));
        if (holder == nullptr || holder->macro() == nullptr) {
            return error_response("Failed to create macro");
        }

        const std::uint32_t handle = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(holder.get()));
        register_pending_command(handle, std::move(holder));
        return success_response(std::to_string(handle));
    }

    if (parts.name == "LOADMACRO") {
        std::uint32_t handle = 0;
        if (!parse_decimal(parts.first_argument, handle)) {
            return error_response("Invalid handle");
        }
        const auto found = pending_commands_.find(handle);
        if (found == pending_commands_.end()) {
            return error_response("Invalid or expired handle");
        }
        found->second->macro()->load_script_text(parts.second_argument);
        return success_response();
    }

    if (parts.name == "REQUESTMACRO") {
        std::uint32_t handle = 0;
        if (!parse_decimal(parts.first_argument, handle)) {
            return error_response("Invalid handle");
        }
        const auto found = pending_commands_.find(handle);
        if (found == pending_commands_.end()) {
            return error_response("Invalid or expired handle");
        }

        std::array<char, kFireCommandOutputBufferSize> output{};
        if (!found->second->reset_result_and_invoke_result_entry(
                output.data())) {
            return error_response("Macro execution failed");
        }
        const std::size_t output_length = std::min<std::size_t>(
            found->second->callback_result(), output.size());
        std::string response = "OK";
        response.push_back(kRecordSeparator);
        response += std::to_string(output_length);
        response.push_back(kRecordSeparator);
        response.append(output.data(), output_length);
        return response;
    }

    if (parts.name == "EXECUTEMACRO") {
        std::uint32_t handle = 0;
        if (!parse_decimal(parts.first_argument, handle)) {
            return error_response("Invalid handle");
        }
        const auto found = pending_commands_.find(handle);
        if (found == pending_commands_.end()) {
            return error_response("Invalid or expired handle");
        }
        found->second->macro()->load_script_text(parts.second_argument);
        return found->second->invoke_dispatch_entry(nullptr)
                   ? success_response()
                   : error_response("Macro execution failed");
    }

    if (parts.name == "DESTROYMACRO") {
        std::uint32_t handle = 0;
        if (!parse_decimal(parts.first_argument, handle)) {
            return error_response("Invalid handle");
        }
        const auto found = pending_commands_.find(handle);
        if (found == pending_commands_.end()) {
            return error_response("Invalid or expired handle");
        }
        pending_commands_.erase(found);
        return success_response();
    }

    if (parts.name == "FIRECOMMAND") {
        std::uint32_t mode_value = 0;
        if (!parse_decimal(parts.first_argument, mode_value) ||
            mode_value > std::numeric_limits<std::uint16_t>::max()) {
            return error_response("Invalid type parameter");
        }
        return execute_fire_command(static_cast<std::uint16_t>(mode_value),
                                    parts.second_argument);
    }

    if (parts.name == "KITQUIT") {
        std::uint32_t tool_index = 0;
        if (!parse_decimal(parts.first_argument, tool_index)) {
            return error_response("Invalid tool ID");
        }
        if (tool_index >= 0x14) {
            return error_response("Tool ID out of range");
        }
        host_.shutdown_embedded_kit_tool(tool_index);
        return success_response("Kit cleaned up");
    }

    std::string response = error_response("Unknown command: ");
    response.append(parts.name);
    return response;
}

std::string PipeServer::execute_fire_command(
    std::uint16_t macro_type, std::string_view macro_source) {
    if (macro_type >= kSupportedMacroTypeCount) {
        return error_response("Invalid macro type");
    }

    std::unique_ptr<MacroHolder> holder = host_.create_macro_holder(
        static_cast<MacroExecutionMode>(macro_type));
    if (holder == nullptr || holder->macro() == nullptr) {
        return error_response("Failed to create macro holder");
    }

    holder->macro()->load_script_text(macro_source);
    holder->macro()->object_context.script_owner =
        host_.selected_creature();

    std::array<char, kFireCommandOutputBufferSize> output{};
    if (!holder->reset_result_and_invoke_result_entry(output.data())) {
        return error_response("Macro execution failed");
    }

    const std::uint32_t output_length = holder->callback_result();
    char header[68] = {};
    const int header_length = std::snprintf(
        header, sizeof(header), "OK%c%d%c", kRecordSeparator,
        static_cast<int>(output_length), kRecordSeparator);

    std::string response(header, header_length > 0 ? header_length : 0);
    const std::size_t bytes_to_append = std::min<std::size_t>(
        output_length, output.size());
    response.append(output.data(), bytes_to_append);
    return response;
}

void PipeServer::register_pending_command(
    std::uint32_t handle, std::unique_ptr<MacroHolder> holder) {
    pending_commands_[handle] = std::move(holder);
}

} // namespace creatures1::scripting
