// Brain: a live map of the creature's brain.
//
// The 1996 page (CScannerPage, dialog 134) painted the brain report's active
// cells as dots over a picture of a human brain (a 64 x 48 lattice, four
// pixels a cell), with nothing to say which lobe a dot was in or what it
// meant.
//
// Here the brain is the grid the game lays it out on: each lobe (from
// `dde: lobe`) is outlined and named in its own colour, each neuron is a
// square shaded by the brain report, the report can measure any of its five
// quantities, pointing at a neuron says which lobe and neuron it is and what
// it stands for, and clicking one follows it with its exact values
// (`dde: cell`) for both of its dendrite rules.

#include "science.hpp"
#include "science_ids.hpp"

#include <algorithm>

namespace science {
namespace {

const TCHAR* const kModeNames[] = {
    _T("Firing strength"),
    _T("Activation"),
    _T("Strongest dendrite weight"),
    _T("Average target weight"),
    _T("Average dendrite state"),
};

COLORREF blend(COLORREF a, COLORREF b, int b_percent) {
    const auto mix = [&](int x, int y) { return (x * (100 - b_percent) + y * b_percent) / 100; };
    return RGB(mix(GetRValue(a), GetRValue(b)), mix(GetGValue(a), GetGValue(b)),
               mix(GetBValue(a), GetBValue(b)));
}

CString neuron_title(const c1kit::NeuronNames& names, int lobe, int neuron) {
    CString title;
    title.Format(_T("%s lobe, neuron %d"), CString(c1kit::lobe_name(lobe)).GetString(), neuron);
    const std::string meaning = c1kit::neuron_meaning(lobe, neuron, names);
    if (!meaning.empty()) {
        title += _T(": ") + CString(meaning.c_str());
    }
    return title;
}

} // namespace

BEGIN_MESSAGE_MAP(BrainPage, SciencePage)
    ON_CBN_SELCHANGE(kControlReportMode, &BrainPage::OnModeChanged)
    ON_CBN_SELCHANGE(kControlReportRule, &BrainPage::OnModeChanged)
END_MESSAGE_MAP()

BrainPage::BrainPage(ScienceSheet& sheet) : SciencePage(sheet, kStringBrainTab) {}

void BrainPage::create_controls() {
    grid_.create(*this, kControlBrainGrid,
                 [this](CDC& dc, const CRect& rect) { draw_grid(dc, rect); });
    grid_.set_mouse_handler([this](CPoint point, bool clicked) { on_mouse(point, clicked); });
    make(mode_label_, _T("STATIC"), _T("Show:"), SS_LEFT, kControlHint);
    make(mode_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
         kControlReportMode);
    for (const TCHAR* name : kModeNames) {
        mode_.AddString(name);
    }
    make(rule_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
         kControlReportRule);
    rule_.AddString(_T("Dendrite rule 0"));
    rule_.AddString(_T("Dendrite rule 1"));
    std::uint32_t mode = c1kit::kReportActivation;
    std::uint32_t rule = 0;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->read_dword(c1kit::SettingsScope::user, "Scanner", mode);
        settings->read_dword(c1kit::SettingsScope::user, "Scanner Rule", rule);
    }
    mode_.SetCurSel(mode <= 4 ? static_cast<int>(mode) : c1kit::kReportActivation);
    rule_.SetCurSel(rule <= 1 ? static_cast<int>(rule) : 0);
    rule_.EnableWindow(mode_.GetCurSel() >= c1kit::kReportStrongestWeight);

    make(lobes_, WC_LISTVIEW, _T(""),
         LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL | LVS_NOCOLUMNHEADER | WS_BORDER,
         kControlLobeList);
    lobes_.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    lobes_.InsertColumn(0, _T("Lobe"), LVCFMT_LEFT, 110);
    lobes_.InsertColumn(1, _T("Neurons"), LVCFMT_RIGHT, 50);
    make(info_, _T("EDIT"), _T(""),
         ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_BORDER, kControlNeuronInfo);
    fill_lobe_list();
    show_neuron_info();
}

void BrainPage::layout(int width, int height) {
    const int margin = 7;
    const int row = text_height() + 8;
    const int side = (std::min)(230, (std::max)(180, width / 3));
    const int bottom = height - margin - row;
    const int grid_width = width - side - 3 * margin;
    place(grid_, margin, margin, grid_width, bottom - 2 * margin);
    const int left = width - side - margin;
    place(mode_label_, left, margin + 4, 36, text_height());
    place(mode_, left + 38, margin, side - 38, 200);
    place(rule_, left + 38, margin + row, side - 38, 200);
    const int list_top = margin + 2 * row + 4;
    const int list_height = (std::min)(9 * (text_height() + 4) + 8, (bottom - list_top) / 2);
    place(lobes_, left, list_top, side, list_height);
    lobes_.SetColumnWidth(0, side - 50 - 6);
    place(info_, left, list_top + list_height + margin, side,
          bottom - 2 * margin - (list_top + list_height));
    place(close_, width - margin - 84, bottom, 84, row);
}

void BrainPage::fill_lobe_list() {
    lobes_.DeleteAllItems();
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    for (std::size_t i = 0; i < lobes.size(); ++i) {
        const int row = lobes_.InsertItem(static_cast<int>(i),
                                          CString(c1kit::lobe_name(static_cast<int>(i))));
        CString count;
        count.Format(_T("%d"), lobes[i].neurons());
        lobes_.SetItemText(row, 1, count);
    }
}

void BrainPage::subject_changed() {
    activity_ = c1kit::BrainActivity();
    followed_lobe_ = followed_neuron_ = -1;
    hover_lobe_ = hover_neuron_ = -1;
    followed_valid_ = false;
    if (created_) {
        fill_lobe_list();
        show_neuron_info();
        grid_.redraw();
    }
}

void BrainPage::OnModeChanged() {
    rule_.EnableWindow(mode_.GetCurSel() >= c1kit::kReportStrongestWeight);
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Scanner", static_cast<std::uint32_t>(mode_.GetCurSel()));
        settings->write_dword("Scanner Rule", static_cast<std::uint32_t>(rule_.GetCurSel()));
    }
    poll();
}

void BrainPage::poll() {
    std::string reply;
    if (sheet_.brain_report(mode_.GetCurSel(), rule_.GetCurSel(), reply)) {
        c1kit::parse_activity_report(reply, activity_);
    }
    if (followed_lobe_ >= 0 &&
        sheet_.query(c1kit::neuron_query(followed_lobe_, followed_neuron_), reply)) {
        followed_valid_ = c1kit::parse_neuron_values(reply, followed_values_);
    }
    show_neuron_info();
    grid_.redraw();
}

// Zooms to the lobes: the smallest part of the grid holding them all, with a
// cell to spare, as large as the view allows.
void BrainPage::fit_view(const CRect& rect) {
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    int left = c1kit::kBrainGridSize, top = c1kit::kBrainGridSize, right = 0, bottom = 0;
    for (const c1kit::LobeLayout& lobe : lobes) {
        left = (std::min)(left, lobe.x);
        top = (std::min)(top, lobe.y);
        right = (std::max)(right, lobe.x + lobe.width);
        bottom = (std::max)(bottom, lobe.y + lobe.height);
    }
    if (lobes.empty() || right <= left || bottom <= top) {
        left = top = 0;
        right = bottom = c1kit::kBrainGridSize;
    }
    // Room for the lobe names above and below.
    view_x_ = (std::max)(0, left - 1);
    view_y_ = (std::max)(0, top - 2);
    view_width_ = (std::min)(c1kit::kBrainGridSize, right + 1) - view_x_;
    view_height_ = (std::min)(c1kit::kBrainGridSize, bottom + 2) - view_y_;
    cell_ = (std::max)(1, (std::min)((rect.Width() - 4) / view_width_,
                                     (rect.Height() - 4) / view_height_));
    view_origin_ = CPoint(rect.left + (rect.Width() - cell_ * view_width_) / 2,
                          rect.top + (rect.Height() - cell_ * view_height_) / 2);
}

bool BrainPage::cell_at(CPoint point, int& x, int& y) const {
    if (cell_ <= 0 || point.x < view_origin_.x || point.y < view_origin_.y) {
        return false;
    }
    x = view_x_ + (point.x - view_origin_.x) / cell_;
    y = view_y_ + (point.y - view_origin_.y) / cell_;
    return x < view_x_ + view_width_ && y < view_y_ + view_height_;
}

void BrainPage::on_mouse(CPoint point, bool clicked) {
    int x = 0, y = 0, lobe = -1, neuron = -1;
    if (cell_at(point, x, y)) {
        c1kit::neuron_at(sheet_.lobes(), x, y, lobe, neuron);
    }
    if (clicked) {
        followed_lobe_ = lobe;
        followed_neuron_ = neuron;
        followed_valid_ = false;
        poll();
        return;
    }
    if (lobe != hover_lobe_ || neuron != hover_neuron_) {
        hover_lobe_ = lobe;
        hover_neuron_ = neuron;
        show_neuron_info();
        grid_.redraw();
    }
}

// What is being pointed at, and the neuron being followed.
void BrainPage::show_neuron_info() {
    if (info_.GetSafeHwnd() == nullptr) {
        return;
    }
    const c1kit::NeuronNames& names = sheet_.neuron_names();
    CString text;
    if (!sheet_.subject().present) {
        text = _T("Select a creature in the game to see its brain.");
    } else if (hover_lobe_ >= 0) {
        text = neuron_title(names, hover_lobe_, hover_neuron_) + _T("\r\n") +
               CString(c1kit::lobe_description(hover_lobe_)) + _T("\r\n\r\n");
    } else {
        text = _T("Each square is a neuron, brighter the stronger it is. Point at one ")
               _T("to see what it is; click it to follow it.\r\n\r\n");
    }
    if (followed_lobe_ >= 0) {
        text += _T("Following ") + neuron_title(names, followed_lobe_, followed_neuron_) + _T("\r\n");
        if (followed_valid_) {
            CString values;
            values.Format(_T("Activation %d, firing strength %d\r\n"),
                          followed_values_[0].activation, followed_values_[0].firing_strength);
            text += values;
            for (int rule = 0; rule < 2; ++rule) {
                const c1kit::NeuronValues& v = followed_values_[rule];
                if (v.dendrites == 0) {
                    values.Format(_T("Rule %d: no dendrites\r\n"), rule);
                } else {
                    values.Format(_T("Rule %d: %d dendrites; average weight %d (target %d, ")
                                  _T("baseline %d), state %d\r\n"),
                                  rule, v.dendrites, v.current_weight_sum / v.dendrites,
                                  v.target_weight_sum / v.dendrites,
                                  v.baseline_weight_sum / v.dendrites,
                                  v.dendrite_state_sum / v.dendrites);
                }
                text += values;
            }
        }
    }
    CString shown;
    info_.GetWindowText(shown);
    if (shown != text) {
        info_.SetWindowText(text);
    }
}

void BrainPage::draw_grid(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, RGB(18, 20, 28));
    fit_view(rect);
    const int cell = cell_;
    // Grid cell (x, y)'s top left.
    const auto cell_x = [this](int x) { return view_origin_.x + (x - view_x_) * cell_; };
    const auto cell_y = [this](int y) { return view_origin_.y + (y - view_y_) * cell_; };
    const CRect area(cell_x(view_x_), cell_y(view_y_), cell_x(view_x_ + view_width_),
                     cell_y(view_y_ + view_height_));
    // Faint lines every cell, stronger every eight.
    for (int x = view_x_; x <= view_x_ + view_width_; ++x) {
        dc.FillSolidRect(cell_x(x), area.top, 1, area.Height(),
                         x % 8 == 0 ? RGB(55, 60, 75) : RGB(32, 35, 46));
    }
    for (int y = view_y_; y <= view_y_ + view_height_; ++y) {
        dc.FillSolidRect(area.left, cell_y(y), area.Width(), 1,
                         y % 8 == 0 ? RGB(55, 60, 75) : RGB(32, 35, 46));
    }
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    dc.SetBkMode(TRANSPARENT);
    for (std::size_t index = 0; index < lobes.size(); ++index) {
        const c1kit::LobeLayout& lobe = lobes[index];
        const COLORREF colour = lobe_colour(static_cast<int>(index));
        const COLORREF dim = blend(RGB(18, 20, 28), colour, 22);
        for (int y = lobe.y; y < lobe.y + lobe.height && y < c1kit::kBrainGridSize; ++y) {
            for (int x = lobe.x; x < lobe.x + lobe.width && x < c1kit::kBrainGridSize; ++x) {
                const int level = activity_.level[x][y];
                const COLORREF shade =
                    level == 0 ? dim : blend(blend(colour, RGB(255, 255, 255), 30), dim,
                                             100 - (level * 100 / 15));
                const int inset = cell > 3 ? 1 : 0;
                dc.FillSolidRect(cell_x(x) + inset, cell_y(y) + inset, cell - inset,
                                 cell - inset, shade);
            }
        }
        const CRect outline(cell_x(lobe.x), cell_y(lobe.y), cell_x(lobe.x + lobe.width) + 1,
                            cell_y(lobe.y + lobe.height) + 1);
        CBrush border(colour);
        dc.FrameRect(outline, &border);
        const bool highlighted = static_cast<int>(index) == hover_lobe_ ||
                                 static_cast<int>(index) == followed_lobe_;
        if (highlighted) {
            CRect thick = outline;
            thick.InflateRect(1, 1);
            dc.FrameRect(thick, &border);
        }
        // The lobe's name above it (below, if it is at the top edge).
        const CString name(c1kit::lobe_name(static_cast<int>(index)));
        const CSize extent = dc.GetTextExtent(name);
        int label_y = outline.top - extent.cy - 1;
        if (label_y < area.top) {
            label_y = outline.bottom + 1;
        }
        dc.SetTextColor(colour);
        const int label_x = (std::max)(static_cast<int>(rect.left) + 2,
                                       (std::min)(static_cast<int>(outline.left),
                                                  static_cast<int>(rect.right - extent.cx) - 2));
        dc.TextOut(label_x, label_y, name);
    }
    // The followed neuron and the one pointed at.
    const auto mark = [&](int lobe_index, int neuron, COLORREF colour) {
        if (lobe_index < 0 || lobe_index >= static_cast<int>(lobes.size())) {
            return;
        }
        const c1kit::LobeLayout& lobe = lobes[static_cast<std::size_t>(lobe_index)];
        const int x = lobe.x + neuron % (lobe.width > 0 ? lobe.width : 1);
        const int y = lobe.y + neuron / (lobe.width > 0 ? lobe.width : 1);
        CRect square(cell_x(x) - 1, cell_y(y) - 1, cell_x(x + 1) + 2, cell_y(y + 1) + 2);
        CBrush brush(colour);
        dc.FrameRect(square, &brush);
    };
    mark(hover_lobe_, hover_neuron_, RGB(255, 255, 255));
    mark(followed_lobe_, followed_neuron_, RGB(255, 255, 0));
    if (lobes.empty()) {
        dc.SetTextColor(RGB(180, 180, 190));
        dc.TextOut(area.left + 12, area.top + 12,
                   sheet_.subject().present ? _T("The game did not describe this brain.")
                                            : _T("Select a creature in the game."));
    }
}

} // namespace science
