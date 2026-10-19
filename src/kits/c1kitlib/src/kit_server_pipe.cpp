// The kit's game -> kit route when COM is not available (Wine): the game
// starts the kit with a plain process launch and writes each Communicate
// call as 8 bytes, {header, payload}, to \\.\pipe\Creatures1_Kit_Tool<N>
// (windows_pipe_dispatch_proxy.cpp on the game side; the wire format and
// word order are unchanged from that file). This server opens that same
// pipe itself, in this process: no class object, no COM at all.
//
// Communicate has to run on the kit's main thread (c1kit.hpp's KitEvents
// comment), so the pipe is read on a plain worker thread that posts each
// record to a small window of its own, made on the thread that calls
// register_class_object (the kit's main thread, same requirement the COM
// path already has). That window's WndProc is the only place
// events.on_communicate is called.

#include "c1kit/c1kit.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

namespace c1kit {
namespace {

constexpr UINT kCommunicateMessage = WM_APP + 0x100;
constexpr const char* kWindowClassName = "C1KitPipeServerWnd";

LRESULT CALLBACK pipe_server_wndproc(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam) {
    if (message == kCommunicateMessage) {
        auto* events = reinterpret_cast<KitEvents*>(
            GetWindowLongPtrA(window, GWLP_USERDATA));
        if (events != nullptr) {
            events->on_communicate(
                static_cast<std::int32_t>(static_cast<std::uint32_t>(wparam)),
                static_cast<std::int32_t>(static_cast<std::uint32_t>(lparam)));
        }
        return 0;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

class PipeKitServer final : public KitServer {
public:
    PipeKitServer(int tool_id, KitEvents& events)
        : tool_id_(tool_id), events_(events) {
        std::snprintf(pipe_name_, sizeof(pipe_name_),
                     "\\\\.\\pipe\\Creatures1_Kit_Tool%d", tool_id);
    }

    ~PipeKitServer() override { stop(); }

    bool register_class_object(long* hresult) override {
        if (hresult != nullptr) {
            *hresult = 0;
        }
        WNDCLASSA window_class{};
        window_class.lpfnWndProc = pipe_server_wndproc;
        window_class.hInstance = GetModuleHandleA(nullptr);
        window_class.lpszClassName = kWindowClassName;
        if (RegisterClassA(&window_class) == 0 &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            if (hresult != nullptr) {
                *hresult = static_cast<long>(GetLastError());
            }
            return false;
        }
        window_ = CreateWindowExA(0, kWindowClassName, "", WS_POPUP, 0, 0, 0,
                                  0, nullptr, nullptr, window_class.hInstance,
                                  nullptr);
        if (window_ == nullptr) {
            if (hresult != nullptr) {
                *hresult = static_cast<long>(GetLastError());
            }
            return false;
        }
        SetWindowLongPtrA(window_, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(&events_));
        thread_ = CreateThread(nullptr, 0, &PipeKitServer::thread_entry, this,
                               0, nullptr);
        if (thread_ == nullptr) {
            if (hresult != nullptr) {
                *hresult = static_cast<long>(GetLastError());
            }
            DestroyWindow(window_);
            window_ = nullptr;
            return false;
        }
        return true;
    }

    void revoke_class_object() override { stop(); }

    void release() override { delete this; }

private:
    static DWORD WINAPI thread_entry(LPVOID parameter) {
        static_cast<PipeKitServer*>(parameter)->run();
        return 0;
    }

    void run() {
        while (!stop_requested_) {
            HANDLE pipe = CreateNamedPipeA(
                pipe_name_, PIPE_ACCESS_INBOUND,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 0,
                sizeof(std::uint32_t) * 2, 0, nullptr);
            if (pipe == INVALID_HANDLE_VALUE) {
                Sleep(200);
                continue;
            }
            const BOOL connected = ConnectNamedPipe(pipe, nullptr);
            if (stop_requested_) {
                CloseHandle(pipe);
                break;
            }
            if (connected || GetLastError() == ERROR_PIPE_CONNECTED) {
                std::uint32_t words[2] = {};
                DWORD read = 0;
                if (ReadFile(pipe, words, sizeof(words), &read, nullptr) &&
                    read == sizeof(words) && window_ != nullptr) {
                    PostMessageA(window_, kCommunicateMessage,
                                static_cast<WPARAM>(words[0]),
                                static_cast<LPARAM>(words[1]));
                }
            }
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }
    }

    void stop() {
        if (thread_ == nullptr) {
            return;
        }
        stop_requested_ = true;
        // Unblock a ConnectNamedPipe the worker is waiting in, the same way
        // the rest of this code's own pipe shutdowns do.
        HANDLE wake = CreateFileA(pipe_name_, GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        if (wake != INVALID_HANDLE_VALUE) {
            CloseHandle(wake);
        }
        WaitForSingleObject(thread_, 2000);
        CloseHandle(thread_);
        thread_ = nullptr;
        if (window_ != nullptr) {
            DestroyWindow(window_);
            window_ = nullptr;
        }
    }

    int tool_id_ = 0;
    KitEvents& events_;
    char pipe_name_[64] = {};
    HWND window_ = nullptr;
    HANDLE thread_ = nullptr;
    volatile bool stop_requested_ = false;
};

} // namespace

KitServer* create_kit_server_pipe(int tool_id, KitEvents& events) {
    return new PipeKitServer(tool_id, events);
}

} // namespace c1kit
