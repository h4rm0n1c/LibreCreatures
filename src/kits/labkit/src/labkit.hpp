#pragma once

// The Lab Kit (Tool slot 11, new in LibreCreatures): the c1-lab test
// harness's own kit.  The game opens it from the Tools menu like any other
// kit, so it gets what every kit gets -- the identity message, the selection,
// name, close and pause broadcasts, and slot messages -- and it talks back
// over SFC.OLE as the kits do.
//
// Every Communicate call is written to "Lab Kit.log" in the world's folder,
// one tab-separated line: milliseconds, header, payload, what it means, and
// the answer given.  The lab drives the kit through \\.\pipe\c1-labkit
// (labkit_protocol.hpp): what to answer, hanging or dying inside a call,
// whether state 8 closes it, and CAOS on the kit's own connection.

#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/conversation.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace labkit {

class LabKitSheet;

class MessagesPage : public c1kitshell::LayoutPage {
public:
    explicit MessagesPage(LabKitSheet& sheet);

    void add_line(const CString& line);

protected:
    void create_controls() override;
    void layout(int width, int height) override;

private:
    CListBox messages_;
};

// One request from the pipe thread, answered on the window thread.  Shared:
// a pipe thread told to stop no longer waits, and a window that is going
// may never answer.
struct PipeRequest {
    PipeRequest() : done(CreateEventA(nullptr, TRUE, FALSE, nullptr)) {}
    ~PipeRequest() {
        if (done != nullptr) {
            CloseHandle(done);
        }
    }
    PipeRequest(const PipeRequest&) = delete;
    PipeRequest& operator=(const PipeRequest&) = delete;

    std::string text;
    std::string reply;
    HANDLE done;
};

class LabKitSheet : public c1kitshell::KitSheet {
public:
    explicit LabKitSheet(CFont& default_font);
    ~LabKitSheet() override;

    bool create_window();
    bool on_communicate_message(const c1kit::KitMessage& message) override;

protected:
    BOOL OnInitDialog() override;
    void on_control_state(std::uint8_t state) override;
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnPipeRequest(WPARAM wparam, LPARAM lparam);
    afx_msg LRESULT OnDeferred(WPARAM wparam, LPARAM lparam);
    DECLARE_MESSAGE_MAP()

private:
    enum class Deferred { none, quit, die };

    void connect();
    std::string handle_request(const std::string& text);
    std::string status() const;
    void log(const char* format, ...);
    void start_pipe_server();
    void stop_pipe_server();
    void serve_pipe();

    CFont& default_font_;
    MessagesPage page_;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    std::string log_path_;
    bool connected_ = false;
    bool paused_ = false;
    unsigned messages_ = 0;
    bool answer_ = true;
    unsigned hang_ms_ = 0;
    unsigned hang_count_ = 0;
    bool die_next_ = false;
    bool close_on_8_ = true;
    Deferred deferred_ = Deferred::none;
    std::thread pipe_thread_;
    std::atomic<bool> stopping_{false};
};

} // namespace labkit
