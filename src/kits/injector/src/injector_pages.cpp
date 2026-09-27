// The Object Injector's pages: COBs and Analysis.  The original's are
// described in ../ORIGINAL.md.

#include "injector.hpp"
#include "injector_ids.hpp"

#include <algorithm>
#include <shlobj.h>

namespace injector {
namespace {

CString text(const std::string& value) {
    return CString(value.c_str());
}

CString classifier_text(const InjectorSheet& sheet, const c1kit::Classifier& c) {
    const std::string name = c1kit::classifier_name(sheet.classifier_names(), c);
    CString numbers;
    numbers.Format(_T("%d %d %d"), c.family, c.genus, c.species);
    return name.empty() ? text(c1kit::family_name(c.family)) + _T(" (") + numbers + _T(")")
                        : text(name) + _T(" (") + numbers + _T(")");
}

CString quantity_text(const InjectorSheet& sheet, const c1kit::Cob& cob) {
    if (sheet.expired(cob)) return c1kitshell::load_string(kStringExpired);
    if (cob.unlimited()) return c1kitshell::load_string(kStringInfinite);
    CString count;
    count.Format(_T("%d"), cob.quantity);
    return count;
}

} // namespace

// ===========================================================================
// The COB list both pages have
// ===========================================================================

BEGIN_MESSAGE_MAP(CobListPage, c1kitshell::LayoutPage)
    ON_EN_CHANGE(kControlFind, &CobListPage::OnFindChanged)
END_MESSAGE_MAP()

CobListPage::CobListPage(InjectorSheet& sheet, UINT title_string)
    : LayoutPage(sheet, kDialogPage, title_string), sheet_(sheet) {}

void CobListPage::create_list() {
    make(find_label_, _T("STATIC"), _T("Find:"), SS_LEFT, kControlFindLabel);
    make(find_, _T("EDIT"), _T(""), ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, kControlFind);
    make(list_, WC_LISTVIEW, _T(""),
         LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP, kControlList);
    list_.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    list_.InsertColumn(0, _T("COB"), LVCFMT_LEFT, 150);
    list_.InsertColumn(1, _T("Left"), LVCFMT_RIGHT, 54);
    fill_list();
}

bool CobListPage::classic() const {
    return sheet_.classic();
}

// The 1996 list was a plain list of names: no heading, and no count column
// (the page shows the count).  The column fills the list, so, unlike the
// original's, it never scrolls sideways.
void CobListPage::place_list_dlu(int find_label_x, int find_y, int find_x, int find_width,
                                 int list_y, int list_width, int list_height) {
    place_dlu(find_label_, find_label_x, find_y + 2, 24, 8);
    place_dlu(find_, find_x, find_y, find_width, 12);
    place_dlu(list_, find_label_x, list_y, list_width, list_height);
    list_.ModifyStyle(0, LVS_NOCOLUMNHEADER);
    CRect inside;
    list_.GetClientRect(&inside);
    list_.SetColumnWidth(1, 0);
    list_.SetColumnWidth(0, inside.Width() - GetSystemMetrics(SM_CXVSCROLL));
}

void CobListPage::place_list(int x, int y, int width, int height) {
    place(find_label_, x, y + 3, 30, text_height());
    place(find_, x + 32, y, width - 32, text_height() + 4);
    place(list_, x, y + text_height() + 10, width, height - text_height() - 10);
    list_.SetColumnWidth(0, width - 54 - GetSystemMetrics(SM_CXVSCROLL) - 4);
}

// Every COB whose name holds the find text, as the original's filter did
// (ApplyCobFilterAndRefresh @ 0x00403d80).  Item data is the entry index.
void CobListPage::fill_list() {
    if (list_.GetSafeHwnd() == nullptr) return;
    const int previous = selected_index();
    CString filter;
    find_.GetWindowText(filter);
    filter.MakeLower();
    list_.SetRedraw(FALSE);
    list_.DeleteAllItems();
    const std::vector<CobEntry>& entries = sheet_.entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        CString name = text(entries[i].cob.name);
        CString lower = name;
        lower.MakeLower();
        if (!filter.IsEmpty() && lower.Find(filter) < 0) continue;
        const int row = list_.InsertItem(list_.GetItemCount(), name);
        list_.SetItemText(row, 1, quantity_text(sheet_, entries[i].cob));
        list_.SetItemData(row, static_cast<DWORD_PTR>(i));
        if (static_cast<int>(i) == previous) {
            list_.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
    list_.SetRedraw(TRUE);
}

int CobListPage::selected_index() const {
    if (list_.GetSafeHwnd() == nullptr) return -1;
    const int row = list_.GetNextItem(-1, LVNI_SELECTED);
    return row < 0 ? -1 : static_cast<int>(list_.GetItemData(row));
}

void CobListPage::cobs_changed() {
    if (list_.GetSafeHwnd() != nullptr) {
        fill_list();
        selection_changed();
    }
}

void CobListPage::OnFindChanged() {
    fill_list();
    selection_changed();
}

BOOL CobListPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == kControlList && header->code == LVN_ITEMCHANGED) {
        const auto* change = reinterpret_cast<const NMLISTVIEW*>(lparam);
        if ((change->uChanged & LVIF_STATE) != 0 &&
            ((change->uNewState ^ change->uOldState) & LVIS_SELECTED) != 0) {
            selection_changed();
        }
    }
    return LayoutPage::OnNotify(wparam, lparam, result);
}

// ===========================================================================
// COBs
// ===========================================================================

BEGIN_MESSAGE_MAP(CobsPage, CobListPage)
    ON_BN_CLICKED(kControlInject, &CobsPage::OnInject)
    ON_BN_CLICKED(kControlRemove, &CobsPage::OnRemove)
    ON_BN_CLICKED(kControlRefresh, &CobsPage::OnRefresh)
    ON_BN_CLICKED(kControlBrowse, &CobsPage::OnBrowse)
    ON_BN_CLICKED(kControlIgnoreAmount, &CobsPage::OnIgnoreAmount)
    ON_BN_CLICKED(kControlAllowWithout, &CobsPage::OnAllowWithout)
END_MESSAGE_MAP()

CobsPage::CobsPage(InjectorSheet& sheet) : CobListPage(sheet, kStringCobsTab) {}

void CobsPage::create_controls() {
    create_list();
    picture_.create(*this, kControlPicture, [this](CDC& dc, const CRect& rect) { draw_picture(dc, rect); });
    make(description_, _T("EDIT"), _T(""),
         ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_BORDER, kControlDescription);
    make(inject_, _T("BUTTON"), _T("Inject"), BS_PUSHBUTTON | WS_TABSTOP, kControlInject);
    make(remove_, _T("BUTTON"), _T("Remove"), BS_PUSHBUTTON | WS_TABSTOP, kControlRemove);
    make(refresh_, _T("BUTTON"), _T("Refresh"), BS_PUSHBUTTON | WS_TABSTOP, kControlRefresh);
    make(browse_, _T("BUTTON"), _T("Browse..."), BS_PUSHBUTTON | WS_TABSTOP, kControlBrowse);
    make(quantity_, _T("STATIC"), _T(""), SS_LEFT, kControlQuantity);
    make(folder_, _T("STATIC"), _T(""), SS_LEFT | SS_PATHELLIPSIS, kControlFolder);
    // The original's Advanced menu, as two ticks.
    make(ignore_amount_, _T("BUTTON"), _T("Ignore amount remaining"), BS_AUTOCHECKBOX | WS_TABSTOP,
         kControlIgnoreAmount);
    make(allow_without_, _T("BUTTON"), _T("Allow without a creature"), BS_AUTOCHECKBOX | WS_TABSTOP,
         kControlAllowWithout);
    ignore_amount_.SetCheck(sheet_.ignore_amount() ? BST_CHECKED : BST_UNCHECKED);
    allow_without_.SetCheck(sheet_.allow_without_subject() ? BST_CHECKED : BST_UNCHECKED);
    show_selected();
}

void CobsPage::layout(int width, int height) {
    if (classic()) {
        layout_classic();
        return;
    }
    const int m = kMargin;
    const int row = button_height();
    const int list_width = (std::max)(170, width * 2 / 5);
    const int bottom = height - m - row;  // the last row: Browse, the folder, Close
    const int actions = bottom - m - row;  // Inject, Remove, Refresh
    const int ticks = actions - m - text_height();
    place_list(m, m, list_width, ticks - 2 * m);
    const int right = 2 * m + list_width;
    const int right_width = width - right - m;
    const int picture_height = (ticks - 2 * m) * 2 / 5;
    place(picture_, right, m, right_width, picture_height);
    place(description_, right, 2 * m + picture_height, right_width, ticks - 3 * m - picture_height);
    place(ignore_amount_, m, ticks, 160, text_height() + 2);
    place(allow_without_, m + 170, ticks, 170, text_height() + 2);
    place(inject_, m, actions, 70, row);
    place(remove_, 2 * m + 70, actions, 70, row);
    place(refresh_, 3 * m + 140, actions, 70, row);
    place(quantity_, 4 * m + 210, actions + 5, width - 5 * m - 210, text_height());
    place(browse_, m, bottom, 70, row);
    place(folder_, 2 * m + 70, bottom + 5, width - 4 * m - 70 - 84, text_height());
    place_close(width, height);
}

// Dialog 150, in its dialog units.  The template put "Quantity remaining:"
// under the Refresh button and its figure in a box beside it; here one line
// of text follows the button.  Close is on the sheet's row, and the two
// Advanced ticks are in the menu.
void CobsPage::layout_classic() {
    place_list_dlu(4, 4, 30, 84, 20, 110, 114);
    place_dlu(picture_, 120, 4, 116, 116);
    place_dlu(description_, 4, 140, 232, 68);
    place_dlu(inject_, 4, 212, 50, 14);
    place_dlu(remove_, 58, 212, 50, 14);
    place_dlu(refresh_, 112, 212, 50, 14);
    place_dlu(quantity_, 166, 215, 70, 10);
    place_dlu(browse_, 4, 232, 50, 14);
    place_dlu(folder_, 58, 235, 178, 10);
    for (CWnd* hidden : {static_cast<CWnd*>(&ignore_amount_), static_cast<CWnd*>(&allow_without_),
                         static_cast<CWnd*>(&close_)}) {
        hidden->ShowWindow(SW_HIDE);
    }
}

void CobsPage::cobs_changed() {
    CobListPage::cobs_changed();
    if (folder_.GetSafeHwnd() != nullptr) {
        folder_.SetWindowText(text(sheet_.folder()));
    }
}

void CobsPage::selection_changed() {
    show_selected();
}

// The description, and the warnings (BuildInjectionSafetyWarnings).
void CobsPage::show_selected() {
    if (description_.GetSafeHwnd() == nullptr) return;
    folder_.SetWindowText(text(sheet_.folder()));
    const int index = selected_index();
    const bool valid = index >= 0 && index < static_cast<int>(sheet_.entries().size());
    inject_.EnableWindow(valid);
    remove_.EnableWindow(valid);
    if (!valid) {
        description_.SetWindowText(sheet_.entries().empty()
                                       ? CString(_T("There are no COBs in this folder. Browse to the folder with your .cob files."))
                                       : CString(_T("Pick a COB.")));
        quantity_.SetWindowText(_T(""));
        picture_.redraw();
        return;
    }
    const c1kit::Cob& cob = sheet_.entries()[static_cast<std::size_t>(index)].cob;
    // The original's words for a COB with no description.
    CString shown = cob.description.empty() ? CString(_T("No description available."))
                                            : text(cob.description);
    shown.Replace(_T("\r\n"), _T("\n"));
    shown.Replace(_T("\n"), _T("\r\n"));
    const std::vector<std::string> warnings = c1kit::cob_warnings(cob, sheet_.expired(cob));
    if (!warnings.empty()) {
        shown += _T("\r\n\r\nWarnings:");
        for (const std::string& warning : warnings) {
            shown += _T("\r\n  ") + text(warning);
        }
    }
    if (c1kit::needs_creature(cob)) {
        shown += _T("\r\n\r\nIt acts on the selected creature.");
    }
    description_.SetWindowText(shown);
    // The classic page has the 1996 template's room for the count.
    quantity_.SetWindowText((classic() ? _T("Quantity: ") : _T("Quantity remaining: ")) +
                            quantity_text(sheet_, cob));
    picture_.redraw();
}

// CDibView::OnPaint @ 0x004047a0: the sheet's Alima.bmp stretched behind
// the picture, which is drawn at its own size, centred, with palette index
// 0 left out.
void CobsPage::draw_picture_classic(CDC& dc, const CRect& rect) {
    if (!backdrop_loaded_) {
        backdrop_loaded_ = true;
        backdrop_.load_file(c1kitshell::game_directory_setting("Main Directory") + kPictureBackdrop);
    }
    dc.FillSolidRect(rect, RGB(0, 0, 0));
    if (backdrop_.width() > 0) {
        CDC source;
        source.CreateCompatibleDC(&dc);
        CBitmap* previous = source.SelectObject(&backdrop_.bitmap());
        dc.SetStretchBltMode(COLORONCOLOR);
        dc.StretchBlt(rect.left, rect.top, rect.Width(), rect.Height(), &source, 0, 0,
                      backdrop_.width(), backdrop_.height(), SRCCOPY);
        source.SelectObject(previous);
    }
    const int index = selected_index();
    if (index < 0 || index >= static_cast<int>(sheet_.entries().size())) return;
    const c1kit::CobSprite& sprite = sheet_.entries()[static_cast<std::size_t>(index)].cob.sprite;
    if (sprite.width <= 0 || sprite.height <= 0 || !canvas_.create(sprite.width, sprite.height)) return;
    canvas_.fill(0);
    canvas_.draw_indexed(sprite.pixels.data(), sprite.width, sprite.height, sprite.stride, false, 0, 0,
                         sheet_.palette());
    // The mask: set where index 0 (the background) is.  Rows of a
    // CreateBitmap monochrome bitmap are padded to 16 bits.
    const int mask_stride = ((sprite.width + 15) / 16) * 2;
    std::vector<BYTE> mask_bits(static_cast<std::size_t>(mask_stride) * sprite.height, 0);
    for (int y = 0; y < sprite.height; ++y) {
        for (int x = 0; x < sprite.width; ++x) {
            if (sprite.pixels[static_cast<std::size_t>(y) * sprite.stride + x] == 0) {
                mask_bits[static_cast<std::size_t>(y) * mask_stride + x / 8] |=
                    static_cast<BYTE>(0x80 >> (x % 8));
            }
        }
    }
    CBitmap mask;
    mask.CreateBitmap(sprite.width, sprite.height, 1, 1, mask_bits.data());
    CBitmap colour;
    colour.CreateCompatibleBitmap(&dc, sprite.width, sprite.height);
    CDC mask_dc, colour_dc;
    mask_dc.CreateCompatibleDC(&dc);
    colour_dc.CreateCompatibleDC(&dc);
    CBitmap* previous_mask = mask_dc.SelectObject(&mask);
    CBitmap* previous_colour = colour_dc.SelectObject(&colour);
    canvas_.present(colour_dc, 0, 0, sprite.width, sprite.height);
    const int x = rect.left + (rect.Width() - sprite.width) / 2;
    const int y = rect.top + (rect.Height() - sprite.height) / 2;
    dc.SetBkColor(RGB(255, 255, 255));
    dc.SetTextColor(RGB(0, 0, 0));
    dc.BitBlt(x, y, sprite.width, sprite.height, &mask_dc, 0, 0, SRCAND);
    dc.BitBlt(x, y, sprite.width, sprite.height, &colour_dc, 0, 0, SRCPAINT);
    mask_dc.SelectObject(previous_mask);
    colour_dc.SelectObject(previous_colour);
}

void CobsPage::draw_picture(CDC& dc, const CRect& rect) {
    if (classic()) {
        draw_picture_classic(dc, rect);
        return;
    }
    dc.FillSolidRect(rect, RGB(0, 0, 0));
    const int index = selected_index();
    if (index < 0 || index >= static_cast<int>(sheet_.entries().size())) return;
    const c1kit::CobSprite& sprite = sheet_.entries()[static_cast<std::size_t>(index)].cob.sprite;
    if (sprite.width <= 0 || sprite.height <= 0 || !canvas_.create(sprite.width, sprite.height)) return;
    canvas_.fill(0);
    canvas_.draw_indexed(sprite.pixels.data(), sprite.width, sprite.height, sprite.stride, false, 0, 0,
                         sheet_.palette());
    const int scale = (std::max)(1, (std::min)((rect.Width() - 8) / sprite.width,
                                               (rect.Height() - 8) / sprite.height));
    const int w = sprite.width * scale;
    const int h = sprite.height * scale;
    CDC source;
    source.CreateCompatibleDC(&dc);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(&dc, sprite.width, sprite.height);
    CBitmap* previous = source.SelectObject(&bitmap);
    canvas_.present(source, 0, 0, sprite.width, sprite.height);
    dc.SetStretchBltMode(COLORONCOLOR);
    dc.StretchBlt(rect.left + (rect.Width() - w) / 2, rect.top + (rect.Height() - h) / 2, w, h,
                  &source, 0, 0, sprite.width, sprite.height, SRCCOPY);
    source.SelectObject(previous);
}

void CobsPage::OnInject() {
    const int index = selected_index();
    if (index < 0) return;
    CString why;
    if (!sheet_.inject(sheet_.entries()[static_cast<std::size_t>(index)], why)) {
        AfxMessageBox(why, MB_ICONINFORMATION);
    }
    fill_list();
    show_selected();
}

void CobsPage::OnRemove() {
    const int index = selected_index();
    if (index < 0) return;
    CString why;
    if (!sheet_.remove(sheet_.entries()[static_cast<std::size_t>(index)], why) && !why.IsEmpty()) {
        AfxMessageBox(why, MB_ICONINFORMATION);
    }
}

void CobsPage::OnRefresh() {
    sheet_.reload();
}

void CobsPage::OnBrowse() {
    sheet_.browse_for_folder(this);
}

void CobsPage::OnIgnoreAmount() {
    sheet_.set_ignore_amount(ignore_amount_.GetCheck() == BST_CHECKED);
}

void CobsPage::OnAllowWithout() {
    sheet_.set_allow_without_subject(allow_without_.GetCheck() == BST_CHECKED);
}

// ===========================================================================
// Analysis
// ===========================================================================
//
// CAnalysisPage::PopulateCobDetailsTree @ 0x00407300: General Information,
// Scripts, Chemicals Affected, Warnings.  Here the scripts are named from
// the game's ClassifierNames.txt, each one's commands are its children, and
// how it would be removed is added.

BEGIN_MESSAGE_MAP(AnalysisPage, CobListPage)
    ON_BN_CLICKED(kControlAnalysisBrowse, &AnalysisPage::OnBrowse)
    ON_BN_CLICKED(kControlAnalysisRefresh, &AnalysisPage::OnRefresh)
END_MESSAGE_MAP()

AnalysisPage::AnalysisPage(InjectorSheet& sheet) : CobListPage(sheet, kStringAnalysisTab) {}

void AnalysisPage::create_controls() {
    create_list();
    make(tree_, WC_TREEVIEW, _T(""),
         TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
         kControlTree);
    if (classic()) {
        make(list_label_, _T("STATIC"), _T("COB list"), SS_LEFT, kControlListLabel);
        make(results_label_, _T("STATIC"), _T("Results of Analysis"), SS_LEFT, kControlResultsLabel);
        make(browse_, _T("BUTTON"), _T("Browse..."), BS_PUSHBUTTON | WS_TABSTOP,
             kControlAnalysisBrowse);
        make(refresh_, _T("BUTTON"), _T("Refresh"), BS_PUSHBUTTON | WS_TABSTOP,
             kControlAnalysisRefresh);
        make(folder_, _T("STATIC"), text(sheet_.folder()), SS_LEFT | SS_PATHELLIPSIS,
             kControlAnalysisFolder);
    }
    fill_tree();
}

void AnalysisPage::cobs_changed() {
    CobListPage::cobs_changed();
    if (folder_.GetSafeHwnd() != nullptr) {
        folder_.SetWindowText(text(sheet_.folder()));
    }
}

void AnalysisPage::OnBrowse() {
    sheet_.browse_for_folder(this);
}

void AnalysisPage::OnRefresh() {
    sheet_.reload();
}

// Dialog 151, in its dialog units; Close is on the sheet's row.
void AnalysisPage::layout_classic() {
    place_dlu(list_label_, 4, 4, 80, 8);
    place_list_dlu(4, 15, 30, 58, 30, 84, 194);
    place_dlu(results_label_, 92, 4, 140, 8);
    place_dlu(tree_, 92, 14, 144, 210);
    place_dlu(browse_, 4, 232, 50, 14);
    place_dlu(refresh_, 58, 232, 50, 14);
    place_dlu(folder_, 112, 235, 124, 10);
    close_.ShowWindow(SW_HIDE);
}

void AnalysisPage::layout(int width, int height) {
    if (classic()) {
        layout_classic();
        return;
    }
    const int m = kMargin;
    const int list_width = (std::max)(150, width / 3);
    const int bottom = height - 2 * m - button_height();
    place_list(m, m, list_width, bottom - m);
    place(tree_, 2 * m + list_width, m, width - 3 * m - list_width, bottom - m);
    place_close(width, height);
}

void AnalysisPage::selection_changed() {
    fill_tree();
}

void AnalysisPage::fill_tree() {
    if (tree_.GetSafeHwnd() == nullptr) return;
    tree_.SetRedraw(FALSE);
    tree_.DeleteAllItems();
    const int index = selected_index();
    if (index < 0 || index >= static_cast<int>(sheet_.entries().size())) {
        tree_.InsertItem(_T("Pick a COB to see what it does."));
        tree_.SetRedraw(TRUE);
        return;
    }
    const CobEntry& entry = sheet_.entries()[static_cast<std::size_t>(index)];
    const c1kit::Cob& cob = entry.cob;
    const auto commands_under = [this](HTREEITEM parent, const std::string& script) {
        std::size_t from = 0;
        while (from < script.size()) {
            std::size_t comma = script.find(',', from);
            if (comma == std::string::npos) comma = script.size();
            const std::string command = script.substr(from, comma - from);
            if (!command.empty()) tree_.InsertItem(text(command), parent);
            from = comma + 1;
        }
    };

    HTREEITEM general = tree_.InsertItem(_T("General Information"));
    tree_.InsertItem(_T("File \"") + text(entry.path) + _T("\""), general);
    for (const c1kit::Classifier& made : c1kit::created_classifiers(cob.inject_scripts)) {
        tree_.InsertItem(_T("Makes: ") + classifier_text(sheet_, made), general);
    }
    tree_.InsertItem(_T("Quantity remaining: ") + quantity_text(sheet_, cob), general);
    if (cob.has_expiry()) {
        CString date;
        date.Format(_T("Use by: %d/%d/%d"), cob.expiry_day, cob.expiry_month, cob.expiry_year);
        tree_.InsertItem(date, general);
    }
    tree_.Expand(general, TVE_EXPAND);

    HTREEITEM scripts = tree_.InsertItem(_T("Scripts:"));
    for (const std::string& script : cob.install_scripts) {
        c1kit::InstalledScript installed;
        CString label = _T("Installation script");
        if (c1kit::installed_script(script, installed)) {
            label = text(c1kit::event_name(installed.event)) + _T(": ") +
                    classifier_text(sheet_, installed.classifier);
        }
        commands_under(tree_.InsertItem(label, scripts), script);
    }
    for (std::size_t i = 0; i < cob.inject_scripts.size(); ++i) {
        CString label;
        label.Format(_T("Injection script %d"), static_cast<int>(i) + 1);
        commands_under(tree_.InsertItem(label, scripts), cob.inject_scripts[i]);
    }
    tree_.Expand(scripts, TVE_EXPAND);

    HTREEITEM chemicals = tree_.InsertItem(_T("Chemicals Affected:"));
    const std::vector<int> affected = c1kit::affected_chemicals(cob);
    if (affected.empty()) {
        tree_.InsertItem(_T("None"), chemicals);
    }
    for (const int chemical : affected) {
        CString label;
        const std::vector<std::string>& names = sheet_.chemical_names();
        const bool named = chemical < static_cast<int>(names.size()) &&
                           !names[static_cast<std::size_t>(chemical)].empty();
        label.Format(_T("%d  %s"), chemical,
                     named ? text(names[static_cast<std::size_t>(chemical)]).GetString() : _T(""));
        tree_.InsertItem(label, chemicals);
    }
    tree_.Expand(chemicals, TVE_EXPAND);

    HTREEITEM warnings = tree_.InsertItem(_T("Warnings:"));
    const std::vector<std::string> found = c1kit::cob_warnings(cob, sheet_.expired(cob));
    if (found.empty()) tree_.InsertItem(_T("None"), warnings);
    for (const std::string& warning : found) tree_.InsertItem(text(warning), warnings);
    tree_.Expand(warnings, TVE_EXPAND);

    HTREEITEM removal = tree_.InsertItem(_T("Removal:"));
    const std::size_t dot = entry.path.find_last_of('.');
    bool has_rcb = false;
    for (const char* extension : {".rcb", ".RCB"}) {
        if (dot != std::string::npos &&
            ::GetFileAttributesA((entry.path.substr(0, dot) + extension).c_str()) != INVALID_FILE_ATTRIBUTES) {
            tree_.InsertItem(_T("By its removal file, ") + text(entry.path.substr(0, dot) + extension), removal);
            has_rcb = true;
            break;
        }
    }
    if (!has_rcb) {
        const std::string generated = c1kit::generated_removal(cob);
        if (generated.empty()) {
            tree_.InsertItem(_T("Nothing to remove"), removal);
        } else {
            commands_under(tree_.InsertItem(_T("No .rcb: a removal would be generated"), removal), generated);
        }
    }
    tree_.Expand(removal, TVE_EXPAND);
    tree_.SetRedraw(TRUE);
}

} // namespace injector
