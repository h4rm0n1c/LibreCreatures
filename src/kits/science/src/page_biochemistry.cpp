// Biochemistry: chemical levels over time.
//
// The 1996 page (CMonitorPage, dialog 133) plotted four chemicals picked in
// four combo boxes, in a graph a third of the page, with themes of four.
// Here the graph fills the page, any of the named chemicals can be ticked
// in a list beside it (up to kMaxTrackedChemicals), the list shows each
// one's level now, and themes hold as many as are ticked.

#include "science.hpp"
#include "science_ids.hpp"

#include <algorithm>

namespace science {
namespace {

constexpr int kMaxSamples = 2000;  // history kept per chemical
constexpr int kSwatchSize = 12;

// The Save Theme dialog (201).
class ThemeNameDialog : public CDialog {
public:
    explicit ThemeNameDialog(CWnd* parent) : CDialog(kDialogThemeName, parent) {}
    CString name;

protected:
    void DoDataExchange(CDataExchange* exchange) override {
        CDialog::DoDataExchange(exchange);
        DDX_Text(exchange, kControlThemeName, name);
        DDV_MaxChars(exchange, name, 60);
    }
};

} // namespace

BEGIN_MESSAGE_MAP(BiochemistryPage, SciencePage)
    ON_CBN_SELCHANGE(kControlThemeCombo, &BiochemistryPage::OnThemeChanged)
    ON_BN_CLICKED(kControlSaveTheme, &BiochemistryPage::OnSaveTheme)
    ON_BN_CLICKED(kControlDeleteTheme, &BiochemistryPage::OnDeleteTheme)
    ON_BN_CLICKED(kControlClearGraph, &BiochemistryPage::OnClearGraph)
END_MESSAGE_MAP()

BiochemistryPage::BiochemistryPage(ScienceSheet& sheet)
    : SciencePage(sheet, kStringBiochemistryTab) {}

void BiochemistryPage::create_controls() {
    graph_.create(*this, kControlGraph,
                  [this](CDC& dc, const CRect& rect) { draw_graph(dc, rect); });
    make(chemicals_, WC_LISTVIEW, _T(""),
         LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_BORDER | WS_TABSTOP,
         kControlChemicalList);
    chemicals_.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    chemicals_.InsertColumn(0, _T("Chemical"), LVCFMT_LEFT, 150);
    chemicals_.InsertColumn(1, _T("No."), LVCFMT_RIGHT, 36);
    chemicals_.InsertColumn(2, _T("Level"), LVCFMT_RIGHT, 48);
    // Row images: a blank, or the colour its line is drawn in.
    swatches_.Create(kSwatchSize, kSwatchSize, ILC_COLOR24, kMaxTrackedChemicals + 1, 0);
    CClientDC screen(this);
    for (int i = 0; i <= kMaxTrackedChemicals; ++i) {
        CDC dc;
        dc.CreateCompatibleDC(&screen);
        CBitmap bitmap;
        bitmap.CreateCompatibleBitmap(&screen, kSwatchSize, kSwatchSize);
        CBitmap* previous = dc.SelectObject(&bitmap);
        dc.FillSolidRect(0, 0, kSwatchSize, kSwatchSize, GetSysColor(COLOR_WINDOW));
        if (i > 0) {
            dc.FillSolidRect(1, 3, kSwatchSize - 2, kSwatchSize - 6, series_colour(i - 1));
        }
        dc.SelectObject(previous);
        swatches_.Add(&bitmap, static_cast<CBitmap*>(nullptr));
    }
    chemicals_.SetImageList(&swatches_, LVSIL_SMALL);

    make(theme_label_, _T("STATIC"), _T("Theme:"), SS_LEFT, kControlHint);
    make(themes_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
         kControlThemeCombo);
    make(save_theme_, _T("BUTTON"), _T("Save theme..."), BS_PUSHBUTTON | WS_TABSTOP,
         kControlSaveTheme);
    make(delete_theme_, _T("BUTTON"), _T("Delete theme"), BS_PUSHBUTTON | WS_TABSTOP,
         kControlDeleteTheme);
    make(clear_, _T("BUTTON"), _T("Clear graph"), BS_PUSHBUTTON | WS_TABSTOP,
         kControlClearGraph);
    fill_chemical_list();
    fill_themes();

    // The chemicals followed last time (the original remembered a theme).
    std::vector<int> tracked;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        std::uint8_t saved[kMaxTrackedChemicals + 1] = {};
        if (settings->read_binary(c1kit::SettingsScope::user, "Chemicals", saved, sizeof(saved))) {
            for (int i = 0; i < saved[0] && i < kMaxTrackedChemicals; ++i) {
                tracked.push_back(saved[i + 1]);
            }
        } else if (!sheet_.themes().empty()) {
            for (const std::uint8_t chemical : sheet_.themes()[0].chemicals) {
                tracked.push_back(chemical);
            }
        }
    }
    set_tracked(tracked);
}

void BiochemistryPage::layout(int width, int height) {
    const int margin = 7;
    const int row = text_height() + 8;
    const int list_width = (std::min)(320, (std::max)(230, width * 2 / 5));
    const int button_width = 84;
    const int bottom = height - margin - row;
    place(graph_, margin, margin, width - list_width - 3 * margin, bottom - 2 * margin);
    const int list_left = width - list_width - margin;
    place(chemicals_, list_left, margin, list_width, bottom - 2 * margin - 2 * row);
    place(theme_label_, list_left, bottom - margin - 2 * row + 4, 44, text_height());
    place(themes_, list_left + 46, bottom - margin - 2 * row, list_width - 46, 300);
    place(save_theme_, list_left, bottom - margin - row + 2, (list_width - margin) / 2, row - 4);
    place(delete_theme_, list_left + (list_width + margin) / 2, bottom - margin - row + 2,
          (list_width - margin) / 2, row - 4);
    place(clear_, margin, bottom, button_width, row);
    place(close_, width - margin - button_width, bottom, button_width, row);
    chemicals_.SetColumnWidth(0, list_width - 36 - 48 - GetSystemMetrics(SM_CXVSCROLL) - 4);
}

// Every chemical with a name, by number (allchemicals.str).
void BiochemistryPage::fill_chemical_list() {
    updating_list_ = true;
    chemicals_.DeleteAllItems();
    list_chemicals_.clear();
    const std::vector<std::string>& names = sheet_.chemical_names();
    for (int chemical = 1; chemical < static_cast<int>(names.size()); ++chemical) {
        if (!c1kit::chemical_is_named(names[static_cast<std::size_t>(chemical)])) {
            continue;
        }
        const int row = chemicals_.GetItemCount();
        chemicals_.InsertItem(LVIF_TEXT | LVIF_IMAGE, row,
                              CString(names[static_cast<std::size_t>(chemical)].c_str()), 0, 0, 0, 0);
        CString number;
        number.Format(_T("%d"), chemical);
        chemicals_.SetItemText(row, 1, number);
        list_chemicals_.push_back(chemical);
    }
    updating_list_ = false;
}

void BiochemistryPage::fill_themes() {
    themes_.ResetContent();
    for (const c1kit::ChemicalTheme& theme : sheet_.themes()) {
        themes_.AddString(CString(theme.name.c_str()));
    }
}

int BiochemistryPage::series_index(int chemical) const {
    for (std::size_t i = 0; i < series_.size(); ++i) {
        if (series_[i].chemical == chemical) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Follows exactly these chemicals, keeping the history of those already
// followed, and ticks them in the list.
void BiochemistryPage::set_tracked(const std::vector<int>& chemicals) {
    std::vector<Series> kept;
    for (const int chemical : chemicals) {
        if (static_cast<int>(kept.size()) >= kMaxTrackedChemicals) {
            break;
        }
        const int existing = series_index(chemical);
        Series series;
        series.chemical = chemical;
        if (existing >= 0) {
            series.values = std::move(series_[static_cast<std::size_t>(existing)].values);
        }
        kept.push_back(std::move(series));
    }
    series_ = std::move(kept);
    updating_list_ = true;
    for (int row = 0; row < chemicals_.GetItemCount(); ++row) {
        const int index = series_index(list_chemicals_[static_cast<std::size_t>(row)]);
        chemicals_.SetCheck(row, index >= 0);
        chemicals_.SetItem(row, 0, LVIF_IMAGE, nullptr, index >= 0 ? index + 1 : 0, 0, 0, 0);
    }
    updating_list_ = false;
    save_selection();
    graph_.redraw();
}

void BiochemistryPage::tracked_from_list() {
    std::vector<int> chemicals;
    // Keep the order they were ticked in, so colours stay put.
    for (const Series& series : series_) {
        for (int row = 0; row < chemicals_.GetItemCount(); ++row) {
            if (list_chemicals_[static_cast<std::size_t>(row)] == series.chemical &&
                chemicals_.GetCheck(row)) {
                chemicals.push_back(series.chemical);
            }
        }
    }
    for (int row = 0; row < chemicals_.GetItemCount(); ++row) {
        const int chemical = list_chemicals_[static_cast<std::size_t>(row)];
        if (chemicals_.GetCheck(row) &&
            std::find(chemicals.begin(), chemicals.end(), chemical) == chemicals.end()) {
            if (static_cast<int>(chemicals.size()) >= kMaxTrackedChemicals) {
                AfxMessageBox(_T("The graph can follow 16 chemicals at once."));
                break;
            }
            chemicals.push_back(chemical);
        }
    }
    set_tracked(chemicals);
    themes_.SetCurSel(-1);
}

BOOL BiochemistryPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == kControlChemicalList && header->code == LVN_ITEMCHANGED &&
        !updating_list_) {
        const auto* change = reinterpret_cast<const NMLISTVIEW*>(lparam);
        if ((change->uChanged & LVIF_STATE) != 0 &&
            ((change->uNewState ^ change->uOldState) & LVIS_STATEIMAGEMASK) != 0) {
            tracked_from_list();
        }
    }
    return SciencePage::OnNotify(wparam, lparam, result);
}

void BiochemistryPage::save_selection() {
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        std::uint8_t saved[kMaxTrackedChemicals + 1] = {};
        saved[0] = static_cast<std::uint8_t>(series_.size());
        for (std::size_t i = 0; i < series_.size(); ++i) {
            saved[i + 1] = static_cast<std::uint8_t>(series_[i].chemical);
        }
        settings->write_binary("Chemicals", saved, sizeof(saved));
    }
}

void BiochemistryPage::OnThemeChanged() {
    const int index = themes_.GetCurSel();
    if (index < 0 || index >= static_cast<int>(sheet_.themes().size())) {
        return;
    }
    std::vector<int> chemicals;
    for (const std::uint8_t chemical : sheet_.themes()[static_cast<std::size_t>(index)].chemicals) {
        chemicals.push_back(chemical);
    }
    set_tracked(chemicals);
}

// Saves what is ticked, under a new name or over a theme of the same name.
void BiochemistryPage::OnSaveTheme() {
    if (series_.empty()) {
        AfxMessageBox(_T("Tick the chemicals to put in the theme first."));
        return;
    }
    ThemeNameDialog dialog(this);
    const int current = themes_.GetCurSel();
    if (current >= 0) {
        themes_.GetLBText(current, dialog.name);
    }
    if (dialog.DoModal() != IDOK || dialog.name.Trim().IsEmpty()) {
        return;
    }
    c1kit::ChemicalTheme theme;
    theme.name = std::string(CStringA(dialog.name));
    for (const Series& series : series_) {
        theme.chemicals.push_back(static_cast<std::uint8_t>(series.chemical));
    }
    std::vector<c1kit::ChemicalTheme>& themes = sheet_.themes();
    int index = -1;
    for (std::size_t i = 0; i < themes.size(); ++i) {
        if (themes[i].name == theme.name) {
            themes[i] = theme;
            index = static_cast<int>(i);
        }
    }
    if (index < 0) {
        themes.push_back(theme);
        index = static_cast<int>(themes.size()) - 1;
    }
    sheet_.save_themes();
    fill_themes();
    themes_.SetCurSel(index);
}

// "Remove the <name> theme ?" (strings 105 and 106).
void BiochemistryPage::OnDeleteTheme() {
    const int index = themes_.GetCurSel();
    if (index < 0) {
        return;
    }
    CString name;
    themes_.GetLBText(index, name);
    if (AfxMessageBox(_T("Remove the ") + name + _T(" theme?"), MB_YESNO | MB_ICONQUESTION) !=
        IDYES) {
        return;
    }
    sheet_.themes().erase(sheet_.themes().begin() + index);
    sheet_.save_themes();
    fill_themes();
}

void BiochemistryPage::OnClearGraph() {
    for (Series& series : series_) {
        series.values.clear();
    }
    graph_.redraw();
}

void BiochemistryPage::subject_changed() {
    OnClearGraph();
}

// Every chemical followed, in one query.
void BiochemistryPage::sample() {
    if (series_.empty()) {
        return;
    }
    std::vector<int> chemicals;
    for (const Series& series : series_) {
        chemicals.push_back(series.chemical);
    }
    std::string reply;
    std::vector<int> values;
    if (!sheet_.query(c1kit::chemical_levels_query(chemicals), reply) ||
        !c1kit::parse_values(reply, chemicals.size(), values)) {
        return;
    }
    for (std::size_t i = 0; i < series_.size(); ++i) {
        series_[i].values.push_back(values[i]);
        if (static_cast<int>(series_[i].values.size()) > kMaxSamples) {
            series_[i].values.pop_front();
        }
    }
    if (GetSafeHwnd() != nullptr && IsWindowVisible()) {
        refresh_levels_column();
        graph_.redraw();
    }
}

void BiochemistryPage::refresh_levels_column() {
    for (int row = 0; row < chemicals_.GetItemCount(); ++row) {
        const int index = series_index(list_chemicals_[static_cast<std::size_t>(row)]);
        CString level;
        if (index >= 0 && !series_[static_cast<std::size_t>(index)].values.empty()) {
            level.Format(_T("%d"), series_[static_cast<std::size_t>(index)].values.back());
        }
        CString shown = chemicals_.GetItemText(row, 2);
        if (shown != level) {
            chemicals_.SetItemText(row, 2, level);
        }
    }
}

// Levels 0-255 up the side, newest sample at the right, a sample every
// half second (two pixels each).
void BiochemistryPage::draw_graph(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, RGB(255, 255, 255));
    const int left = rect.left + 34;
    const int top = rect.top + 8;
    const int right = rect.right - 8;
    const int bottom = rect.bottom - 22;
    if (right <= left || bottom <= top) {
        return;
    }
    dc.SetBkMode(TRANSPARENT);
    CPen grid(PS_SOLID, 1, RGB(225, 225, 225));
    CPen axis(PS_SOLID, 1, RGB(90, 90, 90));
    CPen* previous = dc.SelectObject(&grid);
    dc.SetTextColor(RGB(90, 90, 90));
    for (int level = 0; level <= 256; level += 32) {
        const int y = bottom - (bottom - top) * (level > 255 ? 255 : level) / 255;
        dc.MoveTo(left, y);
        dc.LineTo(right, y);
        CString label;
        label.Format(_T("%d"), level > 255 ? 255 : level);
        dc.TextOut(rect.left + 2, y - 7, label);
    }
    dc.SelectObject(&axis);
    dc.MoveTo(left, top);
    dc.LineTo(left, bottom);
    dc.LineTo(right, bottom);
    const CString time = c1kitshell::load_string(kStringTime);
    dc.TextOut((left + right) / 2 - 12, bottom + 4, time);
    dc.TextOut(right - 22, bottom + 4, _T("now"));

    const int step = 2;
    for (std::size_t s = 0; s < series_.size(); ++s) {
        const std::deque<int>& values = series_[s].values;
        if (values.empty()) {
            continue;
        }
        CPen line(PS_SOLID, 2, series_colour(static_cast<int>(s)));
        dc.SelectObject(&line);
        const int visible = (std::min)(static_cast<int>(values.size()), (right - left) / step + 1);
        for (int i = 0; i < visible; ++i) {
            const int value = values[values.size() - 1 - static_cast<std::size_t>(i)];
            const int x = right - i * step;
            const int y = bottom - (bottom - top) * (std::min)(255, (std::max)(0, value)) / 255;
            if (i == 0) {
                dc.MoveTo(x, y);
            } else {
                dc.LineTo(x, y);
            }
        }
        dc.SelectObject(&axis);
    }
    // Legend, top left inside the plot.
    int legend_y = top + 2;
    for (std::size_t s = 0; s < series_.size(); ++s) {
        dc.FillSolidRect(left + 6, legend_y + 5, 12, 4, series_colour(static_cast<int>(s)));
        CString label(c1kit::chemical_label(sheet_.chemical_names(), series_[s].chemical).c_str());
        if (!series_[s].values.empty()) {
            CString value;
            value.Format(_T("  %d"), series_[s].values.back());
            label += value;
        }
        dc.SetTextColor(RGB(40, 40, 40));
        dc.TextOut(left + 22, legend_y, label);
        legend_y += 14;
    }
    if (!sheet_.subject().present) {
        dc.SetTextColor(RGB(120, 120, 120));
        dc.TextOut(left + 12, (top + bottom) / 2, _T("Select a creature in the game to follow its chemistry."));
    } else if (series_.empty()) {
        dc.SetTextColor(RGB(120, 120, 120));
        dc.TextOut(left + 12, (top + bottom) / 2, _T("Tick chemicals in the list, or pick a theme."));
    }
    dc.SelectObject(previous);
}

} // namespace science
