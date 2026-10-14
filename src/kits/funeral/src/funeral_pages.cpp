// The Funeral Kit's pages: a memorial for each dead creature, the unmarked
// grave, and the graveyard.  The original's behaviour is in ../ORIGINAL.md;
// deviations are marked "Fix (bug N)".

#include "funeral.hpp"
#include "funeral_ids.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace funeral {
namespace {

CString text(const std::string& value) {
    return CString(value.c_str());
}

std::string control_text(CWnd& page, unsigned id) {
    CString value;
    page.GetDlgItemText(id, value);
    return std::string(CStringA(value));
}

// The template's text boxes over the picture are not shown: their text is
// lettered onto the picture in their place.  Returns where, in the picture's
// client coordinates.
CRect take_text_box(CWnd& page, unsigned picture_id, unsigned box_id) {
    CRect box;
    CWnd* control = page.GetDlgItem(box_id);
    CWnd* picture = page.GetDlgItem(picture_id);
    if (control == nullptr || picture == nullptr) {
        return box;
    }
    control->GetWindowRect(&box);
    picture->ScreenToClient(&box);
    control->ShowWindow(SW_HIDE);
    return box;
}

// The templates' picture is a white rectangle the original drew over from a
// window of its own; here it is owner-drawn.  It is first in the templates,
// so first in the Z-order: on top of the buttons drawn over it.  Put it
// underneath, and clip it to them, so repainting it never covers them.
void prepare_picture(CWnd& page) {
    if (CWnd* picture = page.GetDlgItem(kControlPicture)) {
        picture->ModifyStyle(SS_TYPEMASK, SS_OWNERDRAW | WS_CLIPSIBLINGS);
        picture->SetWindowPos(&CWnd::wndBottom, 0, 0, 0, 0,
                              SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

bool prepare_canvas(CWnd& page, c1kitshell::Canvas& canvas) {
    CWnd* control = page.GetDlgItem(kControlPicture);
    if (control == nullptr) {
        return false;
    }
    CRect rect;
    control->GetClientRect(&rect);
    if (!canvas.create(rect.Width(), rect.Height())) {
        return false;
    }
    canvas.fill(GetSysColor(COLOR_BTNFACE) & 0xffffff);
    return true;
}

void present_canvas(const c1kitshell::Canvas& canvas, LPDRAWITEMSTRUCT draw) {
    CDC* dc = CDC::FromHandle(draw->hDC);
    const CRect rect(draw->rcItem);
    canvas.present(*dc, rect.left, rect.top, rect.Width(), rect.Height());
}

// Lettering colours: pale on GRAVE.bmp's dark plinth and on funeral.bmp's
// dark stone.
constexpr COLORREF kPlinthText = RGB(255, 236, 170);
constexpr COLORREF kPlinthShadow = RGB(40, 20, 0);
constexpr COLORREF kStoneText = RGB(228, 228, 236);
constexpr COLORREF kStoneShadow = RGB(16, 16, 24);
// The epitaph box on GRAVE.bmp's lit panel.
constexpr COLORREF kPanelColour = RGB(255, 226, 120);

} // namespace

// ===========================================================================
// Lettering
// ===========================================================================

void Lettering::create(int height, int weight) {
    font_.CreateFont(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                     ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     DEFAULT_QUALITY, VARIABLE_PITCH | FF_ROMAN,
                     _T("Times New Roman"));
}

// Centred both ways in `rect`, wrapped, with a one-pixel shadow.
void Lettering::draw(CDC& dc, const CRect& rect, const CString& value,
                     COLORREF colour, COLORREF shadow) const {
    if (value.IsEmpty() || rect.IsRectEmpty()) {
        return;
    }
    CFont* previous = dc.SelectObject(const_cast<CFont*>(&font_));
    dc.SetBkMode(TRANSPARENT);
    CRect measured = rect;
    dc.DrawText(value, &measured, DT_CENTER | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    CRect placed = rect;
    if (measured.Height() < rect.Height()) {
        placed.top += (rect.Height() - measured.Height()) / 2;
    }
    const UINT format = DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS;
    CRect offset = placed;
    offset.OffsetRect(1, 1);
    dc.SetTextColor(shadow);
    dc.DrawText(value, &offset, format);
    dc.SetTextColor(colour);
    dc.DrawText(value, &placed, format);
    dc.SelectObject(previous);
}

// ===========================================================================
// Memorial (dialog 137)
// ===========================================================================

BEGIN_MESSAGE_MAP(MemorialPage, CPropertyPage)
    ON_WM_DRAWITEM()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(kControlPreviousPhoto, &MemorialPage::OnPreviousPhoto)
    ON_BN_CLICKED(kControlNextPhoto, &MemorialPage::OnNextPhoto)
    ON_EN_KILLFOCUS(kControlEpitaph, &MemorialPage::OnEpitaphKillFocus)
    ON_BN_CLICKED(kControlMakeHeadstone, &MemorialPage::OnMakeHeadstone)
    ON_BN_CLICKED(kControlClose, &MemorialPage::OnCloseKit)
END_MESSAGE_MAP()

MemorialPage::MemorialPage(FuneralSheet& sheet, const std::string& moniker,
                           const CString& tab)
    : CPropertyPage(kDialogMemorial), sheet_(sheet), moniker_(moniker), tab_(tab) {
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = tab_;
}

BOOL MemorialPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    prepare_picture(*this);
    previous_.AutoLoad(kControlPreviousPhoto, this);
    next_.AutoLoad(kControlNextPhoto, this);
    life_span_box_ = take_text_box(*this, kControlPicture, kControlLifeSpan);
    lettering_.create(13, FW_BOLD);
    epitaph_brush_.CreateSolidBrush(kPanelColour);
    if (c1kit::Grave* grave = sheet_.grave(moniker_)) {
        SetDlgItemText(kControlEpitaph, text((*grave)[c1kit::kGraveEpitaph]));
    }
    load_photos();
    initialized_ = true;
    show();
    return TRUE;
}

BOOL MemorialPage::OnSetActive() {
    show();
    return CPropertyPage::OnSetActive();
}

BOOL MemorialPage::OnKillActive() {
    commit_epitaph();
    return CPropertyPage::OnKillActive();
}

// The creature's photographs: the Owner's Kit's "<moniker>.Photo Album"
// (LoadPhotoAlbumItems @ 0x0040a000).  A missing album is no photographs.
// Fix (bug 4): an unreadable one is too; the original reported a nameless
// " was not found.".
void MemorialPage::load_photos() {
    photos_.clear();
    selected_photo_ = 0;
    std::ifstream in(sheet_.world_file(c1kit::album_file_name(moniker_)),
                     std::ios::binary);
    if (!in) {
        return;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    if (!c1kit::parse_album(bytes, photos_)) {
        photos_.clear();
    }
}

void MemorialPage::show() {
    if (!initialized_) {
        return;
    }
    const int count = static_cast<int>(photos_.size());
    GetDlgItem(kControlPreviousPhoto)->ShowWindow(count > 1 ? SW_SHOW : SW_HIDE);
    GetDlgItem(kControlNextPhoto)->ShowWindow(count > 1 ? SW_SHOW : SW_HIDE);
    GetDlgItem(kControlPreviousPhoto)->EnableWindow(selected_photo_ > 0);
    GetDlgItem(kControlNextPhoto)->EnableWindow(selected_photo_ + 1 < count);
    const c1kit::Grave* grave = sheet_.grave(moniker_);
    SetDlgItemText(kControlMakeHeadstone,
                   grave != nullptr && grave->has_headstone()
                       ? _T("Visit Headstone")
                       : _T("Make Headstone"));
    draw_picture();
}

// GRAVE.bmp with the selected photograph in its frame; UNKNOWN.bmp, its
// silhouette, when the creature has none (RefreshPhotoAlbumView @
// 0x00409660).
void MemorialPage::draw_picture() {
    if (!prepare_canvas(*this, picture_)) {
        return;
    }
    if (photos_.empty()) {
        picture_.draw_bitmap_file(kUnknownBackdrop, 0, 0);
    } else {
        picture_.draw_bitmap_file(kGraveBackdrop, 0, 0);
        const c1kit::PhotoBitmap& bitmap =
            photos_[static_cast<std::size_t>(selected_photo_)].bitmap;
        picture_.draw_indexed(bitmap.pixels.data(), bitmap.width, bitmap.height,
                              bitmap.stride, true, kPhotoLeft, kPhotoTop,
                              sheet_.palette());
    }
    GetDlgItem(kControlPicture)->Invalidate(FALSE);
}

void MemorialPage::OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw) {
    if (control_id != static_cast<int>(kControlPicture)) {
        CPropertyPage::OnDrawItem(control_id, draw);
        return;
    }
    present_canvas(picture_, draw);
    // How long it lived, on the plinth (FormatPhotoDateRange @ 0x00409e10).
    if (const c1kit::Grave* grave = sheet_.grave(moniker_)) {
        CDC* dc = CDC::FromHandle(draw->hDC);
        lettering_.draw(*dc, life_span_box_,
                        text(c1kit::format_life_span((*grave)[c1kit::kGraveBirthTime],
                                                     (*grave)[c1kit::kGraveDeathTime])),
                        kPlinthText, kPlinthShadow);
    }
}

HBRUSH MemorialPage::OnCtlColor(CDC* dc, CWnd* window, UINT type) {
    if (window != nullptr && window->GetDlgCtrlID() == static_cast<int>(kControlEpitaph)) {
        dc->SetBkColor(kPanelColour);
        dc->SetTextColor(RGB(40, 20, 0));
        return static_cast<HBRUSH>(epitaph_brush_.GetSafeHandle());
    }
    return CPropertyPage::OnCtlColor(dc, window, type);
}

// The epitaph is typed straight onto the plinth and kept with the grave.
// (The original had an epitaph entry, a multi-line edit (id 32772) it
// created over the picture, and kept what was typed in "Album".)
void MemorialPage::commit_epitaph() {
    if (!initialized_ || GetSafeHwnd() == nullptr) {
        return;
    }
    c1kit::Grave* grave = sheet_.grave(moniker_);
    if (grave == nullptr) {
        return;
    }
    const std::string epitaph = control_text(*this, kControlEpitaph);
    if (epitaph != (*grave)[c1kit::kGraveEpitaph]) {
        (*grave)[c1kit::kGraveEpitaph] = epitaph;
        sheet_.save_graves();
    }
}

void MemorialPage::OnEpitaphKillFocus() {
    commit_epitaph();
}

void MemorialPage::OnPreviousPhoto() {
    if (selected_photo_ > 0) {
        --selected_photo_;
    }
    show();
}

void MemorialPage::OnNextPhoto() {
    if (selected_photo_ + 1 < static_cast<int>(photos_.size())) {
        ++selected_photo_;
    }
    show();
}

void MemorialPage::OnMakeHeadstone() {
    commit_epitaph();
    sheet_.make_headstone(moniker_);
    show();
}

void MemorialPage::OnCloseKit() {
    commit_epitaph();
    sheet_.request_game_quit();
}

// ===========================================================================
// Unmarked grave (dialog 183)
// ===========================================================================

BEGIN_MESSAGE_MAP(UnmarkedPage, CPropertyPage)
    ON_WM_DRAWITEM()
    ON_BN_CLICKED(kControlClose, &UnmarkedPage::OnCloseKit)
END_MESSAGE_MAP()

// The tab says "No Record" (string 104), as the original's.
UnmarkedPage::UnmarkedPage(FuneralSheet& sheet, const std::string& moniker,
                           const std::string& death_time)
    : CPropertyPage(kDialogUnmarked),
      sheet_(sheet),
      moniker_(moniker),
      death_time_(death_time),
      tab_(c1kitshell::load_string(kStringNoRecord)) {
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = tab_;
}

BOOL UnmarkedPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    prepare_picture(*this);
    lettering_.create(13, FW_BOLD);
    if (prepare_canvas(*this, picture_)) {
        picture_.draw_bitmap_file(kUnknownBackdrop, 0, 0);
    }
    return TRUE;
}

// UNKNOWN.bmp; the plinth says the creature was never registered and when
// it died (the original showed the picture alone).
void UnmarkedPage::OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw) {
    if (control_id != static_cast<int>(kControlPicture)) {
        CPropertyPage::OnDrawItem(control_id, draw);
        return;
    }
    present_canvas(picture_, draw);
    CDC* dc = CDC::FromHandle(draw->hDC);
    // Where the memorial page's life span goes (dialog 137's 1065).
    const CRect plinth(0, 196, picture_.width(), 222);
    lettering_.draw(*dc, plinth,
                    c1kitshell::load_string(kStringNoRecord) + _T(": died ") +
                        text(death_time_),
                    kPlinthText, kPlinthShadow);
}

void UnmarkedPage::OnCloseKit() {
    sheet_.request_game_quit();
}

// ===========================================================================
// GraveYard (dialog 182)
// ===========================================================================

BEGIN_MESSAGE_MAP(GraveyardPage, CPropertyPage)
    ON_WM_DRAWITEM()
    ON_BN_CLICKED(kControlPreviousPhoto, &GraveyardPage::OnPreviousGrave)
    ON_BN_CLICKED(kControlNextPhoto, &GraveyardPage::OnNextGrave)
    ON_BN_CLICKED(kControlClose, &GraveyardPage::OnCloseKit)
END_MESSAGE_MAP()

GraveyardPage::GraveyardPage(FuneralSheet& sheet)
    : CPropertyPage(kDialogGraveyard), sheet_(sheet) {}

BOOL GraveyardPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    prepare_picture(*this);
    previous_.AutoLoad(kControlPreviousPhoto, this);
    next_.AutoLoad(kControlNextPhoto, this);
    name_box_ = take_text_box(*this, kControlPicture, kControlHeadstoneName);
    span_box_ = take_text_box(*this, kControlPicture, kControlLifeSpan);
    epitaph_box_ = take_text_box(*this, kControlPicture, kControlHeadstoneEpitaph);
    name_lettering_.create(16, FW_BOLD);
    lettering_.create(12, FW_BOLD);
    initialized_ = true;
    show();
    return TRUE;
}

BOOL GraveyardPage::OnSetActive() {
    show();
    return CPropertyPage::OnSetActive();
}

// The graves with a headstone, oldest first.
std::vector<const c1kit::Grave*> GraveyardPage::headstones() const {
    std::vector<const c1kit::Grave*> made;
    for (const c1kit::Grave& grave : sheet_.graves()) {
        if (grave.has_headstone()) {
            made.push_back(&grave);
        }
    }
    return made;
}

void GraveyardPage::show_grave(const std::string& moniker) {
    selected_ = moniker;
    show();
}

void GraveyardPage::show() {
    if (!initialized_) {
        return;
    }
    const std::vector<const c1kit::Grave*> made = headstones();
    int index = -1;
    for (std::size_t i = 0; i < made.size(); ++i) {
        if ((*made[i])[c1kit::kGraveMoniker] == selected_) {
            index = static_cast<int>(i);
        }
    }
    if (index < 0 && !made.empty()) {
        index = static_cast<int>(made.size()) - 1;
        selected_ = (*made.back())[c1kit::kGraveMoniker];
    }
    const int count = static_cast<int>(made.size());
    GetDlgItem(kControlPreviousPhoto)->ShowWindow(count > 1 ? SW_SHOW : SW_HIDE);
    GetDlgItem(kControlNextPhoto)->ShowWindow(count > 1 ? SW_SHOW : SW_HIDE);
    GetDlgItem(kControlPreviousPhoto)->EnableWindow(index > 0);
    GetDlgItem(kControlNextPhoto)->EnableWindow(index >= 0 && index + 1 < count);
    if (prepare_canvas(*this, picture_)) {
        picture_.draw_bitmap_file(kHeadstoneBackdrop, 0, 0);
    }
    GetDlgItem(kControlPicture)->Invalidate(FALSE);
}

// funeral.bmp, with the name, life span and epitaph cut into the stone
// where the template's boxes are (1066, 1065 and 1064).
void GraveyardPage::OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw) {
    if (control_id != static_cast<int>(kControlPicture)) {
        CPropertyPage::OnDrawItem(control_id, draw);
        return;
    }
    present_canvas(picture_, draw);
    CDC* dc = CDC::FromHandle(draw->hDC);
    const c1kit::Grave* shown = sheet_.grave(selected_);
    if (shown == nullptr || !shown->has_headstone()) {
        lettering_.draw(*dc, epitaph_box_, _T("No headstones yet"), kStoneText,
                        kStoneShadow);
        return;
    }
    name_lettering_.draw(*dc, name_box_, text((*shown)[c1kit::kGraveCreatureName]),
                         kStoneText, kStoneShadow);
    lettering_.draw(*dc, span_box_,
                    text(c1kit::format_life_span((*shown)[c1kit::kGraveBirthTime],
                                                 (*shown)[c1kit::kGraveDeathTime])),
                    kStoneText, kStoneShadow);
    lettering_.draw(*dc, epitaph_box_, text((*shown)[c1kit::kGraveEpitaph]),
                    kStoneText, kStoneShadow);
}

void GraveyardPage::OnPreviousGrave() {
    const std::vector<const c1kit::Grave*> made = headstones();
    for (std::size_t i = 1; i < made.size(); ++i) {
        if ((*made[i])[c1kit::kGraveMoniker] == selected_) {
            selected_ = (*made[i - 1])[c1kit::kGraveMoniker];
            break;
        }
    }
    show();
}

void GraveyardPage::OnNextGrave() {
    const std::vector<const c1kit::Grave*> made = headstones();
    for (std::size_t i = 0; i + 1 < made.size(); ++i) {
        if ((*made[i])[c1kit::kGraveMoniker] == selected_) {
            selected_ = (*made[i + 1])[c1kit::kGraveMoniker];
            break;
        }
    }
    show();
}

void GraveyardPage::OnCloseKit() {
    sheet_.request_game_quit();
}

// ===========================================================================
// Deaths (the modern look; LibreCreatures)
// ===========================================================================

BEGIN_MESSAGE_MAP(DeathsPage, c1kitshell::LayoutPage)
    ON_NOTIFY(LVN_ITEMCHANGED, kControlDeathsList, &DeathsPage::OnSelectionChanged)
END_MESSAGE_MAP()

DeathsPage::DeathsPage(FuneralSheet& sheet)
    : LayoutPage(sheet, kDialogBlankPage, kStringDeathsTab), sheet_(sheet) {}

void DeathsPage::create_controls() {
    make(list_, WC_LISTVIEW, _T(""),
         LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
         kControlDeathsList);
    list_.SetExtendedStyle(list_.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
    list_.InsertColumn(0, _T("Name"), LVCFMT_LEFT, 110);
    list_.InsertColumn(1, _T("Died"), LVCFMT_LEFT, 110);
    list_.InsertColumn(2, _T("Stone"), LVCFMT_LEFT, 60);
    refresh();
}

// The list on the left; the memorial page, at its own (1996) size, on the
// right; Close under it.
void DeathsPage::layout(int width, int height) {
    CRect detail(0, 0, 0, 0);
    if (memorial_ && memorial_->GetSafeHwnd() != nullptr) {
        memorial_->GetWindowRect(&detail);
    } else if (unmarked_ && unmarked_->GetSafeHwnd() != nullptr) {
        unmarked_->GetWindowRect(&detail);
    }
    const int detail_width = detail.Width() > 0 ? detail.Width() : 330;
    const int bottom = height - kMargin - button_height() - kMargin;
    const int list_width = (std::max)(160, width - detail_width - 3 * kMargin);
    place(list_, kMargin, kMargin, list_width, bottom - kMargin);
    // The columns share the list's width: name, date, headstone.
    const int inner = list_width - GetSystemMetrics(SM_CXVSCROLL) - 4;
    list_.SetColumnWidth(0, inner * 35 / 100);
    list_.SetColumnWidth(1, inner * 42 / 100);
    list_.SetColumnWidth(2, inner - inner * 35 / 100 - inner * 42 / 100);
    detail_rect_ = CRect(CPoint(2 * kMargin + list_width, kMargin),
                         CSize(detail_width, (std::max)(detail.Height(), 1)));
    for (CWnd* child : {static_cast<CWnd*>(memorial_.get()), static_cast<CWnd*>(unmarked_.get())}) {
        if (child != nullptr && child->GetSafeHwnd() != nullptr) {
            child->SetWindowPos(nullptr, detail_rect_.left, detail_rect_.top, 0, 0,
                                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    place_close(width, height);
}

void DeathsPage::refresh(const std::string& moniker) {
    if (list_.GetSafeHwnd() == nullptr) {
        if (!moniker.empty()) {
            shown_ = moniker;
        }
        return;
    }
    const std::string wanted = moniker.empty() ? shown_ : moniker;
    list_.SetRedraw(FALSE);
    list_.DeleteAllItems();
    rows_.clear();
    // Newest first: the graves file keeps them in the order they died.
    const std::vector<c1kit::Grave>& graves = sheet_.graves();
    int select = -1;
    for (auto grave = graves.rbegin(); grave != graves.rend(); ++grave) {
        const std::string& id = (*grave)[c1kit::kGraveMoniker];
        const int row = list_.InsertItem(list_.GetItemCount(), sheet_.name_for_moniker(id));
        list_.SetItemText(row, 1, CString((*grave)[c1kit::kGraveDeathTime].c_str()));
        list_.SetItemText(row, 2, c1kit::is_unmarked(*grave) ? _T("No record")
                                  : grave->has_headstone()   ? _T("Made")
                                                             : _T(""));
        rows_.push_back(id);
        if (id == wanted) {
            select = row;
        }
    }
    list_.SetRedraw(TRUE);
    if (select < 0 && !rows_.empty()) {
        select = 0;
    }
    if (select >= 0) {
        list_.SetItemState(select, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        list_.EnsureVisible(select, FALSE);
        if (IsWindowVisible()) {
            list_.SetFocus();  // so the selection shows in the highlight colour
        }
        show_detail(rows_[static_cast<std::size_t>(select)]);
    } else {
        show_detail(std::string());
    }
}

void DeathsPage::OnSelectionChanged(NMHDR* header, LRESULT* result) {
    *result = 0;
    const auto* change = reinterpret_cast<NMLISTVIEW*>(header);
    if ((change->uNewState & LVIS_SELECTED) != 0 && change->iItem >= 0 &&
        change->iItem < static_cast<int>(rows_.size())) {
        show_detail(rows_[static_cast<std::size_t>(change->iItem)]);
    }
}

void DeathsPage::commit_epitaph() {
    if (memorial_ && memorial_->GetSafeHwnd() != nullptr) {
        memorial_->commit_epitaph();
    }
}

// The memorial (or "No Record") page for `moniker`, made as a child of this
// page; its own Close button hides, since this page has one.
void DeathsPage::show_detail(const std::string& moniker) {
    if (moniker == shown_ && (memorial_ || unmarked_)) {
        if (memorial_) {
            memorial_->show();
        }
        return;
    }
    commit_epitaph();
    if (memorial_ && memorial_->GetSafeHwnd() != nullptr) {
        memorial_->DestroyWindow();
    }
    if (unmarked_ && unmarked_->GetSafeHwnd() != nullptr) {
        unmarked_->DestroyWindow();
    }
    memorial_.reset();
    unmarked_.reset();
    shown_ = moniker;
    const c1kit::Grave* grave = moniker.empty() ? nullptr : sheet_.grave(moniker);
    if (grave == nullptr) {
        return;
    }
    CWnd* child = nullptr;
    if (c1kit::is_unmarked(*grave)) {
        unmarked_ = std::make_unique<UnmarkedPage>(sheet_, moniker,
                                                   (*grave)[c1kit::kGraveDeathTime]);
        if (unmarked_->Create(kDialogUnmarked, this)) {
            child = unmarked_.get();
        }
    } else {
        memorial_ = std::make_unique<MemorialPage>(sheet_, moniker, sheet_.name_for_moniker(moniker));
        if (memorial_->Create(kDialogMemorial, this)) {
            child = memorial_.get();
        }
    }
    if (child == nullptr) {
        return;
    }
    // The 1996 templates are pop-ups (property pages the sheet parented);
    // made here they would float, so they become this page's child.
    child->ModifyStyle(WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | DS_MODALFRAME,
                       WS_CHILD);
    child->ModifyStyleEx(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_APPWINDOW, 0);
    child->SetParent(this);
    if (CWnd* close = child->GetDlgItem(kControlClose)) {
        close->ShowWindow(SW_HIDE);
    }
    child->SetWindowPos(nullptr, detail_rect_.left, detail_rect_.top, 0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    CRect client;
    GetClientRect(&client);
    layout(client.Width(), client.Height());
}

} // namespace funeral
