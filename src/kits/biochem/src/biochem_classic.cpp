// The Biochemistry Kit's classic skin: the v1.2 kit's own pages on its
// templates (dialogs 150, 151 and 152), sharing only the sheet's data with
// this build's pages.  See biochem.hpp.

#include "biochem.hpp"
#include "biochem_ids.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace biochem {
namespace {

CString text(const std::string& value) {
    return CString(value.c_str());
}

CString chemical_entry(const std::vector<std::string>& names, int chemical) {
    CString entry;
    entry.Format(_T("%s (%d)"), text(c1kitshell::chemical_display_name(names, chemical)).GetString(), chemical);
    return entry;
}

std::string read_text(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool write_text(const std::string& path, const std::string& value) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << value;
    return static_cast<bool>(out);
}

int number_in(const CWnd* control, int fallback) {
    CString value;
    if (control != nullptr) control->GetWindowText(value);
    return value.IsEmpty() ? fallback : _ttoi(value);
}

CComboBox* combo(CWnd* page, UINT id) {
    return static_cast<CComboBox*>(page->GetDlgItem(id));
}

// Dialog 150's, 151's and 152's controls.
constexpr UINT kMonitorFilter = 1201, kMonitorChemical = 1200, kAdd = 1202, kRemove = 1203, kClear = 1204;
constexpr UINT kSaved = 1206, kLoad = 1231, kSave = 1207, kDelete = 1208, kFollowed = 1205, kGraph = 1019;
constexpr UINT kSyringe = 1216, kInjectChemical = 1210, kInjectFilter = 1211, kDose = 1212, kAmount = 1213;
constexpr UINT kInjectButton = 1214, kRepeat = 1217, kEvery = 1218, kCount = 1219, kRemaining = 1240;
constexpr UINT kStop = 1241, kStatus = 1215;
constexpr UINT kSearch = 1221, kNames = 1220, kIndex = 1225, kName = 1222, kRename = 1223, kSaveNames = 1224;

}  // namespace

ClassicBiochemPage::ClassicBiochemPage(BiochemSheet& sheet, UINT dialog, UINT title_string)
    : CPropertyPage(dialog), sheet_(sheet) {
    title_ = c1kitshell::load_string(title_string);
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = title_;
}

// ===========================================================================
// Biochemistry: CMonitorPage, dialog 150
// ===========================================================================
//
// Filter, Chemical, Add, Remove, Clear; saved sets (biochem_saved.txt) with
// Load, Save and Delete; the chemicals followed, each with its colour and
// level; and the v1.2 graph, sampled once a second, whose tooltip (Show
// Tooltips, on the window's row) gives the levels under the pointer.  Fixes:
// at most the graph's 32 channels (bug 1), a history that keeps going
// (bug 2), and a list whose columns fit it (the original's scrolled
// sideways).

BEGIN_MESSAGE_MAP(ClassicMonitorPage, ClassicBiochemPage)
    ON_EN_CHANGE(kMonitorFilter, &ClassicMonitorPage::OnFilterChanged)
    ON_BN_CLICKED(kAdd, &ClassicMonitorPage::OnAdd)
    ON_BN_CLICKED(kRemove, &ClassicMonitorPage::OnRemove)
    ON_BN_CLICKED(kClear, &ClassicMonitorPage::OnClear)
    ON_BN_CLICKED(kLoad, &ClassicMonitorPage::OnLoad)
    ON_BN_CLICKED(kSave, &ClassicMonitorPage::OnSave)
    ON_BN_CLICKED(kDelete, &ClassicMonitorPage::OnDelete)
END_MESSAGE_MAP()

ClassicMonitorPage::ClassicMonitorPage(BiochemSheet& sheet)
    : ClassicBiochemPage(sheet, 150, kStringMonitorTab) {}

BOOL ClassicMonitorPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    followed_.SubclassDlgItem(kFollowed, this);
    followed_.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    CRect inside;
    followed_.GetClientRect(&inside);
    const int colour = 6, value = 36;
    followed_.InsertColumn(0, _T(""), LVCFMT_LEFT, colour);
    followed_.InsertColumn(1, _T("Chemical"), LVCFMT_LEFT,
                           inside.Width() - colour - value - GetSystemMetrics(SM_CXVSCROLL));
    followed_.InsertColumn(2, _T("Val"), LVCFMT_RIGHT, value);
    if (CWnd* frame = GetDlgItem(kGraph)) {
        CRect area;
        frame->GetWindowRect(&area);
        ScreenToClient(&area);
        frame->ShowWindow(SW_HIDE);
        graph_.create(*this, 0x7f00 + kGraph, [this](CDC& dc, const CRect& rect) {
            plot_.draw(dc, rect, sheet_.chemical_names(), CString());
        });
        graph_.SetWindowPos(&wndBottom, area.left, area.top, area.Width(), area.Height(), SWP_NOACTIVATE);
        graph_.set_mouse_handler([this](CPoint point, bool) {
            plot_.set_pointer(tooltips_ ? point.x : -1);
            graph_.redraw();
        });
    }
    if (CComboBox* chemical = combo(this, kMonitorChemical)) {
        fill_chemical_combo(*chemical, sheet_.chemical_names(), CString(), -1);
    }
    fill_saved();
    return TRUE;
}

BOOL ClassicMonitorPage::OnSetActive() {
    sheet_.show_tooltips_check(true);
    return CPropertyPage::OnSetActive();
}

BOOL ClassicMonitorPage::OnKillActive() {
    sheet_.show_tooltips_check(false);
    return CPropertyPage::OnKillActive();
}

void ClassicMonitorPage::set_tooltips(bool on) {
    tooltips_ = on;
    if (!on) {
        plot_.set_pointer(-1);
        graph_.redraw();
    }
}

void ClassicMonitorPage::names_changed() {
    if (GetSafeHwnd() == nullptr) return;
    OnFilterChanged();
    fill_followed();
    graph_.redraw();
}

void ClassicMonitorPage::subject_changed() {
    plot_.clear();
    if (GetSafeHwnd() != nullptr) {
        fill_followed();
        graph_.redraw();
    }
}

void ClassicMonitorPage::OnFilterChanged() {
    CString filter;
    GetDlgItemText(kMonitorFilter, filter);
    if (CComboBox* chemical = combo(this, kMonitorChemical)) {
        fill_chemical_combo(*chemical, sheet_.chemical_names(), filter, combo_chemical(*chemical));
    }
}

void ClassicMonitorPage::set_followed(const std::vector<int>& chemicals) {
    std::vector<int> kept;
    for (const int chemical : chemicals) {
        if (static_cast<int>(kept.size()) >= kMaxChannels) break;
        if (std::find(kept.begin(), kept.end(), chemical) == kept.end()) kept.push_back(chemical);
    }
    plot_.set_chemicals(kept);
    fill_followed();
    graph_.redraw();
}

// Each row: its line's colour, the chemical and its level.
void ClassicMonitorPage::fill_followed() {
    if (followed_.GetSafeHwnd() == nullptr) return;
    followed_.SetRedraw(FALSE);
    followed_.DeleteAllItems();
    const std::vector<c1kitshell::ChemicalGraph::Series>& series = plot_.series();
    for (std::size_t i = 0; i < series.size(); ++i) {
        const int row = followed_.InsertItem(static_cast<int>(i), _T(""));
        followed_.SetItemText(row, 1, chemical_entry(sheet_.chemical_names(), series[i].chemical));
        if (!series[i].values.empty()) {
            CString level;
            level.Format(_T("%d"), series[i].values.back());
            followed_.SetItemText(row, 2, level);
        }
    }
    followed_.SetRedraw(TRUE);
}

BOOL ClassicMonitorPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == kFollowed && header->code == NM_CUSTOMDRAW) {
        auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(lparam);
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) {
            *result = CDRF_NOTIFYITEMDRAW;
            return TRUE;
        }
        if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            *result = CDRF_NOTIFYSUBITEMDRAW;
            return TRUE;
        }
        if (draw->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM) && draw->iSubItem == 0) {
            CRect cell;
            followed_.GetSubItemRect(static_cast<int>(draw->nmcd.dwItemSpec), 0, LVIR_BOUNDS, cell);
            cell.right = cell.left + 6;
            ::FillRect(draw->nmcd.hdc, &cell, CBrush(c1kitshell::series_colour(static_cast<int>(draw->nmcd.dwItemSpec))));
            *result = CDRF_SKIPDEFAULT;
            return TRUE;
        }
    }
    return CPropertyPage::OnNotify(wparam, lparam, result);
}

void ClassicMonitorPage::OnAdd() {
    const CComboBox* chemical_box = combo(this, kMonitorChemical);
    const int chemical = chemical_box != nullptr ? combo_chemical(*chemical_box) : -1;
    if (chemical < 0) return;
    if (static_cast<int>(plot_.series().size()) >= kMaxChannels) {
        AfxMessageBox(_T("The graph can follow 32 chemicals at once."));
        return;
    }
    std::vector<int> chemicals = plot_.chemicals();
    chemicals.push_back(chemical);
    set_followed(chemicals);
}

void ClassicMonitorPage::OnRemove() {
    const int row = followed_.GetNextItem(-1, LVNI_SELECTED);
    if (row < 0) return;
    std::vector<int> chemicals = plot_.chemicals();
    chemicals.erase(chemicals.begin() + row);
    set_followed(chemicals);
}

void ClassicMonitorPage::OnClear() {
    set_followed({});
}

std::string ClassicMonitorPage::saved_path() const {
    return sheet_.kit_file(c1kit::kBiochemSavedFileName);
}

void ClassicMonitorPage::fill_saved() {
    CComboBox* saved = combo(this, kSaved);
    if (saved == nullptr) return;
    CString typed;
    saved->GetWindowText(typed);
    saved->ResetContent();
    for (const c1kit::SavedChemicalSet& set : c1kit::parse_saved_sets(read_text(saved_path()))) {
        saved->AddString(text(set.name));
    }
    saved->SetWindowText(typed);
}

void ClassicMonitorPage::OnLoad() {
    CString name;
    GetDlgItemText(kSaved, name);
    for (const c1kit::SavedChemicalSet& set : c1kit::parse_saved_sets(read_text(saved_path()))) {
        if (c1kit::same_name(set.name, std::string(CStringA(name)))) {
            set_followed(set.chemicals);
            return;
        }
    }
}

void ClassicMonitorPage::OnSave() {
    CString name;
    GetDlgItemText(kSaved, name);
    name.Trim();
    if (name.IsEmpty() || plot_.series().empty()) {
        AfxMessageBox(_T("Type a name for the set, and add chemicals to it."));
        return;
    }
    c1kit::SavedChemicalSet set;
    set.name = std::string(CStringA(name));
    set.chemicals = plot_.chemicals();
    if (!write_text(saved_path(), c1kit::update_saved_sets(read_text(saved_path()), set))) {
        AfxMessageBox(_T("Could not write biochem_saved.txt"));
    }
    fill_saved();
}

void ClassicMonitorPage::OnDelete() {
    CString name;
    GetDlgItemText(kSaved, name);
    if (name.IsEmpty()) return;
    c1kit::SavedChemicalSet set;
    set.name = std::string(CStringA(name));
    write_text(saved_path(), c1kit::update_saved_sets(read_text(saved_path()), set, true));
    SetDlgItemText(kSaved, _T(""));
    fill_saved();
}

void ClassicMonitorPage::sample() {
    const std::vector<int> chemicals = plot_.chemicals();
    if (chemicals.empty() || !sheet_.subject_present()) return;
    std::string reply;
    std::vector<int> values;
    if (!sheet_.query(c1kit::chemical_levels_query(chemicals), reply) ||
        !c1kit::parse_values(reply, chemicals.size(), values)) {
        return;
    }
    plot_.add_sample(values);
    if (GetSafeHwnd() != nullptr && IsWindowVisible()) {
        for (std::size_t i = 0; i < values.size() && static_cast<int>(i) < followed_.GetItemCount(); ++i) {
            CString level;
            level.Format(_T("%d"), values[i]);
            followed_.SetItemText(static_cast<int>(i), 2, level);
        }
        graph_.redraw();
    }
}

// ===========================================================================
// Injections: CInjectPage, dialog 151
// ===========================================================================
//
// The syringe (DOSE.bmp and the chemical's dosage sprite), the chemical and
// its filter, the upright dosage slider (most at the top) and the amount,
// Inject, and the repeat: every n seconds, n times (0: no limit), with what
// remains, stopped by Stop or by changing the chemical.  The syringe plays
// its injection and refills to the dose, where the original's stayed empty.

BEGIN_MESSAGE_MAP(ClassicInjectPage, ClassicBiochemPage)
    ON_EN_CHANGE(kInjectFilter, &ClassicInjectPage::OnFilterChanged)
    ON_CBN_SELCHANGE(kInjectChemical, &ClassicInjectPage::OnChemicalChanged)
    ON_BN_CLICKED(kInjectButton, &ClassicInjectPage::OnInject)
    ON_BN_CLICKED(kRepeat, &ClassicInjectPage::OnRepeatChanged)
    ON_BN_CLICKED(kStop, &ClassicInjectPage::OnStop)
    ON_EN_CHANGE(kAmount, &ClassicInjectPage::OnAmountChanged)
    ON_WM_VSCROLL()
    ON_WM_TIMER()
END_MESSAGE_MAP()

ClassicInjectPage::ClassicInjectPage(BiochemSheet& sheet)
    : ClassicBiochemPage(sheet, 151, kStringInjectTab) {}

BOOL ClassicInjectPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    const CString palettes = c1kitshell::game_directory_setting("Palette Directory");
    palette_.load(std::string(CStringA(palettes.IsEmpty() ? CString(_T("Palettes\\")) : palettes)) + "palette.dta");
    syringe_.load(std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))), kDosageFiles[0],
                  palette_);
    liquid_ = 0;
    if (CWnd* frame = GetDlgItem(kSyringe)) {
        CRect area;
        frame->GetWindowRect(&area);
        ScreenToClient(&area);
        frame->ShowWindow(SW_HIDE);
        syringe_view_.create(*this, 0x7f00 + kSyringe, [this](CDC& dc, const CRect& rect) { syringe_.draw(dc, rect); });
        syringe_view_.SetWindowPos(&wndBottom, area.left, area.top, area.Width(), area.Height(), SWP_NOACTIVATE);
    }
    if (auto* slider = static_cast<CSliderCtrl*>(GetDlgItem(kDose))) slider->SetRange(0, 255);
    SetDlgItemText(kEvery, _T("5"));
    SetDlgItemText(kCount, _T("0"));
    if (CComboBox* chemical = combo(this, kInjectChemical)) {
        fill_chemical_combo(*chemical, sheet_.chemical_names(), CString(), -1);
    }
    syncing_ = true;
    SetDlgItemText(kAmount, _T("0"));
    syncing_ = false;
    set_dose(0);
    change_liquid();
    update_repeat_controls();
    return TRUE;
}

void ClassicInjectPage::names_changed() {
    if (GetSafeHwnd() == nullptr) return;
    OnFilterChanged();
}

int ClassicInjectPage::dose() const {
    const auto* slider = static_cast<const CSliderCtrl*>(GetDlgItem(kDose));
    return slider != nullptr ? 255 - slider->GetPos() : 0;
}

void ClassicInjectPage::set_dose(int value) {
    if (auto* slider = static_cast<CSliderCtrl*>(GetDlgItem(kDose))) slider->SetPos(255 - value);
    syringe_.set_dose(value);
    syringe_view_.redraw();
}

// OnChemicalSelectionChanged @ 0x00407cd0: the liquid is the chemical's
// (number mod 7, the kit's path table at 0x00421020).
void ClassicInjectPage::change_liquid() {
    const CComboBox* chemical_box = combo(this, kInjectChemical);
    const int chemical = chemical_box != nullptr ? combo_chemical(*chemical_box) : -1;
    const int liquid = chemical < 0 ? 0 : chemical % 7;
    if (liquid != liquid_ && syringe_.set_liquid(kDosageFiles[liquid])) {
        liquid_ = liquid;
        syringe_view_.redraw();
    }
}

void ClassicInjectPage::OnFilterChanged() {
    CString filter;
    GetDlgItemText(kInjectFilter, filter);
    if (CComboBox* chemical = combo(this, kInjectChemical)) {
        fill_chemical_combo(*chemical, sheet_.chemical_names(), filter, combo_chemical(*chemical));
    }
    OnChemicalChanged();
}

void ClassicInjectPage::OnChemicalChanged() {
    const CComboBox* chemical_box = combo(this, kInjectChemical);
    if (repeating_ && chemical_box != nullptr && combo_chemical(*chemical_box) != repeat_chemical_) {
        stop_repeat(_T("Repeat stopped (chemical changed)"));
    }
    change_liquid();
}

void ClassicInjectPage::OnVScroll(UINT code, UINT position, CScrollBar* bar) {
    CPropertyPage::OnVScroll(code, position, bar);
    if (syringe_.injecting()) return;
    syncing_ = true;
    CString value;
    value.Format(_T("%d"), dose());
    SetDlgItemText(kAmount, value);
    syncing_ = false;
    syringe_.set_dose(dose());
    syringe_view_.redraw();
}

void ClassicInjectPage::OnAmountChanged() {
    if (syncing_) return;
    set_dose((std::min)(255, (std::max)(0, number_in(GetDlgItem(kAmount), 0))));
}

bool ClassicInjectPage::inject_once() {
    const CComboBox* chemical_box = combo(this, kInjectChemical);
    const int chemical = repeating_ ? repeat_chemical_ : (chemical_box != nullptr ? combo_chemical(*chemical_box) : -1);
    const int amount = (std::min)(255, (std::max)(0, number_in(GetDlgItem(kAmount), 0)));
    std::string reply;
    if (chemical < 0 || !sheet_.query(c1kit::injection_script(chemical, amount), reply)) {
        SetDlgItemText(kStatus, _T("The game did not take the injection."));
        return false;
    }
    if (syringe_.loaded() && !syringe_.injecting()) {
        syringe_.start_injection();
        SetTimer(kTimerSyringe, c1kitshell::Syringe::kTickMs, nullptr);
    }
    CString done;
    done.Format(_T("Injected %d of %s"), amount,
                text(c1kitshell::chemical_display_name(sheet_.chemical_names(), chemical)).GetString());
    SetDlgItemText(kStatus, done);
    return true;
}

// Inject: once, or start the repeat (StartRepeatInjection @ 0x00408610).
void ClassicInjectPage::OnInject() {
    const auto* repeat = static_cast<const CButton*>(GetDlgItem(kRepeat));
    if (repeat == nullptr || repeat->GetCheck() != BST_CHECKED) {
        inject_once();
        return;
    }
    if (repeating_) return;
    const CComboBox* chemical_box = combo(this, kInjectChemical);
    repeat_chemical_ = chemical_box != nullptr ? combo_chemical(*chemical_box) : -1;
    repeat_total_ = (std::max)(0, number_in(GetDlgItem(kCount), 0));
    repeat_done_ = 0;
    repeating_ = true;
    update_repeat_controls();
    repeat_tick();
    if (repeating_) {
        sheet_.SetTimer(kTimerRepeat, static_cast<UINT>((std::max)(1, number_in(GetDlgItem(kEvery), 5)) * 1000), nullptr);
    }
}

void ClassicInjectPage::repeat_tick() {
    if (!repeating_) return;
    if (!inject_once()) {
        stop_repeat(_T("Repeat injection stopped"));
        return;
    }
    ++repeat_done_;
    if (repeat_total_ > 0 && repeat_done_ >= repeat_total_) {
        stop_repeat(_T("Repeat injection complete"));
        return;
    }
    update_repeat_controls();
}

void ClassicInjectPage::OnStop() {
    stop_repeat(_T("Repeat injection stopped"));
}

void ClassicInjectPage::stop_repeat(const CString& why) {
    if (repeating_) {
        sheet_.KillTimer(kTimerRepeat);
        repeating_ = false;
        SetDlgItemText(kStatus, why);
    }
    update_repeat_controls();
}

void ClassicInjectPage::OnRepeatChanged() {
    const auto* repeat = static_cast<const CButton*>(GetDlgItem(kRepeat));
    if (repeat != nullptr && repeat->GetCheck() != BST_CHECKED && repeating_) {
        stop_repeat(_T("Repeat injection stopped"));
    }
    update_repeat_controls();
}

void ClassicInjectPage::update_repeat_controls() {
    const auto* repeat = static_cast<const CButton*>(GetDlgItem(kRepeat));
    const bool on = repeat != nullptr && repeat->GetCheck() == BST_CHECKED;
    if (CWnd* every = GetDlgItem(kEvery)) every->EnableWindow(on && !repeating_);
    if (CWnd* count = GetDlgItem(kCount)) count->EnableWindow(on && !repeating_);
    if (CWnd* stop = GetDlgItem(kStop)) stop->EnableWindow(repeating_);
    CString remaining;
    if (repeating_) {
        if (repeat_total_ > 0) {
            remaining.Format(_T("%d"), repeat_total_ - repeat_done_);
        } else {
            remaining = _T("no limit");
        }
    }
    SetDlgItemText(kRemaining, remaining);
}

void ClassicInjectPage::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerSyringe) {
        const bool more = syringe_.tick();
        if (auto* slider = static_cast<CSliderCtrl*>(GetDlgItem(kDose))) {
            slider->SetPos(255 - (more ? syringe_.shown_dose() : syringe_.dose()));
        }
        syringe_view_.redraw();
        if (!more) KillTimer(kTimerSyringe);
        return;
    }
    CPropertyPage::OnTimer(timer_id);
}

// ===========================================================================
// Chemical Names: CChemicalsPage, dialog 152
// ===========================================================================
//
// Every chemical by number with Search; the picked one's index and name,
// Rename, and Save, which writes allchemicals.str (asking first: every kit
// reads it).

BEGIN_MESSAGE_MAP(ClassicNamesPage, ClassicBiochemPage)
    ON_EN_CHANGE(kSearch, &ClassicNamesPage::OnSearchChanged)
    ON_BN_CLICKED(kRename, &ClassicNamesPage::OnRename)
    ON_BN_CLICKED(kSaveNames, &ClassicNamesPage::OnSaveNames)
END_MESSAGE_MAP()

ClassicNamesPage::ClassicNamesPage(BiochemSheet& sheet)
    : ClassicBiochemPage(sheet, 152, kStringNamesTab) {}

BOOL ClassicNamesPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    names_.SubclassDlgItem(kNames, this);
    names_.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    CRect inside;
    names_.GetClientRect(&inside);
    names_.InsertColumn(0, _T("#"), LVCFMT_LEFT, 40);
    names_.InsertColumn(1, _T("Chemical Name"), LVCFMT_LEFT, inside.Width() - 40 - GetSystemMetrics(SM_CXVSCROLL));
    fill();
    return TRUE;
}

void ClassicNamesPage::names_changed() {
    if (GetSafeHwnd() != nullptr) fill();
}

void ClassicNamesPage::fill() {
    CString filter;
    GetDlgItemText(kSearch, filter);
    filter.MakeLower();
    names_.SetRedraw(FALSE);
    names_.DeleteAllItems();
    const std::vector<std::string>& names = sheet_.chemical_names();
    for (std::size_t i = 0; i < names.size(); ++i) {
        CString name = text(names[i]);
        CString lower = name;
        lower.MakeLower();
        CString number;
        number.Format(_T("%d"), static_cast<int>(i));
        if (!filter.IsEmpty() && lower.Find(filter) < 0 && number.Find(filter) < 0) continue;
        const int row = names_.InsertItem(names_.GetItemCount(), number);
        names_.SetItemText(row, 1, name);
        names_.SetItemData(row, static_cast<DWORD_PTR>(i));
    }
    names_.SetRedraw(TRUE);
}

int ClassicNamesPage::selected_chemical() const {
    const int row = names_.GetNextItem(-1, LVNI_SELECTED);
    return row < 0 ? -1 : static_cast<int>(names_.GetItemData(row));
}

BOOL ClassicNamesPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == kNames && header->code == LVN_ITEMCHANGED) {
        const int chemical = selected_chemical();
        CString index;
        if (chemical >= 0) index.Format(_T("%d"), chemical);
        SetDlgItemText(kIndex, index);
        const std::vector<std::string>& names = sheet_.chemical_names();
        SetDlgItemText(kName, chemical >= 0 && chemical < static_cast<int>(names.size())
                                  ? text(names[static_cast<std::size_t>(chemical)]) : CString());
    }
    return CPropertyPage::OnNotify(wparam, lparam, result);
}

void ClassicNamesPage::OnSearchChanged() {
    fill();
}

void ClassicNamesPage::OnRename() {
    const int chemical = selected_chemical();
    std::vector<std::string>& names = sheet_.chemical_names();
    if (chemical < 0 || chemical >= static_cast<int>(names.size())) return;
    CString name;
    GetDlgItemText(kName, name);
    names[static_cast<std::size_t>(chemical)] = std::string(CStringA(name));
    sheet_.names_changed();
}

void ClassicNamesPage::OnSaveNames() {
    if (AfxMessageBox(_T("Save the chemical names to allchemicals.str? Every kit reads them."),
                      MB_YESNO | MB_ICONQUESTION) == IDYES) {
        sheet_.save_chemical_names();
    }
}

}  // namespace biochem
