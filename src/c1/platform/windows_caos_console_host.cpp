#include "windows_shell.hpp"

#include <fstream>

namespace creatures1::platform {
namespace {

// The native keeps the console in g_caos_console_dialog and re-shows an
// existing one rather than constructing a second.
C1CaosConsoleDialog* g_caos_console_dialog = nullptr;

// The pipe-server protocol answers "OK" or "ERROR" followed by a record
// separator and the payload.
constexpr char kRecordSeparator = '\x1e';

creatures1::ui::CaosRect to_caos_rect(const RECT& rect) {
    return {rect.left, rect.top, rect.right, rect.bottom};
}

} // namespace

BEGIN_MESSAGE_MAP(C1CaosConsoleDialog, CDialog)
    ON_BN_CLICKED(C1CaosConsoleDialog::kSendButton,
                  &C1CaosConsoleDialog::OnSend)
    ON_BN_CLICKED(C1CaosConsoleDialog::kClearButton,
                  &C1CaosConsoleDialog::OnClear)
    ON_BN_CLICKED(C1CaosConsoleDialog::kLoadButton,
                  &C1CaosConsoleDialog::OnLoad)
    ON_BN_CLICKED(C1CaosConsoleDialog::kAlwaysOnTopCheckBox,
                  &C1CaosConsoleDialog::OnToggleAlwaysOnTop)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

C1CaosConsoleDialog::C1CaosConsoleDialog() : CDialog(148, nullptr) {}

bool C1CaosConsoleDialog::create_modeless() {
    if (Create(MAKEINTRESOURCEA(148), nullptr) == FALSE) {
        return false;
    }
    g_caos_console_dialog = this;
    return true;
}

bool C1MainFrame::caos_console_exists() const {
    return g_caos_console_dialog != nullptr;
}

void C1MainFrame::activate_caos_console() {
    if (g_caos_console_dialog != nullptr) {
        g_caos_console_dialog->bind_dispatch(this);
        g_caos_console_dialog->ShowWindow(SW_SHOW);
    }
}

void C1MainFrame::create_caos_console() {
    auto* dialog = new C1CaosConsoleDialog();
    dialog->bind_dispatch(this);
    if (!dialog->create_modeless()) {
        delete dialog;
    }
}

void C1MainFrame::show_caos_console() {
    if (g_caos_console_dialog != nullptr) {
        g_caos_console_dialog->ShowWindow(SW_SHOW);
    }
}

void C1CaosConsoleDialog::PostNcDestroy() {
    // CAOSConsoleDlg::PostNcDestroy @ 0x0040f180 clears the global
    // unconditionally, then forwards to CWnd::PostNcDestroy before the
    // deleting destructor -- the base-class forward was missing here.
    g_caos_console_dialog = nullptr;
    CDialog::PostNcDestroy();
    delete this;
}

BOOL C1CaosConsoleDialog::OnInitDialog() {
    creatures1::ui::initialise_caos_console(state_, *this);
    return TRUE;
}

UINT C1CaosConsoleDialog::control_id(
    creatures1::ui::CaosControl control) const {
    switch (control) {
    case creatures1::ui::CaosControl::output:
        return kOutputEdit;
    case creatures1::ui::CaosControl::command_input:
        return kCommandInputEdit;
    case creatures1::ui::CaosControl::always_on_top:
        return kAlwaysOnTopCheckBox;
    case creatures1::ui::CaosControl::send:
        return kSendButton;
    case creatures1::ui::CaosControl::clear:
        return kClearButton;
    case creatures1::ui::CaosControl::load:
        return kLoadButton;
    }
    return 0;
}

void C1CaosConsoleDialog::initialise_base_dialog(
    creatures1::ui::CaosConsoleState& state) {
    CDialog::OnInitDialog();
    // The native seeds the history cursor to -1; the policy stores it as an
    // index one past the end, which is the same "nothing recalled" state.
    state.history_selected_index = 0;
}

creatures1::ui::CaosRect C1CaosConsoleDialog::client_rect() const {
    RECT rect{};
    const_cast<C1CaosConsoleDialog*>(this)->GetClientRect(&rect);
    return to_caos_rect(rect);
}

bool C1CaosConsoleDialog::control_exists(
    creatures1::ui::CaosControl control) const {
    return const_cast<C1CaosConsoleDialog*>(this)->GetDlgItem(
               static_cast<int>(control_id(control))) != nullptr;
}

creatures1::ui::CaosRect C1CaosConsoleDialog::control_screen_rect(
    creatures1::ui::CaosControl control) const {
    RECT rect{};
    CWnd* item = const_cast<C1CaosConsoleDialog*>(this)->GetDlgItem(
        static_cast<int>(control_id(control)));
    if (item != nullptr) {
        item->GetWindowRect(&rect);
    }
    return to_caos_rect(rect);
}

creatures1::ui::CaosRect C1CaosConsoleDialog::screen_to_client(
    creatures1::ui::CaosRect rect) const {
    RECT native{rect.left, rect.top, rect.right, rect.bottom};
    const_cast<C1CaosConsoleDialog*>(this)->ScreenToClient(&native);
    return to_caos_rect(native);
}

creatures1::ui::CaosRect C1CaosConsoleDialog::dialog_screen_rect() const {
    RECT rect{};
    const_cast<C1CaosConsoleDialog*>(this)->GetWindowRect(&rect);
    return to_caos_rect(rect);
}

bool C1CaosConsoleDialog::main_window_client_rect_in_screen(
    creatures1::ui::CaosRect& rect) const {
    CWnd* main_window = AfxGetMainWnd();
    if (main_window == nullptr || main_window->GetSafeHwnd() == nullptr) {
        return false;
    }
    RECT native{};
    main_window->GetClientRect(&native);
    main_window->ClientToScreen(&native);
    rect = to_caos_rect(native);
    return true;
}

void C1CaosConsoleDialog::position_dialog(long x, long y) {
    SetWindowPos(nullptr, static_cast<int>(x), static_cast<int>(y), 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void C1CaosConsoleDialog::set_always_on_top_checked(bool checked) {
    CWnd* control = GetDlgItem(kAlwaysOnTopCheckBox);
    if (control != nullptr) {
        control->SendMessageA(BM_SETCHECK,
                              checked ? BST_CHECKED : BST_UNCHECKED);
    }
}

void C1CaosConsoleDialog::apply_always_on_top(bool enabled) {
    SetWindowPos(enabled ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE);
}

void C1CaosConsoleDialog::focus_command_input() {
    CWnd* control = GetDlgItem(kCommandInputEdit);
    if (control != nullptr) {
        control->SetFocus();
    }
}

std::string C1CaosConsoleDialog::command_input_text() const {
    CString value;
    const_cast<C1CaosConsoleDialog*>(this)->GetDlgItemTextA(kCommandInputEdit,
                                                            value);
    return value.GetString();
}

void C1CaosConsoleDialog::echo_submitted_command(std::string_view command) {
    std::string line(command);
    line += "\r\n";
    creatures1::ui::append_caos_output(*this, line);
}

creatures1::ui::CaosExecutionResult C1CaosConsoleDialog::execute_command(
    std::string_view command) {
    creatures1::ui::CaosExecutionResult result;
    if (frame_ == nullptr) {
        return result;
    }

    // Reuse the pipe server's CAOS dispatch: the console is a second front end
    // onto the same interpreter, not a separate execution path.
    const std::string response =
        static_cast<creatures1::application::MainFramePipeServerPlatform&>(
            *frame_)
            .dispatch_pipe_command(command);

    const std::size_t separator = response.find(kRecordSeparator);
    const std::string status = response.substr(0, separator);
    result.succeeded = status == "OK";
    if (separator != std::string::npos) {
        result.output = response.substr(separator + 1);
    }
    return result;
}

void C1CaosConsoleDialog::set_control_text(creatures1::ui::CaosControl control,
                                           std::string_view text) {
    const std::string value(text);
    SetDlgItemTextA(static_cast<int>(control_id(control)), value.c_str());
}

std::size_t C1CaosConsoleDialog::output_text_length() const {
    CWnd* control = const_cast<C1CaosConsoleDialog*>(this)->GetDlgItem(
        kOutputEdit);
    if (control == nullptr) {
        return 0;
    }
    const int length = control->GetWindowTextLengthA();
    return length < 0 ? 0 : static_cast<std::size_t>(length);
}

std::string C1CaosConsoleDialog::output_text() const {
    CString value;
    const_cast<C1CaosConsoleDialog*>(this)->GetDlgItemTextA(kOutputEdit, value);
    return value.GetString();
}

void C1CaosConsoleDialog::select_output(std::size_t start, std::size_t end) {
    CWnd* control = GetDlgItem(kOutputEdit);
    if (control != nullptr) {
        control->SendMessageA(EM_SETSEL, static_cast<WPARAM>(start),
                              static_cast<LPARAM>(end));
    }
}

void C1CaosConsoleDialog::replace_output_selection(std::string_view text) {
    CWnd* control = GetDlgItem(kOutputEdit);
    if (control != nullptr) {
        const std::string value(text);
        control->SendMessageA(EM_REPLACESEL, FALSE,
                              reinterpret_cast<LPARAM>(value.c_str()));
    }
}

void C1CaosConsoleDialog::move_output_caret_to_end() {
    CWnd* control = GetDlgItem(kOutputEdit);
    if (control == nullptr) {
        return;
    }
    const int length = control->GetWindowTextLengthA();
    control->SendMessageA(EM_SETSEL, static_cast<WPARAM>(length),
                          static_cast<LPARAM>(length));
    control->SendMessageA(EM_SCROLLCARET, 0, 0);
}

void C1CaosConsoleDialog::set_control_position(
    creatures1::ui::CaosControl control, long x, long y, long width,
    long height, unsigned flags) {
    CWnd* item = GetDlgItem(static_cast<int>(control_id(control)));
    if (item != nullptr) {
        item->MoveWindow(static_cast<int>(x), static_cast<int>(y),
                         static_cast<int>(width), static_cast<int>(height),
                         flags != 0 ? TRUE : FALSE);
    }
}

void C1CaosConsoleDialog::invalidate_dialog() { Invalidate(TRUE); }

void C1CaosConsoleDialog::set_minimum_tracking_size(long width, long height) {
    if (pending_min_max_info_ != nullptr) {
        pending_min_max_info_->ptMinTrackSize.x = static_cast<LONG>(width);
        pending_min_max_info_->ptMinTrackSize.y = static_cast<LONG>(height);
    }
}

void C1CaosConsoleDialog::default_window_message() { Default(); }

void C1CaosConsoleDialog::handle_console_shortcut() {
    // Ctrl+Shift+C toggles the console; from inside it that means closing.
    DestroyWindow();
}

bool C1CaosConsoleDialog::default_pre_translate(
    const creatures1::ui::CaosKeyMessage& message) {
    static_cast<void>(message);
    return false;
}

// --- CaosScriptLoadHost ----------------------------------------------------

bool C1CaosConsoleDialog::choose_script_file(std::string& selected_path) {
    // Native (LoadCaosScriptFromFile @ 0x0040f380) passes 0x1004 and so lets
    // the dialog change the current directory; LibreCreatures adds
    // OFN_NOCHANGEDIR so relative paths stay anchored to the Main Directory.
    CFileDialog dialog(TRUE, "cos", nullptr,
                       OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR,
                       "CAOS scripts (*.cos)|*.cos|All files (*.*)|*.*||",
                       this);
    if (dialog.DoModal() != IDOK) {
        return false;
    }
    selected_path = dialog.GetPathName().GetString();
    return true;
}

bool C1CaosConsoleDialog::read_script_lines(std::string_view selected_path,
                                            std::vector<std::string>& lines) {
    const std::string path(selected_path);
    std::ifstream stream(path.c_str());
    if (!stream.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return true;
}

void C1CaosConsoleDialog::report_script_open_failure() {
    AfxMessageBox("Could not open the selected CAOS script.", MB_OK, 0);
}

void C1CaosConsoleDialog::set_script_editor_text(std::string_view text) {
    set_control_text(creatures1::ui::CaosControl::command_input, text);
}

void C1CaosConsoleDialog::focus_script_editor() { focus_command_input(); }

void C1CaosConsoleDialog::select_all_script_editor_text() {
    CWnd* control = GetDlgItem(kCommandInputEdit);
    if (control != nullptr) {
        control->SendMessageA(EM_SETSEL, 0, -1);
    }
}

// --- message handlers ------------------------------------------------------

void C1CaosConsoleDialog::OnSend() {
    creatures1::ui::submit_caos_command(state_, *this);
}

void C1CaosConsoleDialog::OnClear() {
    creatures1::ui::clear_caos_output(*this);
}

void C1CaosConsoleDialog::OnLoad() {
    creatures1::ui::load_caos_script_from_file(*this);
}

void C1CaosConsoleDialog::OnToggleAlwaysOnTop() {
    creatures1::ui::toggle_caos_always_on_top(state_, *this);
}

void C1CaosConsoleDialog::OnSize(UINT size_type, int client_width,
                                 int client_height) {
    CDialog::OnSize(size_type, client_width, client_height);
    creatures1::ui::resize_caos_console(state_, *this, size_type,
                                        client_width, client_height);
}

void C1CaosConsoleDialog::OnGetMinMaxInfo(MINMAXINFO* min_max_info) {
    pending_min_max_info_ = min_max_info;
    creatures1::ui::apply_caos_minimum_size(*this);
    pending_min_max_info_ = nullptr;
}

BOOL C1CaosConsoleDialog::PreTranslateMessage(MSG* message) {
    creatures1::ui::CaosKeyMessage key{};
    key.message = message->message;
    key.key = static_cast<std::uint32_t>(message->wParam);
    key.control_down = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    key.shift_down = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
    CWnd* input = GetDlgItem(kCommandInputEdit);
    key.targets_command_input =
        input != nullptr && message->hwnd == input->GetSafeHwnd();

    if (creatures1::ui::translate_caos_key(state_, *this, key)) {
        return TRUE;
    }
    return CDialog::PreTranslateMessage(message);
}

} // namespace creatures1::platform
