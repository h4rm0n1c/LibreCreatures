// The Health Kit's classic look: the 1996 pages (dialogs 138, 139 and 134)
// with their pictures drawn from the game's files as the original drew them.
// The Doctor's page is the shell's ClassicShopPage.  See health.hpp.

#include "health.hpp"
#include "health_ids.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace health {
namespace {

std::string main_file(const char* name) {
    return std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))) + name;
}

// An 8-bit .bmp file's palette indices, top row first.
bool read_bmp8(const std::string& path, int& width, int& height, std::vector<std::uint8_t>& pixels) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    if (bytes.size() < 54 || bytes[0] != 'B' || bytes[1] != 'M') return false;
    const auto u32 = [&](std::size_t at) {
        return static_cast<std::uint32_t>(bytes[at] | (bytes[at + 1] << 8) | (bytes[at + 2] << 16) |
                                          (bytes[at + 3] << 24));
    };
    const std::uint32_t offset = u32(10);
    const int w = static_cast<int>(u32(18));
    const int h = static_cast<int>(u32(22));
    const int bits = bytes[28] | (bytes[29] << 8);
    if (bits != 8 || w <= 0 || h <= 0) return false;
    const std::size_t stride = (static_cast<std::size_t>(w) + 3) & ~static_cast<std::size_t>(3);
    if (offset + stride * h > bytes.size()) return false;
    width = w;
    height = h;
    pixels.resize(static_cast<std::size_t>(w) * h);
    for (int row = 0; row < h; ++row) {
        std::memcpy(pixels.data() + static_cast<std::size_t>(row) * w,
                    bytes.data() + offset + (h - 1 - row) * stride, w);
    }
    return true;
}

// The 1996 Fitness page's geometry (CFitnessPage and its helpers).
constexpr int kTraceLeft = 0x1d;       // lobe_render_x_origin: the sweep's start
constexpr int kTraceWidth = 0xb7;      // it starts again once past this
constexpr int kTraceTop = 0x87;        // the monitor's rows
constexpr int kTraceBottom = 0xc3;
constexpr int kBaseline = 0xa5;
constexpr int kStep = 4;               // pixels a 50 ms tick
constexpr int kMercuryLeft = 0x37;     // the thermometer's tube
constexpr int kMercuryRight = 0xb1;
constexpr int kMercuryTop = 0x60;
constexpr int kMercuryBottom = 0x6a;
constexpr int kThermX = 0x1b, kThermY = 0x59;
constexpr int kHeartX = 199, kHeartY = 0x84;
constexpr int kEyeX = 0xaf, kEyeY = 0x1f;
// Palette indices: the trace's glow, line and bright spike, and the
// thermometer's empty tube and mercury.
constexpr std::uint8_t kDim = 0x1e, kLine = 0x56, kBright = 0xbc;
constexpr std::uint8_t kGlass = 0x22, kMercury = 0x7e;

// Lobes.bmp's six lit lobes (CScannerPage's constructor).
constexpr int kLobeX[6] = {0x12, 0xac, 199, 0x67, 0x19, 9};
constexpr int kLobeY[6] = {6, 0x19, 0x74, 0x7c, 0x95, 0x48};

}  // namespace

// ===========================================================================
// ClassicHealthPage
// ===========================================================================

namespace {
constexpr UINT kClassicClose = 1123;
constexpr UINT_PTR kPollTimer = 11;
constexpr UINT_PTR kAnimationTimer = 12;
}  // namespace

BEGIN_MESSAGE_MAP(ClassicHealthPage, CPropertyPage)
    ON_WM_TIMER()
    ON_BN_CLICKED(kClassicClose, &ClassicHealthPage::OnCloseKit)
END_MESSAGE_MAP()

ClassicHealthPage::ClassicHealthPage(HealthSheet& sheet, UINT dialog, UINT title_string, UINT poll_ms)
    : CPropertyPage(dialog), sheet_(sheet), poll_ms_(poll_ms) {
    title_ = c1kitshell::load_string(title_string);
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = title_;
}

BOOL ClassicHealthPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    init();
    return TRUE;
}

// The original's pages polled only while they showed.
BOOL ClassicHealthPage::OnSetActive() {
    poll();
    SetTimer(kPollTimer, poll_ms_, nullptr);
    if (animation_ms_ != 0) SetTimer(kAnimationTimer, animation_ms_, nullptr);
    return CPropertyPage::OnSetActive();
}

BOOL ClassicHealthPage::OnKillActive() {
    KillTimer(kPollTimer);
    KillTimer(kAnimationTimer);
    return CPropertyPage::OnKillActive();
}

void ClassicHealthPage::start_animation(UINT ms) {
    animation_ms_ = ms;
}

void ClassicHealthPage::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kPollTimer) {
        poll();
    } else if (timer_id == kAnimationTimer) {
        tick();
    } else {
        CPropertyPage::OnTimer(timer_id);
    }
}

void ClassicHealthPage::replace_picture(UINT control, c1kitshell::PaintedView& view,
                                        c1kitshell::PaintedView::Painter painter) {
    CWnd* picture = GetDlgItem(control);
    if (picture == nullptr) return;
    CRect area;
    picture->GetWindowRect(&area);
    ScreenToClient(&area);
    picture->ShowWindow(SW_HIDE);
    view.create(*this, 0x7f00 + control, std::move(painter));
    view.ModifyStyle(0, WS_CLIPSIBLINGS);
    view.SetWindowPos(&wndBottom, area.left, area.top, area.Width(), area.Height(), SWP_NOACTIVATE);
}

void ClassicHealthPage::OnCloseKit() {
    sheet_.request_game_quit();
}

// ===========================================================================
// Fitness page
// ===========================================================================
//
// InitializeHealthScannerSession @ 0x0040c590 and AdvanceScannerAnimation
// @ 0x0040db10.  Once a second the page reads carbon dioxide, glycogen,
// coldness and hotness; every 50 ms the monitor's trace moves on four
// pixels.  Carbon dioxide sets the pause between beats (34 ticks down to 8),
// glycogen the height of each beat (up to 25 pixels), and coldness against
// hotness the mercury.  A beat is a spike up, down past the baseline, a
// smaller one up and down, and back (AnimateHealthScannerTransition
// @ 0x0040d2c0); the line glows bright where it was just drawn and fades
// through two greens some 100 pixels behind (RenderLobeOverlay
// @ 0x0040d7a0).  The heart beats (Hearts.spr) and the eye blinks now and
// then (Blink.spr).

ClassicFitnessPage::ClassicFitnessPage(HealthSheet& sheet)
    : ClassicHealthPage(sheet, kDialogClassicFitness, kStringFitnessTab, 1000) {}

void ClassicFitnessPage::init() {
    loaded_ = read_bmp8(main_file("Skeleton.bmp"), width_, height_, skeleton_) &&
              read_bmp8(main_file("Therm.bmp"), therm_width_, therm_height_, therm_) &&
              hearts_.load(main_file("Hearts.spr")) && blink_.load(main_file("Blink.spr")) &&
              canvas_.create(width_, height_);
    frame_ = skeleton_;
    x_ = kTraceLeft;
    y_ = kBaseline;
    state_ = pending_ = 0x14;  // InitializeHealthScannerAnimation @ 0x0040c830
    mercury_x_ = kMercuryLeft;
    replace_picture(kControlClassicPicture, picture_, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
    start_animation(50);
}

void ClassicFitnessPage::poll() {
    std::string reply;
    std::vector<int> values;
    if (!sheet_.subject().present ||
        !sheet_.query("inst,dde: putv chem 62,dde: putv chem 59,dde: putv chem 4,dde: putv chem 5,endm",
                      reply) ||
        !c1kit::parse_values(reply, 4, values)) {
        have_data_ = false;
        return;
    }
    const auto clamp = [](int v) { return (std::max)(0, (std::min)(255, v)); };
    // ReadHealthScannerDdeResponse @ 0x0040d9b0 and its three tables.
    pending_ = ((0x1a00 - 0x1a * clamp(values[0])) >> 8) + 8;
    gap_ = (clamp(values[1]) * 0x1a) >> 8;
    const int half = (kMercuryRight - kMercuryLeft) >> 1;
    const int level = half - ((half * clamp(values[2])) >> 8) + ((half * clamp(values[3])) >> 8);
    // UpdateHealthLobeIndicator @ 0x0040ce50: hotter, the mercury runs
    // further from the mouth.
    mercury_x_ = level < 0 ? kMercuryRight : level < 0x80 ? kMercuryRight - level : 0x32;
    have_data_ = true;
    thermometer();
}

void ClassicFitnessPage::thermometer() {
    if (!loaded_) return;
    for (int y = kMercuryTop; y < kMercuryBottom; ++y) {
        for (int x = kMercuryLeft; x < kMercuryRight; ++x) {
            frame_[static_cast<std::size_t>(y) * width_ + x] = x < mercury_x_ ? kGlass : kMercury;
        }
    }
    for (int row = 0; row < therm_height_; ++row) {
        for (int column = 0; column < therm_width_; ++column) {
            const std::uint8_t index = therm_[static_cast<std::size_t>(row) * therm_width_ + column];
            const int x = kThermX + column, y = kThermY + row;
            if (index != 0 && x < width_ && y < height_) frame_[static_cast<std::size_t>(y) * width_ + x] = index;
        }
    }
}

// Behind the sweep, three strips of four columns fade a step each: bright to
// line to glow to black, then glow to black, then everything to black.
void ClassicFitnessPage::fade(int scan_x) {
    const int wrap = kTraceWidth + 1;
    const int strips[3] = {(scan_x < 0x61 ? scan_x + wrap : scan_x) - 0x60,
                           (scan_x < 0x69 ? scan_x + wrap : scan_x) - 0x68,
                           (scan_x > 0x70 ? scan_x : scan_x + wrap) - 0x70};
    for (int strip = 0; strip < 3; ++strip) {
        for (int column = strips[strip]; column < strips[strip] + 4; ++column) {
            if (column < 0 || column >= width_) continue;
            for (int y = kTraceTop; y < kTraceBottom; ++y) {
                std::uint8_t& pixel = frame_[static_cast<std::size_t>(y) * width_ + column];
                if (pixel != kDim && pixel != kLine && pixel != kBright) continue;
                if (strip == 2 || pixel == kDim) {
                    pixel = skeleton_[static_cast<std::size_t>(y) * width_ + column];
                } else if (pixel == kLine) {
                    pixel = kDim;
                } else if (strip == 0) {
                    pixel = kLine;
                }
            }
        }
    }
}

// RenderHealthLobeTransitionMarker @ 0x0040d560: four pixels of line with
// glow above and below.
void ClassicFitnessPage::flat() {
    fade(x_);
    for (int i = 0; i < kStep; ++i) {
        const int x = x_ + i;
        if (x >= width_) break;
        frame_[static_cast<std::size_t>(y_) * width_ + x] = kLine;
        frame_[static_cast<std::size_t>(y_ - 1) * width_ + x] = kDim;
        frame_[static_cast<std::size_t>(y_ + 1) * width_ + x] = kDim;
    }
}

// AnimateLobeOverlayUp/Down @ 0x0040d5d0/0x0040d680: a steep stroke to
// `target_y`, moving right up to four pixels on the way.
void ClassicFitnessPage::line_to(int target_y, bool up) {
    fade(x_);
    target_y = (std::max)(kTraceTop, (std::min)(kTraceBottom - 1, target_y));
    const int distance = up ? y_ - target_y : target_y - y_;
    int error = -(distance >> 1);
    int x = x_;
    const auto put = [&](int px, int py, std::uint8_t index) {
        if (px >= 0 && px < width_ && py >= kTraceTop && py < kTraceBottom) {
            frame_[static_cast<std::size_t>(py) * width_ + px] = index;
        }
    };
    put(x, y_, kBright);
    while (up ? target_y < y_ : y_ < target_y) {
        y_ += up ? -1 : 1;
        error += kStep;
        if (error >= 0) {
            ++x;
            error -= distance;
        }
        put(x, y_, kBright);
        put(up ? x + 1 : x - 1, y_, kLine);
    }
}

void ClassicFitnessPage::tick() {
    if (!loaded_) return;
    // The heart beats on its own count (frames at ticks 2, 6 and 11 of 29).
    if (heart_tick_ == 2 || heart_tick_ == 11) heart_frame_ = 0;
    if (heart_tick_ == 6) heart_frame_ = 1;
    if (heart_tick_ == 29) heart_tick_ = 0;
    ++heart_tick_;
    eye_frame_ = 0;
    if (have_data_) {
        if (x_ > kTraceWidth) x_ = kTraceLeft;  // ResetHealthLobeAnimation @ 0x0040d530
        switch (state_) {
        case 1:
            line_to(kBaseline, true);
            state_ = pending_;
            break;
        case 2:
            line_to(kBaseline + (gap_ >> 1), false);
            break;
        case 3:
            if (++blink_counter_ == 8) eye_frame_ = 1;
            if (blink_counter_ == 15) {
                eye_frame_ = 1;
                blink_counter_ = 0;
            }
            line_to(kBaseline - (gap_ >> 1), true);
            break;
        case 4:
            line_to(kBaseline + gap_, false);
            break;
        case 5:
            line_to(kBaseline - gap_, true);
            break;
        default:
            flat();
        }
        --state_;
        x_ += kStep;
        if (blink_counter_ == 9) eye_frame_ = 1;
    }
    picture_.redraw();
}

void ClassicFitnessPage::draw(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    if (!loaded_) return;
    const c1kitshell::GamePalette& palette = sheet_.palette();
    canvas_.draw_indexed(frame_.data(), width_, height_, width_, false, 0, 0, palette);
    canvas_.draw_frame_keyed(hearts_, heart_frame_, kHeartX, kHeartY, palette);
    canvas_.draw_frame_keyed(blink_, eye_frame_, kEyeX, kEyeY, palette);
    canvas_.present(dc, rect.left + (rect.Width() - width_) / 2, rect.top + (rect.Height() - height_) / 2,
                    width_, height_);
}

// ===========================================================================
// Drives and needs
// ===========================================================================
//
// CStatePage::InitializeChemicalControls @ 0x00403bf0: pain, hunger,
// tiredness ("Exhaustion"), sleepiness and boredom, each a Gauge.spr gauge
// (its bar, and its pointer drawn over it at level * (bar - 20) / 256 plus
// half the pointer: CGauge::InitializeHealthGaugeDisplay @ 0x0040de90),
// polled once a second, named in a large bold face.

namespace {
constexpr UINT kGaugeControls[5] = {1047, 1051, 1048, 1090, 1050};
constexpr UINT kLabelControls[5] = {1084, 1085, 1086, 1087, 1088};
constexpr int kGaugeChemicals[5] = {1, 3, 6, 7, 11};
constexpr UINT kLabelStrings[5] = {104, 105, 106, 107, 108};
}  // namespace

ClassicDrivesPage::ClassicDrivesPage(HealthSheet& sheet)
    : ClassicHealthPage(sheet, kDialogClassicDrives, kStringDrivesTab, 1000) {}

void ClassicDrivesPage::init() {
    gauge_.load(main_file("Gauge.spr"));
    // The 1996 labels: bold, about twice the page's text (a scalable face,
    // as the dialog font's bitmap one does not embolden at that size).
    LOGFONT face = {};
    GetFont()->GetLogFont(&face);
    face.lfHeight = face.lfHeight * 3 / 2;
    face.lfWeight = FW_BOLD;
    lstrcpy(face.lfFaceName, _T("Arial"));
    label_font_.CreateFontIndirect(&face);
    for (int i = 0; i < 5; ++i) {
        replace_picture(kGaugeControls[i], gauges_[i],
                        [this, i](CDC& dc, const CRect& rect) { draw_gauge(i, dc, rect); });
        if (CWnd* label = GetDlgItem(kLabelControls[i])) {
            label->SetWindowText(c1kitshell::load_string(kLabelStrings[i]));
            label->SetFont(&label_font_);
        }
    }
}

void ClassicDrivesPage::poll() {
    std::string reply;
    std::vector<int> values;
    const std::vector<int> chemicals(std::begin(kGaugeChemicals), std::end(kGaugeChemicals));
    if (sheet_.subject().present && sheet_.query(c1kit::chemical_levels_query(chemicals), reply) &&
        c1kit::parse_values(reply, chemicals.size(), values)) {
        for (int i = 0; i < 5; ++i) levels_[i] = (std::max)(0, (std::min)(255, values[static_cast<std::size_t>(i)]));
    }
    for (c1kitshell::PaintedView& view : gauges_) view.redraw();
}

void ClassicDrivesPage::draw_gauge(int index, CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    const c1kitshell::KitSprite::Frame* bar = gauge_.frame(0);
    const c1kitshell::KitSprite::Frame* pointer = gauge_.frame(1);
    if (bar == nullptr || pointer == nullptr || !canvas_.create(bar->width, bar->height)) return;
    const c1kitshell::GamePalette& palette = sheet_.palette();
    canvas_.draw_frame(gauge_, 0, 0, 0, palette);
    const int x = ((levels_[index] * (bar->width - 20)) >> 8) + (pointer->width >> 1);
    canvas_.draw_frame_keyed(gauge_, 1, x, 0, palette);
    canvas_.present(dc, rect.left + (rect.Width() - bar->width) / 2,
                    rect.top + (rect.Height() - bar->height) / 2, bar->width, bar->height);
}

// ===========================================================================
// Brain activity
// ===========================================================================
//
// CScannerPage: Lobes.bmp, and for each of the first six lobes one of
// LOBES.SPR's eight frames by how busy it is -- the brain report's levels
// over the lobe, averaged (or summed, for a lobe whose flag's bit 0 is set),
// halved and rounded (UpdateHealthLobeSprites @ 0x0040b810), once a second.
// The original could ask for a ninth frame; here the brightest is the last.

ClassicBrainPage::ClassicBrainPage(HealthSheet& sheet)
    : ClassicHealthPage(sheet, kDialogClassicBrain, kStringBrainTab, 1000) {}

void ClassicBrainPage::init() {
    c1kitshell::KitSprite::load_set(main_file("LOBES.SPR"), lobe_sprites_);
    replace_picture(kControlClassicPicture, picture_, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
}

void ClassicBrainPage::poll() {
    std::fill(std::begin(frames_), std::end(frames_), 0);
    std::string reply;
    auto activity = std::make_unique<c1kit::BrainActivity>();
    if (sheet_.subject().present && sheet_.brain_report(c1kit::kReportFiringStrength, reply)) {
        c1kit::parse_activity_report(reply, *activity);
        const std::vector<c1kit::LobeLayout>& lobes = sheet_.lobes();
        for (std::size_t i = 0; i < lobes.size() && i < 6; ++i) {
            const c1kit::LobeLayout& lobe = lobes[i];
            int sum = 0;
            for (int x = lobe.x; x < lobe.x + lobe.width && x < c1kit::kBrainGridSize; ++x) {
                for (int y = lobe.y; y < lobe.y + lobe.height && y < c1kit::kBrainGridSize; ++y) {
                    if (activity->reported[x][y]) sum += activity->level[x][y];
                }
            }
            const int value = (lobe.flags & 1) != 0 || lobe.neurons() <= 0 ? sum : sum / lobe.neurons();
            frames_[i] = (std::min)(7, (value + 1) >> 1);
        }
    }
    picture_.redraw();
}

void ClassicBrainPage::draw(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    int width = 0, height = 0;
    std::vector<std::uint8_t> lobes;
    if (!read_bmp8(main_file("Lobes.bmp"), width, height, lobes) || !canvas_.create(width, height)) return;
    const c1kitshell::GamePalette& palette = sheet_.palette();
    canvas_.draw_indexed(lobes.data(), width, height, width, false, 0, 0, palette);
    for (std::size_t i = 0; i < lobe_sprites_.size() && i < 6; ++i) {
        canvas_.draw_frame_keyed(lobe_sprites_[i], frames_[i], kLobeX[i], kLobeY[i], palette);
    }
    canvas_.present(dc, rect.left + (rect.Width() - width) / 2, rect.top + (rect.Height() - height) / 2,
                    width, height);
}

}  // namespace health
