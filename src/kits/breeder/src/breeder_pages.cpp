// Fertility: the selected creature's sex hormones and pregnancy.
//
// The 1996 page (CFertilityPage, dialog 138) asked for five chemicals every
// second (CBreedersKitPage8A::RefreshSelectedCreatureBreedingStatus
// @ 0x00409140): for a female oestrogen, sex drive, progesterone,
// gonadotrophin and glycogen; for a male testosterone in place of oestrogen
// and glycogen in place of progesterone.  It drew three unnumbered gauges
// ("Sex Drive", "Health", "Fertility"), a pregnancy silhouette (crossed out
// for a male) and a trace of oestrogen or testosterone.
//
// Here the same chemicals are labelled bars with their levels, the
// creature's pregnancy, age and life force come from the game's overview
// (`getb ovvd`), and all of them are graphed over the last minutes.

#include "breeder.hpp"
#include "breeder_ids.hpp"

#include <algorithm>

namespace breeder {
namespace {

using c1kitshell::blend;
using c1kitshell::series_colour;

constexpr int kOestrogen = 63;
constexpr int kTestosterone = 64;
constexpr int kGonadotrophin = 65;
constexpr int kProgesterone = 66;
constexpr int kSexDrive = 13;
constexpr int kGlycogen = 59;
constexpr std::size_t kHistory = 600;  // samples (a second each)

CString text(const std::string& value) {
    return CString(value.c_str());
}

} // namespace

FertilityPage::FertilityPage(BreederSheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringFertilityTab), sheet_(sheet) {}

void FertilityPage::create_controls() {
    view_.create(*this, kControlFertility, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
}

void FertilityPage::layout(int width, int height) {
    place(view_, kMargin, kMargin, width - 2 * kMargin, height - 3 * kMargin - button_height());
    place_close(width, height);
}

// The chemicals the 1996 page asked for, for this creature's sex.
void FertilityPage::subject_changed() {
    const Subject& subject = sheet_.subject();
    if (subject.sex == 2) {
        chemicals_ = {kOestrogen, kProgesterone, kGonadotrophin, kSexDrive, kGlycogen};
    } else {
        chemicals_ = {kTestosterone, kGonadotrophin, kSexDrive, kGlycogen};
    }
    history_.assign(chemicals_.size(), std::deque<int>());
    have_overview_ = false;
    view_.redraw();
}

void FertilityPage::poll() {
    const Subject& subject = sheet_.subject();
    if (!subject.present || chemicals_.empty()) {
        return;
    }
    std::string reply;
    if (sheet_.query("dde: getb ovvd,endm", reply)) {
        for (const c1kit::OverviewRecord& record : c1kit::parse_overview(reply, 64)) {
            if (record[c1kit::kOverviewMoniker] == subject.moniker) {
                life_force_ = c1kit::life_force_percent(record[c1kit::kOverviewLifeForce]);
                age_ = record[c1kit::kOverviewAge];
                pregnancy_ = record[c1kit::kOverviewPregnancy];
                have_overview_ = true;
            }
        }
    }
    std::vector<int> values;
    if (sheet_.query(c1kit::chemical_levels_query(chemicals_), reply) &&
        c1kit::parse_values(reply, chemicals_.size(), values)) {
        for (std::size_t i = 0; i < values.size() && i < history_.size(); ++i) {
            history_[i].push_back(values[i]);
            if (history_[i].size() > kHistory) {
                history_[i].pop_front();
            }
        }
    }
    if (GetSafeHwnd() != nullptr && IsWindowVisible()) {
        view_.redraw();
    }
}

void FertilityPage::draw(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    dc.SetBkMode(TRANSPARENT);
    const Subject& subject = sheet_.subject();
    if (!subject.present) {
        dc.SetTextColor(RGB(110, 110, 110));
        dc.TextOut(rect.left + 12, rect.top + 12, _T("Select a creature in the game."));
        return;
    }
    // Who: the 1996 kit's female or male icon, and the overview.
    const HICON icon = AfxGetApp()->LoadIcon(subject.sex == 2 ? kIconFemale : kIconMale);
    dc.DrawIcon(rect.left + 8, rect.top + 6, icon);
    dc.SetTextColor(RGB(0, 0, 0));
    CString who = text(subject.name) + (subject.sex == 2 ? _T(", female") : _T(", male"));
    if (have_overview_) {
        CString rest;
        rest.Format(_T("    age %s    life force %d%%"), text(age_).GetString(), life_force_);
        who += rest;
    }
    dc.TextOut(rect.left + 48, rect.top + 8, who);
    CString pregnancy;
    if (subject.sex != 2) {
        pregnancy = _T("Males cannot become pregnant.");
    } else if (!have_overview_) {
        pregnancy = _T("");
    } else if (pregnancy_ == "No" || pregnancy_.empty()) {
        pregnancy = _T("Not pregnant.");
    } else {
        pregnancy = _T("Pregnant: ") + text(pregnancy_);
    }
    dc.SetTextColor(subject.sex == 2 && pregnancy_ != "No" && !pregnancy_.empty() ? RGB(170, 0, 90)
                                                                                    : RGB(60, 60, 60));
    dc.TextOut(rect.left + 48, rect.top + 26, pregnancy);

    // Each chemical now.
    const int row = 22;
    int y = rect.top + 52;
    const int label_width = 130;
    for (std::size_t i = 0; i < chemicals_.size(); ++i) {
        const int value = history_[i].empty() ? 0 : history_[i].back();
        const COLORREF colour = series_colour(static_cast<int>(i));
        dc.FillSolidRect(rect.left + 8, y + 7, 10, 8, colour);
        dc.SetTextColor(RGB(0, 0, 0));
        dc.TextOut(rect.left + 24, y + 3,
                   CString(c1kit::chemical_label(sheet_.chemical_names(), chemicals_[i]).c_str()));
        const CRect bar(rect.left + 24 + label_width, y + 4, rect.right - 50, y + row - 4);
        dc.FillSolidRect(bar, RGB(255, 255, 255));
        dc.FillSolidRect(bar.left, bar.top, bar.Width() * (std::min)(255, (std::max)(0, value)) / 255,
                         bar.Height(), colour);
        CBrush frame(RGB(150, 150, 150));
        dc.FrameRect(bar, &frame);
        CString number;
        number.Format(_T("%d"), value);
        dc.TextOut(bar.right + 6, y + 3, number);
        y += row;
    }
    draw_graph(dc, CRect(rect.left + 8, y + 10, rect.right - 8, rect.bottom - 6));
}

// The last minutes of every chemical above, newest at the right.
void FertilityPage::draw_graph(CDC& dc, const CRect& rect) {
    if (rect.Height() < 40) {
        return;
    }
    dc.FillSolidRect(rect, RGB(255, 255, 255));
    CBrush frame(RGB(150, 150, 150));
    dc.FrameRect(rect, &frame);
    const CRect plot(rect.left + 4, rect.top + 4, rect.right - 4, rect.bottom - 18);
    for (int level = 64; level < 256; level += 64) {
        const int y = plot.bottom - plot.Height() * level / 255;
        dc.FillSolidRect(plot.left, y, plot.Width(), 1, RGB(230, 230, 230));
    }
    dc.SetTextColor(RGB(110, 110, 110));
    dc.TextOut(rect.left + 6, rect.bottom - 16, _T("the last minutes"));
    dc.TextOut(rect.right - 30, rect.bottom - 16, _T("now"));
    const int step = 2;
    for (std::size_t s = 0; s < history_.size(); ++s) {
        const std::deque<int>& values = history_[s];
        if (values.empty()) {
            continue;
        }
        CPen pen(PS_SOLID, 2, series_colour(static_cast<int>(s)));
        CPen* previous = dc.SelectObject(&pen);
        const int visible = (std::min)(static_cast<int>(values.size()), plot.Width() / step + 1);
        for (int i = 0; i < visible; ++i) {
            const int value = (std::min)(255, (std::max)(0, values[values.size() - 1 - static_cast<std::size_t>(i)]));
            const int x = plot.right - i * step;
            const int y = plot.bottom - plot.Height() * value / 255;
            if (i == 0) {
                dc.MoveTo(x, y);
            } else {
                dc.LineTo(x, y);
            }
        }
        dc.SelectObject(previous);
    }
}

} // namespace breeder
