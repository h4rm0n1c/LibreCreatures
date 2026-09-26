// The Health Kit's pages.  The original's are described in ../ORIGINAL.md;
// each page here says what it replaces.

#include "health.hpp"
#include "health_ids.hpp"

#include <algorithm>

namespace health {
namespace {

using c1kitshell::blend;

// The Fitness page's chemicals, in one query.
const int kFitnessChemicals[] = {
    c1kit::kChemicalColdness, c1kit::kChemicalHotness, c1kit::kChemicalCarbonDioxide,
    c1kit::kChemicalGlycogen, c1kit::kChemicalGlucose, c1kit::kChemicalStarch,
};
enum FitnessIndex { kColdness, kHotness, kCarbonDioxide, kGlycogen, kGlucose, kStarch };

CString text(const std::string& value) {
    return CString(value.c_str());
}

// A labelled bar: the label on the left, the bar, the value, and a word
// about it on the right.
void draw_reading(CDC& dc, const CRect& row, const CString& label, int value, int maximum,
                  COLORREF colour, const CString& note, const CString& value_text = CString()) {
    const int label_width = (std::min)(150, row.Width() / 3);
    const int note_width = (std::min)(150, row.Width() / 4);
    dc.SetTextColor(RGB(0, 0, 0));
    dc.TextOut(row.left, row.top + (row.Height() - 14) / 2, label);
    const CRect bar(row.left + label_width, row.top + 3, row.right - note_width - 44,
                    row.bottom - 3);
    dc.FillSolidRect(bar, RGB(255, 255, 255));
    const int length = maximum > 0 ? bar.Width() * (std::min)((std::max)(value, 0), maximum) / maximum : 0;
    dc.FillSolidRect(bar.left, bar.top, length, bar.Height(), colour);
    CBrush frame(RGB(150, 150, 150));
    dc.FrameRect(bar, &frame);
    CString number = value_text;
    if (number.IsEmpty()) {
        number.Format(_T("%d"), value);
    }
    dc.TextOut(bar.right + 6, row.top + (row.Height() - 14) / 2, number);
    dc.SetTextColor(RGB(90, 90, 90));
    dc.TextOut(row.right - note_width + 4, row.top + (row.Height() - 14) / 2, note);
}

void draw_heading(CDC& dc, int x, int y, const CString& heading) {
    dc.SetTextColor(RGB(20, 40, 90));
    dc.TextOut(x, y, heading);
}

void no_creature(CDC& dc, const CRect& rect) {
    dc.SetTextColor(RGB(110, 110, 110));
    dc.TextOut(rect.left + 12, rect.top + 12, _T("Select a creature in the game."));
}

} // namespace

// ===========================================================================
// Fitness
// ===========================================================================
//
// The 1996 page (dialog 138) showed a skeleton with a thermometer (hotness
// against coldness) and a heart monitor whose beat followed carbon dioxide
// and glycogen, with no numbers.  Here those readings are labelled bars,
// with the creature's life force, health and age from the game's overview
// (`getb ovvd`) and its other energy stores (glucose and starch).

FitnessPage::FitnessPage(HealthSheet& sheet) : HealthPage(sheet, kStringFitnessTab) {}

void FitnessPage::create_controls() {
    vitals_.create(*this, kControlVitals, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
}

void FitnessPage::layout(int width, int height) {
    place(vitals_, kMargin, kMargin, width - 2 * kMargin,
          height - 3 * kMargin - button_height());
    place_close(width, height);
}

void FitnessPage::subject_changed() {
    have_readings_ = false;
    vitals_.redraw();
}

void FitnessPage::poll() {
    const Subject& subject = sheet_.subject();
    std::string reply;
    if (sheet_.query("dde: getb ovvd,endm", reply)) {
        for (const c1kit::OverviewRecord& record : c1kit::parse_overview(reply, 64)) {
            if (record[c1kit::kOverviewMoniker] == subject.moniker) {
                life_force_ = c1kit::life_force_percent(record[c1kit::kOverviewLifeForce]);
                health_ = record[c1kit::kOverviewMedical];
                age_ = record[c1kit::kOverviewAge];
                pregnancy_ = record[c1kit::kOverviewPregnancy];
            }
        }
    }
    const std::vector<int> chemicals(std::begin(kFitnessChemicals), std::end(kFitnessChemicals));
    std::vector<int> values;
    if (sheet_.query(c1kit::chemical_levels_query(chemicals), reply) &&
        c1kit::parse_values(reply, chemicals.size(), values)) {
        chemicals_ = values;
        have_readings_ = true;
    }
    vitals_.redraw();
}

void FitnessPage::draw(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    dc.SetBkMode(TRANSPARENT);
    if (!sheet_.subject().present) {
        no_creature(dc, rect);
        return;
    }
    if (!have_readings_) {
        dc.TextOut(rect.left + 12, rect.top + 12, _T("Waiting for the game..."));
        return;
    }
    const int row = (std::max)(20, (std::min)(30, rect.Height() / 13));
    const int left = rect.left + 8;
    const int right = rect.right - 8;
    int y = rect.top + 6;
    const auto next = [&]() {
        const CRect r(left + 12, y, right, y + row);
        y += row;
        return r;
    };

    draw_heading(dc, left, y, _T("Overall"));
    y += row - 4;
    CString life;
    life.Format(_T("%d%%"), life_force_);
    draw_reading(dc, next(), _T("Life force"), life_force_, 100,
                 life_force_ >= 60 ? RGB(40, 170, 60) : life_force_ >= 30 ? RGB(230, 170, 0)
                                                                          : RGB(210, 40, 40),
                 _T(""), life);
    dc.SetTextColor(RGB(0, 0, 0));
    CString status = _T("Health: ") + text(health_) + _T("     Age: ") + text(age_);
    if (!pregnancy_.empty() && pregnancy_ != "N/A") {
        status += _T("     Pregnant: ") + text(pregnancy_);
    }
    dc.TextOut(left + 12, y + (row - 14) / 2, status);
    y += row + 6;

    // Temperature: coldness pulls one way, hotness the other.
    draw_heading(dc, left, y, _T("Temperature"));
    y += row - 4;
    const int cold = chemicals_[kColdness];
    const int hot = chemicals_[kHotness];
    {
        const CRect r = next();
        const int label_width = (std::min)(150, r.Width() / 3);
        const CRect scale(r.left + label_width, r.top + 3, r.right - (std::min)(150, r.Width() / 4) - 44,
                          r.bottom - 3);
        for (int x = scale.left; x < scale.right; ++x) {
            const int t = (x - scale.left) * 100 / (std::max)(1, scale.Width() - 1);
            const COLORREF c = t < 50 ? blend(RGB(60, 110, 255), RGB(235, 235, 235), t * 2)
                                      : blend(RGB(235, 235, 235), RGB(240, 60, 40), (t - 50) * 2);
            dc.FillSolidRect(x, scale.top, 1, scale.Height(), c);
        }
        CBrush frame(RGB(150, 150, 150));
        dc.FrameRect(scale, &frame);
        const int balance = hot - cold;  // -255 .. 255; the marker shows which wins
        const int marker = scale.left + scale.Width() / 2 + balance * (scale.Width() / 2) / 255;
        dc.FillSolidRect(marker - 2, scale.top - 2, 4, scale.Height() + 4, RGB(0, 0, 0));
        dc.SetTextColor(RGB(0, 0, 0));
        dc.TextOut(r.left, r.top + (r.Height() - 14) / 2, _T("Cold  /  hot"));
        dc.SetTextColor(RGB(90, 90, 90));
        dc.TextOut(r.right - (std::min)(150, r.Width() / 4) + 4, r.top + (r.Height() - 14) / 2,
                   balance < 0 ? _T("Feels cold") : balance > 0 ? _T("Feels hot") : _T("Neither"));
    }
    draw_reading(dc, next(), _T("Coldness"), cold, 255, RGB(60, 110, 255), _T(""));
    draw_reading(dc, next(), _T("Hotness"), hot, 255, RGB(240, 60, 40), _T(""));
    y += 6;

    draw_heading(dc, left, y, _T("Breathing and energy"));
    y += row - 4;
    const int co2 = chemicals_[kCarbonDioxide];
    draw_reading(dc, next(), _T("Carbon dioxide"), co2, 255, RGB(120, 120, 140), _T(""));
    draw_reading(dc, next(), _T("Glycogen (stored)"), chemicals_[kGlycogen], 255, RGB(200, 130, 30),
                 _T(""));
    draw_reading(dc, next(), _T("Glucose (in use)"), chemicals_[kGlucose], 255, RGB(230, 180, 40),
                 _T(""));
    draw_reading(dc, next(), _T("Starch (eaten)"), chemicals_[kStarch], 255, RGB(170, 140, 90),
                 _T(""));
}

// ===========================================================================
// Drives and needs
// ===========================================================================
//
// The 1996 page (dialog 139) read all sixteen drive chemicals but had
// gauges for five: pain, hunger, tiredness ("Exhaustion"), sleepiness and
// boredom.  Here every drive has a gauge, with its level, and the strongest
// is marked.

DrivesPage::DrivesPage(HealthSheet& sheet) : HealthPage(sheet, kStringDrivesTab) {}

void DrivesPage::create_controls() {
    drives_.create(*this, kControlDrives, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
}

void DrivesPage::layout(int width, int height) {
    place(drives_, kMargin, kMargin, width - 2 * kMargin, height - 3 * kMargin - button_height());
    place_close(width, height);
}

void DrivesPage::subject_changed() {
    levels_.clear();
    drives_.redraw();
}

void DrivesPage::poll() {
    std::vector<int> chemicals;
    for (int i = 0; i < c1kit::kDriveCount; ++i) {
        chemicals.push_back(c1kit::kFirstDrive + i);
    }
    std::string reply;
    std::vector<int> values;
    if (sheet_.query(c1kit::chemical_levels_query(chemicals), reply) &&
        c1kit::parse_values(reply, chemicals.size(), values)) {
        levels_ = values;
    }
    drives_.redraw();
}

void DrivesPage::draw(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    dc.SetBkMode(TRANSPARENT);
    if (!sheet_.subject().present) {
        no_creature(dc, rect);
        return;
    }
    if (levels_.empty()) {
        dc.TextOut(rect.left + 12, rect.top + 12, _T("Waiting for the game..."));
        return;
    }
    int strongest = 0;
    for (std::size_t i = 1; i < levels_.size(); ++i) {
        if (levels_[i] > levels_[static_cast<std::size_t>(strongest)]) {
            strongest = static_cast<int>(i);
        }
    }
    const std::string strongest_name =
        c1kit::chemical_label(sheet_.chemical_names(), c1kit::kFirstDrive + strongest);
    dc.SetTextColor(RGB(20, 40, 90));
    CString summary = levels_[static_cast<std::size_t>(strongest)] > 0
                          ? _T("Feeling most: ") + text(strongest_name)
                          : CString(_T("No drive is felt at all."));
    dc.TextOut(rect.left + 8, rect.top + 6, summary);

    // One column, or two when there is room.
    const int count = static_cast<int>(levels_.size());
    const int columns = rect.Width() >= 560 ? 2 : 1;
    const int per_column = (count + columns - 1) / columns;
    const int top = rect.top + 28;
    const int row = (std::max)(18, (std::min)(34, static_cast<int>(rect.bottom - top - 4) / per_column));
    const int column_width = (rect.Width() - 16) / columns;
    for (int i = 0; i < count; ++i) {
        const int column = i / per_column;
        const int x = rect.left + 8 + column * column_width;
        const int y = top + (i % per_column) * row;
        const CString name(c1kit::chemical_label(sheet_.chemical_names(), c1kit::kFirstDrive + i).c_str());
        const int level = levels_[static_cast<std::size_t>(i)];
        const int label_width = 120;
        dc.SetTextColor(i == strongest && level > 0 ? RGB(170, 0, 0) : RGB(0, 0, 0));
        dc.TextOut(x, y + (row - 14) / 2, name);
        // The 1996 gauge: green, yellow, red.
        const CRect gauge(x + label_width, y + 4, x + column_width - 48, y + row - 4);
        for (int px = gauge.left; px < gauge.right; ++px) {
            const int t = (px - gauge.left) * 100 / (std::max)(1, gauge.Width() - 1);
            const COLORREF c = t < 50 ? blend(RGB(90, 170, 60), RGB(250, 225, 40), t * 2)
                                      : blend(RGB(250, 225, 40), RGB(200, 40, 30), (t - 50) * 2);
            dc.FillSolidRect(px, gauge.top, 1, gauge.Height(), blend(c, RGB(255, 255, 255), 55));
        }
        const int filled = gauge.Width() * (std::min)(255, (std::max)(0, level)) / 255;
        for (int px = gauge.left; px < gauge.left + filled; ++px) {
            const int t = (px - gauge.left) * 100 / (std::max)(1, gauge.Width() - 1);
            const COLORREF c = t < 50 ? blend(RGB(90, 170, 60), RGB(250, 225, 40), t * 2)
                                      : blend(RGB(250, 225, 40), RGB(200, 40, 30), (t - 50) * 2);
            dc.FillSolidRect(px, gauge.top, 1, gauge.Height(), c);
        }
        CBrush frame(RGB(130, 130, 130));
        dc.FrameRect(gauge, &frame);
        CString number;
        number.Format(_T("%d"), level);
        dc.SetTextColor(RGB(0, 0, 0));
        dc.TextOut(gauge.right + 6, y + (row - 14) / 2, number);
    }
}

// ===========================================================================
// Brain activity
// ===========================================================================
//
// The 1996 page (dialog 134) lit up the busiest part of a cartoon brain
// (Lobes.bmp, Lobe_<n>.spr) from the brain report.  Here each lobe is a row:
// its name, what it does, and how much of it is active.

BrainPage::BrainPage(HealthSheet& sheet) : HealthPage(sheet, kStringBrainTab) {}

void BrainPage::create_controls() {
    lobes_.create(*this, kControlLobes, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
}

void BrainPage::layout(int width, int height) {
    place(lobes_, kMargin, kMargin, width - 2 * kMargin, height - 3 * kMargin - button_height());
    place_close(width, height);
}

void BrainPage::subject_changed() {
    have_report_ = false;
    lobes_.redraw();
}

void BrainPage::poll() {
    std::string reply;
    if (sheet_.brain_report(c1kit::kReportActivation, reply)) {
        c1kit::parse_activity_report(reply, activity_);
        have_report_ = true;
    }
    lobes_.redraw();
}

void BrainPage::draw(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    dc.SetBkMode(TRANSPARENT);
    const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
    if (!sheet_.subject().present || lobes.empty()) {
        no_creature(dc, rect);
        return;
    }
    // How active each lobe is: the share of its neurons the report shows, and
    // how strongly.
    std::vector<int> active(lobes.size(), 0);
    std::vector<int> strength(lobes.size(), 0);
    for (std::size_t i = 0; i < lobes.size(); ++i) {
        const c1kit::LobeLayout& lobe = lobes[i];
        for (int y = lobe.y; y < lobe.y + lobe.height && y < c1kit::kBrainGridSize; ++y) {
            for (int x = lobe.x; x < lobe.x + lobe.width && x < c1kit::kBrainGridSize; ++x) {
                const int level = activity_.level[x][y];
                if (level > 0) {
                    ++active[i];
                    strength[i] += level;
                }
            }
        }
    }
    int busiest = -1;
    int busiest_share = 0;
    for (std::size_t i = 0; i < lobes.size(); ++i) {
        const int share = lobes[i].neurons() > 0 ? active[i] * 100 / lobes[i].neurons() : 0;
        if (share > busiest_share) {
            busiest_share = share;
            busiest = static_cast<int>(i);
        }
    }
    const int row = (std::max)(30, (std::min)(46, (rect.Height() - 8) / static_cast<int>(lobes.size())));
    const int label_width = 130;
    for (std::size_t i = 0; i < lobes.size(); ++i) {
        const int y = rect.top + 4 + static_cast<int>(i) * row;
        const int share = lobes[i].neurons() > 0 ? active[i] * 100 / lobes[i].neurons() : 0;
        const COLORREF colour = c1kitshell::lobe_colour(static_cast<int>(i));
        dc.FillSolidRect(rect.left + 6, y + 4, 10, row - 10, colour);
        dc.SetTextColor(static_cast<int>(i) == busiest ? RGB(170, 0, 0) : RGB(0, 0, 0));
        dc.TextOut(rect.left + 22, y + 2, CString(c1kit::lobe_name(static_cast<int>(i))));
        dc.SetTextColor(RGB(100, 100, 100));
        dc.TextOut(rect.left + 22, y + row / 2, CString(c1kit::lobe_description(static_cast<int>(i))));
        const CRect bar(rect.left + label_width + 200 > rect.right - 80 ? rect.left + label_width
                                                                         : rect.right - 230,
                        y + 4, rect.right - 60, y + row / 2 - 1);
        dc.FillSolidRect(bar, RGB(255, 255, 255));
        dc.FillSolidRect(bar.left, bar.top, bar.Width() * share / 100, bar.Height(),
                         blend(colour, RGB(0, 0, 0), 20));
        CBrush frame(RGB(150, 150, 150));
        dc.FrameRect(bar, &frame);
        CString value;
        value.Format(_T("%d%%"), share);
        dc.SetTextColor(RGB(0, 0, 0));
        dc.TextOut(bar.right + 6, y + 2, value);
    }
    if (!have_report_) {
        dc.SetTextColor(RGB(110, 110, 110));
        dc.TextOut(rect.left + 12, rect.bottom - 20, _T("Waiting for the game..."));
    }
}

// ===========================================================================
// Doctor's page
// ===========================================================================
//
// The 1996 page (dialog 142) showed one shop item at a time in a picture
// frame (Shop.bmp), stepped through with arrows, with its name, how many are
// left, and what it does; the middle button ran its CAOS and took one off
// the count.  Here every item is in a list, the selected one shown large with
// its details, and "Put one in the world" does the same.

BEGIN_MESSAGE_MAP(DoctorPage, HealthPage)
    ON_LBN_SELCHANGE(kControlShopList, &DoctorPage::OnSelectionChanged)
    ON_BN_CLICKED(kControlShopAdd, &DoctorPage::OnAddToWorld)
END_MESSAGE_MAP()

DoctorPage::DoctorPage(HealthSheet& sheet) : HealthPage(sheet, kStringDoctorTab) {}

void DoctorPage::create_controls() {
    make(items_, _T("LISTBOX"), _T(""), LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_TABSTOP,
         kControlShopList);
    picture_.create(*this, kControlShopPicture,
                    [this](CDC& dc, const CRect& rect) { draw_picture(dc, rect); });
    make(add_, _T("BUTTON"), _T("Put one in the world"), BS_PUSHBUTTON | WS_TABSTOP,
         kControlShopAdd);
    make(status_, _T("STATIC"), _T(""), SS_LEFT, kControlShopStatus);
    fill_list();
    items_.SetCurSel(sheet_.shop().empty() ? -1 : 0);
    show_selected();
}

void DoctorPage::layout(int width, int height) {
    const int list_width = (std::min)(200, width / 3);
    const int bottom = height - 2 * kMargin - button_height();
    place(items_, kMargin, kMargin, list_width, bottom - kMargin);
    const int left = 2 * kMargin + list_width;
    place(picture_, left, kMargin, width - left - kMargin, bottom - 3 * kMargin - 2 * button_height());
    place(add_, left, bottom - 2 * button_height() - kMargin, 160, button_height());
    place(status_, left, bottom - button_height(), width - left - kMargin, text_height());
    place_close(width, height);
}

void DoctorPage::fill_list() {
    const int selected = items_.GetCurSel();
    items_.ResetContent();
    for (const c1kit::ShopItem& item : sheet_.shop()) {
        CString line;
        line.Format(_T("%s  (%d left)"), text(item.name).GetString(), item.quantity);
        items_.AddString(line);
    }
    if (selected >= 0 && selected < items_.GetCount()) {
        items_.SetCurSel(selected);
    }
}

void DoctorPage::subject_changed() {
    if (created_) {
        show_selected();
    }
}

void DoctorPage::OnSelectionChanged() {
    status_.SetWindowText(_T(""));
    show_selected();
}

void DoctorPage::show_selected() {
    const int index = items_.GetCurSel();
    const bool valid = index >= 0 && index < static_cast<int>(sheet_.shop().size());
    add_.EnableWindow(valid && sheet_.shop()[static_cast<std::size_t>(index)].quantity > 0);
    picture_.redraw();
}

void DoctorPage::draw_picture(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, RGB(24, 24, 28));
    dc.SetBkMode(TRANSPARENT);
    const int index = items_.GetSafeHwnd() != nullptr ? items_.GetCurSel() : -1;
    if (index < 0 || index >= static_cast<int>(sheet_.shop().size())) {
        dc.SetTextColor(RGB(200, 200, 200));
        dc.TextOut(rect.left + 12, rect.top + 12,
                   sheet_.shop().empty() ? _T("The shop file (Health) could not be read.")
                                         : _T("Pick something from the list."));
        return;
    }
    const c1kit::ShopItem& item = sheet_.shop()[static_cast<std::size_t>(index)];
    // The picture, scaled up to fit, above its name and description.
    const int text_space = 60;
    const c1kit::PhotoBitmap& picture = item.picture;
    if (picture.width > 0 && picture.height > 0 &&
        canvas_.create(picture.width, picture.height)) {
        canvas_.fill(0);
        canvas_.draw_indexed(picture.pixels.data(), picture.width, picture.height, picture.stride,
                             true, 0, 0, sheet_.palette());
        const int scale = (std::max)(1, (std::min)((rect.Width() - 20) / picture.width,
                                                   (rect.Height() - text_space - 20) / picture.height));
        const int w = picture.width * scale;
        const int h = picture.height * scale;
        CDC source;
        source.CreateCompatibleDC(&dc);
        CBitmap bitmap;
        bitmap.CreateCompatibleBitmap(&dc, picture.width, picture.height);
        CBitmap* previous = source.SelectObject(&bitmap);
        canvas_.present(source, 0, 0, picture.width, picture.height);
        dc.SetStretchBltMode(COLORONCOLOR);
        dc.StretchBlt(rect.left + (rect.Width() - w) / 2, rect.top + 10, w, h, &source, 0, 0,
                      picture.width, picture.height, SRCCOPY);
        source.SelectObject(previous);
    }
    CRect words(rect.left + 10, rect.bottom - text_space, rect.right - 10, rect.bottom - 4);
    dc.SetTextColor(RGB(255, 255, 255));
    CString name = text(item.name);
    CString count;
    count.Format(_T("   (%d left)"), item.quantity);
    dc.DrawText(name + count, CRect(words.left, words.top, words.right, words.top + 20),
                DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    dc.SetTextColor(RGB(200, 200, 200));
    dc.DrawText(text(item.description), CRect(words.left, words.top + 22, words.right, words.bottom),
                DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
}

// SubmitSelectedHealthValue @ 0x00405c80: run the item's CAOS (which makes
// the object and puts it in the pointer's hand), then take one off.
void DoctorPage::OnAddToWorld() {
    const int index = items_.GetCurSel();
    if (index < 0 || index >= static_cast<int>(sheet_.shop().size())) {
        return;
    }
    c1kit::ShopItem& item = sheet_.shop()[static_cast<std::size_t>(index)];
    if (item.quantity <= 0) {
        return;
    }
    std::string reply;
    if (!sheet_.query(item.command, reply)) {
        status_.SetWindowText(_T("The game did not take it."));
        return;
    }
    --item.quantity;
    sheet_.save_shop();
    fill_list();
    show_selected();
    status_.SetWindowText(text(item.name) + _T(" is on the pointer: click in the world to drop it."));
}

} // namespace health
