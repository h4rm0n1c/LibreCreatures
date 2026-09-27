// The Observation Kit's pages and dialogs.  The original's behaviour is in
// ../ORIGINAL.md; deviations are marked "Fix (bug N)".

#include "observation.hpp"
#include "observation_ids.hpp"

#include <algorithm>

namespace observation {

namespace {

// This kit's own marks for a creature about to give birth (an egg), near
// death (a headstone) and pregnant (the female symbol), drawn to fit `r`;
// the classic look uses the 1996 pictures instead.
void draw_status_glyph(CDC& dc, int kind, const CRect& r) {
    const int size = (std::min)(r.Width(), r.Height());
    const int x = r.left + (r.Width() - size) / 2, y = r.top + (r.Height() - size) / 2;
    const auto at = [&](double fx, double fy) { return CPoint(x + int(fx * size), y + int(fy * size)); };
    CGdiObject* old_brush = nullptr;
    CPen* old_pen = nullptr;
    if (kind == kIconBirth) {
        CBrush shell(RGB(236, 222, 186));
        CPen edge(PS_SOLID, 1, RGB(120, 96, 60));
        old_brush = dc.SelectObject(&shell);
        old_pen = dc.SelectObject(&edge);
        const CPoint a = at(0.22, 0.08), b = at(0.78, 0.94);
        dc.Ellipse(a.x, a.y, b.x, b.y);
        dc.SelectObject(old_brush);
        dc.SelectObject(old_pen);
    } else if (kind == kIconNearDeath) {
        CBrush stone(RGB(128, 134, 142));
        CPen edge(PS_SOLID, 1, RGB(70, 74, 80));
        old_brush = dc.SelectObject(&stone);
        old_pen = dc.SelectObject(&edge);
        const CPoint a = at(0.22, 0.10), b = at(0.78, 0.92);
        dc.RoundRect(a.x, a.y, b.x, b.y, size / 2, size / 2);
        dc.SelectObject(old_brush);
        dc.SelectObject(old_pen);
        const CPoint c = at(0.5, 0.45);
        const int arm = (std::max)(1, size / 8);
        dc.FillSolidRect(c.x - (std::max)(1, size / 16), c.y - arm * 2, (std::max)(2, size / 8), arm * 4,
                         RGB(40, 42, 46));
        dc.FillSolidRect(c.x - arm * 2 + 1, c.y - arm, arm * 4 - 1, (std::max)(2, size / 8),
                         RGB(40, 42, 46));
    } else if (kind == kIconPregnant) {
        CPen pen(PS_SOLID, (std::max)(1, size / 8), RGB(200, 60, 120));
        old_pen = dc.SelectObject(&pen);
        old_brush = dc.SelectStockObject(NULL_BRUSH);
        const CPoint a = at(0.25, 0.06), b = at(0.75, 0.56);
        dc.Ellipse(a.x, a.y, b.x, b.y);
        const CPoint top = at(0.5, 0.56), bottom = at(0.5, 0.96);
        dc.MoveTo(top);
        dc.LineTo(bottom);
        const CPoint left = at(0.3, 0.78), right = at(0.7, 0.78);
        dc.MoveTo(left);
        dc.LineTo(right);
        dc.SelectObject(old_brush);
        dc.SelectObject(old_pen);
    }
}

} // namespace

namespace {

CString sex_text(const std::string& field) {
    return c1kitshell::load_string(field == "1" ? kStringSexMale
                                                : kStringSexFemale);
}

} // namespace

// ===========================================================================
// Overview page
// ===========================================================================

BEGIN_MESSAGE_MAP(OverviewPage, CPropertyPage)
    ON_WM_SIZE()
END_MESSAGE_MAP()

OverviewPage::OverviewPage(ObservationSheet& sheet)
    : CPropertyPage(kDialogOverview), sheet_(sheet) {}

// InitializeOverviewDisplay @ 0x00405920.
BOOL OverviewPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    list_.SubclassDlgItem(kControlOverviewList, this);
    list_.SetExtendedStyle(list_.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
    // The egg, grave and sex marks: the 1996 strip, from the original kit
    // in the classic look; otherwise drawn here.
    {
        CBitmap strip;
        if (const HMODULE art = sheet_.art_module()) {
            strip.Attach(static_cast<HBITMAP>(LoadImage(
                art, MAKEINTRESOURCE(kBitmapListIcons), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)));
        } else {
            CClientDC screen(this);
            strip.CreateCompatibleBitmap(&screen, 64, 16);
            CDC dc;
            dc.CreateCompatibleDC(&screen);
            CBitmap* previous = dc.SelectObject(&strip);
            dc.FillSolidRect(0, 0, 64, 16, RGB(255, 255, 255));  // the mask colour
            for (int kind = kIconBirth; kind <= kIconPregnant; ++kind) {
                draw_status_glyph(dc, kind, CRect(kind * 16 + 1, 1, kind * 16 + 15, 15));
            }
            dc.SelectObject(previous);
        }
        BITMAP info = {};
        if (strip.GetSafeHandle() != nullptr && strip.GetBitmap(&info) != 0) {
            icons_.Create(16, info.bmHeight, ILC_COLOR24 | ILC_MASK, info.bmWidth / 16, 1);
            icons_.Add(&strip, RGB(255, 255, 255));
        }
    }
    list_.SetImageList(&icons_, LVSIL_SMALL);

    struct Column {
        unsigned title;
        int width;
    };
    // The original's columns (Name 85, Sex 30, Age 40, Pregnant 55, Life
    // Force 60, Medical 55 px) truncated their own headings; these fit them.
    // The moniker column stays 0 wide: it is the row key.
    const Column columns[] = {
        {kStringColumnName, 120},     {kStringColumnMoniker, 0},
        {kStringColumnSex, 40},       {kStringColumnAge, 55},
        {kStringColumnPregnant, 70},  {kStringColumnLifeForce, 70},
        {kStringColumnMedical, 90},  // then fitted to the page
    };
    int subitem = 0;
    for (const Column& column : columns) {
        list_.InsertColumn(subitem, c1kitshell::load_string(column.title),
                           LVCFMT_LEFT, column.width, subitem);
        ++subitem;
    }

    anchors_.capture(*this);
    anchors_.add(*this, kControlOverviewList,
                 c1kitshell::ControlAnchors::kGrowX |
                     c1kitshell::ControlAnchors::kGrowY);
    show(sheet_.records(), sheet_.settings());
    fit_last_column();
    return TRUE;
}

int OverviewPage::find_row(const CString& moniker) const {
    for (int item = 0; item < list_.GetItemCount(); ++item) {
        if (list_.GetItemText(item, 1) == moniker) {
            return item;
        }
    }
    return -1;
}

void OverviewPage::set_row(int item, const c1kit::OverviewRecord& record,
                           int icon) {
    list_.SetItem(item, 0, LVIF_TEXT | LVIF_IMAGE,
                  CString(record[c1kit::kOverviewName].c_str()), icon, 0, 0,
                  0);
    list_.SetItemText(item, 2, sex_text(record[c1kit::kOverviewSex]));
    list_.SetItemText(item, 3, CString(record[c1kit::kOverviewAge].c_str()));
    list_.SetItemText(item, 4,
                      CString(record[c1kit::kOverviewPregnancy].c_str()));
    list_.SetItemText(item, 5,
                      CString(record[c1kit::kOverviewLifeForce].c_str()));
    list_.SetItemText(item, 6, CString(record[c1kit::kOverviewMedical].c_str()));
}

// UpdateOverviewList @ 0x00405b00: update rows found by moniker, insert new
// ones, delete rows no longer reported.
void OverviewPage::show(const std::vector<c1kit::OverviewRecord>& records,
                        const AlertSettings& settings) {
    if (list_.GetSafeHwnd() == nullptr) {
        return;
    }
    std::vector<CString> present;
    present.reserve(records.size());
    for (const c1kit::OverviewRecord& record : records) {
        const CString moniker(record[c1kit::kOverviewMoniker].c_str());
        present.push_back(moniker);
        const int icon = row_icon(record, settings);
        int item = find_row(moniker);
        if (item < 0) {
            item = list_.InsertItem(list_.GetItemCount(),
                                    CString(record[c1kit::kOverviewName].c_str()),
                                    icon);
            list_.SetItemText(item, 1, moniker);
        }
        set_row(item, record, icon);
    }
    // Fix (bug 14): walk backwards, so deleting a row cannot skip the one
    // that slides into its place.
    for (int item = list_.GetItemCount() - 1; item >= 0; --item) {
        const CString moniker = list_.GetItemText(item, 1);
        bool reported = false;
        for (const CString& key : present) {
            reported = reported || key == moniker;
        }
        if (!reported) {
            list_.DeleteItem(item);
        }
    }
}

// The last column takes whatever width the others leave (not less than its
// starting width), so a wider window shows no empty strip.
void OverviewPage::fit_last_column() {
    if (list_.GetSafeHwnd() == nullptr) {
        return;
    }
    constexpr int kLastColumn = 6;
    constexpr int kLastColumnMinimum = 90;
    CRect client;
    list_.GetClientRect(&client);
    int others = 0;
    for (int column = 0; column < kLastColumn; ++column) {
        others += list_.GetColumnWidth(column);
    }
    const int remaining = client.Width() - others;
    list_.SetColumnWidth(kLastColumn, remaining > kLastColumnMinimum
                                          ? remaining : kLastColumnMinimum);
}

void OverviewPage::OnSize(UINT type, int cx, int cy) {
    CPropertyPage::OnSize(type, cx, cy);
    anchors_.apply(*this);
    fit_last_column();
}

// ===========================================================================
// Options page
// ===========================================================================

BEGIN_MESSAGE_MAP(WarnLevelEdit, CEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

// COverviewSelectionEdit::OnChar @ 0x004067e0: digits and backspace only.
void WarnLevelEdit::OnChar(UINT character, UINT repeat, UINT flags) {
    if ((character < '0' || character > '9') && character != '\b') {
        MessageBeep(MB_ICONASTERISK);
        return;
    }
    CEdit::OnChar(character, repeat, flags);
}

BEGIN_MESSAGE_MAP(OptionsPage, CPropertyPage)
    ON_BN_CLICKED(kControlClose, &OptionsPage::OnCloseKit)
    ON_BN_CLICKED(kControlOnTop, &OptionsPage::OnAlwaysOnTop)
    ON_BN_CLICKED(kControlAbout, &OptionsPage::OnAbout)
    ON_BN_CLICKED(kControlAlertNearDeath, &OptionsPage::OnSettingChanged)
    ON_BN_CLICKED(kControlAlertBirth, &OptionsPage::OnSettingChanged)
    ON_BN_CLICKED(kControlMessageBox, &OptionsPage::OnSettingChanged)
    ON_BN_CLICKED(kControlAlertPregnancy, &OptionsPage::OnSettingChanged)
    ON_EN_CHANGE(kControlWarnLevel, &OptionsPage::OnSettingChanged)
    ON_WM_SIZE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

OptionsPage::OptionsPage(ObservationSheet& sheet)
    : CPropertyPage(kDialogOptions), sheet_(sheet) {}

void OptionsPage::DoDataExchange(CDataExchange* exchange) {
    CPropertyPage::DoDataExchange(exchange);
    DDX_Control(exchange, kControlWarnSpin, spin_);
    DDX_Control(exchange, kControlWarnLevel, warn_edit_);
}

// InitializeSelectionDisplayState @ 0x00401560.
BOOL OptionsPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    warn_edit_.SetLimitText(2);
    spin_.SetRange(0, 99);
    anchors_.capture(*this);
    anchors_.add(*this, kControlOnTop, c1kitshell::ControlAnchors::kMoveY);
    anchors_.add(*this, kControlAbout,
                 c1kitshell::ControlAnchors::kMoveX |
                     c1kitshell::ControlAnchors::kMoveY);
    anchors_.add(*this, kControlClose,
                 c1kitshell::ControlAnchors::kMoveX |
                     c1kitshell::ControlAnchors::kMoveY);
    initialized_ = true;
    show_settings();
    return TRUE;
}

// The controls always show the sheet's settings, and every change is stored
// straight back to the sheet (the original stored them only on some events,
// and read the alerts' settings from these controls: bug 15).
void OptionsPage::show_settings() {
    if (!initialized_) {
        return;
    }
    showing_ = true;
    const AlertSettings& settings = sheet_.settings();
    CheckDlgButton(kControlOnTop, sheet_.always_on_top() ? BST_CHECKED
                                                         : BST_UNCHECKED);
    CheckDlgButton(kControlAlertNearDeath, settings.alert_near_death != 0);
    CheckDlgButton(kControlAlertPregnancy, settings.alert_on_pregnancy != 0);
    CheckDlgButton(kControlAlertBirth, settings.alert_on_birth != 0);
    CheckDlgButton(kControlMessageBox, settings.message_box != 0);
    CString level;
    level.Format(_T("%u"), settings.warn_level);
    warn_edit_.SetWindowText(level);
    showing_ = false;
}

void OptionsPage::store_settings() {
    if (!initialized_ || showing_ || GetSafeHwnd() == nullptr) {
        return;
    }
    AlertSettings settings = sheet_.settings();
    settings.alert_near_death =
        IsDlgButtonChecked(kControlAlertNearDeath) == BST_CHECKED;
    settings.alert_on_pregnancy =
        IsDlgButtonChecked(kControlAlertPregnancy) == BST_CHECKED;
    settings.alert_on_birth =
        IsDlgButtonChecked(kControlAlertBirth) == BST_CHECKED;
    settings.message_box = IsDlgButtonChecked(kControlMessageBox) == BST_CHECKED;
    CString text;
    warn_edit_.GetWindowText(text);
    settings.warn_level = static_cast<std::uint32_t>(_ttoi(text));
    sheet_.set_settings(settings);
}

BOOL OptionsPage::OnSetActive() {
    const BOOL result = CPropertyPage::OnSetActive();
    show_settings();
    return result;
}

BOOL OptionsPage::OnKillActive() {
    store_settings();
    return CPropertyPage::OnKillActive();
}

void OptionsPage::OnSettingChanged() {
    store_settings();
}

// ToggleOverviewAlwaysOnTop @ 0x00401340.
void OptionsPage::OnAlwaysOnTop() {
    sheet_.set_always_on_top(IsDlgButtonChecked(kControlOnTop) == BST_CHECKED);
}

// COverviewOptionsPage::OnAbout @ 0x00401370.
void OptionsPage::OnAbout() {
    AboutDialog about;
    about.DoModal();
}

// SendSelectedCreatureCommandAndQuit @ 0x00401300.
void OptionsPage::OnCloseKit() {
    store_settings();
    sheet_.request_game_quit();
}

void OptionsPage::OnSize(UINT type, int cx, int cy) {
    CPropertyPage::OnSize(type, cx, cy);
    anchors_.apply(*this);
}

void OptionsPage::OnDestroy() {
    store_settings();
    initialized_ = false;
    CPropertyPage::OnDestroy();
}

// ===========================================================================
// Dialogs
// ===========================================================================

BEGIN_MESSAGE_MAP(AlertWindow, CDialog)
    ON_WM_PAINT()
END_MESSAGE_MAP()

AlertWindow::AlertWindow(const CString& text, int alert_type, HMODULE art)
    : CDialog(kDialogAlert), text_(text), alert_type_(alert_type), art_(art) {}

std::vector<AlertWindow*> AlertWindow::open_;

// Centred over the kit, each further open alert stepped down and right so
// none hides another.  An alert still open with the same text is not
// repeated (the near-death alert recurs every 30 seconds while a creature
// stays low, and an unattended kit would otherwise pile them up).
void AlertWindow::open(CWnd& owner, const CString& text, int alert_type, HMODULE art) {
    for (const AlertWindow* existing : open_) {
        if (existing->text_ == text) {
            return;
        }
    }
    auto* alert = new AlertWindow(text, alert_type, art);
    if (!alert->Create(kDialogAlert, &owner)) {
        delete alert;
        return;
    }
    CRect kit;
    CRect box;
    owner.GetWindowRect(&kit);
    alert->GetWindowRect(&box);
    const int step = 24 * static_cast<int>(open_.size() % 8);
    alert->SetWindowPos(nullptr,
                        kit.left + (kit.Width() - box.Width()) / 2 + step,
                        kit.top + (kit.Height() - box.Height()) / 2 + step, 0,
                        0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    open_.push_back(alert);
    alert->ShowWindow(SW_SHOWNOACTIVATE);
}

// LoadOverviewModeBitmapDialog_OnInitDialog @ 0x00404df0: the face bitmap
// for the alert type, palette realised, window made topmost.
BOOL AlertWindow::OnInitDialog() {
    CDialog::OnInitDialog();
    SetDlgItemText(kControlAlertText, text_);
    const unsigned bitmap = alert_type_ == kAlertPregnancy ? kBitmapAlertPregnancy
                          : alert_type_ == kAlertBirth     ? kBitmapAlertBirth
                                                           : kBitmapAlertDeath;
    // The 1996 face in the classic look; otherwise OnPaint draws a mark.
    if (art_ != nullptr) {
        face_.load(bitmap, art_);
    }
    face_.realize(*this);
    SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    return TRUE;
}

void AlertWindow::OnOK() {
    DestroyWindow();
}

void AlertWindow::OnCancel() {
    DestroyWindow();
}

void AlertWindow::PostNcDestroy() {
    for (auto it = open_.begin(); it != open_.end(); ++it) {
        if (*it == this) {
            open_.erase(it);
            break;
        }
    }
    delete this;
}

void AlertWindow::OnPaint() {
    CPaintDC dc(this);
    if (art_ != nullptr) {
        face_.draw(dc, 7, 7);
        return;
    }
    const int kind = alert_type_ == kAlertPregnancy ? kIconPregnant
                   : alert_type_ == kAlertBirth     ? kIconBirth
                                                    : kIconNearDeath;
    draw_status_glyph(dc, kind, CRect(7, 7, 7 + 52, 7 + 43));
}

// COverviewAboutPage @ 0x004026a0.
AboutDialog::AboutDialog() : CDialog(kDialogAbout) {}

BOOL AboutDialog::OnInitDialog() {
    CDialog::OnInitDialog();
    SetDlgItemText(kControlAboutText, _T("Observation Kit - LibreCreatures"));
    return TRUE;
}

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_observation_window(CFont& font) {
    auto* sheet = new ObservationSheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    // The original's server identity, so the game's Tools menu entry and
    // any existing registration launch this build in its place.
    kit.identity.prog_id = "OVERVIEW.OLE";
    // {82720CE1-C6E4-11D0-A8BB-00A0C9008A48}
    const GUID clsid = {0x82720ce1, 0xc6e4, 0x11d0,
                        {0xa8, 0xbb, 0x00, 0xa0, 0xc9, 0x00, 0x8a, 0x48}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 6;
    kit.tool_value_prog_id = "Overview.OLE";
    kit.tool_name_string = kStringToolName;
    kit.tool_help_string = kStringToolHelp;
    kit.ole_init_failed_string = kStringOleInitFailed;
    kit.create_main_window = &create_observation_window;
    return kit;
}

} // namespace

} // namespace observation

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = observation::make_definition();
    return kit;
}
