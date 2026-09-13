#include "windows_shell.hpp"

#include <algorithm>
#include <cstdio>

namespace creatures1::platform {
namespace {

// The native keeps the singleton in g_debug_console_dialog; every logging
// call site tests that pointer before formatting anything.
C1DebugConsoleDialog* g_debug_console_dialog = nullptr;

// The console's pause control acts on the world, which the document owns.
C1WindowsDocument* active_c1_document() {
    C1MainFrame* frame = active_main_frame();
    return frame == nullptr
               ? nullptr
               : DYNAMIC_DOWNCAST(C1WindowsDocument, frame->GetActiveDocument());
}

constexpr UINT kLogFlushTimerId = 2;
constexpr UINT kWorldUpdateTimerId = 3;
// DebugLog trims the console buffer once it passes this many characters.
constexpr std::size_t kConsoleTextCapacity = 0x8000;

} // namespace

C1DebugConsoleDialog* active_debug_console() { return g_debug_console_dialog; }

BEGIN_MESSAGE_MAP(C1DebugConsoleDialog, CDialog)
    ON_BN_CLICKED(C1DebugConsoleDialog::kClearLogButton, OnClearLog)
    ON_BN_CLICKED(C1DebugConsoleDialog::kPauseCheckBox, OnTogglePause)
    ON_BN_CLICKED(C1DebugConsoleDialog::kFilterCheckBox, OnToggleFilter)
    ON_BN_CLICKED(C1DebugConsoleDialog::kWholeLogButton, OnCopyWholeLog)
    ON_BN_CLICKED(C1DebugConsoleDialog::kThisPageButton, OnCopyThisPage)
    ON_BN_CLICKED(C1DebugConsoleDialog::kMirrorButton, OnToggleMirror)
    ON_BN_CLICKED(C1DebugConsoleDialog::kCloseButton, OnCloseConsole)
    ON_EN_CHANGE(C1DebugConsoleDialog::kFilterTextEdit, OnFilterTextChanged)
END_MESSAGE_MAP()

C1DebugConsoleDialog::C1DebugConsoleDialog() : CDialog(144, nullptr) {}

C1DebugConsoleDialog::~C1DebugConsoleDialog() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
    if (g_debug_console_dialog == this) {
        g_debug_console_dialog = nullptr;
    }
}

bool C1DebugConsoleDialog::create_modeless() {
    if (Create(MAKEINTRESOURCEA(144), nullptr) == FALSE) {
        return false;
    }
    g_debug_console_dialog = this;
    ShowWindow(SW_SHOW);
    return true;
}

void ensure_debug_console_dialog() {
    if (g_debug_console_dialog != nullptr) {
        // The native reopens a closed stream and re-shows the existing dialog
        // rather than constructing a second one.
        g_debug_console_dialog->ShowWindow(SW_SHOW);
        return;
    }
    auto* dialog = new C1DebugConsoleDialog();
    if (!dialog->create_modeless()) {
        delete dialog;
    }
}

BOOL C1DebugConsoleDialog::OnInitDialog() {
    creatures1::ui::initialise_debug_console(state_, *this);
    return TRUE;
}

void C1DebugConsoleDialog::PostNcDestroy() {
    if (g_debug_console_dialog == this) {
        g_debug_console_dialog = nullptr;
    }
    delete this;
}

void C1DebugConsoleDialog::DoDataExchange(CDataExchange* exchange) {
    exchange_ = exchange;
    creatures1::ui::exchange_debug_console_data(
        state_, *this, exchange->m_bSaveAndValidate != FALSE);
    exchange_ = nullptr;
}

// --- DebugConsoleHost ------------------------------------------------------

void C1DebugConsoleDialog::initialise_base_dialog() {
    CDialog::OnInitDialog();
}

void C1DebugConsoleDialog::set_checkbox_checked(std::uint32_t control_id,
                                                bool checked) {
    CWnd* control = GetDlgItem(static_cast<int>(control_id));
    if (control != nullptr) {
        control->SendMessageA(BM_SETCHECK, checked ? BST_CHECKED
                                                   : BST_UNCHECKED);
    }
}

void C1DebugConsoleDialog::set_dialog_always_on_top(bool enabled) {
    SetWindowPos(enabled ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE);
}

void C1DebugConsoleDialog::update_dialog_data(bool save_and_validate) {
    UpdateData(save_and_validate ? TRUE : FALSE);
}

void C1DebugConsoleDialog::bind_control(std::uint32_t control_id) {
    if (exchange_ == nullptr) {
        return;
    }
    switch (control_id) {
    case kMirrorButton:
        DDX_Control(exchange_, kMirrorButton, mirror_button_);
        break;
    case kTopmostCheckBox:
        DDX_Control(exchange_, kTopmostCheckBox, topmost_checkbox_);
        break;
    case kFilterCheckBox:
        DDX_Control(exchange_, kFilterCheckBox, filter_checkbox_);
        break;
    case kFilterTextEdit:
        DDX_Control(exchange_, kFilterTextEdit, filter_text_edit_);
        break;
    case kPauseCheckBox:
        DDX_Control(exchange_, kPauseCheckBox, pause_checkbox_);
        break;
    case kWorldUpdateControl:
        DDX_Control(exchange_, kWorldUpdateControl, world_update_control_);
        break;
    case kStepFunctionsList:
        DDX_Control(exchange_, kStepFunctionsList, step_functions_list_);
        break;
    case kLogOutputEdit:
        DDX_Control(exchange_, kLogOutputEdit, log_output_edit_);
        break;
    default:
        break;
    }
}

void C1DebugConsoleDialog::exchange_log_output_text(std::string& value,
                                                    bool save_and_validate) {
    if (exchange_ == nullptr) {
        return;
    }
    if (!save_and_validate) {
        log_output_binding_ = value.c_str();
    }
    DDX_Text(exchange_, kLogOutputEdit, log_output_binding_);
    if (save_and_validate) {
        value = log_output_binding_.GetString();
    }
}

void C1DebugConsoleDialog::exchange_world_update_paused(
    bool& value, bool save_and_validate) {
    if (exchange_ == nullptr) {
        return;
    }
    if (!save_and_validate) {
        world_update_paused_binding_ = value ? TRUE : FALSE;
    }
    DDX_Check(exchange_, kPauseCheckBox, world_update_paused_binding_);
    if (save_and_validate) {
        value = world_update_paused_binding_ != FALSE;
    }
}

void C1DebugConsoleDialog::set_log_output_text(std::string_view text) {
    if (log_output_edit_.GetSafeHwnd() != nullptr) {
        const std::string value(text);
        log_output_edit_.SetWindowTextA(value.c_str());
    }
}

void C1DebugConsoleDialog::move_log_output_caret_to_end() {
    if (log_output_edit_.GetSafeHwnd() != nullptr) {
        const int length = log_output_edit_.GetWindowTextLengthA();
        log_output_edit_.SetSel(length, length);
    }
}

bool C1DebugConsoleDialog::pause_checkbox_checked() const {
    return pause_checkbox_.GetSafeHwnd() != nullptr &&
           const_cast<CButton&>(pause_checkbox_).GetCheck() == BST_CHECKED;
}

void C1DebugConsoleDialog::service_world_update_timer() {
    // DebugConsoleDialog::ToggleWorldUpdatePause @ 0x00410870 calls the
    // DOCUMENT's SFCDoc::ServiceWorldUpdateTimer, not a timer of its own.
    // CWnd::KillTimer here killed a timer on the console dialog and left the
    // world running.
    C1WindowsDocument* document = active_c1_document();
    if (document != nullptr) {
        creatures1::application::service_world_update_timer(*document);
    }
}

void C1DebugConsoleDialog::set_world_update_control_enabled(bool enabled) {
    if (world_update_control_.GetSafeHwnd() != nullptr) {
        world_update_control_.EnableWindow(enabled ? TRUE : FALSE);
    }
}

void C1DebugConsoleDialog::arm_world_update_timer() {
    // Likewise the resume half: ArmWorldUpdateTimer @ 0x004337e0 on the
    // document, which re-installs the main frame's timer at the interval the
    // pause preserved -- not a fixed 1ms timer on this dialog.
    C1WindowsDocument* document = active_c1_document();
    if (document != nullptr) {
        creatures1::application::arm_world_update_timer(*document);
    }
}

bool C1DebugConsoleDialog::debug_log_category_enabled(
    std::uint32_t category) const {
    return debug_log_state_ != nullptr &&
           (debug_log_state_->category_mask & category) != 0;
}

void C1DebugConsoleDialog::disable_debug_log_category(std::uint32_t category) {
    if (debug_log_state_ != nullptr) {
        debug_log_state_->category_mask &= ~category;
    }
}

std::string C1DebugConsoleDialog::read_filter_control_text() const {
    if (filter_text_edit_.GetSafeHwnd() == nullptr) {
        return std::string();
    }
    CString value;
    const_cast<CEdit&>(filter_text_edit_).GetWindowTextA(value);
    return value.GetString();
}

void C1DebugConsoleDialog::set_filter_control_enabled(bool enabled) {
    if (filter_text_edit_.GetSafeHwnd() != nullptr) {
        filter_text_edit_.EnableWindow(enabled ? TRUE : FALSE);
    }
}

std::size_t C1DebugConsoleDialog::first_visible_log_line() const {
    if (log_output_edit_.GetSafeHwnd() == nullptr) {
        return 0;
    }
    const int line = const_cast<CEdit&>(log_output_edit_).GetFirstVisibleLine();
    return line < 0 ? 0 : static_cast<std::size_t>(line);
}

std::size_t C1DebugConsoleDialog::log_line_start_offset(
    std::size_t line) const {
    if (log_output_edit_.GetSafeHwnd() == nullptr) {
        return 0;
    }
    const int offset = const_cast<CEdit&>(log_output_edit_)
                           .LineIndex(static_cast<int>(line));
    return offset < 0 ? 0 : static_cast<std::size_t>(offset);
}

void C1DebugConsoleDialog::select_log_output(std::size_t start,
                                             std::size_t end) {
    if (log_output_edit_.GetSafeHwnd() == nullptr) {
        return;
    }
    // The recovered policy passes (std::size_t)-1 for "to the end", which is
    // the -1 the native hands to EM_SETSEL.
    const int native_end =
        end == static_cast<std::size_t>(-1) ? -1 : static_cast<int>(end);
    log_output_edit_.SetSel(static_cast<int>(start), native_end);
}

void C1DebugConsoleDialog::scroll_log_output_caret() {
    if (log_output_edit_.GetSafeHwnd() != nullptr) {
        log_output_edit_.SendMessageA(EM_SCROLLCARET, 0, 0);
    }
}

void C1DebugConsoleDialog::copy_log_output_selection() {
    if (log_output_edit_.GetSafeHwnd() != nullptr) {
        log_output_edit_.Copy();
    }
}

bool C1DebugConsoleDialog::log_file_is_open() const {
    return log_file_.is_open();
}

void C1DebugConsoleDialog::open_log_file(std::string_view path) {
    const std::string filename(path);
    log_file_.open(filename.c_str(), std::ios::out | std::ios::app);
}

void C1DebugConsoleDialog::close_log_file() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void C1DebugConsoleDialog::set_mirror_button_checked(bool checked) {
    if (mirror_button_.GetSafeHwnd() != nullptr) {
        mirror_button_.SetCheck(checked ? BST_CHECKED : BST_UNCHECKED);
    }
}

void C1DebugConsoleDialog::set_dialog_title(std::string_view title) {
    const std::string text(title);
    SetWindowTextA(text.c_str());
}

void C1DebugConsoleDialog::start_log_flush_timer(std::uint32_t interval_ms) {
    SetTimer(kLogFlushTimerId, static_cast<UINT>(interval_ms), nullptr);
}

void C1DebugConsoleDialog::stop_log_flush_timer() {
    KillTimer(kLogFlushTimerId);
}

void C1DebugConsoleDialog::flash_dialog() { FlashWindow(TRUE); }

// --- DebugLogHost ----------------------------------------------------------

std::string C1DebugConsoleDialog::format_message(const char* format,
                                                 std::va_list arguments) {
    if (format == nullptr) {
        return std::string();
    }
    std::va_list copy;
    va_copy(copy, arguments);
    const int needed = _vscprintf(format, copy);
    va_end(copy);
    if (needed <= 0) {
        return std::string();
    }
    std::string value(static_cast<std::size_t>(needed), '\0');
    vsnprintf_s(value.data(), value.size() + 1, _TRUNCATE, format, arguments);
    return value;
}

std::string C1DebugConsoleDialog::format_line_prefix(
    std::string_view category_tag, std::uint32_t world_tick) {
    char buffer[64] = {};
    const std::string tag(category_tag);
    _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%s %u: ", tag.c_str(),
                world_tick);
    return buffer;
}

std::string C1DebugConsoleDialog::category_tag(
    std::size_t highest_set_bit) const {
    char buffer[32] = {};
    _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "[%u]",
                static_cast<unsigned>(highest_set_bit));
    return buffer;
}

std::uint32_t C1DebugConsoleDialog::world_tick() const { return 0; }

bool C1DebugConsoleDialog::debug_category_enabled(
    std::uint32_t category_mask) const {
    return debug_log_state_ != nullptr &&
           debug_log_state_->logging_is_enabled &&
           (debug_log_state_->category_mask & category_mask) != 0;
}

void C1DebugConsoleDialog::handle_oversized_log_line(std::string_view line,
                                                     std::size_t capacity) {
    static_cast<void>(line);
    static_cast<void>(capacity);
}

bool C1DebugConsoleDialog::log_filter_accepts(std::string_view line) const {
    if (!state_.filter_enabled || state_.filter_text.empty()) {
        return true;
    }
    return line.find(state_.filter_text) != std::string_view::npos;
}

void C1DebugConsoleDialog::append_console_text(std::string_view line) {
    state_.accumulated_log_text.append(line);
}

std::size_t C1DebugConsoleDialog::console_text_length() const {
    return state_.accumulated_log_text.size();
}

void C1DebugConsoleDialog::retain_newest_console_text(
    std::size_t character_count) {
    const std::size_t size = state_.accumulated_log_text.size();
    if (character_count >= size) {
        return;
    }
    state_.accumulated_log_text.erase(0, size - character_count);
}

void C1DebugConsoleDialog::mark_console_log_dirty() { state_.log_dirty = true; }

void C1DebugConsoleDialog::refresh_console_output() {
    creatures1::ui::refresh_debug_log(state_, *this);
}

void C1DebugConsoleDialog::mirror_to_log_file(std::string_view line) {
    if (log_file_.is_open()) {
        log_file_.write(line.data(), static_cast<std::streamsize>(line.size()));
    }
}

// --- command handlers ------------------------------------------------------

void C1DebugConsoleDialog::OnClearLog() {
    creatures1::ui::clear_debug_log(state_, *this);
}

void C1DebugConsoleDialog::OnTogglePause() {
    creatures1::ui::toggle_world_update_pause(state_, *this);
}

void C1DebugConsoleDialog::OnFilterTextChanged() {
    creatures1::ui::copy_filter_control_text(state_, *this);
}

void C1DebugConsoleDialog::OnToggleFilter() {
    creatures1::ui::toggle_filter_enabled(state_, *this);
}

void C1DebugConsoleDialog::OnCopyWholeLog() {
    creatures1::ui::copy_all_debug_log(*this);
}

void C1DebugConsoleDialog::OnCopyThisPage() {
    creatures1::ui::copy_visible_debug_log(*this);
}

void C1DebugConsoleDialog::OnToggleMirror() {
    creatures1::ui::toggle_log_file_mirroring(*this);
}

void C1DebugConsoleDialog::OnCloseConsole() { DestroyWindow(); }

} // namespace creatures1::platform
