// The Object Injector's classic skin: the 2.0 kit's own pages on its
// templates (dialogs 150 and 151), sharing only the sheet's data and
// actions with this build's pages.  See injector.hpp.

#include "injector.hpp"
#include "injector_ids.hpp"

#include <algorithm>

namespace injector {
namespace {

CString text(const std::string& value) {
    return CString(value.c_str());
}

CString quantity_text(const InjectorSheet& sheet, const c1kit::Cob& cob) {
    if (sheet.expired(cob)) return c1kitshell::load_string(kStringExpired);
    if (cob.unlimited()) return c1kitshell::load_string(kStringInfinite);
    CString count;
    count.Format(_T("%d"), cob.quantity);
    return count;
}

// PopulateCobDetailsTree @ 0x00407300's chemicals drawn red, with the red
// mark: the drives and their increases, punishment, ageing, glycotoxin,
// alcohol, the adrenalines, geddonase, the histamines and the antigens.
bool harmful(int chemical) {
    return (chemical >= 1 && chemical <= 13) || (chemical >= 17 && chemical <= 29) || chemical == 50 ||
           chemical == 56 || chemical == 67 || chemical == 68 || chemical == 69 || chemical == 101 ||
           chemical == 231 || (chemical >= 232 && chemical < 240) || (chemical >= 248 && chemical < 256);
}

// Dialog 150's and 151's controls.
constexpr UINT kCobsFind = 1230, kCobsList = 1200, kPicture = 1201, kDescription = 1202;
constexpr UINT kInject = 1203, kRemove = 1204, kRefresh = 1209, kQuantityLabel = 1208, kQuantity = 1205;
constexpr UINT kBrowse = 1206, kFolder = 1207;
constexpr UINT kAnalysisFind = 1232, kAnalysisList = 1210, kTree = 1211, kAnalysisBrowse = 1212;
constexpr UINT kAnalysisRefresh = 1216, kAnalysisFolder = 1213;
// Tree images, from the original's bitmap 200 (eight of 16 x 16).
constexpr UINT kBitmapTreeImages = 200;
enum TreeImage { kImageGeneral, kImageScripts, kImageChemicals, kImageWarnings, kImageItem, kImageScript,
                 kImageGood, kImageBad };
constexpr LPARAM kRedItem = 1;
constexpr LPARAM kBoldItem = 2;

}  // namespace

// ===========================================================================
// The find box and COB list
// ===========================================================================

ClassicCobListPage::ClassicCobListPage(InjectorSheet& sheet, UINT dialog, UINT title_string, UINT find,
                                       UINT list, UINT browse, UINT refresh, UINT folder)
    : CPropertyPage(dialog), sheet_(sheet), find_id_(find), list_id_(list), browse_id_(browse),
      refresh_id_(refresh), folder_id_(folder) {
    title_ = c1kitshell::load_string(title_string);
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = title_;
}

// The 2.0 list was a plain list of names; its one column is as wide as the
// list, so it never scrolls sideways as the original's did.
BOOL ClassicCobListPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    list_.SubclassDlgItem(list_id_, this);
    list_.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    CRect inside;
    list_.GetClientRect(&inside);
    list_.InsertColumn(0, _T(""), LVCFMT_LEFT, inside.Width() - GetSystemMetrics(SM_CXVSCROLL));
    fill_list();
    return TRUE;
}

// ApplyCobFilterAndRefresh @ 0x00403d80: the COBs whose name holds the find
// text.  Item data is the entry index.
void ClassicCobListPage::fill_list() {
    if (list_.GetSafeHwnd() == nullptr) return;
    const int previous = selected_index();
    CString filter;
    GetDlgItemText(find_id_, filter);
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
        list_.SetItemData(row, static_cast<DWORD_PTR>(i));
        if (static_cast<int>(i) == previous) {
            list_.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
    list_.SetRedraw(TRUE);
    SetDlgItemText(folder_id_, text(sheet_.folder()));
}

int ClassicCobListPage::selected_index() const {
    if (list_.GetSafeHwnd() == nullptr) return -1;
    const int row = list_.GetNextItem(-1, LVNI_SELECTED);
    return row < 0 ? -1 : static_cast<int>(list_.GetItemData(row));
}

void ClassicCobListPage::cobs_changed() {
    if (list_.GetSafeHwnd() != nullptr) {
        fill_list();
        selection_changed();
    }
}

BOOL ClassicCobListPage::OnCommand(WPARAM wparam, LPARAM lparam) {
    const UINT id = LOWORD(wparam);
    const UINT code = HIWORD(wparam);
    if (id == find_id_ && code == EN_CHANGE) {
        fill_list();
        selection_changed();
        return TRUE;
    }
    if (id == browse_id_ && code == BN_CLICKED) {
        sheet_.browse_for_folder(this);
        return TRUE;
    }
    if (id == refresh_id_ && code == BN_CLICKED) {
        sheet_.reload();
        return TRUE;
    }
    return CPropertyPage::OnCommand(wparam, lparam);
}

BOOL ClassicCobListPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == list_id_ && header->code == LVN_ITEMCHANGED) {
        const auto* change = reinterpret_cast<const NMLISTVIEW*>(lparam);
        if ((change->uChanged & LVIF_STATE) != 0 && ((change->uNewState ^ change->uOldState) & LVIS_SELECTED) != 0) {
            selection_changed();
        }
    }
    return CPropertyPage::OnNotify(wparam, lparam, result);
}

// ===========================================================================
// COBs: CAgentsPage, dialog 150
// ===========================================================================
//
// The picture (CDibView: the sheet's Alima.bmp stretched behind the COB's
// picture, drawn at its own size with index 0 left out), the description,
// Inject, Remove and Refresh, how many are left, Browse and the folder.  The
// template put "Quantity remaining:" under the Refresh button; here it sits
// clear of it, shortened to fit, beside its box.

ClassicCobsPage::ClassicCobsPage(InjectorSheet& sheet)
    : ClassicCobListPage(sheet, 150, kStringCobsTab, kCobsFind, kCobsList, kBrowse, kRefresh, kFolder) {}

BOOL ClassicCobsPage::OnInitDialog() {
    ClassicCobListPage::OnInitDialog();
    if (CWnd* frame = GetDlgItem(kPicture)) {
        CRect area;
        frame->GetWindowRect(&area);
        ScreenToClient(&area);
        frame->ShowWindow(SW_HIDE);
        picture_.create(*this, 0x7f00 + kPicture, [this](CDC& dc, const CRect& rect) { draw_picture(dc, rect); });
        picture_.SetWindowPos(&wndBottom, area.left, area.top, area.Width(), area.Height(), SWP_NOACTIVATE);
    }
    backdrop_.load_file(c1kitshell::game_directory_setting("Main Directory") + kPictureBackdrop);
    if (CWnd* label = GetDlgItem(kQuantityLabel)) {
        CRect place(166, 216, 196, 224);
        MapDialogRect(&place);
        label->SetWindowText(_T("Quantity:"));
        label->MoveWindow(&place);
        CRect box(198, 213, 236, 225);
        MapDialogRect(&box);
        if (CWnd* quantity = GetDlgItem(kQuantity)) quantity->MoveWindow(&box);
    }
    selection_changed();
    return TRUE;
}

// The description and warnings (BuildInjectionSafetyWarnings), and the count.
void ClassicCobsPage::selection_changed() {
    const int index = selected_index();
    const bool valid = index >= 0 && index < static_cast<int>(sheet_.entries().size());
    if (CWnd* inject = GetDlgItem(kInject)) inject->EnableWindow(valid);
    if (CWnd* remove = GetDlgItem(kRemove)) remove->EnableWindow(valid);
    if (!valid) {
        SetDlgItemText(kDescription, _T(""));
        SetDlgItemText(kQuantity, _T(""));
        picture_.redraw();
        return;
    }
    const c1kit::Cob& cob = sheet_.entries()[static_cast<std::size_t>(index)].cob;
    CString shown = cob.description.empty() ? CString(_T("No description available.")) : text(cob.description);
    shown.Replace(_T("\r\n"), _T("\n"));
    shown.Replace(_T("\n"), _T("\r\n"));
    for (const std::string& warning : c1kit::cob_warnings(cob, sheet_.expired(cob))) {
        shown += _T("\r\n") + text(warning);
    }
    SetDlgItemText(kDescription, shown);
    SetDlgItemText(kQuantity, quantity_text(sheet_, cob));
    picture_.redraw();
}

BOOL ClassicCobsPage::OnCommand(WPARAM wparam, LPARAM lparam) {
    const UINT id = LOWORD(wparam);
    if (HIWORD(wparam) == BN_CLICKED && (id == kInject || id == kRemove)) {
        const int index = selected_index();
        if (index < 0 || index >= static_cast<int>(sheet_.entries().size())) return TRUE;
        CString why;
        CobEntry& entry = sheet_.entries()[static_cast<std::size_t>(index)];
        const bool done = id == kInject ? sheet_.inject(entry, why) : sheet_.remove(entry, why);
        if (!done && !why.IsEmpty()) AfxMessageBox(why, MB_ICONINFORMATION);
        fill_list();
        selection_changed();
        return TRUE;
    }
    return ClassicCobListPage::OnCommand(wparam, lparam);
}

void ClassicCobsPage::draw_picture(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, RGB(0, 0, 0));
    if (backdrop_.width() > 0) {
        CDC source;
        source.CreateCompatibleDC(&dc);
        CBitmap* previous = source.SelectObject(&backdrop_.bitmap());
        dc.SetStretchBltMode(COLORONCOLOR);
        dc.StretchBlt(rect.left, rect.top, rect.Width(), rect.Height(), &source, 0, 0, backdrop_.width(),
                      backdrop_.height(), SRCCOPY);
        source.SelectObject(previous);
    }
    const int index = selected_index();
    if (index < 0 || index >= static_cast<int>(sheet_.entries().size())) return;
    const c1kit::CobSprite& sprite = sheet_.entries()[static_cast<std::size_t>(index)].cob.sprite;
    if (sprite.width <= 0 || sprite.height <= 0 || !canvas_.create(sprite.width, sprite.height)) return;
    canvas_.fill(0);
    canvas_.draw_indexed(sprite.pixels.data(), sprite.width, sprite.height, sprite.stride, false, 0, 0,
                         sheet_.palette());
    // A mask set where index 0 (the background) is; a CreateBitmap
    // monochrome bitmap's rows are padded to 16 bits.
    const int mask_stride = ((sprite.width + 15) / 16) * 2;
    std::vector<BYTE> mask_bits(static_cast<std::size_t>(mask_stride) * sprite.height, 0);
    for (int y = 0; y < sprite.height; ++y) {
        for (int x = 0; x < sprite.width; ++x) {
            if (sprite.pixels[static_cast<std::size_t>(y) * sprite.stride + x] == 0) {
                mask_bits[static_cast<std::size_t>(y) * mask_stride + x / 8] |= static_cast<BYTE>(0x80 >> (x % 8));
            }
        }
    }
    CBitmap mask, colour;
    mask.CreateBitmap(sprite.width, sprite.height, 1, 1, mask_bits.data());
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

// ===========================================================================
// Analysis: CAnalysisPage, dialog 151
// ===========================================================================
//
// PopulateCobDetailsTree @ 0x00407300: General Information (the file and the
// kind of thing it makes), Scripts (the installation script, then each
// script it installs by its classifier and event), Chemicals Affected (red
// for the harmful ones, green for the rest) and Warnings (DANGER in bold),
// with the original's own tree pictures.

ClassicAnalysisPage::ClassicAnalysisPage(InjectorSheet& sheet)
    : ClassicCobListPage(sheet, 151, kStringAnalysisTab, kAnalysisFind, kAnalysisList, kAnalysisBrowse,
                         kAnalysisRefresh, kAnalysisFolder) {}

BOOL ClassicAnalysisPage::OnInitDialog() {
    ClassicCobListPage::OnInitDialog();
    tree_.SubclassDlgItem(kTree, this);
    if (const c1kitshell::ClassicArt* art = sheet_.classic_art()) {
        HBITMAP strip = static_cast<HBITMAP>(
            ::LoadImage(art->module(), MAKEINTRESOURCE(kBitmapTreeImages), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        if (strip != nullptr) {
            CBitmap bitmap;
            bitmap.Attach(strip);
            CDC probe;
            probe.CreateCompatibleDC(nullptr);
            CBitmap* previous = probe.SelectObject(&bitmap);
            const COLORREF key = probe.GetPixel(0, 0);
            probe.SelectObject(previous);
            images_.Create(16, 16, ILC_COLOR24 | ILC_MASK, 8, 0);
            images_.Add(&bitmap, key);
            tree_.SetImageList(&images_, TVSIL_NORMAL);
        }
    }
    selection_changed();
    return TRUE;
}

void ClassicAnalysisPage::selection_changed() {
    if (tree_.GetSafeHwnd() == nullptr) return;
    tree_.SetRedraw(FALSE);
    tree_.DeleteAllItems();
    const int index = selected_index();
    if (index < 0 || index >= static_cast<int>(sheet_.entries().size())) {
        tree_.SetRedraw(TRUE);
        return;
    }
    const CobEntry& entry = sheet_.entries()[static_cast<std::size_t>(index)];
    const c1kit::Cob& cob = entry.cob;
    const auto add = [this](const CString& label, int image, HTREEITEM parent, LPARAM flags = 0) {
        const UINT state = (flags & kBoldItem) != 0 ? TVIS_BOLD : 0;
        return tree_.InsertItem(TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_STATE, label,
                                image, image, state, state, flags, parent, TVI_LAST);
    };
    const auto classifier = [](const c1kit::Classifier& c) {
        CString numbers;
        numbers.Format(_T("%d %d %d"), c.family, c.genus, c.species);
        return numbers;
    };

    HTREEITEM general = add(_T("General Information"), kImageGeneral, TVI_ROOT);
    const std::size_t slash = entry.path.find_last_of("\\/");
    add(text(slash == std::string::npos ? entry.path : entry.path.substr(slash + 1)), kImageItem, general);
    const std::vector<c1kit::Classifier> made = c1kit::created_classifiers(cob.inject_scripts);
    if (!made.empty()) {
        static const TCHAR* const kKinds[] = {_T("Unknown"), _T("Scenery"), _T("Simple Object"),
                                              _T("Compound Object"), _T("Creature")};
        const int family = made.front().family;
        add(CString(_T("Type: ")) + (family >= 1 && family <= 4 ? kKinds[family] : kKinds[0]) + _T(" (") +
                classifier(made.front()) + _T(")"),
            kImageItem, general);
    }

    HTREEITEM scripts = add(_T("Scripts:"), kImageScripts, TVI_ROOT);
    for (std::size_t i = 0; i < cob.inject_scripts.size(); ++i) {
        CString label = _T("Installation script");
        if (cob.inject_scripts.size() > 1) label.AppendFormat(_T(" %d"), static_cast<int>(i) + 1);
        add(label, kImageItem, scripts);
    }
    for (const std::string& script : cob.install_scripts) {
        c1kit::InstalledScript installed;
        if (c1kit::installed_script(script, installed)) {
            add(_T("<") + classifier(installed.classifier) + _T("> ") + text(c1kit::event_name(installed.event)),
                kImageScript, scripts);
        }
    }

    HTREEITEM chemicals = add(_T("Chemicals Affected:"), kImageChemicals, TVI_ROOT);
    const std::vector<int> affected = c1kit::affected_chemicals(cob);
    const std::vector<std::string>& names = sheet_.chemical_names();
    for (const int chemical : affected) {
        CString label;
        if (chemical >= 0 && chemical < static_cast<int>(names.size()) && !names[static_cast<std::size_t>(chemical)].empty()) {
            label = text(names[static_cast<std::size_t>(chemical)]);
        } else {
            label.Format(_T("Chemical %d"), chemical);
        }
        const bool bad = harmful(chemical);
        add(label, bad ? kImageBad : kImageGood, chemicals, bad ? kRedItem : 0);
    }
    if (affected.empty()) add(_T("None"), kImageItem, chemicals);

    HTREEITEM warnings = add(_T("Warnings:"), kImageWarnings, TVI_ROOT);
    const std::vector<std::string> found = c1kit::cob_warnings(cob, sheet_.expired(cob));
    for (const std::string& warning : found) {
        add(text(warning), kImageWarnings, warnings, warning == "DANGER" ? kBoldItem : 0);
    }
    if (found.empty()) add(_T("None"), kImageItem, warnings);
    for (HTREEITEM root : {general, scripts, chemicals, warnings}) tree_.Expand(root, TVE_EXPAND);
    tree_.SetRedraw(TRUE);
}

// The harmful chemicals in red, DANGER in bold.
BOOL ClassicAnalysisPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == kTree && header->code == NM_CUSTOMDRAW) {
        auto* draw = reinterpret_cast<NMTVCUSTOMDRAW*>(lparam);
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) {
            *result = CDRF_NOTIFYITEMDRAW;
            return TRUE;
        }
        if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            if (draw->nmcd.lItemlParam == kRedItem && (draw->nmcd.uItemState & CDIS_SELECTED) == 0) {
                draw->clrText = RGB(220, 0, 0);
            }
            *result = CDRF_DODEFAULT;
            return TRUE;
        }
    }
    return ClassicCobListPage::OnNotify(wparam, lparam, result);
}

}  // namespace injector
