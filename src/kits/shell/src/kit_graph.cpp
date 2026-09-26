// The shared chemical graph.  See kit_graph.hpp.

#include "c1kitshell/kit_graph.hpp"
#include "c1kitshell/kit_widgets.hpp"

#include <algorithm>

namespace c1kitshell {
namespace {

constexpr int kStep = 2;  // pixels per sample

CString elapsed(int seconds) {
    CString text;
    if (seconds >= 60) {
        text.Format(_T("-%d:%02d"), seconds / 60, seconds % 60);
    } else {
        text.Format(_T("-%ds"), seconds);
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
}

void ChemicalGraph::clear() {
    for (Series& s : series_) s.values.clear();
}

void ChemicalGraph::draw(CDC& dc, const CRect& rect, const std::vector<std::string>& names,
                         const CString& empty_message) const {
    dc.FillSolidRect(rect, RGB(255, 255, 255));
    const int left = rect.left + 34;
    const int top = rect.top + 8;
    const int right = rect.right - 8;
    const int bottom = rect.bottom - 22;
    if (right <= left || bottom <= top) return;
    dc.SetBkMode(TRANSPARENT);
    // Levels up the side.
    dc.SetTextColor(RGB(90, 90, 90));
    for (int level = 0; level <= 256; level += 32) {
        const int shown = level > 255 ? 255 : level;
        const int y = bottom - (bottom - top) * shown / 255;
        dc.FillSolidRect(left, y, right - left, 1, RGB(228, 228, 228));
        CString label;
        label.Format(_T("%d"), shown);
        dc.TextOut(rect.left + 2, y - 7, label);
    }
    // Time along the bottom, a mark every 60 samples.
    const int visible = (right - left) / kStep + 1;
    for (int sample = 60; sample < visible; sample += 60) {
        const int x = right - sample * kStep;
        dc.FillSolidRect(x, top, 1, bottom - top, RGB(238, 238, 238));
        dc.TextOut(x - 14, bottom + 4, elapsed(sample * milliseconds_per_sample_ / 1000));
    }
    dc.FillSolidRect(left, top, 1, bottom - top, RGB(90, 90, 90));
    dc.FillSolidRect(left, bottom, right - left, 1, RGB(90, 90, 90));
    dc.TextOut(right - 22, bottom + 4, _T("now"));

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
        dc.TextOut(left + 8, legend_y, elapsed(back * milliseconds_per_sample_ / 1000));
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
