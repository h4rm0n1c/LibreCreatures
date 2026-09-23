#include "windows_shell.hpp"

namespace creatures1::platform {
namespace {

constexpr UINT kTipArtworkResource = 103;
constexpr UINT kTipHeaderResource = 109;

}  // namespace

C1NativeTipFile::C1NativeTipFile(std::string path) : path_(std::move(path)) {
    file_ = std::fopen(path_.c_str(), "r");
}

C1NativeTipFile::~C1NativeTipFile() {
    if (file_ != nullptr) {
        std::fclose(file_);
    }
}

bool C1NativeTipFile::is_open() const { return file_ != nullptr; }


bool C1NativeTipFile::read_line(std::string& line) {
    line.clear();
    if (file_ == nullptr) {
        return false;
    }
    std::array<char, 1024> buffer{};
    if (std::fgets(buffer.data(), static_cast<int>(buffer.size()), file_) ==
        nullptr) {
        return false;
    }
    line.assign(buffer.data());
    while (!line.empty() &&
           (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    return true;
}

bool C1NativeTipFile::rewind() {
    if (file_ == nullptr) {
        return false;
    }
    std::rewind(file_);
    return true;
}

bool C1NativeTipFile::seek(std::int32_t position) {
    return file_ != nullptr && std::fseek(file_, position, SEEK_SET) == 0;
}

std::int32_t C1NativeTipFile::position() const {
    if (file_ == nullptr) {
        return 0;
    }
    const long offset = std::ftell(file_);
    return offset < 0 ? 0 : static_cast<std::int32_t>(offset);
}

std::string C1NativeTipFile::modification_timestamp() const {
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExA(path_.c_str(), GetFileExInfoStandard,
                              &attributes)) {
        return "NULL";
    }
    return std::to_string(attributes.ftLastWriteTime.dwHighDateTime) +
           ":" + std::to_string(attributes.ftLastWriteTime.dwLowDateTime);
}

C1TipDialogPlatform::C1TipDialogPlatform(std::string primary_directory, CWnd* owner) : primary_directory_(std::move(primary_directory)), owner_(owner) {}


bool C1TipDialogPlatform::read_registry_dword(std::string_view name, std::uint32_t& value) const {
    HKEY key = nullptr;
    if (!open_key(KEY_READ, key)) {
        return false;
    }
    const bool present = creatures1::platform::read_registry_dword(key,
                                                std::string(name).c_str(),
                                                value);
    RegCloseKey(key);
    return present;
}

void C1TipDialogPlatform::write_registry_dword(std::string_view name, std::uint32_t value) {
    HKEY key = nullptr;
    if (open_key(KEY_SET_VALUE, key)) {
        creatures1::platform::write_registry_dword(key, std::string(name).c_str(), value);
        RegCloseKey(key);
    }
}

bool C1TipDialogPlatform::read_registry_string(std::string_view name, std::string& value) const {
    HKEY key = nullptr;
    if (!open_key(KEY_READ, key)) {
        return false;
    }
    const bool present = creatures1::platform::read_registry_string(
        key, std::string(name).c_str(), value);
    RegCloseKey(key);
    return present;
}

void C1TipDialogPlatform::write_registry_string(std::string_view name, std::string_view value) {
    HKEY key = nullptr;
    if (!open_key(KEY_SET_VALUE, key)) {
        return;
    }
    const std::string text(value);
    RegSetValueExA(key, std::string(name).c_str(), 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(text.c_str()),
                   static_cast<DWORD>(text.size() + 1));
    RegCloseKey(key);
}

std::string C1TipDialogPlatform::primary_resource_directory() const {
    return primary_directory_;
}

std::unique_ptr<creatures1::ui::TipFile> C1TipDialogPlatform::open_tips_file( std::string_view path) const {
    auto file = std::make_unique<C1NativeTipFile>(std::string(path));
    return file->is_open()
               ? std::unique_ptr<creatures1::ui::TipFile>(std::move(file))
               : nullptr;
}

std::string C1TipDialogPlatform::load_string(std::uint32_t resource_id) const {
    CStringA text;
    return text.LoadStringA(static_cast<UINT>(resource_id))
               ? std::string(text.GetString())
               : std::string{};
}

void C1TipDialogPlatform::show_resource_message(std::uint32_t resource_id) {
    AfxMessageBox(static_cast<UINT>(resource_id));
}

void C1TipDialogPlatform::forward_default_timer() {
    if (native_dialog_ != nullptr) {
        native_dialog_->forward_default_message();
    }
}


bool C1TipDialogPlatform::is_tip_control(std::uint32_t control_id) const {
    // CTipDlg::OnCtlColor @ 0x00443ac0 special-cases only control 0x3ec
    // (1004, the tip-text static). Control 1000 (the artwork bitmap) and
    // everything else falls through to the real default handler.
    return control_id == 1004;
}

void C1TipDialogPlatform::use_tip_control_brush() {
    pending_brush_ = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
}

void C1TipDialogPlatform::forward_default_control_color() {
    // Native: `CWnd::OnCtlColor((CWnd *)this,param_1,param_2,param_3);
    // return;` -- forwards to the base class and returns *its* brush, not
    // a fixed one. Every other control (the checkbox, the two buttons,
    // the artwork bitmap) gets whatever CDialog's own default produces.
    pending_brush_ =
        native_dialog_ != nullptr
            ? native_dialog_->ForwardDefaultCtlColor(
                  ctl_color_dc_, ctl_color_control_, ctl_color_type_)
            : nullptr;
}

HBRUSH C1TipDialogPlatform::requested_brush() const {
    return pending_brush_;
}

void C1TipDialogPlatform::set_ctl_color_context(CDC* device_context,
                                                CWnd* control,
                                                UINT control_type) {
    ctl_color_dc_ = device_context;
    ctl_color_control_ = control;
    ctl_color_type_ = control_type;
}

void C1TipDialogPlatform::bind(C1TipDialogWindow* dialog, creatures1::ui::TipDialog* semantic_dialog) {
    native_dialog_ = dialog;
    semantic_dialog_ = semantic_dialog;
}

CWnd* C1TipDialogPlatform::owner() const { return owner_; }


bool C1TipDialogPlatform::open_key(REGSAM access, HKEY& key) const {
    return open_c1_secondary_registry(key, access);
}

C1TipDialogWindow::C1TipDialogWindow(C1TipDialogPlatform& platform, creatures1::ui::TipDialog& dialog) : CDialog(MAKEINTRESOURCEA(106), platform.owner()), platform_(platform), dialog_(dialog) {}


BOOL C1TipDialogWindow::OnInitDialog() {
    const BOOL result = CDialog::OnInitDialog();
    platform_.bind(this, &dialog_);
    platform_.update_data(false);
    platform_.paint_tip(dialog_.current_tip_text());
    SetTimer(1, 1000, nullptr);
    return result;
}

afx_msg void C1TipDialogWindow::OnTimer(UINT_PTR timer_id) {
    if (timer_id == 1) {
        dialog_.on_timer();
    }
    CDialog::OnTimer(timer_id);
}

afx_msg void C1TipDialogWindow::OnNextTip() {
    dialog_.on_next_tip();
    platform_.paint_tip(dialog_.current_tip_text());
}

afx_msg void C1TipDialogWindow::OnPaint() {
    CPaintDC paint_dc(this);
    dialog_.on_paint(&paint_dc);
}

afx_msg HBRUSH C1TipDialogWindow::OnCtlColor(CDC* device_context, CWnd* control, UINT control_type) {
    platform_.set_ctl_color_context(device_context, control, control_type);
    dialog_.on_ctl_color(control == nullptr
                             ? 0
                             : static_cast<std::uint32_t>(
                                   control->GetDlgCtrlID()));
    return platform_.requested_brush();
}

void C1TipDialogWindow::OnOK() {
    platform_.update_data(true);
    dialog_.on_ok();
}

BEGIN_MESSAGE_MAP(C1TipDialogWindow, CDialog)
    ON_WM_TIMER()
    ON_BN_CLICKED(1002, &C1TipDialogWindow::OnNextTip)
    ON_WM_PAINT()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

void C1TipDialogPlatform::update_data(bool save_and_validate) {
    if (native_dialog_ == nullptr || semantic_dialog_ == nullptr) {
        return;
    }
    // CTipDlg's DoDataExchange @004439c0 is DDX_Check(1001, show_at_startup),
    // which talks to the control through its window handle.  Downcasting
    // GetDlgItem to CButton could never work here: an unsubclassed control
    // comes back as a temporary CWnd, so the checkbox was neither set when
    // the dialog opened nor read when it closed.
    constexpr int kShowTipsAtStartupCheckbox = 1001;
    if (native_dialog_->GetDlgItem(kShowTipsAtStartupCheckbox) == nullptr) {
        return;
    }
    if (save_and_validate) {
        semantic_dialog_->set_show_at_startup(
            native_dialog_->IsDlgButtonChecked(kShowTipsAtStartupCheckbox) !=
            BST_UNCHECKED);
    } else {
        native_dialog_->CheckDlgButton(
            kShowTipsAtStartupCheckbox,
            semantic_dialog_->show_at_startup() ? BST_CHECKED : BST_UNCHECKED);
    }
}

void C1TipDialogPlatform::paint_tip(std::string_view text,
                                    void* paint_device_context) {
    if (native_dialog_ == nullptr) {
        return;
    }

    if (paint_device_context == nullptr) {
        native_dialog_->SetDlgItemTextA(1004, std::string(text).c_str());
        return;
    }

    auto* paint_dc = static_cast<CDC*>(paint_device_context);
    CWnd* tip_control = native_dialog_->GetDlgItem(1000);
    if (paint_dc == nullptr || tip_control == nullptr) {
        return;
    }

    CRect tip_control_rect;
    tip_control->GetWindowRect(&tip_control_rect);
    native_dialog_->ScreenToClient(&tip_control_rect);
    ::FillRect(paint_dc->GetSafeHdc(), &tip_control_rect,
               static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    const LPCSTR bitmap_resource = MAKEINTRESOURCEA(kTipArtworkResource);
    const HINSTANCE resource_instance =
        AfxFindResourceHandle(bitmap_resource, RT_BITMAP);
    const HBITMAP bitmap_handle =
        ::LoadBitmapA(resource_instance, bitmap_resource);
    if (bitmap_handle == nullptr) {
        return;
    }

    CBitmap bitmap;
    bitmap.Attach(bitmap_handle);
    BITMAP bitmap_info{};
    if (bitmap.GetBitmap(&bitmap_info) == 0) {
        return;
    }

    CDC memory_dc;
    if (!memory_dc.CreateCompatibleDC(paint_dc)) {
        return;
    }
    CBitmap* previous_bitmap = memory_dc.SelectObject(&bitmap);
    if (previous_bitmap == nullptr) {
        return;
    }

    tip_control_rect.bottom = tip_control_rect.top + bitmap_info.bmHeight;
    paint_dc->BitBlt(tip_control_rect.left, tip_control_rect.top,
                     tip_control_rect.Width(),
                     tip_control_rect.Height(), &memory_dc, 0, 0,
                     SRCCOPY);

    CStringA header;
    header.LoadStringA(kTipHeaderResource);
    tip_control_rect.left += bitmap_info.bmWidth;
    paint_dc->DrawTextA(header.GetString(), header.GetLength(),
                        &tip_control_rect, DT_SINGLELINE | DT_VCENTER);

    memory_dc.SelectObject(previous_bitmap);
}

int C1TipDialogPlatform::show_modal(creatures1::ui::TipDialog& dialog) {
    C1TipDialogWindow native_dialog(*this, dialog);
    return native_dialog.DoModal();
}

void C1TipDialogPlatform::close_dialog() {
    if (native_dialog_ != nullptr) {
        native_dialog_->EndDialog(IDOK);
    }
}

} // namespace creatures1::platform
