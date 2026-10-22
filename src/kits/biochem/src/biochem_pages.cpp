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
// until stopped).  It sends "inst,chem <n> <amount>,endm".  The 1996 page's
// syringe is here too, from the game's DOSE.bmp and dosage sprites as the
// 1996 page read them (the liquid is the chemical's number mod 7), and
// plays its injection; beside it, the chemical's level in the creature now
// and after the dose.

BEGIN_MESSAGE_MAP(InjectPage, c1kitshell::LayoutPage)
    ON_EN_CHANGE(kControlFilter, &InjectPage::OnFilterChanged)
    ON_LBN_SELCHANGE(kControlChemicalList, &InjectPage::OnChemicalChanged)
    ON_BN_CLICKED(kControlInject, &InjectPage::OnInject)
    ON_BN_CLICKED(kControlRepeat, &InjectPage::OnRepeatChanged)
    ON_BN_CLICKED(kControlStop, &InjectPage::OnStop)
    ON_EN_CHANGE(kControlAmount, &InjectPage::OnAmountChanged)
    ON_WM_HSCROLL()
    ON_WM_TIMER()
END_MESSAGE_MAP()

InjectPage::InjectPage(BiochemSheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringInjectTab), sheet_(sheet) {}

void InjectPage::create_controls() {
    make(filter_label_, _T("STATIC"), _T("Find:"), SS_LEFT, kControlLabel);
    make(filter_, _T("EDIT"), _T(""), ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, kControlFilter);
    make(chemicals_, _T("LISTBOX"), _T(""), LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_BORDER | WS_VSCROLL | WS_TABSTOP,
         kControlChemicalList);
    const CString palettes = c1kitshell::game_directory_setting("Palette Directory");
    palette_.load(std::string(CStringA(palettes.IsEmpty() ? CString(_T("Palettes\\")) : palettes)) + "palette.dta");
    syringe_.load(std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))), kDosageFiles[0],
                  palette_);
    liquid_ = 0;
    syringe_view_.create(*this, kControlSyringeView, [this](CDC& dc, const CRect& rect) {
        CRect card = rect;
        card.DeflateRect(1, 1);
        CBrush fill(RGB(255, 255, 255));
        CPen edge(PS_SOLID, 1, RGB(208, 204, 196));
        CBrush* old_brush = dc.SelectObject(&fill);
        CPen* old_pen = dc.SelectObject(&edge);
        dc.RoundRect(card, CPoint(10, 10));
        dc.SelectObject(old_pen);
        dc.SelectObject(old_brush);
        if (syringe_.loaded()) {
            syringe_.present(dc, rect);
        } else {
            CRect note = card;
            note.DeflateRect(10, 10);
            dc.SetBkMode(TRANSPARENT);
            dc.SetTextColor(RGB(110, 104, 96));
            CFont* old_font = dc.SelectObject(GetFont());
            dc.DrawText(_T("The syringe needs DOSE.bmp and Dosage.spr in the game's folder."), &note,
                        DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
            dc.SelectObject(old_font);
        }
    });
    make(dose_label_, _T("STATIC"), _T("Dose:"), SS_LEFT, kControlLabel);
    make(dose_, TRACKBAR_CLASS, _T(""), TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, kControlDose);
    dose_.SetRange(0, 255);
    dose_.SetTicFreq(32);
    dose_.SetPageSize(16);
    make(amount_, _T("EDIT"), _T("0"), ES_NUMBER | WS_BORDER | WS_TABSTOP, kControlAmount);
    make(inject_, _T("BUTTON"), _T("Inject"), BS_DEFPUSHBUTTON | WS_TABSTOP, kControlInject);
    make(stop_, _T("BUTTON"), _T("Stop"), BS_PUSHBUTTON | WS_TABSTOP, kControlStop);
    reading_.create(*this, kControlReading, [this](CDC& dc, const CRect& rect) { draw_reading(dc, rect); });
    make(repeat_, _T("BUTTON"), _T("Repeat"), BS_AUTOCHECKBOX | WS_TABSTOP, kControlRepeat);
    make(every_label_, _T("STATIC"), _T("Every (seconds):"), SS_LEFT, kControlLabel);
    make(every_, _T("EDIT"), _T("5"), ES_NUMBER | WS_BORDER | WS_TABSTOP, kControlEvery);
    make(count_label_, _T("STATIC"), _T("Times (0 = no limit):"), SS_LEFT, kControlLabel);
    make(count_, _T("EDIT"), _T("0"), ES_NUMBER | WS_BORDER | WS_TABSTOP, kControlCount);
    make(status_, _T("STATIC"), _T(""), SS_LEFT | SS_ENDELLIPSIS, kControlStatus);
    fill_chemicals(-1);
    change_liquid();
    update_repeat_controls();
}

// The chemicals on the left; the syringe beside them; the dose, Inject and
// Stop, the reading and the repeat on the right; the status and Close along
// the bottom.
void InjectPage::layout(int width, int height) {
    const int m = kMargin;
    const int row = button_height();
    const int bottom = height - m - row - m;
    const CSize picture_size = c1kitshell::Syringe::picture_size();
    const int picture_width = static_cast<int>(picture_size.cx);
    const int picture_height = static_cast<int>(picture_size.cy);
    const int list_width = (std::max)(150, (std::min)(220, width / 3));
    place(filter_label_, m, m + 4, 32, text_height());
    place(filter_, m + 36, m, list_width - 36, row - 2);
    place(chemicals_, m, m + row + 4, list_width, (std::max)(60, bottom - (m + row + 4)));
    const int syringe_left = 2 * m + list_width;
    const int syringe_width = picture_width + 24;
    place(syringe_view_, syringe_left, m, syringe_width, (std::max)(60, (std::min)(picture_height + 24, bottom - m)));
    const int right = syringe_left + syringe_width + m;
    const int right_width = (std::max)(170, width - right - m);
    int y = m;
    place(dose_label_, right, y + 4, 40, text_height());
    place(amount_, right + right_width - 48, y + 2, 48, row - 4);
    y += row;
    place(dose_, right, y, right_width, 28);
    y += 32;
    place(inject_, right, y, 90, row);
    place(stop_, right + 98, y, 70, row);
    y += row + m;
    const int reading_height = 2 * text_height() + 52;
    place(reading_, right, y, right_width, reading_height);
    y += reading_height + m + 4;
    place(repeat_, right, y, right_width, text_height() + 2);
    y += text_height() + 8;
    place(every_label_, right + 8, y + 4, right_width - 8 - 46, text_height());
    place(every_, right + right_width - 42, y, 42, row - 4);
    y += row;
    place(count_label_, right + 8, y + 4, right_width - 8 - 46, text_height());
    place(count_, right + right_width - 42, y, 42, row - 4);
    place(status_, m, height - m - row + 4, (std::max)(1, width - 3 * m - 84), text_height());
    place_close(width, height);
}

BOOL InjectPage::OnSetActive() {
    const BOOL shown = LayoutPage::OnSetActive();
    read_level();
    return shown;
}

int InjectPage::selected_chemical() const {
    const int row = chemicals_.GetCurSel();
    return row < 0 ? -1 : static_cast<int>(chemicals_.GetItemData(row));
}

void InjectPage::fill_chemicals(int keep_chemical) {
    CString filter;
    filter_.GetWindowText(filter);
    filter.MakeLower();
    chemicals_.SetRedraw(FALSE);
    chemicals_.ResetContent();
    int keep_row = -1;
    for (int chemical = 1; chemical < 256; ++chemical) {
        const CString entry = chemical_entry(sheet_.chemical_names(), chemical);
        CString lower = entry;
        lower.MakeLower();
        if (!filter.IsEmpty() && lower.Find(filter) < 0) continue;
        const int row = chemicals_.AddString(entry);
        chemicals_.SetItemData(row, static_cast<DWORD_PTR>(chemical));
        if (chemical == keep_chemical) keep_row = row;
    }
    chemicals_.SetCurSel(keep_row >= 0 ? keep_row : (chemicals_.GetCount() > 0 ? 0 : -1));
    chemicals_.SetRedraw(TRUE);
    chemicals_.Invalidate();
}

void InjectPage::names_changed() {
    if (!created_) return;
    fill_chemicals(selected_chemical());
    reading_.redraw();
}

void InjectPage::OnFilterChanged() {
    fill_chemicals(selected_chemical());
    OnChemicalChanged();
}

// A repeat injects the chemical it started with; changing the chemical stops
// it (OnChemicalSelectionChanged @ 0x00407cd0).
void InjectPage::OnChemicalChanged() {
    if (repeating_ && selected_chemical() != repeat_chemical_) {
        stop_repeat(_T("Repeat stopped (chemical changed)"));
    }
    change_liquid();
    level_now_ = -1;
    sample_level();
    reading_.redraw();
}

// The liquid is the chemical's (number mod 7, the 1996 kit's path table at
// 0x00421020), as on the classic page.
void InjectPage::change_liquid() {
    const int chemical = selected_chemical();
    const int liquid = chemical < 0 ? 0 : chemical % 7;
    if (liquid != liquid_ && syringe_.set_liquid(kDosageFiles[liquid])) {
        liquid_ = liquid;
        syringe_view_.redraw();
    }
}

void InjectPage::set_dose(int dose) {
    if (!syringe_.injecting()) {
        syringe_.set_dose(dose);
        syringe_view_.redraw();
    }
    reading_.redraw();
}

void InjectPage::OnHScroll(UINT code, UINT position, CScrollBar* bar) {
    LayoutPage::OnHScroll(code, position, bar);
    if (syringe_.injecting()) return;
    syncing_ = true;
    CString value;
    value.Format(_T("%d"), dose_.GetPos());
    amount_.SetWindowText(value);
    syncing_ = false;
    set_dose(dose_.GetPos());
}

void InjectPage::OnAmountChanged() {
    if (syncing_) return;
    const int dose = (std::min)(255, (std::max)(0, edit_number(amount_, 0)));
    if (!syringe_.injecting()) dose_.SetPos(dose);
    set_dose(dose);
}

void InjectPage::sample_level() {
    if (created_ && IsWindowVisible()) read_level();
}

void InjectPage::read_level() {
    if (!created_) return;
    const int chemical = selected_chemical();
    int level = -1;
    std::string reply;
    std::vector<int> values;
    if (chemical > 0 && sheet_.subject_present() &&
        sheet_.query(c1kit::chemical_levels_query({chemical}), reply) &&
        c1kit::parse_values(reply, 1, values)) {
        level = values[0];
    }
    if (level != level_now_) {
        level_now_ = level;
        reading_.redraw();
    }
}

void InjectPage::draw_reading(CDC& dc, const CRect& rect) {
    const int chemical = selected_chemical();
    const CString name = chemical < 0 ? CString(_T("No chemical chosen"))
                                       : chemical_entry(sheet_.chemical_names(), chemical);
    const int dose = (std::min)(255, (std::max)(0, edit_number(amount_, 0)));
    c1kitshell::draw_dose_reading(dc, rect, GetFont(), name, level_now_, dose, syringe_.liquid_colour());
}

bool InjectPage::inject_once() {
    const int chemical = repeating_ ? repeat_chemical_ : selected_chemical();
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
    // The syringe squeezes out a drop and drains (OnDosagePreviewTimer
    // @ 0x00408970), then fills to the dose again.
    if (syringe_.loaded() && !syringe_.injecting()) {
        syringe_.start_injection();
        SetTimer(kTimerSyringe, c1kitshell::Syringe::kTickMs, nullptr);
    }
    sample_level();
    return true;
}

// The syringe's animation; the slider follows the barrel as it drains.
void InjectPage::OnTimer(UINT_PTR timer_id) {
    if (timer_id != kTimerSyringe) {
        LayoutPage::OnTimer(timer_id);
        return;
    }
    const bool more = syringe_.tick();
    dose_.SetPos(more ? syringe_.shown_dose() : syringe_.dose());
    syringe_view_.redraw();
    if (!more) {
        KillTimer(kTimerSyringe);
        sample_level();
    }
}

// Inject: once, or start the repeat (StartRepeatInjection @ 0x00408610),
// which injects now and then every n seconds.
void InjectPage::OnInject() {
    if (repeat_.GetCheck() != BST_CHECKED) {
        inject_once();
        return;
    }
    if (repeating_) return;
    repeat_chemical_ = selected_chemical();
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
    every_label_.EnableWindow(on);
    count_label_.EnableWindow(on);
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
