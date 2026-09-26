// The Biochemistry Kit's pages: Biochemistry, Injections and Chemical Names.
// The original's are described in ../ORIGINAL.md.

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
    entry.Format(_T("%s (%d)"), text(c1kitshell::chemical_display_name(names, chemical)).GetString(),
                 chemical);
    return entry;
}

std::string read_text(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool write_text(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return static_cast<bool>(out);
}

int edit_number(const CEdit& edit, int fallback) {
    CString value;
    edit.GetWindowText(value);
    return value.IsEmpty() ? fallback : _ttoi(value);
}

} // namespace

// "Name (n)" for chemicals 1-255 whose name holds `filter` (the original's
// "%s (%d)" entries); item data is the chemical.
void fill_chemical_combo(CComboBox& combo, const std::vector<std::string>& names,
                         const CString& filter, int keep_chemical) {
    CString lower_filter = filter;
    lower_filter.MakeLower();
    combo.ResetContent();
    int keep_row = -1;
    for (int chemical = 1; chemical < 256; ++chemical) {
        const CString entry = chemical_entry(names, chemical);
        CString lower = entry;
        lower.MakeLower();
        if (!lower_filter.IsEmpty() && lower.Find(lower_filter) < 0) continue;
        const int row = combo.AddString(entry);
        combo.SetItemData(row, static_cast<DWORD_PTR>(chemical));
        if (chemical == keep_chemical) keep_row = row;
    }
    combo.SetCurSel(keep_row >= 0 ? keep_row : (combo.GetCount() > 0 ? 0 : -1));
}

int combo_chemical(const CComboBox& combo) {
    const int row = combo.GetCurSel();
    return row < 0 ? -1 : static_cast<int>(combo.GetItemData(row));
}

// ===========================================================================
// Biochemistry
// ===========================================================================
//
// CMonitorPage (dialog 150): Filter, Chemical, Add/Remove/Clear, saved
// sets (biochem_saved.txt: Load/Save/Delete), the list of chemicals followed
// and a graph.  Here the graph is the one the Science Kit uses, and pointing
// at it shows every level at that moment (the original's tooltip).

BEGIN_MESSAGE_MAP(MonitorPage, c1kitshell::LayoutPage)
    ON_EN_CHANGE(kControlFilter, &MonitorPage::OnFilterChanged)
    ON_BN_CLICKED(kControlAdd, &MonitorPage::OnAdd)
    ON_BN_CLICKED(kControlRemove, &MonitorPage::OnRemove)
    ON_BN_CLICKED(kControlClear, &MonitorPage::OnClear)
    ON_BN_CLICKED(kControlLoad, &MonitorPage::OnLoad)
    ON_BN_CLICKED(kControlSave, &MonitorPage::OnSave)
    ON_BN_CLICKED(kControlDelete, &MonitorPage::OnDelete)
END_MESSAGE_MAP()

MonitorPage::MonitorPage(BiochemSheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringMonitorTab), sheet_(sheet) {}

void MonitorPage::create_controls() {
    make(filter_label_, _T("STATIC"), _T("Filter:"), SS_LEFT, kControlLabel);
    make(filter_, _T("EDIT"), _T(""), ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, kControlFilter);
    make(chemical_label_, _T("STATIC"), _T("Chemical:"), SS_LEFT, kControlLabel);
    make(chemical_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, kControlChemical);
    make(add_, _T("BUTTON"), _T("Add"), BS_PUSHBUTTON | WS_TABSTOP, kControlAdd);
    make(remove_, _T("BUTTON"), _T("Remove"), BS_PUSHBUTTON | WS_TABSTOP, kControlRemove);
    make(clear_, _T("BUTTON"), _T("Clear"), BS_PUSHBUTTON | WS_TABSTOP, kControlClear);
    make(saved_label_, _T("STATIC"), _T("Saved:"), SS_LEFT, kControlLabel);
    make(saved_, _T("COMBOBOX"), _T(""), CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, kControlSaved);
    make(load_, _T("BUTTON"), _T("Load"), BS_PUSHBUTTON | WS_TABSTOP, kControlLoad);
    make(save_, _T("BUTTON"), _T("Save"), BS_PUSHBUTTON | WS_TABSTOP, kControlSave);
    make(delete_, _T("BUTTON"), _T("Delete"), BS_PUSHBUTTON | WS_TABSTOP, kControlDelete);
    make(followed_, WC_LISTVIEW, _T(""), LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
         kControlFollowed);
    followed_.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    followed_.InsertColumn(0, _T("Chemical"), LVCFMT_LEFT, 120);
    followed_.InsertColumn(1, _T("Level"), LVCFMT_RIGHT, 44);
    graph_.create(*this, kControlGraph, [this](CDC& dc, const CRect& rect) {
        plot_.draw(dc, rect, sheet_.chemical_names(),
                   sheet_.subject_present() ? CString(_T("Add chemicals to follow, or load a saved set."))
                                            : CString(_T("Select a creature in the game.")));
    });
    graph_.set_mouse_handler([this](CPoint point, bool) {
        plot_.set_pointer(point.x);
        graph_.redraw();
    });
    fill_chemical_combo(chemical_, sheet_.chemical_names(), CString(), -1);
    fill_saved();
}

void MonitorPage::layout(int width, int height) {
    const int m = kMargin;
    const int row = button_height();
    const int line1 = m;
    const int line2 = m + row + 4;
    place(filter_label_, m, line1 + 4, 34, text_height());
    place(filter_, m + 36, line1, 110, row - 2);
    place(chemical_label_, m + 156, line1 + 4, 50, text_height());
    const int buttons = 3 * 60 + 2 * m;
    place(chemical_, m + 208, line1, width - m - 208 - buttons - 2 * m, 300);
    place(add_, width - m - buttons, line1, 60, row - 2);
    place(remove_, width - m - buttons + 60 + m, line1, 60, row - 2);
    place(clear_, width - m - 60, line1, 60, row - 2);
    place(saved_label_, m, line2 + 4, 34, text_height());
    place(saved_, m + 36, line2, 170, 300);
    place(load_, m + 214, line2, 60, row - 2);
    place(save_, m + 214 + 60 + m, line2, 60, row - 2);
    place(delete_, m + 214 + 2 * (60 + m), line2, 60, row - 2);
    const int top = line2 + row + 4;
    const int bottom = height - 2 * m - row;
    const int list_width = (std::max)(150, width / 4);
    place(followed_, m, top, list_width, bottom - top);
    followed_.SetColumnWidth(0, list_width - 44 - 6);
    place(graph_, 2 * m + list_width, top, width - 3 * m - list_width, bottom - top);
    place_close(width, height);
}

void MonitorPage::names_changed() {
    if (!created_) return;
    CString filter;
    filter_.GetWindowText(filter);
    fill_chemical_combo(chemical_, sheet_.chemical_names(), filter, combo_chemical(chemical_));
    fill_followed();
    graph_.redraw();
}

void MonitorPage::subject_changed() {
    plot_.clear();
    if (created_) {
        fill_followed();
        graph_.redraw();
    }
}

void MonitorPage::OnFilterChanged() {
    CString filter;
    filter_.GetWindowText(filter);
    fill_chemical_combo(chemical_, sheet_.chemical_names(), filter, combo_chemical(chemical_));
}

void MonitorPage::set_followed(const std::vector<int>& chemicals) {
    // Fix (bug 1): no more than the graph's 32 channels; the original did not
    // check, and wrote past them.
    std::vector<int> kept;
    for (const int chemical : chemicals) {
        if (static_cast<int>(kept.size()) >= kMaxChannels) break;
        if (std::find(kept.begin(), kept.end(), chemical) == kept.end()) kept.push_back(chemical);
    }
    plot_.set_chemicals(kept);
    fill_followed();
    graph_.redraw();
}

void MonitorPage::fill_followed() {
    if (followed_.GetSafeHwnd() == nullptr) return;
    const int selected = followed_.GetNextItem(-1, LVNI_SELECTED);
    followed_.SetRedraw(FALSE);
    followed_.DeleteAllItems();
    const std::vector<c1kitshell::ChemicalGraph::Series>& series = plot_.series();
    for (std::size_t i = 0; i < series.size(); ++i) {
        const int row = followed_.InsertItem(static_cast<int>(i),
                                             chemical_entry(sheet_.chemical_names(), series[i].chemical));
        if (!series[i].values.empty()) {
            CString level;
            level.Format(_T("%d"), series[i].values.back());
            followed_.SetItemText(row, 1, level);
        }
    }
    if (selected >= 0 && selected < followed_.GetItemCount()) {
        followed_.SetItemState(selected, LVIS_SELECTED, LVIS_SELECTED);
    }
    followed_.SetRedraw(TRUE);
}

void MonitorPage::OnAdd() {
    const int chemical = combo_chemical(chemical_);
    if (chemical < 0) return;
    if (static_cast<int>(plot_.series().size()) >= kMaxChannels) {
        AfxMessageBox(_T("The graph can follow 32 chemicals at once."));
        return;
    }
    std::vector<int> chemicals = plot_.chemicals();
    chemicals.push_back(chemical);
    set_followed(chemicals);
}

void MonitorPage::OnRemove() {
    const int row = followed_.GetNextItem(-1, LVNI_SELECTED);
    if (row < 0) return;
    std::vector<int> chemicals = plot_.chemicals();
    chemicals.erase(chemicals.begin() + row);
    set_followed(chemicals);
}

void MonitorPage::OnClear() {
    set_followed({});
}

std::string MonitorPage::saved_path() const {
    return sheet_.kit_file(c1kit::kBiochemSavedFileName);
}

void MonitorPage::fill_saved() {
    CString typed;
    saved_.GetWindowText(typed);
    saved_.ResetContent();
    for (const c1kit::SavedChemicalSet& set : c1kit::parse_saved_sets(read_text(saved_path()))) {
        saved_.AddString(text(set.name));
    }
    saved_.SetWindowText(typed);
}

void MonitorPage::OnLoad() {
    CString name;
    saved_.GetWindowText(name);
    for (const c1kit::SavedChemicalSet& set : c1kit::parse_saved_sets(read_text(saved_path()))) {
        if (c1kit::same_name(set.name, std::string(CStringA(name)))) {
            set_followed(set.chemicals);
            return;
        }
    }
}

// SaveChemicalGroupFromEditControl @ 0x004056f0: the typed name, the chemicals
// followed now.
void MonitorPage::OnSave() {
    CString name;
    saved_.GetWindowText(name);
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

void MonitorPage::OnDelete() {
    CString name;
    saved_.GetWindowText(name);
    if (name.IsEmpty()) return;
    c1kit::SavedChemicalSet set;
    set.name = std::string(CStringA(name));
    write_text(saved_path(), c1kit::update_saved_sets(read_text(saved_path()), set, true));
    saved_.SetWindowText(_T(""));
    fill_saved();
}

// Fix (bug 2): the history keeps going; the original stopped recording after
// its 2048 samples (about 34 minutes).
void MonitorPage::sample() {
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
            followed_.SetItemText(static_cast<int>(i), 1, level);
        }
        graph_.redraw();
    }
}

// ===========================================================================
// Injections
// ===========================================================================
//
// CInjectPage (dialog 151): a chemical (with a filter), the dosage (a
// slider and an amount), Inject, and a repeat: every n seconds, n times (0:
// until stopped).  It sends "inst,chem <n> <amount>,endm".  The syringe
// animation is left out.

BEGIN_MESSAGE_MAP(InjectPage, c1kitshell::LayoutPage)
    ON_EN_CHANGE(kControlFilter, &InjectPage::OnFilterChanged)
    ON_CBN_SELCHANGE(kControlChemical, &InjectPage::OnChemicalChanged)
    ON_BN_CLICKED(kControlInject, &InjectPage::OnInject)
    ON_BN_CLICKED(kControlRepeat, &InjectPage::OnRepeatChanged)
    ON_BN_CLICKED(kControlStop, &InjectPage::OnStop)
    ON_EN_CHANGE(kControlAmount, &InjectPage::OnAmountChanged)
    ON_WM_HSCROLL()
END_MESSAGE_MAP()

InjectPage::InjectPage(BiochemSheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringInjectTab), sheet_(sheet) {}

void InjectPage::create_controls() {
    make(chemical_label_, _T("STATIC"), _T("Chemical:"), SS_LEFT, kControlLabel);
    make(chemical_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, kControlChemical);
    make(filter_label_, _T("STATIC"), _T("Filter:"), SS_LEFT, kControlLabel);
    make(filter_, _T("EDIT"), _T(""), ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, kControlFilter);
    make(dose_label_, _T("STATIC"), _T("Amount:"), SS_LEFT, kControlLabel);
    make(dose_, TRACKBAR_CLASS, _T(""), TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, kControlDose);
    dose_.SetRange(0, 255);
    dose_.SetTicFreq(32);
    dose_.SetPageSize(16);
    make(amount_, _T("EDIT"), _T("0"), ES_NUMBER | WS_BORDER | WS_TABSTOP, kControlAmount);
    make(inject_, _T("BUTTON"), _T("Inject"), BS_DEFPUSHBUTTON | WS_TABSTOP, kControlInject);
    make(repeat_, _T("BUTTON"), _T("Repeat"), BS_AUTOCHECKBOX | WS_TABSTOP, kControlRepeat);
    make(every_label_, _T("STATIC"), _T("Every (seconds):"), SS_LEFT, kControlLabel);
    make(every_, _T("EDIT"), _T("5"), ES_NUMBER | WS_BORDER | WS_TABSTOP, kControlEvery);
    make(count_label_, _T("STATIC"), _T("Times (0 = until stopped):"), SS_LEFT, kControlLabel);
    make(count_, _T("EDIT"), _T("0"), ES_NUMBER | WS_BORDER | WS_TABSTOP, kControlCount);
    make(stop_, _T("BUTTON"), _T("Stop"), BS_PUSHBUTTON | WS_TABSTOP, kControlStop);
    make(status_, _T("STATIC"), _T(""), SS_LEFT, kControlStatus);
    fill_chemical_combo(chemical_, sheet_.chemical_names(), CString(), -1);
    update_repeat_controls();
}

void InjectPage::layout(int width, int height) {
    const int m = kMargin;
    const int row = button_height();
    int y = m;
    place(chemical_label_, m, y + 4, 60, text_height());
    place(chemical_, m + 70, y, (std::min)(320, width - 2 * m - 70), 300);
    y += row + 4;
    place(filter_label_, m, y + 4, 60, text_height());
    place(filter_, m + 70, y, 160, row - 2);
    y += row + 12;
    place(dose_label_, m, y + 4, 60, text_height());
    place(dose_, m + 70, y, (std::min)(320, width - 2 * m - 70 - 60), 30);
    place(amount_, m + 70 + (std::min)(320, width - 2 * m - 70 - 60) + m, y + 4, 44, row - 4);
    y += 36;
    place(inject_, m + 70, y, 90, row);
    y += row + 16;
    place(repeat_, m, y, 120, text_height() + 2);
    y += text_height() + 10;
    place(every_label_, m + 18, y + 4, 140, text_height());
    place(every_, m + 170, y, 44, row - 4);
    y += row + 2;
    place(count_label_, m + 18, y + 4, 150, text_height());
    place(count_, m + 170, y, 44, row - 4);
    y += row + 6;
    place(stop_, m + 18, y, 90, row);
    place(status_, m, height - m - row + 5, width - 3 * m - 84, text_height());
    place_close(width, height);
}

void InjectPage::names_changed() {
    if (!created_) return;
    CString filter;
    filter_.GetWindowText(filter);
    fill_chemical_combo(chemical_, sheet_.chemical_names(), filter, combo_chemical(chemical_));
}

void InjectPage::OnFilterChanged() {
    CString filter;
    filter_.GetWindowText(filter);
    fill_chemical_combo(chemical_, sheet_.chemical_names(), filter, combo_chemical(chemical_));
    OnChemicalChanged();
}

// A repeat injects the chemical it started with; changing the chemical stops
// it (OnChemicalSelectionChanged @ 0x00407cd0).
void InjectPage::OnChemicalChanged() {
    if (repeating_ && combo_chemical(chemical_) != repeat_chemical_) {
        stop_repeat(_T("Repeat stopped (chemical changed)"));
    }
}

void InjectPage::OnHScroll(UINT code, UINT position, CScrollBar* bar) {
    LayoutPage::OnHScroll(code, position, bar);
    syncing_ = true;
    CString value;
    value.Format(_T("%d"), dose_.GetPos());
    amount_.SetWindowText(value);
    syncing_ = false;
}

void InjectPage::OnAmountChanged() {
    if (syncing_) return;
    dose_.SetPos((std::min)(255, (std::max)(0, edit_number(amount_, 0))));
}

bool InjectPage::inject_once() {
    const int chemical = repeating_ ? repeat_chemical_ : combo_chemical(chemical_);
    const int amount = (std::min)(255, (std::max)(0, edit_number(amount_, 0)));
    std::string reply;
    if (chemical < 0 || !sheet_.query(c1kit::injection_script(chemical, amount), reply)) {
        status_.SetWindowText(_T("The game did not take the injection."));
        return false;
    }
    CString done;
    done.Format(_T("Injected %d of %s"), amount,
                text(c1kitshell::chemical_display_name(sheet_.chemical_names(), chemical)).GetString());
    status_.SetWindowText(done);
    return true;
}

// Inject: once, or start the repeat (StartRepeatInjection @ 0x00408610),
// which injects now and then every n seconds.
void InjectPage::OnInject() {
    if (repeat_.GetCheck() != BST_CHECKED) {
        inject_once();
        return;
    }
    if (repeating_) return;
    repeat_chemical_ = combo_chemical(chemical_);
    repeat_total_ = (std::max)(0, edit_number(count_, 0));
    repeat_done_ = 0;
    repeating_ = true;
    update_repeat_controls();
    repeat_tick();
    if (repeating_) {
        kit_sheet_.SetTimer(kTimerRepeat, static_cast<UINT>((std::max)(1, edit_number(every_, 5)) * 1000), nullptr);
    }
}

void InjectPage::repeat_tick() {
    if (!repeating_) return;
    if (!inject_once()) {
        stop_repeat(_T("Repeat injection stopped"));
        return;
    }
    ++repeat_done_;
    CString progress;
    if (repeat_total_ > 0) {
        progress.Format(_T("  (%d of %d)"), repeat_done_, repeat_total_);
    } else {
        progress.Format(_T("  (%d injected)"), repeat_done_);
    }
    CString shown;
    status_.GetWindowText(shown);
    status_.SetWindowText(shown + progress);
    if (repeat_total_ > 0 && repeat_done_ >= repeat_total_) {
        stop_repeat(_T("Repeat injection complete"));
    }
}

void InjectPage::OnStop() {
    stop_repeat(_T("Repeat injection stopped"));
}

void InjectPage::stop_repeat(const CString& why) {
    if (repeating_) {
        kit_sheet_.KillTimer(kTimerRepeat);
        repeating_ = false;
        status_.SetWindowText(why);
    }
    update_repeat_controls();
}

void InjectPage::OnRepeatChanged() {
    if (repeat_.GetCheck() != BST_CHECKED && repeating_) {
        stop_repeat(_T("Repeat injection stopped"));
    }
    update_repeat_controls();
}

void InjectPage::update_repeat_controls() {
    const bool on = repeat_.GetCheck() == BST_CHECKED;
    every_.EnableWindow(on && !repeating_);
    count_.EnableWindow(on && !repeating_);
    stop_.EnableWindow(repeating_);
    inject_.SetWindowText(on ? _T("Start") : _T("Inject"));
}

// ===========================================================================
// Chemical Names
// ===========================================================================
//
// CChemicalsPage (dialog 152): every chemical by number with Search; Rename
// changes one, Save writes allchemicals.str (every kit's names).

BEGIN_MESSAGE_MAP(NamesPage, c1kitshell::LayoutPage)
    ON_EN_CHANGE(kControlSearch, &NamesPage::OnSearchChanged)
    ON_BN_CLICKED(kControlRename, &NamesPage::OnRename)
    ON_BN_CLICKED(kControlSaveNames, &NamesPage::OnSaveNames)
END_MESSAGE_MAP()

NamesPage::NamesPage(BiochemSheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringNamesTab), sheet_(sheet) {}

void NamesPage::create_controls() {
    make(search_label_, _T("STATIC"), _T("Search:"), SS_LEFT, kControlLabel);
    make(search_, _T("EDIT"), _T(""), ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, kControlSearch);
    make(names_, WC_LISTVIEW, _T(""), LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP,
         kControlNames);
    names_.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    names_.InsertColumn(0, _T("#"), LVCFMT_LEFT, 40);
    names_.InsertColumn(1, _T("Chemical Name"), LVCFMT_LEFT, 240);
    make(index_, _T("STATIC"), _T("Index:"), SS_LEFT, kControlLabel);
    make(name_label_, _T("STATIC"), _T("Name:"), SS_LEFT, kControlLabel);
    make(name_, _T("EDIT"), _T(""), ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, kControlName);
    make(rename_, _T("BUTTON"), _T("Rename"), BS_PUSHBUTTON | WS_TABSTOP, kControlRename);
    make(save_, _T("BUTTON"), _T("Save"), BS_PUSHBUTTON | WS_TABSTOP, kControlSaveNames);
    fill();
}

void NamesPage::layout(int width, int height) {
    const int m = kMargin;
    const int row = button_height();
    place(search_label_, m, m + 4, 44, text_height());
    place(search_, m + 48, m, 200, row - 2);
    const int bottom = height - 2 * m - row;
    const int edit_row = bottom - m - row;
    place(names_, m, m + row + 4, width - 2 * m, edit_row - m - (m + row + 4));
    names_.SetColumnWidth(1, width - 2 * m - 40 - GetSystemMetrics(SM_CXVSCROLL) - 4);
    place(index_, m, edit_row + 4, 70, text_height());
    place(name_label_, m + 80, edit_row + 4, 36, text_height());
    place(name_, m + 118, edit_row, (std::max)(120, width - m - 118 - 2 * (70 + m) - m), row - 2);
    place(rename_, width - m - 2 * 70 - m, edit_row, 70, row - 2);
    place(save_, width - m - 70, edit_row, 70, row - 2);
    place_close(width, height);
}

void NamesPage::fill() {
    CString filter;
    search_.GetWindowText(filter);
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
        if (!filter.IsEmpty() && lower.Find(filter) < 0 && number != filter) continue;
        const int row = names_.InsertItem(names_.GetItemCount(), number);
        names_.SetItemText(row, 1, name);
        names_.SetItemData(row, static_cast<DWORD_PTR>(i));
    }
    names_.SetRedraw(TRUE);
}

int NamesPage::selected_chemical() const {
    const int row = names_.GetNextItem(-1, LVNI_SELECTED);
    return row < 0 ? -1 : static_cast<int>(names_.GetItemData(row));
}

void NamesPage::OnSearchChanged() {
    fill();
}

BOOL NamesPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == kControlNames && header->code == LVN_ITEMCHANGED) {
        const int chemical = selected_chemical();
        CString index;
        index.Format(_T("Index: %d"), chemical);
        index_.SetWindowText(chemical >= 0 ? index : CString(_T("Index:")));
        if (chemical >= 0) name_.SetWindowText(text(sheet_.chemical_names()[static_cast<std::size_t>(chemical)]));
    }
    return LayoutPage::OnNotify(wparam, lparam, result);
}

void NamesPage::OnRename() {
    const int chemical = selected_chemical();
    if (chemical < 0) return;
    CString name;
    name_.GetWindowText(name);
    sheet_.chemical_names()[static_cast<std::size_t>(chemical)] = std::string(CStringA(name));
    const int row = names_.GetNextItem(-1, LVNI_SELECTED);
    names_.SetItemText(row, 1, name);
    sheet_.names_changed();
}

void NamesPage::OnSaveNames() {
    if (AfxMessageBox(_T("Save the chemical names to allchemicals.str? Every kit that names ")
                      _T("chemicals will use them."),
                      MB_YESNO | MB_ICONQUESTION) == IDYES) {
        sheet_.save_chemical_names();
    }
}

} // namespace biochem
