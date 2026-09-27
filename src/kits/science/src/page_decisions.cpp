// Decisions: the decision lobe.
//
// The 1996 page (CDecisionPage, dialog 135) drew a bar per action named in
// decision.str, and two for the reward and punishment echo chemicals (54 and
// 55, with icons 181 and 182), from one query: `putv _it_`, both chemicals,
// then `cell 6 n 0` for sixteen neurons.  It showed one of the seven values
// `cell` answers, with no numbers.
//
// Here the bars have their values, the action with the strongest output
// (what the creature is doing) is marked, and any of the seven values can be
// shown.

#include "science.hpp"
#include "science_ids.hpp"

#include <algorithm>

namespace science {
namespace {

// What `dde: cell` answers, in order.
const TCHAR* const kValueNames[] = {
    _T("Output (firing strength)"),
    _T("Activation"),
    _T("Dendrites"),
    _T("Sum of dendrite weights"),
    _T("Sum of target weights"),
    _T("Sum of baseline weights"),
    _T("Sum of dendrite states"),
};

constexpr int kDecisionNeurons = 16;

} // namespace

BEGIN_MESSAGE_MAP(DecisionsPage, SciencePage)
    ON_CBN_SELCHANGE(kControlDecisionValue, &DecisionsPage::OnValueChanged)
END_MESSAGE_MAP()

DecisionsPage::DecisionsPage(ScienceSheet& sheet) : SciencePage(sheet, kStringDecisionsTab) {}

void DecisionsPage::create_controls() {
    bars_.create(*this, kControlDecisionBars,
                 [this](CDC& dc, const CRect& rect) { draw_bars(dc, rect); });
    make(value_label_, _T("STATIC"), _T("Show:"), SS_LEFT, kControlHint);
    make(value_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
         kControlDecisionValue);
    for (const TCHAR* name : kValueNames) {
        value_.AddString(name);
    }
    value_.SetCurSel(0);
    reward_.LoadBitmap(kBitmapReward);
    punishment_.LoadBitmap(kBitmapPunishment);
}

void DecisionsPage::layout(int width, int height) {
    const int margin = 7;
    const int row = text_height() + 8;
    const int bottom = height - margin - row;
    place(bars_, margin, margin, width - 2 * margin, bottom - 2 * margin);
    place(value_label_, margin, bottom + 4, 36, text_height());
    place(value_, margin + 38, bottom, 200, 200);
    place(close_, width - margin - 84, bottom, 84, row);
}

void DecisionsPage::OnValueChanged() {
    bars_.redraw();
}

void DecisionsPage::poll() {
    std::string reply;
    if (!sheet_.query(c1kit::decisions_query(kDecisionNeurons), reply)) {
        return;
    }
    std::vector<int> values;
    if (!c1kit::parse_values(reply, 3 + 7 * kDecisionNeurons, values)) {
        return;
    }
    reward_level_ = values[1];
    punishment_level_ = values[2];
    neurons_.assign(kDecisionNeurons, {});
    for (int n = 0; n < kDecisionNeurons; ++n) {
        for (int v = 0; v < 7; ++v) {
            neurons_[static_cast<std::size_t>(n)][static_cast<std::size_t>(v)] =
                values[static_cast<std::size_t>(3 + n * 7 + v)];
        }
    }
    bars_.redraw();
}

void DecisionsPage::draw_bars(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    dc.SetBkMode(TRANSPARENT);
    const std::vector<std::string>& names = sheet_.neuron_names().decisions;
    const int value_index = value_.GetSafeHwnd() != nullptr ? (std::max)(0, value_.GetCurSel()) : 0;
    const int rows = static_cast<int>(names.size()) + 3;
    const int row_height = (std::max)(16, (std::min)(30, rect.Height() / (rows > 0 ? rows : 1)));
    const int label_width = 110;
    const int left = rect.left + label_width;
    const int right = rect.right - 60;
    if (neurons_.empty()) {
        dc.SetTextColor(RGB(90, 90, 90));
        dc.TextOut(rect.left + 8, rect.top + 8,
                   sheet_.subject().present ? _T("Waiting for the game...")
                                            : _T("Select a creature in the game."));
        return;
    }
    // The scale: 0-255 for output and activation, else the largest value.
    int scale = 255;
    if (value_index >= 2) {
        scale = 1;
        for (const auto& neuron : neurons_) {
            scale = (std::max)(scale, neuron[static_cast<std::size_t>(value_index)]);
        }
    }
    int strongest = -1;
    int strongest_output = 0;
    for (std::size_t n = 0; n < names.size() && n < neurons_.size(); ++n) {
        if (neurons_[n][0] > strongest_output) {
            strongest_output = neurons_[n][0];
            strongest = static_cast<int>(n);
        }
    }
    const auto bar = [&](int y, const CString& label, int value, int maximum, COLORREF colour,
                         bool marked) {
        const int bar_height = row_height - 6;
        dc.SetTextColor(marked ? RGB(160, 0, 0) : RGB(0, 0, 0));
        dc.TextOut(rect.left + 6, y + (row_height - 14) / 2, label);
        dc.FillSolidRect(left, y + 3, right - left, bar_height, RGB(255, 255, 255));
        const int length = maximum > 0 ? (right - left) * (std::min)(value, maximum) / maximum : 0;
        dc.FillSolidRect(left, y + 3, length, bar_height, colour);
        CBrush frame(RGB(128, 128, 128));
        dc.FrameRect(CRect(left, y + 3, right, y + 3 + bar_height), &frame);
        CString number;
        number.Format(_T("%d"), value);
        dc.TextOut(right + 6, y + (row_height - 14) / 2, number);
    };
    int y = rect.top + 4;
    for (std::size_t n = 0; n < names.size() && n < neurons_.size(); ++n) {
        const bool marked = static_cast<int>(n) == strongest;
        bar(y, CString(names[n].c_str()) + (marked ? _T("  <") : _T("")),
            neurons_[n][static_cast<std::size_t>(value_index)], scale,
            marked ? RGB(200, 30, 30) : RGB(128, 0, 0), marked);
        y += row_height;
    }
    y += row_height / 2;
    // Reward and punishment (the learning chemicals), with the 1996 icons.
    const auto icon = [&](CBitmap& bitmap, int top) {
        CDC memory;
        memory.CreateCompatibleDC(&dc);
        CBitmap* previous = memory.SelectObject(&bitmap);
        dc.BitBlt(rect.left + label_width - 24, top + (row_height - 17) / 2, 17, 17, &memory, 0, 0,
                  SRCCOPY);
        memory.SelectObject(previous);
    };
    bar(y, _T("Reward"), reward_level_, 255, RGB(0, 140, 0), false);
    icon(reward_, y);
    y += row_height;
    bar(y, _T("Punishment"), punishment_level_, 255, RGB(0, 0, 160), false);
    icon(punishment_, y);
}

} // namespace science
