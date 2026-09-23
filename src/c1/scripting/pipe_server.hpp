#pragma once

#include <atomic>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <oaidl.h>

#include "macro_holder.hpp"

namespace creatures1::scripting {

using PipeHandle = std::uintptr_t;

// The worker is a C1-owned routine, while its entry point is registered with
// the Win32 thread API.  Keep the recovered x86 calling conventions at this
// narrow boundary; the worker implementation itself is translated separately
// from the platform callback.
#if defined(_MSC_VER) && defined(_M_IX86)
void __fastcall pipe_server_thread_run(void* thread_parameter);
#else
void pipe_server_thread_run(void* thread_parameter);
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
std::uint32_t __stdcall pipe_server_thread_entry(void* thread_parameter);
#else
std::uint32_t pipe_server_thread_entry(void* thread_parameter);
#endif

enum class WorkerWaitResult {
    worker_finished,
    messages_available,
    timed_out,
    failed,
};

enum class ClientWaitResult {
    read_completed,
    stop_requested,
};

enum class PipeConnectWaitResult {
    client_connected,
    stop_requested,
    failed,
};

enum class CommandWaitResult {
    completed,
    stop_requested,
    timed_out,
    failed,
};

struct NamedPipeConfiguration {
    const char* name = nullptr;
    std::uint32_t open_mode = 0;
    std::uint32_t pipe_mode = 0;
    std::uint32_t instance_count = 0;
    std::uint32_t output_buffer_size = 0;
    std::uint32_t input_buffer_size = 0;
    std::uint32_t default_timeout_milliseconds = 0;
};

struct PipeConnectResult {
    bool connected = false;
    bool pending = false;
    std::uint32_t error_code = 0;
};

// This is the typed payload carried by the private main-window message.  It
// deliberately contains source-level strings, not the executable's 24-byte
// MSVC string representation.
// Shared between the pipe worker and the UI thread.  The UI thread can reach
// a command after the worker has given up waiting (a modal box inside the
// command -- scrp's Duplicate Script prompt -- can hold it past the timeout),
// so neither side may own it alone; `completed` tells the worker that the
// completion signal it woke on is this command's and not a late one.
struct PipeServerCommandContext {
    std::string command;
    std::string response;
    std::atomic<bool> completed{false};
};
using PipeServerCommandHandle = std::shared_ptr<PipeServerCommandContext>;

struct PipeReadResult {
    bool succeeded = false;
    bool pending = false;
    bool disconnected = false;
    std::uint32_t error_code = 0;
};

struct PipeWriteResult {
    bool succeeded = false;
    std::uint32_t error_code = 0;
};

// Named-pipe handles, overlapped I/O, message pumping, and native
// string/MFC storage are platform/framework concerns. The recovered
// pipe policy is kept here in terms of small operations so those concerns do
// not leak into the C1 source lane.
class PipeServerHost {
public:
    virtual ~PipeServerHost() = default;

    virtual bool exchange_running_state(bool running) = 0;
    virtual PipeHandle stop_event() const = 0;
    virtual bool stop_requested() const = 0;
    virtual PipeHandle command_done_event() const = 0;
    virtual PipeHandle take_worker_thread_handle() = 0;
    virtual void signal_event(PipeHandle event) = 0;
    virtual WorkerWaitResult wait_for_worker_or_messages(
        PipeHandle worker, std::uint32_t timeout_milliseconds) = 0;
    virtual void pump_window_messages() = 0;
    virtual void discard_pipe_messages() = 0;

    virtual PipeHandle create_named_pipe(
        const NamedPipeConfiguration& configuration) = 0;
    virtual PipeHandle create_pipe_connect_event() = 0;
    virtual PipeConnectResult connect_named_pipe(PipeHandle pipe,
                                                  PipeHandle connect_event) = 0;
    virtual PipeConnectWaitResult wait_for_pipe_or_stop(
        PipeHandle connect_event, PipeHandle stop_event) = 0;
    virtual void cancel_pipe_io(PipeHandle pipe) = 0;
    virtual void disconnect_named_pipe(PipeHandle pipe) = 0;

    virtual bool main_frame_available() const = 0;
    virtual bool main_window_available() const = 0;
    // Takes ownership of `posted` when it returns true.
    virtual bool post_pipe_command(PipeServerCommandHandle* posted) = 0;
    virtual CommandWaitResult wait_for_pipe_command(
        std::uint32_t timeout_milliseconds) = 0;

    virtual bool debug_console_exists() const = 0;
    virtual void log(std::string_view message) = 0;
    virtual void close_handle(PipeHandle handle) = 0;

    virtual std::unique_ptr<MacroHolder> create_macro_holder(
        MacroExecutionMode execution_mode) = 0;
    virtual objects::Object* selected_creature() const = 0;
    virtual bool shutdown_embedded_kit_tool(std::uint32_t tool_index) = 0;

    virtual PipeHandle create_manual_reset_event() = 0;
    virtual ClientWaitResult wait_for_client_or_stop(
        PipeHandle read_event, PipeHandle stop_event) = 0;
    virtual PipeReadResult read_overlapped(PipeHandle pipe,
                                           char* destination,
                                           std::size_t capacity,
                                           PipeHandle read_event,
                                           std::uint32_t& bytes_read) = 0;
    virtual void cancel_read(PipeHandle pipe) = 0;
    virtual PipeReadResult complete_overlapped_read(
        PipeHandle pipe, PipeHandle read_event,
        std::uint32_t& bytes_read) = 0;
    virtual PipeWriteResult write(PipeHandle pipe,
                                  const char* bytes,
                                  std::size_t byte_count) = 0;
    virtual void flush(PipeHandle pipe) = 0;
};

// This is a C1-owned COM transport object used by the Wine kit bridge.  The
// Windows COM ABI supplies IDispatch's vtable; the class contains only the
// state proven by the executable (reference count, tool number, and the
// derived pipe name).  In particular, this is not a hand-written vtable or a
// wrapper around the decompiler's C1ComVariant/C1DispatchParameters records.
class CPipeDispatchProxy final : public IDispatch {
public:
    explicit CPipeDispatchProxy(std::uint32_t tool_index);
    ~CPipeDispatchProxy();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id,
                                              void** object_out) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count_out) override;
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT type_info_index,
                                          LCID locale_id,
                                          ITypeInfo** type_info_out) override;
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID interface_id,
                                            LPOLESTR* names,
                                            UINT name_count,
                                            LCID locale_id,
                                            DISPID* dispatch_ids) override;
    HRESULT STDMETHODCALLTYPE Invoke(DISPID dispatch_id,
                                     REFIID interface_id,
                                     LCID locale_id,
                                     WORD invoke_flags,
                                     DISPPARAMS* parameters,
                                     VARIANT* result,
                                     EXCEPINFO* exception_info,
                                     UINT* argument_error) override;

private:
    HRESULT send_arguments(const VARIANTARG& first_argument,
                           const VARIANTARG& second_argument,
                           VARIANT* result,
                           EXCEPINFO* exception_info);
    static void initialize_exception(EXCEPINFO* exception_info);

    LONG reference_count_ = 1;
    std::uint32_t tool_index_ = 0;
    char pipe_name_[64] = {};
};

static_assert(sizeof(CPipeDispatchProxy) == 76,
              "Creatures1/CPipeDispatchProxy size mismatch");

class PipeServer {
public:
    explicit PipeServer(PipeServerHost& host);

    void run_worker();
    void stop();
    void serve_client(PipeHandle pipe);
    std::string dispatch_command(std::string_view command);
    std::string execute_fire_command(std::uint16_t macro_type,
                                     std::string_view macro_source);

    void register_pending_command(
        std::uint32_t handle, std::unique_ptr<MacroHolder> holder);

private:
    std::string marshal_command_to_main_thread(std::string_view command);
    void ensure_receive_buffer_capacity();
    void log_if_enabled(std::string_view message);

    PipeServerHost& host_;
    std::vector<char> receive_buffer_;
    std::map<std::uint32_t, std::unique_ptr<MacroHolder>> pending_commands_;
};

// The platform thread API receives an opaque parameter, but the clean
// project passes this typed context.  The original image passed its packed
// PipeServerSharedState because that record owned both the Win32 handles and
// the worker routine.  The reconstructed source keeps those responsibilities
// in PipeServerHost and carries only the owning PipeServer pointer here.
struct PipeServerThreadContext {
    PipeServer* server = nullptr;
};

} // namespace creatures1::scripting
