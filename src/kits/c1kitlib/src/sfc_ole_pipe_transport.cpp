// SFC.OLE client over the named pipe directly: the kit side of the macro
// conversation when COM between processes is not available (Wine).
//
// This speaks the exact text protocol the game's own PipeServer parses
// (src/c1/scripting/pipe_server.cpp, dispatch_command): one command per
// request, "<VERB>\x1e<arg1>[\x1e<arg2>]", the message's trailing NUL
// included in what is written and read (the server strips it; see its
// comment on why that matters), the reply "OK[...]" or "ERROR\x1e<detail>".
// It is the same conversation c1kit::SfcOleTransport has over COM
// (sfc_ole_transport.cpp); this is a second, independent client of the same
// server, which already serves exactly this protocol for the Community
// Edition's Wine proxy and for tools/sfcole_client in c1-lab.

#include "c1kit/c1kit.hpp"
#include "c1kit/protocol.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace c1kit {
namespace {

constexpr char kRecordSeparator = '\x1e';
constexpr const char* kPipeName = "\\\\.\\pipe\\SFC_OLE";
// Generous enough for the largest REQUESTMACRO reply the game ever sends
// (its own output buffer is 0x10000) plus the "OK\x1e<len>\x1e" prefix.
constexpr std::size_t kReadBufferBytes = 0x20000;

class PipeMacroTransport final : public MacroTransport {
public:
    PipeMacroTransport() { read_buffer_.resize(kReadBufferBytes); }

    ~PipeMacroTransport() override { close_pipe(); }

    bool create_macro(short mode, long& handle) override {
        handle = 0;
        std::string command = "CREATEMACRO";
        command += kRecordSeparator;
        command += std::to_string(static_cast<int>(mode));
        std::string_view body;
        if (!round_trip(command, body) || !starts_with_ok(body)) {
            return false;
        }
        const std::string_view after_ok = skip_field(body);
        if (after_ok.empty()) {
            return false;
        }
        handle = std::strtol(std::string(after_ok).c_str(), nullptr, 10);
        return true;
    }

    bool destroy_macro(long handle) override {
        std::string command = "DESTROYMACRO";
        command += kRecordSeparator;
        command += std::to_string(handle);
        std::string_view body;
        return round_trip(command, body) && starts_with_ok(body);
    }

    bool load_macro(long handle, const char* script) override {
        std::string_view body;
        return send_with_script("LOADMACRO", handle, script, body) &&
               starts_with_ok(body);
    }

    bool execute_macro(long handle, const char* script) override {
        std::string_view body;
        return send_with_script("EXECUTEMACRO", handle, script, body) &&
               starts_with_ok(body);
    }

    bool request_macro(long handle) override {
        reply_length_ = 0;
        reply_bytes_ = 0;
        reply_.clear();
        std::string command = "REQUESTMACRO";
        command += kRecordSeparator;
        command += std::to_string(handle);
        std::string_view body;
        if (!round_trip(command, body) || !starts_with_ok(body)) {
            return false;
        }
        // "OK\x1e<byte count>\x1e<that many raw bytes>": the byte count is
        // what lets a binary reply (`dde: lobe`) carry embedded NULs or
        // 0x1E bytes through unharmed, so it is read by that count, not by
        // scanning for a delimiter inside the payload.
        const std::string_view after_ok = skip_field(body);
        const std::size_t separator = after_ok.find(kRecordSeparator);
        if (separator == std::string_view::npos) {
            return false;
        }
        const std::string_view length_text = after_ok.substr(0, separator);
        char* end = nullptr;
        const long declared_length =
            std::strtol(std::string(length_text).c_str(), &end, 10);
        if (end == nullptr || *end != '\0' || declared_length < 0) {
            return false;
        }
        const std::string_view payload = after_ok.substr(separator + 1);
        if (static_cast<std::size_t>(declared_length) > payload.size()) {
            return false;  // truncated: our read buffer was too small
        }
        reply_.assign(payload.data(), static_cast<std::size_t>(declared_length));
        reply_bytes_ = reply_.size();
        reply_length_ = std::strlen(reply_.c_str());
        return true;
    }

    const char* reply() const override { return reply_.c_str(); }
    std::size_t reply_length() const override { return reply_length_; }
    std::size_t reply_bytes() const override { return reply_bytes_; }
    void release() override { delete this; }

    DWORD last_connect_error() const { return last_error_; }

private:
    // Skips one field and its following separator, as c1kit::take_field does
    // for a reply already in hand; returns the rest, or empty with none left.
    static std::string_view skip_field(std::string_view text) {
        const std::size_t at = text.find(kRecordSeparator);
        return at == std::string_view::npos ? std::string_view{}
                                            : text.substr(at + 1);
    }

    static bool starts_with_ok(std::string_view body) {
        return body.size() >= 2 && body[0] == 'O' && body[1] == 'K';
    }

    bool send_with_script(const char* verb, long handle, const char* script,
                          std::string_view& body) {
        if (script == nullptr) {
            return false;
        }
        std::string command = verb;
        command += kRecordSeparator;
        command += std::to_string(handle);
        command += kRecordSeparator;
        command += script;
        return round_trip(command, body);
    }

    bool ensure_connected() {
        if (pipe_ != INVALID_HANDLE_VALUE) {
            return true;
        }
        DWORD last_error = 0;
        for (int attempt = 0; attempt < 20; ++attempt) {
            pipe_ = CreateFileA(kPipeName, GENERIC_READ | GENERIC_WRITE, 0,
                                nullptr, OPEN_EXISTING, 0, nullptr);
            if (pipe_ != INVALID_HANDLE_VALUE) {
                break;
            }
            last_error = GetLastError();
            if (last_error == ERROR_PIPE_BUSY) {
                WaitNamedPipeA(kPipeName, 2000);
                continue;
            }
            if (last_error != ERROR_FILE_NOT_FOUND) {
                break;
            }
            Sleep(100);  // the game may still be starting its pipe server
        }
        if (pipe_ == INVALID_HANDLE_VALUE) {
            last_error_ = last_error;
            return false;
        }
        DWORD mode = 2;  // PIPE_READMODE_MESSAGE
        SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr);
        return true;
    }

    void close_pipe() {
        if (pipe_ != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe_);
            pipe_ = INVALID_HANDLE_VALUE;
        }
    }

    // One request, one reply, over whichever connection is open; a failed
    // write or read closes the pipe and is tried once more on a fresh one,
    // in case the game's pipe server cycled between calls.
    bool round_trip(const std::string& command, std::string_view& body) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (!ensure_connected()) {
                return false;
            }
            if (send_and_receive(command, body)) {
                return true;
            }
            close_pipe();
        }
        return false;
    }

    bool send_and_receive(const std::string& command, std::string_view& body) {
        // The trailing NUL is part of the message on this pipe (both the
        // game's own writes and every existing client's); write it too.
        DWORD written = 0;
        if (!WriteFile(pipe_, command.c_str(),
                       static_cast<DWORD>(command.size() + 1), &written,
                       nullptr) ||
            written != command.size() + 1) {
            return false;
        }
        FlushFileBuffers(pipe_);
        DWORD read = 0;
        if (!ReadFile(pipe_, read_buffer_.data(),
                     static_cast<DWORD>(read_buffer_.size()), &read, nullptr) ||
            read == 0) {
            return false;
        }
        std::size_t length = read;
        if (length > 0 && read_buffer_[length - 1] == '\0') {
            --length;  // the protocol's own trailing NUL, not reply content
        }
        body = std::string_view(read_buffer_.data(), length);
        return true;
    }

    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    DWORD last_error_ = 0;
    std::vector<char> read_buffer_;
    std::string reply_;
    std::size_t reply_length_ = 0;
    std::size_t reply_bytes_ = 0;
};

} // namespace

MacroTransport* connect_sfc_ole_pipe(std::size_t /*buffer_bytes*/,
                                     ConnectResult* result,
                                     std::uint32_t* win32_error) {
    // buffer_bytes is the COM path's fixed command-buffer size; this
    // transport has nothing analogous (the pipe carries the script as
    // plain text, sized to the command itself), so the parameter exists
    // only to keep the call sites symmetric with connect_sfc_ole.
    if (result != nullptr) {
        *result = ConnectResult::connected;
    }
    if (win32_error != nullptr) {
        *win32_error = 0;
    }
    auto* transport = new PipeMacroTransport();
    // Confirm the game is actually there before handing back a transport
    // that will otherwise fail its first real call silently.
    long probe_handle = 0;
    if (!transport->create_macro(kMacroModeQuery, probe_handle) ||
        probe_handle == 0) {
        if (win32_error != nullptr) {
            *win32_error = static_cast<std::uint32_t>(
                transport->last_connect_error());
        }
        transport->release();
        if (result != nullptr) {
            *result = ConnectResult::create_failed;
        }
        return nullptr;
    }
    transport->destroy_macro(probe_handle);
    return transport;
}

} // namespace c1kit
