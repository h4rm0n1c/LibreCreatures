// The Lab Kit: its window, its record of what the game sends, and the pipe
// the lab drives it through.

#include "labkit.hpp"
#include "labkit_ids.hpp"
#include "labkit_protocol.hpp"

#include "c1kitshell/kit_art.hpp"

#include <cstdarg>
#include <cstdio>

namespace labkit {

// ===========================================================================
// The messages page: the newest lines of the log
// ===========================================================================

namespace {
constexpr int kShownLines = 500;
}

MessagesPage::MessagesPage(LabKitSheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringMessagesTab) {}

void MessagesPage::create_controls() {
    make(messages_, _T("LISTBOX"), _T(""),
         LBS_NOINTEGRALHEIGHT | LBS_NOSEL | WS_VSCROLL | WS_HSCROLL | WS_BORDER,
         kControlMessages);
    messages_.SetHorizontalExtent(2000);
}

void MessagesPage::layout(int width, int height) {
    const int row = button_height();
    place(messages_, kMargin, kMargin, width - 2 * kMargin,
          height - 3 * kMargin - row);
    place_close(width, height);
}

void MessagesPage::add_line(const CString& line) {
    if (messages_.GetSafeHwnd() == nullptr) {
        return;
    }
    if (messages_.GetCount() >= kShownLines) {
        messages_.DeleteString(0);
    }
    const int index = messages_.AddString(line);
    messages_.SetTopIndex(index);
}

// ===========================================================================
// The sheet
// ===========================================================================

BEGIN_MESSAGE_MAP(LabKitSheet, c1kitshell::KitSheet)
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_MESSAGE(kMessagePipeRequest, &LabKitSheet::OnPipeRequest)
    ON_MESSAGE(kMessageDeferred, &LabKitSheet::OnDeferred)
END_MESSAGE_MAP()

LabKitSheet::LabKitSheet(CFont& default_font)
    : KitSheet(kStringKitName, kStringPausedSuffix),
      default_font_(default_font),
      page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    AddPage(&page_);
    // The world's folder: each lab instance has its own.
    log_path_ = std::string(CStringA(c1kitshell::game_directory_setting(
                    "Main Directory", c1kit::GameDirectory::world))) +
                kLogName;
}

LabKitSheet::~LabKitSheet() {
    stop_pipe_server();
}

bool LabKitSheet::create_window() {
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU |
                      WS_MINIMIZEBOX | WS_THICKFRAME,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

BOOL LabKitSheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    SendMessage(WM_SETFONT,
                reinterpret_cast<WPARAM>(default_font_.GetSafeHandle()), 0);
    enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    install_options(nullptr, false);
    log("started\tpid=%lu log=%s", static_cast<unsigned long>(GetCurrentProcessId()),
        log_path_.c_str());
    start_pipe_server();
    SetTimer(kTimerStartup, kStartupDelayMs, nullptr);
    return result;
}

void LabKitSheet::log(const char* format, ...) {
    char text[1024];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    // Windows' uptime, the clock the game's lab trace uses too.
    const ULONGLONG ms = GetTickCount64();
    char line[1100];
    std::snprintf(line, sizeof(line), "%llu\t%s\n", ms, text);
    if (std::FILE* file = std::fopen(log_path_.c_str(), "a")) {
        std::fputs(line, file);
        std::fclose(file);
    }
    CString shown(line);
    shown.TrimRight();
    shown.Replace(_T("\t"), _T("  "));
    page_.add_line(shown);
}

void LabKitSheet::connect() {
    if (!connect_to_game(kCommandBufferBytes)) {
        log("connect\tfailed");
        return;
    }
    connected_ = true;
    if (c1kit::MacroTransport* game = transport()) {
        conversation_ = std::make_unique<c1kit::MacroConversation>(*game);
    }
    log("connect\tok");
}

void LabKitSheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connect();
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

// Game -> kit.  Recorded before anything else, so a message that hangs or
// ends the kit is in the log.
bool LabKitSheet::on_communicate_message(const c1kit::KitMessage& message) {
    ++messages_;
    const std::uint32_t header = static_cast<std::uint32_t>(message.kind) |
                                 (static_cast<std::uint32_t>(message.code) << 8) |
                                 (static_cast<std::uint32_t>(message.aux) << 16);
    const std::string meaning =
        describe_message(message.kind, message.code, message.aux, message.payload);
    unsigned hang = 0;
    if (hang_count_ != 0) {
        hang = hang_ms_;
        --hang_count_;
    }
    log("communicate\theader=%08lx payload=%08lx\t%s\tanswer=%s%s%s",
        static_cast<unsigned long>(header), static_cast<unsigned long>(message.payload),
        meaning.c_str(), answer_ ? "true" : "false",
        hang != 0 ? " after hang" : "", die_next_ ? " (dying)" : "");
    if (die_next_) {
        TerminateProcess(GetCurrentProcess(), 3);
    }
    if (hang != 0) {
        Sleep(hang);
        log("hang\tended after %u ms", hang);
    }
    handle_kit_message(message);
    return answer_;
}

void LabKitSheet::on_control_state(std::uint8_t state) {
    if (state == c1kit::kControlStateClose) {
        if (close_on_8_) {
            PostMessage(WM_CLOSE);
        }
        return;
    }
    if (state == c1kit::kControlStatePause) {
        paused_ = !paused_;
        if (paused_) {
            show_paused_title();
        } else {
            show_normal_title();
        }
    }
}

std::string LabKitSheet::status() const {
    char text[256];
    std::snprintf(text, sizeof(text),
                  "tool=%d connected=%d messages=%u answer=%s hang=%ux%u "
                  "close_on_8=%d paused=%d",
                  tool_id(), connected_ ? 1 : 0, messages_,
                  answer_ ? "true" : "false", hang_ms_, hang_count_,
                  close_on_8_ ? 1 : 0, paused_ ? 1 : 0);
    return text;
}

std::string LabKitSheet::handle_request(const std::string& text) {
    const Request request = parse_request(text);
    const std::string& verb = request.verb;
    const std::string& argument = request.argument;
    if (verb == "PING") {
        return "OK pong";
    }
    if (verb == "STATUS") {
        return "OK " + status();
    }
    if (verb == "ANSWER") {
        if (argument != "true" && argument != "false") {
            return "ERR ANSWER takes true or false";
        }
        answer_ = argument == "true";
        log("lab\tanswer=%s", argument.c_str());
        return "OK " + status();
    }
    if (verb == "HANG") {
        unsigned ms = 0;
        unsigned count = 0;
        if (!parse_hang(argument, ms, count)) {
            return "ERR HANG takes <ms> [<count>]";
        }
        hang_ms_ = ms;
        hang_count_ = ms == 0 ? 0 : count;
        log("lab\thang %u ms x%u", hang_ms_, hang_count_);
        return "OK " + status();
    }
    if (verb == "DIE") {
        if (argument == "next") {
            die_next_ = true;
            log("lab\tdie inside the next message");
            return "OK " + status();
        }
        if (argument == "now") {
            log("lab\tdie now");
            deferred_ = Deferred::die;
            PostMessage(kMessageDeferred);
            return "OK dying";
        }
        return "ERR DIE takes next or now";
    }
    if (verb == "CLOSEON8") {
        if (argument != "0" && argument != "1") {
            return "ERR CLOSEON8 takes 0 or 1";
        }
        close_on_8_ = argument == "1";
        log("lab\tclose_on_8=%s", argument.c_str());
        return "OK " + status();
    }
    if (verb == "CAOS") {
        if (!conversation_ || quitting()) {
            return "ERR not connected";
        }
        std::string reply;
        if (!conversation_->query_binary(c1kit::kMacroModeQuery, argument.c_str(),
                                         reply)) {
            return "ERR the game did not run it";
        }
        return "OK " + reply;
    }
    if (verb == "SCHEDULE") {
        c1kit::MacroTransport* game = transport();
        if (game == nullptr || quitting()) {
            return "ERR not connected";
        }
        return c1kit::execute_scheduled(*game, argument.c_str(), false)
                   ? std::string("OK")
                   : std::string("ERR the game refused it");
    }
    if (verb == "QUIT") {
        log("lab\tquit");
        deferred_ = Deferred::quit;
        PostMessage(kMessageDeferred);
        return "OK quitting";
    }
    return "ERR unknown request " + verb;
}

LRESULT LabKitSheet::OnPipeRequest(WPARAM wparam, LPARAM) {
    const std::unique_ptr<std::shared_ptr<PipeRequest>> held(
        reinterpret_cast<std::shared_ptr<PipeRequest>*>(wparam));
    PipeRequest& request = **held;
    request.reply = handle_request(request.text);
    SetEvent(request.done);
    return 0;
}

// After the reply to QUIT or DIE now has gone back to the lab.
LRESULT LabKitSheet::OnDeferred(WPARAM, LPARAM) {
    const Deferred action = deferred_;
    deferred_ = Deferred::none;
    if (action == Deferred::die) {
        TerminateProcess(GetCurrentProcess(), 3);
    } else if (action == Deferred::quit) {
        request_game_quit();
    }
    return 0;
}

void LabKitSheet::OnClose() {
    log("close\tby the window or state 8");
    request_game_quit();
}

void LabKitSheet::OnDestroy() {
    stop_pipe_server();
    if (conversation_ && !quitting()) {
        conversation_->close();
    }
    c1kitshell::KitSheet::OnDestroy();
}

// ---------------------------------------------------------------------------
// The pipe
// ---------------------------------------------------------------------------

void LabKitSheet::start_pipe_server() {
    stopping_ = false;
    pipe_thread_ = std::thread([this] { serve_pipe(); });
}

void LabKitSheet::stop_pipe_server() {
    if (!pipe_thread_.joinable()) {
        return;
    }
    stopping_ = true;
    // Wake ConnectNamedPipe with a connection of our own.
    const HANDLE wake = CreateFileA(kPipeName, GENERIC_READ | GENERIC_WRITE, 0,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
    if (wake != INVALID_HANDLE_VALUE) {
        CloseHandle(wake);
    }
    pipe_thread_.join();
}

void LabKitSheet::serve_pipe() {
    static char buffer[65536];
    while (!stopping_) {
        const HANDLE pipe = CreateNamedPipeA(
            kPipeName, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
            sizeof(buffer), sizeof(buffer), 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(500);
            continue;
        }
        const BOOL connected = ConnectNamedPipe(pipe, nullptr) != FALSE ||
                               GetLastError() == ERROR_PIPE_CONNECTED;
        if (connected && !stopping_) {
            DWORD read = 0;
            if (ReadFile(pipe, buffer, sizeof(buffer) - 1, &read, nullptr) &&
                read != 0) {
                auto request = std::make_shared<PipeRequest>();
                request->text.assign(buffer, read);
                auto* held = new std::shared_ptr<PipeRequest>(request);
                if (!::PostMessageA(GetSafeHwnd(), kMessagePipeRequest,
                                    reinterpret_cast<WPARAM>(held), 0)) {
                    delete held;
                    request->reply = "ERR the kit's window is gone";
                } else {
                    // A hang the lab asked for holds the window thread; wait
                    // as long as it may take, but not past a stop.
                    while (WaitForSingleObject(request->done, 100) == WAIT_TIMEOUT) {
                        if (stopping_) {
                            break;
                        }
                    }
                    if (WaitForSingleObject(request->done, 0) != WAIT_OBJECT_0) {
                        request->reply = "ERR the kit is closing";
                    }
                }
                DWORD written = 0;
                WriteFile(pipe, request->reply.data(),
                          static_cast<DWORD>(request->reply.size()), &written,
                          nullptr);
                FlushFileBuffers(pipe);
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

} // namespace labkit

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_labkit_window(CFont& font) {
    auto* sheet = new labkit::LabKitSheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    kit.identity.prog_id = "LabKit.OLE";
    // {32596286-31B9-4047-B9B4-4F2CB33133E5}
    const GUID clsid = {0x32596286, 0x31b9, 0x4047,
                        {0xb9, 0xb4, 0x4f, 0x2c, 0xb3, 0x31, 0x33, 0xe5}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 11;  // after the Ecology Kit
    kit.original_file_name = nullptr;  // new: there is no 1996 look
    kit.tool_value_prog_id = "LabKit.OLE";
    kit.tool_name_string = labkit::kStringToolName;
    kit.tool_help_string = labkit::kStringToolHelp;
    kit.ole_init_failed_string = labkit::kStringOleInitFailed;
    kit.create_main_window = &create_labkit_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
