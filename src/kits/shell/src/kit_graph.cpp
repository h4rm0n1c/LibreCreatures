// The shared chemical graph.  See kit_graph.hpp.

#include "c1kitshell/kit_graph.hpp"
#include "c1kitshell/kit_widgets.hpp"

#include <algorithm>

namespace c1kitshell {
namespace {

// CMonitorPage's geometry: four pixels a sample, a grid and ticks every 16
// pixels, a time label every 64 (16 samples).
constexpr int kStep = 4;
constexpr int kGrid = 16;
constexpr int kTimeLabelSamples = 16;
constexpr COLORREF kGridColour = RGB(220, 220, 220);  // its grid_pen, 0xdcdcdc
constexpr COLORREF kAxisColour = RGB(0, 0, 0);        // its graph_pen

// DrawGraphTimeLabels @ 0x00403f30: "%ds" under a minute, else "%d:%02d".
CString elapsed(int seconds) {
    CString text;
    if (seconds >= 60) {
        text.Format(_T("%d:%02d"), seconds / 60, seconds % 60);
    } else {
        text.Format(_T("%ds"), seconds);
    }
    return text;
}

} // namespace

std::string chemical_display_name(const std::vector<std::string>& names, int chemical) {
    if (chemical >= 0 && chemical < static_cast<int>(names.size())) {
        const std::string& name = names[static_cast<std::size_t>(chemical)];
        bool numeric = !name.empty();
        for (const char c : name) {
            numeric = numeric && (c >= '0' && c <= '9');
        }
        if (!name.empty() && !numeric && name != "<NONE>") {
            return name;
        }
    }
    return "Chemical " + std::to_string(chemical);
}

std::vector<int> ChemicalGraph::chemicals() const {
    std::vector<int> result;
    for (const Series& s : series_) result.push_back(s.chemical);
    return result;
}

int ChemicalGraph::index_of(int chemical) const {
    for (std::size_t i = 0; i < series_.size(); ++i) {
        if (series_[i].chemical == chemical) return static_cast<int>(i);
    }
    return -1;
}

void ChemicalGraph::set_chemicals(const std::vector<int>& chemicals) {
    std::vector<Series> kept;
    for (const int chemical : chemicals) {
        Series s;
        s.chemical = chemical;
        const int existing = index_of(chemical);
        if (existing >= 0) s.values = std::move(series_[static_cast<std::size_t>(existing)].values);
        kept.push_back(std::move(s));
    }
    series_ = std::move(kept);
}

void ChemicalGraph::add_sample(const std::vector<int>& levels) {
    for (std::size_t i = 0; i < series_.size() && i < levels.size(); ++i) {
        series_[i].values.push_back((std::min)(255, (std::max)(0, levels[i])));
        if (series_[i].values.size() > max_samples_) series_[i].values.pop_front();
    }
    if (!series_.empty()) ++total_samples_;
}

void ChemicalGraph::clear() {
    for (Series& s : series_) s.values.clear();
    total_samples_ = 0;
}

void ChemicalGraph::draw(CDC& dc, const CRect& rect, const std::vector<std::string>& names,
                         const CString& empty_message) const {
    dc.FillSolidRect(rect, RGB(255, 255, 255));
    // The plot: the view less 28 left, 6 top, 6 right and 32 below.
    const int left = rect.left + 0x1c;
    const int top = rect.top + 6;
    const int right = rect.right - 6;
    const int bottom = rect.bottom - 0x20;
    if (right - left < kGrid || bottom - top < kGrid) return;
    dc.SetBkMode(TRANSPARENT);

    // The grid: rows up from the axis, columns that move left with the
    // samples (the original scrolled its bitmap, grid and all).
    const int phase = static_cast<int>((total_samples_ * kStep) % kGrid);
    for (int y = bottom; y > top; y -= kGrid) {
        dc.FillSolidRect(left, y, right - left, 1, kGridColour);
    }
    for (int x = right - phase; x > left; x -= kGrid) {
        dc.FillSolidRect(x, top, 1, bottom - top, kGridColour);
    }

    // The axes, with a two-pixel tick every 16 pixels.
    dc.FillSolidRect(left, bottom, right - left, 1, kAxisColour);
    dc.FillSolidRect(left, top, 1, bottom - top + 1, kAxisColour);
    for (int x = left; x < right; x += kGrid) {
        dc.FillSolidRect(x, bottom, 1, 3, kAxisColour);
    }
    for (int y = bottom; y > top; y -= kGrid) {
        dc.FillSolidRect(left - 2, y, 2, 1, kAxisColour);
    }
    // 0, 64, 128, 192, 255, right-aligned against the axis.
    dc.SetTextColor(kAxisColour);
    const UINT previous_align = dc.SetTextAlign(TA_RIGHT | TA_TOP);
    for (const int level : {0, 64, 128, 192, 255}) {
        CString label;
        label.Format(_T("%d"), level);
        dc.TextOut(left - 3, bottom - level * (bottom - top) / 255 - 6, label);
    }
    // How long ago, every 16 samples, centred, none within 20 pixels of
    // either end.
    dc.SetTextAlign(TA_CENTER | TA_TOP);
    const int visible = (right - left) / kStep + 1;
    const long long newest = total_samples_ - 1;
    for (int back = 0; back < visible; ++back) {
        if ((newest - back) % kTimeLabelSamples != 0 || newest - back < 0) continue;
        const int x = right - back * kStep;
        if (x < left + 20 || x > right - 20) continue;
        dc.TextOut(x, bottom + 6, elapsed(back * milliseconds_per_sample_ / 1000));
    }
    dc.SetTextAlign(previous_align);

    for (std::size_t s = 0; s < series_.size(); ++s) {
        const std::deque<int>& values = series_[s].values;
        if (values.empty()) continue;
        CPen pen(PS_SOLID, 2, series_colour(static_cast<int>(s)));
        CPen* previous = dc.SelectObject(&pen);
        const int count = (std::min)(static_cast<int>(values.size()), visible);
        for (int i = 0; i < count; ++i) {
            const int value = values[values.size() - 1 - static_cast<std::size_t>(i)];
            const int x = right - i * kStep;
            const int y = bottom - (bottom - top) * value / 255;
            if (i == 0) dc.MoveTo(x, y); else dc.LineTo(x, y);
        }
        dc.SelectObject(previous);
    }

    // The moment under the pointer, or now.
    int back = 0;
    if (pointer_x_ >= left && pointer_x_ <= right) {
        back = (right - pointer_x_ + kStep / 2) / kStep;
        dc.FillSolidRect(right - back * kStep, top, 1, bottom - top, RGB(120, 120, 120));
    }
    int legend_y = top + 2;
    if (back > 0 && !series_.empty()) {
        dc.SetTextColor(RGB(60, 60, 60));
        dc.TextOut(left + 8, legend_y, elapsed(back * milliseconds_per_sample_ / 1000) + _T(" ago"));
        legend_y += 14;
    }
    for (std::size_t s = 0; s < series_.size(); ++s) {
        dc.FillSolidRect(left + 8, legend_y + 5, 12, 4, series_colour(static_cast<int>(s)));
        CString label(chemical_display_name(names, series_[s].chemical).c_str());
        const std::deque<int>& values = series_[s].values;
        if (static_cast<int>(values.size()) > back) {
            CString value;
            value.Format(_T("  %d"), values[values.size() - 1 - static_cast<std::size_t>(back)]);
            label += value;
        }
        dc.SetTextColor(RGB(40, 40, 40));
        dc.TextOut(left + 24, legend_y, label);
        legend_y += 14;
    }
    if (series_.empty() && !empty_message.IsEmpty()) {
        dc.SetTextColor(RGB(120, 120, 120));
        dc.TextOut(left + 12, (top + bottom) / 2, empty_message);
    }
}

} // namespace c1kitshell
