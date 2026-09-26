// Brain: a live map of the creature's brain.
//
// The 1996 page (CScannerPage, dialog 134) painted the brain report's active
// cells as dots over a picture of a human brain (a 64 x 48 lattice, four
// pixels a cell), with nothing to say which lobe a dot was in or what it
// meant.
//
// Here the brain is the grid the game lays it out on, as large as the page
// allows: each lobe (from `dde: lobe`) is outlined and named in its own
// colour, with the names placed clear of the other lobes, and each neuron is
// a square shaded by its firing strength (the brain report), its activation,
// or its dendrites' average target weight or state (`cell`, every neuron
// about once a second).  The list beside it says how active each lobe is.  Pointing at
// a neuron says which lobe and neuron it is, what it stands for and its
// value; clicking one follows it with its exact values (`dde: cell`) for
// both of its dendrite rules.
//
// The report gives each neuron not at zero as value/16, so everything from 1
// to 15 comes through as level 0.  That is still told apart from zero, and
// the shading rises with the square root of the value, so that the quiet
// neurons, where most of a brain is, are not all one colour.  Selecting a
// lobe (in the list, or by clicking in it) reads its neurons' exact values
// with `cell`, a few dozen a query, and shades it with those instead.
//
// Under the lobes run the genome's connections (c1kit::brain_wiring): which
// lobe each lobe's dendrites read, and which lobes copy their firing into
// Perception, lit and pulsing with the source lobe's firing.  The game does
// not report its dendrites, so these are lobe to lobe, from the genome, and
// only drawn when the genome's lobes are where the game's `lobe` reply puts
// them.
// For the neuron pointed at or followed, each rule's dendrites are traced
// into the lobe they read (c1kit::dendrite_reach): the spot the first was
// placed on, the cells the rest could land on, and which of those fire.
// Where a rule lets dendrites move (its connection mode), that is where they
// started, and the page says so.

#include "science.hpp"
#include "science_ids.hpp"

#include <algorithm>
#include <cmath>

namespace science {
namespace {

// What the map can show.  The brain report measures what its holder's first
// two work values say, but the original game never runs a report holder's
// script, so there it is always firing strength and the 1996 page's
// `setv var0 1` never took (see brain_map.hpp).  LibreCreatures runs it.
// The page asks the game which it is (report_probe_script); where the report
// cannot give a measure, `cell` gives it exactly, bar the strongest dendrite
// weight, which `cell` sums rather than reporting the largest.
//
// Firing strength is the default: it is what a neuron passes on (Lobe
// ::update_late_phase: activation less the lobe's threshold), what other
// neurons' dendrites read and what the decision lobe's winner is picked by.
// Activation is read after it has relaxed towards the lobe's resting level
// for the tick, so fast-relaxing lobes read near zero while they fire.
struct Measure {
    const TCHAR* name;
    int mode;
    bool uses_rule;
    const TCHAR* about;
};
const Measure kMeasures[] = {
    {_T("Firing strength"), c1kit::kReportFiringStrength, false,
     _T("Firing strength is what each neuron passes on: how far its activation is above ")
     _T("its lobe's threshold. Other neurons respond to it, and the creature's decision ")
     _T("is the strongest-firing decision neuron.")},
    {_T("Activation"), c1kit::kReportActivation, false,
     _T("Activation is each neuron's inner level, which drifts back to its lobe's resting ")
     _T("level every moment. Lobes that settle fast, such as Perception and Drive, read ")
     _T("near zero here even while firing hard.")},
    {_T("Strongest dendrite weight"), c1kit::kReportStrongestWeight, true,
     _T("The strongest current weight among each neuron's dendrites under the chosen ")
     _T("dendrite rule.")},
    {_T("Average target weight"), c1kit::kReportAverageTargetWeight, true,
     _T("The average target weight of each neuron's dendrites under the chosen rule: ")
     _T("how much the neuron weighs what those inputs fire.")},
    {_T("Average dendrite state"), c1kit::kReportAverageDendriteState, true,
     _T("The average state of each neuron's dendrites under the chosen rule.")},
};
constexpr int kMeasureCount = sizeof(kMeasures) / sizeof(kMeasures[0]);

const Measure& measure_at(int index) {
    return kMeasures[index >= 0 && index < kMeasureCount ? index : 0];
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

// "37", or the range a report level stands for.
CString value_text(int value, bool exact) {
    CString text;
    if (value < 0) {
        text = _T("0 (not in the report)");
    } else if (exact) {
        text.Format(_T("%d"), value);
    } else if (value < 16) {
        text = _T("1 to 15 (the report gives value/16)");
    } else {
        const int low = value / 16 * 16;
        text.Format(_T("%d to %d (the report gives value/16)"), low, low + 15);
    }
    return text;
}

// How much of `box` the others cover, in square pixels.
long overlap_area(const CRect& box, const std::vector<CRect>& others) {
    long area = 0;
    for (const CRect& other : others) {
        CRect both;
        if (both.IntersectRect(box, other)) {
            area += static_cast<long>(both.Width()) * both.Height();
        }
    }
    return area;
}

constexpr COLORREF kBackground = RGB(18, 20, 28);

} // namespace

BEGIN_MESSAGE_MAP(BrainPage, SciencePage)
    ON_CBN_SELCHANGE(kControlReportMode, &BrainPage::OnModeChanged)
    ON_CBN_SELCHANGE(kControlReportRule, &BrainPage::OnModeChanged)
    ON_BN_CLICKED(kControlWiring, &BrainPage::OnWiringToggled)
END_MESSAGE_MAP()

BrainPage::BrainPage(ScienceSheet& sheet) : SciencePage(sheet, kStringBrainTab) {}

void BrainPage::create_controls() {
    grid_.create(*this, kControlBrainGrid,
                 [this](CDC& dc, const CRect& rect) { draw_grid(dc, rect); });
    grid_.set_mouse_handler([this](CPoint point, bool clicked) { on_mouse(point, clicked); });
    make(mode_label_, _T("STATIC"), _T("Show:"), SS_LEFT, kControlHint);
    make(mode_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
         kControlReportMode);
    for (const Measure& measure : kMeasures) {
        mode_.AddString(measure.name);
    }
    make(rule_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
         kControlReportRule);
    rule_.AddString(_T("Dendrite rule 0"));
    rule_.AddString(_T("Dendrite rule 1"));
    std::uint32_t mode = c1kit::kReportFiringStrength;
    std::uint32_t rule = 0;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->read_dword(c1kit::SettingsScope::user, "Scanner", mode);
        settings->read_dword(c1kit::SettingsScope::user, "Scanner Rule", rule);
    }
    int selection = 0;
    for (int i = 0; i < kMeasureCount; ++i) {
        if (kMeasures[i].mode == static_cast<int>(mode)) selection = i;
    }
    mode_.SetCurSel(selection);
    rule_.SetCurSel(rule <= 1 ? static_cast<int>(rule) : 0);
    rule_.EnableWindow(measure_at(selection).uses_rule);
    make(wiring_check_, _T("BUTTON"), _T("Connections"), BS_AUTOCHECKBOX | WS_TABSTOP,
         kControlWiring);
    std::uint32_t wiring = 1;
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->read_dword(c1kit::SettingsScope::user, "Scanner Wiring", wiring);
    }
    wiring_check_.SetCheck(wiring != 0 ? BST_CHECKED : BST_UNCHECKED);

    make(lobes_, WC_LISTVIEW, _T(""),
         LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER,
         kControlLobeList);
    lobes_.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    lobes_.InsertColumn(0, _T("Lobe"), LVCFMT_LEFT, 110);
    lobes_.InsertColumn(1, _T("Active"), LVCFMT_RIGHT, 48);
    lobes_.InsertColumn(2, _T("Mean"), LVCFMT_RIGHT, 54);
    make(info_, _T("EDIT"), _T(""),
         ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_BORDER, kControlNeuronInfo);
    fill_lobe_list();
    show_neuron_info();
}

// The map takes all the page but a column on the right, with what it shows
// above it; the column holds the lobes, what is pointed at, and Close.
void BrainPage::layout(int width, int height) {
    const int margin = kMargin;
    const int row = text_height() + 8;
    const int side = (std::min)(250, (std::max)(215, width / 4));
    const int grid_width = width - side - 3 * margin;
    place(mode_label_, margin, margin + 4, 36, text_height());
    place(mode_, margin + 38, margin, 190, 200);
    place(rule_, margin + 38 + 196, margin, 130, 200);

    const int grid_top = margin + row + 4;
    place(grid_, margin, grid_top, grid_width, height - margin - grid_top);
    const int left = width - side - margin;
    const int list_height = 10 * (text_height() + 5) + 8;
    place(lobes_, left, margin, side, list_height);
    lobes_.SetColumnWidth(0, side - 48 - 54 - 6);
    const int info_top = margin + list_height + margin;
    place(info_, left, info_top, side, height - 2 * margin - button_height() - margin - info_top);
    place_close(width, height);
    // Beside Close, at the foot of the column.
    place(wiring_check_, left, height - margin - button_height() + 4, side - 84 - margin,
          text_height() + 4);
}

void BrainPage::fill_lobe_list() {
    updating_list_ = true;
    lobes_.DeleteAllItems();
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    for (std::size_t i = 0; i < lobes.size(); ++i) {
        lobes_.InsertItem(static_cast<int>(i), CString(c1kit::lobe_name(static_cast<int>(i))));
    }
    if (selected_lobe_ >= 0 && selected_lobe_ < static_cast<int>(lobes.size())) {
        lobes_.SetItemState(selected_lobe_, LVIS_SELECTED, LVIS_SELECTED);
    }
    updating_list_ = false;
    update_lobe_list();
}

// Each lobe's share of neurons not at zero, and its mean value (exact
// for the selected lobe once read, else the report's estimate).
void BrainPage::update_lobe_list() {
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    for (std::size_t i = 0; i < lobes.size() && static_cast<int>(i) < lobes_.GetItemCount(); ++i) {
        const c1kit::LobeLayout& lobe = lobes[i];
        int active = 0;
        long total = 0;
        for (int y = lobe.y; y < lobe.y + lobe.height && y < c1kit::kBrainGridSize; ++y) {
            for (int x = lobe.x; x < lobe.x + lobe.width && x < c1kit::kBrainGridSize; ++x) {
                bool exact = false;
                const int value = cell_value(static_cast<int>(i), x, y, exact);
                if (value > 0) {
                    ++active;
                    total += value;
                }
            }
        }
        const int neurons = (std::max)(1, lobe.neurons());
        CString share;
        CString average;
        if (sheet_.subject().present) {
            share.Format(_T("%d%%"), active * 100 / neurons);
            average.Format(_T("%ld"), total / neurons);
        }
        const int item = static_cast<int>(i);
        if (lobes_.GetItemText(item, 1) != share) lobes_.SetItemText(item, 1, share);
        if (lobes_.GetItemText(item, 2) != average) lobes_.SetItemText(item, 2, average);
    }
}

void BrainPage::select_lobe(int lobe) {
    selected_lobe_ = lobe;
    if (lobes_.GetSafeHwnd() != nullptr) {
        updating_list_ = true;
        if (lobe >= 0) {
            lobes_.SetItemState(lobe, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            lobes_.EnsureVisible(lobe, FALSE);
        } else {
            const int selected = lobes_.GetNextItem(-1, LVNI_SELECTED);
            if (selected >= 0) lobes_.SetItemState(selected, 0, LVIS_SELECTED);
        }
        updating_list_ = false;
    }
    poll();
}

BOOL BrainPage::OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) {
    const auto* header = reinterpret_cast<const NMHDR*>(lparam);
    if (header->idFrom == kControlLobeList && header->code == LVN_ITEMCHANGED &&
        !updating_list_) {
        const auto* change = reinterpret_cast<const NMLISTVIEW*>(lparam);
        if ((change->uChanged & LVIF_STATE) != 0 && (change->uNewState & LVIS_SELECTED) != 0 &&
            (change->uOldState & LVIS_SELECTED) == 0) {
            select_lobe(change->iItem);
        }
    }
    return SciencePage::OnNotify(wparam, lparam, result);
}

int BrainPage::report_mode() const {
    return measure_at(mode_.GetCurSel()).mode;
}

int BrainPage::report_rule() const {
    return measure_at(mode_.GetCurSel()).uses_rule ? rule_.GetCurSel() : 0;
}

bool BrainPage::report_covers_measure() const {
    return report_mode() == c1kit::kReportFiringStrength || report_honours_measure_ == 1;
}

// Reads the next few dozen neurons exactly: of the selected lobe while the
// report gives the measure (it covers the rest), else of every lobe.
void BrainPage::refresh_exact() {
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    const int mode = report_mode();
    const int rule = report_rule();
    if (exact_.size() != lobes.size() || mode != exact_mode_ || rule != exact_rule_) {
        exact_.assign(lobes.size(), std::vector<int>());
        for (std::size_t i = 0; i < lobes.size(); ++i) {
            exact_[i].assign(static_cast<std::size_t>((std::max)(0, lobes[i].neurons())), -1);
        }
        exact_mode_ = mode;
        exact_rule_ = rule;
        exact_lobe_ = selected_lobe_;
        exact_next_ = 0;
    }
    if (lobes.empty()) {
        return;
    }
    const bool all_lobes = !report_covers_measure();
    if ((!all_lobes && selected_lobe_ < 0) || mode == c1kit::kReportStrongestWeight) {
        return;  // (`cell` cannot give the strongest weight)
    }
    // Up to four queries a poll: a whole Concept lobe in under a second, the
    // whole brain in about a second.
    for (int query = 0; query < 4; ++query) {
        if (!all_lobes || exact_lobe_ < 0 || exact_lobe_ >= static_cast<int>(lobes.size())) {
            if (!all_lobes) exact_lobe_ = selected_lobe_;
            if (exact_lobe_ < 0 || exact_lobe_ >= static_cast<int>(lobes.size())) exact_lobe_ = 0;
        }
        const int neurons = static_cast<int>(exact_[static_cast<std::size_t>(exact_lobe_)].size());
        if (exact_next_ >= neurons) {
            exact_next_ = 0;
            if (all_lobes) {
                exact_lobe_ = (exact_lobe_ + 1) % static_cast<int>(lobes.size());
                continue;
            }
            if (neurons == 0) return;
        }
        const int count = (std::min)(c1kit::kCellsPerQuery, neurons - exact_next_);
        std::string reply;
        std::vector<c1kit::NeuronValues> values;
        if (!sheet_.query(c1kit::lobe_cells_query(exact_lobe_, exact_next_, count, rule), reply) ||
            !c1kit::parse_cell_batch(reply, count, values)) {
            return;
        }
        std::vector<int>& lobe_values = exact_[static_cast<std::size_t>(exact_lobe_)];
        for (int i = 0; i < count; ++i) {
            lobe_values[static_cast<std::size_t>(exact_next_ + i)] =
                c1kit::exact_report_value(values[static_cast<std::size_t>(i)], mode);
        }
        exact_next_ += count;
        if (!all_lobes && neurons <= c1kit::kCellsPerQuery) {
            break;
        }
    }
}

int BrainPage::cell_value(int lobe, int x, int y, bool& exact) const {
    exact = false;
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    if (lobe >= 0 && lobe < static_cast<int>(lobes.size()) &&
        lobe < static_cast<int>(exact_.size())) {
        const c1kit::LobeLayout& layout = lobes[static_cast<std::size_t>(lobe)];
        const std::vector<int>& values = exact_[static_cast<std::size_t>(lobe)];
        const int neuron = (y - layout.y) * layout.width + (x - layout.x);
        if (neuron >= 0 && neuron < static_cast<int>(values.size()) &&
            values[static_cast<std::size_t>(neuron)] >= 0) {
            exact = true;
            return values[static_cast<std::size_t>(neuron)];
        }
    }
    if (!report_covers_measure() || x < 0 ||
        x >= c1kit::kBrainGridSize || y < 0 || y >= c1kit::kBrainGridSize ||
        !activity_.reported[x][y]) {
        return -1;
    }
    return c1kit::estimated_value(activity_, x, y);
}

void BrainPage::subject_changed() {
    activity_.clear();
    followed_lobe_ = followed_neuron_ = -1;
    hover_lobe_ = hover_neuron_ = -1;
    followed_valid_ = false;
    selected_lobe_ = -1;
    exact_.clear();
    wiring_.clear();
    wiring_loaded_ = wiring_valid_ = false;
    firing_.clear();
    if (created_) {
        fill_lobe_list();
        show_neuron_info();
        grid_.redraw();
    }
}

void BrainPage::OnWiringToggled() {
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Scanner Wiring", wiring_check_.GetCheck() == BST_CHECKED ? 1u : 0u);
    }
    poll();
}

void BrainPage::load_wiring() {
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    if (!sheet_.subject().present || lobes.empty()) {
        return;  // try again once the game has described the brain
    }
    wiring_loaded_ = true;
    std::vector<c1kit::Gene> genes;
    std::size_t bytes = 0;
    if (!sheet_.read_genome(genes, bytes)) {
        return;
    }
    // The plan is only drawn if it is this brain's: the same lobes, where
    // the game says they are, laid out on C1's grid or, failing that, on
    // LibreCreatures' extended one.
    const bool male = sheet_.subject().sex == 1;
    wiring_ = c1kit::brain_wiring(genes, male);
    wiring_valid_ = c1kit::wiring_matches(wiring_, lobes);
    if (!wiring_valid_) {
        wiring_ = c1kit::brain_wiring(genes, male, true);
        wiring_valid_ = c1kit::wiring_matches(wiring_, lobes, true);
    }
}

bool BrainPage::wiring_shown() const {
    return wiring_valid_ && wiring_check_.GetSafeHwnd() != nullptr &&
           wiring_check_.GetCheck() == BST_CHECKED;
}

void BrainPage::lobe_firing(int lobe, int& share_percent, int& mean) const {
    share_percent = mean = 0;
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    if (lobe < 0 || lobe >= static_cast<int>(lobes.size())) {
        return;
    }
    const c1kit::LobeLayout& layout = lobes[static_cast<std::size_t>(lobe)];
    int firing = 0;
    long total = 0;
    for (int y = layout.y; y < layout.y + layout.height && y < c1kit::kBrainGridSize; ++y) {
        for (int x = layout.x; x < layout.x + layout.width && x < c1kit::kBrainGridSize; ++x) {
            if (firing_.reported[x][y]) {
                ++firing;
                total += c1kit::estimated_value(firing_, x, y);
            }
        }
    }
    if (layout.neurons() > 0) share_percent = firing * 100 / layout.neurons();
    if (firing > 0) mean = static_cast<int>(total / firing);
}

// Where a neuron's dendrites land, rule by rule: exactly how many it grew if
// it is the neuron followed (`cell`), else as many as the genome allows.
CString BrainPage::reach_text(int lobe, int neuron) const {
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    if (!wiring_valid_ || lobe < 0 || lobe >= static_cast<int>(wiring_.size()) ||
        lobe >= static_cast<int>(lobes.size())) {
        return CString();
    }
    const bool counted = lobe == followed_lobe_ && neuron == followed_neuron_ && followed_valid_;
    CString text;
    for (int r = 0; r < 2; ++r) {
        const c1kit::DendriteRule& rule = wiring_[static_cast<std::size_t>(lobe)].rules[r];
        if (rule.most == 0 || rule.source >= static_cast<int>(lobes.size())) continue;
        const c1kit::LobeLayout& source = lobes[static_cast<std::size_t>(rule.source)];
        const int dendrites = counted ? followed_values_[r].dendrites : rule.most;
        c1kit::DendriteReach reach;
        CString line;
        if (!c1kit::dendrite_reach(neuron, lobes[static_cast<std::size_t>(lobe)].neurons(),
                                   source.width, source.height, rule.spread, dendrites, reach)) {
            if (counted) {
                line.Format(_T("  Rule %d: no dendrites into %s\r\n"), r,
                            CString(c1kit::lobe_name(rule.source)).GetString());
                text += line;
            }
            continue;
        }
        const int spot = reach.spot_y * source.width + reach.spot_x;
        const CString source_name(c1kit::lobe_name(rule.source));
        if (counted) {
            line.Format(_T("Rule %d: %d dendrite%s into %s, the first on neuron %d"), r, dendrites,
                        dendrites == 1 ? _T("") : _T("s"), source_name.GetString(), spot);
        } else {
            line.Format(_T("Rule %d reads %s neuron %d"), r, source_name.GetString(), spot);
        }
        if (dendrites > 1) {
            CString more;
            if (rule.spread == 0) {
                more.Format(counted ? _T(", all %d there") : _T(", up to %d there"), dendrites);
            } else if (counted) {
                more.Format(_T(", the others within %d cell%s of it"), rule.spread,
                            rule.spread == 1 ? _T("") : _T("s"));
            } else {
                more.Format(_T(", and up to %d more within %d cell%s of it"), dendrites - 1,
                            rule.spread, rule.spread == 1 ? _T("") : _T("s"));
            }
            line += more;
        }
        if (rule.mode != 0) {
            line += _T(" when the brain was built; they move to firing neurons as it learns");
        }
        text += _T("  ") + line + _T("\r\n");
    }
    if (!text.IsEmpty()) {
        text = _T("Its dendrites:\r\n") + text;
    }
    return text;
}

CString BrainPage::wiring_text(int lobe) const {
    if (!wiring_valid_ || lobe < 0 || lobe >= static_cast<int>(wiring_.size())) {
        return CString();
    }
    const auto name = [](int index) { return CString(c1kit::lobe_name(index)); };
    CString fed;
    const c1kit::LobeWiring& own = wiring_[static_cast<std::size_t>(lobe)];
    for (int r = 0; r < 2; ++r) {
        const c1kit::DendriteRule& rule = own.rules[r];
        if (rule.most == 0) continue;
        CString line;
        if (rule.fewest == rule.most) {
            line.Format(_T("  %s (rule %d): %d dendrite%s a neuron"), name(rule.source).GetString(), r,
                        rule.most, rule.most == 1 ? _T("") : _T("s"));
        } else {
            line.Format(_T("  %s (rule %d): %d to %d dendrites a neuron"),
                        name(rule.source).GetString(), r, rule.fewest, rule.most);
        }
        if (rule.spread > 0 && rule.most > 1) {
            CString spread;
            spread.Format(_T(", within %d cell%s of the matching spot"), rule.spread,
                          rule.spread == 1 ? _T("") : _T("s"));
            line += spread;
        }
        fed += line + _T("\r\n");
    }
    if (lobe == 0) {
        for (std::size_t i = 1; i < wiring_.size(); ++i) {
            if (wiring_[i].perception_copy != 0) {
                fed += _T("  ") + name(static_cast<int>(i)) + _T(" (copies its firing)\r\n");
            }
        }
    }
    CString feeds;
    for (std::size_t i = 0; i < wiring_.size(); ++i) {
        for (int r = 0; r < 2; ++r) {
            const c1kit::DendriteRule& rule = wiring_[i].rules[r];
            if (rule.most > 0 && rule.source == lobe) {
                CString line;
                line.Format(_T("  %s (rule %d)\r\n"), name(static_cast<int>(i)).GetString(), r);
                feeds += line;
            }
        }
    }
    if (lobe != 0 && own.perception_copy != 0) {
        feeds += _T("  Perception (copies its firing)\r\n");
    }
    CString text;
    if (!fed.IsEmpty()) text += _T("Fed by:\r\n") + fed;
    if (!feeds.IsEmpty()) text += _T("Feeds:\r\n") + feeds;
    return text;
}

// The genome's wiring, under the lobes: a curve from each lobe a dendrite
// rule reaches into to the lobe that grows the dendrites (thicker the more
// dendrites), dotted for the firing input lobes copy into Perception, each in
// its source's colour, brighter the more of the source is firing, with dots
// running along it as it fires.  Pointing at or selecting a lobe dims the
// curves that do not touch it.
void BrainPage::draw_wiring(CDC& dc, const std::vector<CRect>& outlines) {
    struct Edge {
        int source;
        int destination;
        long dendrites;
        bool copy;
    };
    std::vector<Edge> edges;
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    for (std::size_t i = 0; i < wiring_.size() && i < outlines.size(); ++i) {
        const int destination = static_cast<int>(i);
        for (const c1kit::DendriteRule& rule : wiring_[i].rules) {
            if (rule.most == 0) continue;
            const long dendrites = static_cast<long>(lobes[i].neurons()) * (rule.fewest + rule.most) / 2;
            auto same = std::find_if(edges.begin(), edges.end(), [&](const Edge& e) {
                return !e.copy && e.source == rule.source && e.destination == destination;
            });
            if (same != edges.end()) {
                same->dendrites += dendrites;
            } else {
                edges.push_back({rule.source, destination, dendrites, false});
            }
        }
        if (i > 0 && wiring_[i].perception_copy != 0) {
            edges.push_back({destination, 0, lobes[i].neurons(), true});
        }
    }
    const int focus = hover_lobe_ >= 0 ? hover_lobe_ : selected_lobe_;
    for (const Edge& edge : edges) {
        const CRect& from = outlines[static_cast<std::size_t>(edge.source)];
        const CRect& to = outlines[static_cast<std::size_t>(edge.destination)];
        int share = 0, mean = 0;
        lobe_firing(edge.source, share, mean);
        const double drive = share > 0 ? std::sqrt(mean / 255.0) : 0.0;
        int level = share > 0 ? 45 + int(55 * drive) : 30;
        if (focus >= 0 && edge.source != focus && edge.destination != focus) {
            level = level * 35 / 100;
        }
        const COLORREF colour = blend(kBackground, lobe_colour(edge.source), level);
        const int width = (std::max)(1, (std::min)(4, int(std::log2(double(edge.dendrites) + 1) / 3)));

        // The curve: out of the source's outline, into the destination's,
        // bowed to one side so that two lobes feeding each other show two.
        POINT points[4];
        if (edge.source == edge.destination) {
            const int top = from.top + from.Height() / 4;
            const int bottom = from.bottom - from.Height() / 4;
            points[0] = {from.right, top};
            points[1] = {from.right + 22, top - 6};
            points[2] = {from.right + 22, bottom + 6};
            points[3] = {from.right, bottom};
        } else {
            const auto exit_point = [](const CRect& r, double fx, double fy, double tx, double ty) {
                // Where the line from the centre towards (tx, ty) leaves r.
                const double dx = tx - fx, dy = ty - fy;
                double t = 1.0;
                if (dx != 0) t = (std::min)(t, std::abs((dx > 0 ? r.right - fx : r.left - fx) / dx));
                if (dy != 0) t = (std::min)(t, std::abs((dy > 0 ? r.bottom - fy : r.top - fy) / dy));
                return POINT{LONG(fx + dx * t), LONG(fy + dy * t)};
            };
            const double fx = from.CenterPoint().x, fy = from.CenterPoint().y;
            const double tx = to.CenterPoint().x, ty = to.CenterPoint().y;
            const POINT p0 = exit_point(from, fx, fy, tx, ty);
            const POINT p3 = exit_point(to, tx, ty, fx, fy);
            const double dx = p3.x - p0.x, dy = p3.y - p0.y;
            const double length = (std::max)(1.0, std::sqrt(dx * dx + dy * dy));
            const double bow = (edge.copy ? -0.12 : 0.12) * length;
            const double nx = -dy / length * bow, ny = dx / length * bow;
            points[0] = p0;
            points[1] = {LONG(p0.x + dx / 3 + nx), LONG(p0.y + dy / 3 + ny)};
            points[2] = {LONG(p0.x + 2 * dx / 3 + nx), LONG(p0.y + 2 * dy / 3 + ny)};
            points[3] = p3;
        }
        CPen pen(edge.copy ? PS_DOT : PS_SOLID, edge.copy ? 1 : width, colour);
        CPen* previous_pen = dc.SelectObject(&pen);
        dc.PolyBezier(points, 4);
        dc.SelectObject(previous_pen);

        // The arrowhead, into the destination.
        {
            double ax = points[3].x - points[2].x, ay = points[3].y - points[2].y;
            const double length = (std::max)(1.0, std::sqrt(ax * ax + ay * ay));
            ax /= length;
            ay /= length;
            const double size = 5 + width;
            POINT head[3] = {
                points[3],
                {LONG(points[3].x - ax * size - ay * size * 0.6), LONG(points[3].y - ay * size + ax * size * 0.6)},
                {LONG(points[3].x - ax * size + ay * size * 0.6), LONG(points[3].y - ay * size - ax * size * 0.6)},
            };
            CBrush brush(colour);
            CPen outline(PS_SOLID, 1, colour);
            CBrush* previous_brush = dc.SelectObject(&brush);
            previous_pen = dc.SelectObject(&outline);
            dc.Polygon(head, 3);
            dc.SelectObject(previous_brush);
            dc.SelectObject(previous_pen);
        }

        // Firing running along it: a dot for every quarter of the source
        // firing, moving from source to destination a little each poll.
        if (share > 0) {
            const int dots = (std::min)(5, 1 + share / 25);
            const COLORREF bright = blend(lobe_colour(edge.source), RGB(255, 255, 255),
                                          focus >= 0 && edge.source != focus &&
                                                  edge.destination != focus ? 0 : 45);
            for (int k = 0; k < dots; ++k) {
                double t = pulse_frame_ * 0.06 + double(k) / dots;
                t -= std::floor(t);
                const double u = 1 - t;
                const double x = u * u * u * points[0].x + 3 * u * u * t * points[1].x +
                                 3 * u * t * t * points[2].x + t * t * t * points[3].x;
                const double y = u * u * u * points[0].y + 3 * u * u * t * points[1].y +
                                 3 * u * t * t * points[2].y + t * t * t * points[3].y;
                const int radius = 1 + width / 2 + int(2 * drive);
                dc.FillSolidRect(int(x) - radius, int(y) - radius, 2 * radius + 1, 2 * radius + 1,
                                 level < 40 ? blend(kBackground, bright, 50) : bright);
            }
        }
    }
}

void BrainPage::OnModeChanged() {
    rule_.EnableWindow(measure_at(mode_.GetCurSel()).uses_rule);
    if (c1kit::KitSettings* settings = sheet_.settings()) {
        settings->write_dword("Scanner", static_cast<std::uint32_t>(report_mode()));
        settings->write_dword("Scanner Rule", static_cast<std::uint32_t>(rule_.GetCurSel()));
    }
    poll();
}

void BrainPage::poll() {
    std::string reply;
    if (report_honours_measure_ < 0 && sheet_.subject().present) {
        std::string probe;
        if (sheet_.brain_report(c1kit::kReportFiringStrength, 0, reply) && !reply.empty() &&
            sheet_.brain_report(c1kit::kReportAverageTargetWeight, 7, probe)) {
            report_honours_measure_ = probe.empty() ? 1 : 0;
        }
    }
    if (!report_covers_measure()) {
        activity_.clear();
    } else if (sheet_.brain_report(report_mode(), report_rule(), reply)) {
        c1kit::parse_activity_report(reply, activity_);
    }
    if (!wiring_loaded_) {
        load_wiring();
    }
    if (wiring_shown()) {
        if (report_mode() == c1kit::kReportFiringStrength) {
            firing_ = activity_;
        } else if (sheet_.brain_report(c1kit::kReportFiringStrength, 0, reply)) {
            c1kit::parse_activity_report(reply, firing_);
        }
        ++pulse_frame_;
    }
    if (followed_lobe_ >= 0 &&
        sheet_.query(c1kit::neuron_query(followed_lobe_, followed_neuron_), reply)) {
        followed_valid_ = c1kit::parse_neuron_values(reply, followed_values_);
    }
    refresh_exact();
    update_lobe_list();
    show_neuron_info();
    grid_.redraw();
}

// Zooms to the lobes: the smallest part of the grid holding them all, as
// large as the view allows (not a whole number of pixels a cell, so that it
// fills the view), with room around it for the lobes' names.
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
        right = bottom = c1kit::kStandardBrainGrid;
    }
    view_x_ = left;
    view_y_ = top;
    view_width_ = right - left;
    view_height_ = bottom - top;
    const int margin_x = text_height() + 4;  // room for a name down the side
    const int margin_y = text_height() + 4;
    scale_ = (std::max)(1.0, (std::min)(double(rect.Width() - 2 * margin_x) / view_width_,
                                        double(rect.Height() - 2 * margin_y) / view_height_));
    view_origin_ = CPoint(rect.left + int((rect.Width() - scale_ * view_width_) / 2),
                          rect.top + int((rect.Height() - scale_ * view_height_) / 2));
}

bool BrainPage::cell_at(CPoint point, int& x, int& y) const {
    if (scale_ <= 0 || point.x < view_origin_.x || point.y < view_origin_.y) {
        return false;
    }
    x = view_x_ + int((point.x - view_origin_.x) / scale_);
    y = view_y_ + int((point.y - view_origin_.y) / scale_);
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
        if (lobe >= 0) {
            select_lobe(lobe);  // polls
        } else {
            poll();
        }
        return;
    }
    if (lobe != hover_lobe_ || neuron != hover_neuron_) {
        hover_lobe_ = lobe;
        hover_neuron_ = neuron;
        show_neuron_info();
        grid_.redraw();
    }
}

// What is being pointed at, the selected lobe, and the neuron followed.
void BrainPage::show_neuron_info() {
    if (info_.GetSafeHwnd() == nullptr) {
        return;
    }
    const c1kit::NeuronNames& names = sheet_.neuron_names();
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    const CString measure = measure_at(mode_.GetCurSel()).name;
    CString text;
    if (!sheet_.subject().present) {
        text = _T("Select a creature in the game to see its brain.");
    } else if (hover_lobe_ >= 0 && hover_lobe_ < static_cast<int>(lobes.size())) {
        const c1kit::LobeLayout& lobe = lobes[static_cast<std::size_t>(hover_lobe_)];
        bool exact = false;
        const int value = cell_value(hover_lobe_, lobe.x + hover_neuron_ % (std::max)(1, lobe.width),
                                     lobe.y + hover_neuron_ / (std::max)(1, lobe.width), exact);
        text = neuron_title(names, hover_lobe_, hover_neuron_) + _T("\r\n") +
               CString(c1kit::lobe_description(hover_lobe_)) + _T("\r\n") + measure + _T(": ") +
               value_text(value, exact) + _T("\r\n");
        if (!exact && report_covers_measure() &&
            report_mode() != c1kit::kReportStrongestWeight) {
            text += _T("Click to follow it and read its lobe exactly.\r\n");
        }
        const CString reach = reach_text(hover_lobe_, hover_neuron_);
        if (!reach.IsEmpty()) {
            text += _T("\r\n") + reach;
        }
        const CString wiring = wiring_text(hover_lobe_);
        if (!wiring.IsEmpty()) {
            text += _T("\r\n") + wiring;
        }
        text += _T("\r\n");
    } else if (selected_lobe_ >= 0 && selected_lobe_ < static_cast<int>(lobes.size())) {
        CString lobe;
        lobe.Format(_T("%s lobe, %d neurons\r\n"),
                    CString(c1kit::lobe_name(selected_lobe_)).GetString(),
                    lobes[static_cast<std::size_t>(selected_lobe_)].neurons());
        text = lobe + CString(c1kit::lobe_description(selected_lobe_)) + _T("\r\n");
        const CString wiring = wiring_text(selected_lobe_);
        if (!wiring.IsEmpty()) {
            text += _T("\r\n") + wiring + _T("\r\n");
        }
        if (report_mode() == c1kit::kReportStrongestWeight) {
            text += _T("The strongest weight comes only from the report.\r\n");
        } else {
            text += _T("Shaded with exact values.\r\n");
        }
        text += _T("\r\n");
    } else {
        text = _T("Each square is a neuron, brighter the stronger it is; dark ones are at ")
               _T("zero. Point at one to see what it is; click it to follow it.");
        if (!report_covers_measure() && report_mode() == c1kit::kReportStrongestWeight) {
            text = _T("This game cannot report the strongest dendrite weight: it only ")
                   _T("reports firing strength. LibreCreatures can.");
        } else if (report_covers_measure() &&
                   report_mode() != c1kit::kReportStrongestWeight) {
            text += _T(" Select a lobe to shade it with exact values.");
        }
        text += _T("\r\n\r\n") + CString(measure_at(mode_.GetCurSel()).about) + _T("\r\n\r\n");
        if (wiring_loaded_ && !wiring_valid_ && wiring_check_.GetCheck() == BST_CHECKED) {
            text += _T("The connections cannot be shown: the creature's genome file does not ")
                    _T("match its brain.\r\n\r\n");
        } else if (wiring_shown()) {
            text += _T("The lines are the genome's wiring: each runs from the lobe a set of ")
                    _T("dendrites reads to the lobe that grows them, thicker the more ")
                    _T("dendrites, brighter and busier the more of its source is firing. ")
                    _T("Dotted lines are the input lobes copying their firing into ")
                    _T("Perception.\r\n\r\n");
        }
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
            if (hover_lobe_ != followed_lobe_ || hover_neuron_ != followed_neuron_) {
                text += reach_text(followed_lobe_, followed_neuron_);
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
    dc.FillSolidRect(rect, kBackground);
    fit_view(rect);
    // Grid cell (x, y)'s top left.
    const auto cell_x = [this](int x) { return view_origin_.x + int(std::lround((x - view_x_) * scale_)); };
    const auto cell_y = [this](int y) { return view_origin_.y + int(std::lround((y - view_y_) * scale_)); };
    const CRect area(cell_x(view_x_), cell_y(view_y_), cell_x(view_x_ + view_width_),
                     cell_y(view_y_ + view_height_));
    // Lines every cell when there is room for them, stronger every eight.
    for (int x = view_x_; x <= view_x_ + view_width_; ++x) {
        if (x % 8 == 0 || scale_ >= 5) {
            dc.FillSolidRect(cell_x(x), area.top, 1, area.Height(),
                             x % 8 == 0 ? RGB(55, 60, 75) : RGB(32, 35, 46));
        }
    }
    for (int y = view_y_; y <= view_y_ + view_height_; ++y) {
        if (y % 8 == 0 || scale_ >= 5) {
            dc.FillSolidRect(area.left, cell_y(y), area.Width(), 1,
                             y % 8 == 0 ? RGB(55, 60, 75) : RGB(32, 35, 46));
        }
    }
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    dc.SetBkMode(TRANSPARENT);
    std::vector<CRect> outlines;
    for (const c1kit::LobeLayout& lobe : lobes) {
        outlines.emplace_back(cell_x(lobe.x), cell_y(lobe.y), cell_x(lobe.x + lobe.width) + 1,
                              cell_y(lobe.y + lobe.height) + 1);
    }
    if (wiring_shown()) {
        draw_wiring(dc, outlines);
    }
    for (std::size_t index = 0; index < lobes.size(); ++index) {
        const c1kit::LobeLayout& lobe = lobes[index];
        const int lobe_index = static_cast<int>(index);
        dc.FillSolidRect(outlines[index], kBackground);  // the curves pass under the lobes
        const COLORREF colour = lobe_colour(lobe_index);
        const COLORREF dim = blend(kBackground, colour, 18);
        const COLORREF bright = blend(colour, RGB(255, 255, 255), 35);
        for (int y = lobe.y; y < lobe.y + lobe.height && y < c1kit::kBrainGridSize; ++y) {
            for (int x = lobe.x; x < lobe.x + lobe.width && x < c1kit::kBrainGridSize; ++x) {
                bool exact = false;
                const int value = cell_value(lobe_index, x, y, exact);
                // Rising with the square root, from a quarter of the way up,
                // so that 1..15 is plainly not zero and 16..63 spans a third.
                const COLORREF shade =
                    value <= 0 ? dim
                               : blend(dim, bright,
                                       25 + int(75 * std::sqrt((std::min)(value, 255) / 255.0)));
                const int inset = scale_ > 4 ? 1 : 0;
                const int x0 = cell_x(x) + inset;
                const int y0 = cell_y(y) + inset;
                dc.FillSolidRect(x0, y0, cell_x(x + 1) - x0, cell_y(y + 1) - y0, shade);
            }
        }
        const CRect& outline = outlines[index];
        CBrush border(colour);
        dc.FrameRect(outline, &border);
        if (lobe_index == hover_lobe_ || lobe_index == selected_lobe_) {
            CRect thick = outline;
            thick.InflateRect(1, 1);
            dc.FrameRect(thick, &border);
            if (lobe_index == selected_lobe_) {
                thick.InflateRect(1, 1);
                dc.FrameRect(thick, &border);
            }
        }
    }
    // The names: each in the first place clear of every other lobe and name
    // (above, below, then running down beside it, beside, inside), within
    // the view, or failing that the place least in the way.  Down the side
    // is for the tall thin lobes (the decision lobe, one neuron wide).
    LOGFONT down_font_description = {};
    if (CFont* font = dc.GetCurrentFont()) {
        font->GetLogFont(&down_font_description);
    }
    down_font_description.lfEscapement = 2700;  // reads top to bottom
    down_font_description.lfOrientation = 2700;
    CFont down_font;
    down_font.CreateFontIndirect(&down_font_description);
    struct Place {
        CPoint at;
        bool down;
    };
    std::vector<CRect> taken;
    for (std::size_t index = 0; index < lobes.size(); ++index) {
        const CRect& outline = outlines[index];
        const CString name(c1kit::lobe_name(static_cast<int>(index)));
        const CSize extent = dc.GetTextExtent(name);
        const CSize down_extent(extent.cy, extent.cx);
        const int middle_y = (outline.top + outline.bottom - extent.cy) / 2;
        // Above or below, pulled in from the view's edges.
        const int pulled_x = (std::max)(static_cast<int>(rect.left) + 2,
                                        (std::min)(static_cast<int>(outline.left),
                                                   static_cast<int>(rect.right - extent.cx) - 2));
        // A tall thin lobe is named down its side first.
        const bool tall = outline.Height() > 3 * outline.Width();
        const Place places[] = {
            {{outline.right + 2, outline.top}, true},
            {{outline.left - extent.cy - 2, outline.top}, true},
            {{outline.left, outline.top - extent.cy - 2}, false},
            {{outline.left, outline.bottom + 2}, false},
            {{outline.right - extent.cx, outline.top - extent.cy - 2}, false},
            {{outline.right - extent.cx, outline.bottom + 2}, false},
            {{pulled_x, outline.top - extent.cy - 2}, false},
            {{pulled_x, outline.bottom + 2}, false},
            {{outline.right + 2, outline.top}, true},
            {{outline.left - extent.cy - 2, outline.top}, true},
            {{outline.right + 4, middle_y}, false},
            {{outline.left - extent.cx - 4, middle_y}, false},
            {{outline.left + 3, outline.top + 2}, false},
        };
        const std::size_t place_count = sizeof(places) / sizeof(places[0]);
        std::vector<CRect> obstacles = taken;
        for (std::size_t other = 0; other < outlines.size(); ++other) {
            if (other != index) {
                CRect grown = outlines[other];
                grown.InflateRect(1, 1);
                obstacles.push_back(grown);
            }
        }
        CRect chosen(places[2].at, extent);
        bool chosen_down = false;
        long least = -1;
        for (std::size_t c = tall ? 0 : 2; c < place_count; ++c) {
            const CRect box(places[c].at, places[c].down ? down_extent : extent);
            if (box.left < rect.left || box.top < rect.top || box.right > rect.right ||
                box.bottom > rect.bottom) {
                continue;
            }
            if (c + 1 == place_count && (box.right > outline.right || box.bottom > outline.bottom)) {
                continue;  // inside, only if it fits
            }
            const long area = overlap_area(box, obstacles);
            if (least < 0 || area < least) {
                least = area;
                chosen = box;
                chosen_down = places[c].down;
            }
            if (area == 0) {
                break;
            }
        }
        taken.push_back(chosen);
        dc.SetTextColor(lobe_colour(static_cast<int>(index)));
        if (chosen_down && down_font.GetSafeHandle() != nullptr) {
            // Turned a quarter clockwise, the text's top left is the box's
            // top right.
            CFont* previous = dc.SelectObject(&down_font);
            dc.TextOut(chosen.right, chosen.top, name);
            dc.SelectObject(previous);
        } else {
            dc.TextOut(chosen.left, chosen.top, name);
        }
    }
    // One neuron's dendrites, for the neuron pointed at (else the one
    // followed): a line to the spot each rule's first dendrite was placed on,
    // that spot framed, the cells the rest could land on outlined (dotted
    // where they have since been free to move), the ones of those firing
    // now framed, and a dot running in along the line while the spot fires.
    const int reach_lobe = hover_lobe_ >= 0 ? hover_lobe_ : followed_lobe_;
    const int reach_neuron = hover_lobe_ >= 0 ? hover_neuron_ : followed_neuron_;
    if (wiring_shown() && reach_lobe >= 0 && reach_lobe < static_cast<int>(wiring_.size()) &&
        reach_lobe < static_cast<int>(lobes.size())) {
        const c1kit::LobeLayout& own = lobes[static_cast<std::size_t>(reach_lobe)];
        const bool counted =
            reach_lobe == followed_lobe_ && reach_neuron == followed_neuron_ && followed_valid_;
        const int own_width = (std::max)(1, own.width);
        const CPoint from((cell_x(own.x + reach_neuron % own_width) +
                           cell_x(own.x + reach_neuron % own_width + 1)) / 2,
                          (cell_y(own.y + reach_neuron / own_width) +
                           cell_y(own.y + reach_neuron / own_width + 1)) / 2);
        for (int r = 0; r < 2; ++r) {
            const c1kit::DendriteRule& rule = wiring_[static_cast<std::size_t>(reach_lobe)].rules[r];
            if (rule.most == 0 || rule.source >= static_cast<int>(lobes.size())) continue;
            const c1kit::LobeLayout& source = lobes[static_cast<std::size_t>(rule.source)];
            c1kit::DendriteReach reach;
            if (!c1kit::dendrite_reach(reach_neuron, own.neurons(), source.width, source.height,
                                       rule.spread, counted ? followed_values_[r].dendrites
                                                            : rule.most,
                                       reach)) {
                continue;
            }
            const COLORREF colour = r == 0 ? RGB(255, 255, 255) : RGB(255, 190, 80);
            CBrush brush(colour);
            // Where the rest could land.
            if (reach.right > reach.left || reach.bottom > reach.top) {
                const CRect region(cell_x(source.x + reach.left) - 2, cell_y(source.y + reach.top) - 2,
                                   cell_x(source.x + reach.right + 1) + 2,
                                   cell_y(source.y + reach.bottom + 1) + 2);
                CPen pen(rule.mode != 0 ? PS_DOT : PS_SOLID, 1, blend(kBackground, colour, 70));
                CPen* previous_pen = dc.SelectObject(&pen);
                CGdiObject* previous_brush = dc.SelectStockObject(NULL_BRUSH);
                dc.Rectangle(region);
                dc.SelectObject(previous_brush);
                dc.SelectObject(previous_pen);
                for (int y = reach.top; y <= reach.bottom; ++y) {
                    for (int x = reach.left; x <= reach.right; ++x) {
                        if (firing_.reported[source.x + x][source.y + y]) {
                            CBrush firing(blend(kBackground, colour, 80));
                            dc.FrameRect(CRect(cell_x(source.x + x), cell_y(source.y + y),
                                               cell_x(source.x + x + 1), cell_y(source.y + y + 1)),
                                         &firing);
                        }
                    }
                }
            }
            // The spot, and the line to it.
            const int sx = source.x + reach.spot_x;
            const int sy = source.y + reach.spot_y;
            CRect spot(cell_x(sx) - 1, cell_y(sy) - 1, cell_x(sx + 1) + 2, cell_y(sy + 1) + 2);
            dc.FrameRect(spot, &brush);
            spot.DeflateRect(1, 1);
            dc.FrameRect(spot, &brush);
            const CPoint to = spot.CenterPoint();
            CPen line(PS_SOLID, 1, colour);
            CPen* previous_pen = dc.SelectObject(&line);
            dc.MoveTo(to);
            dc.LineTo(from);
            dc.SelectObject(previous_pen);
            if (firing_.reported[sx][sy]) {
                double t = pulse_frame_ * 0.1;
                t -= std::floor(t);
                const int x = to.x + int((from.x - to.x) * t);
                const int y = to.y + int((from.y - to.y) * t);
                dc.FillSolidRect(x - 2, y - 2, 5, 5, colour);
            }
        }
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
