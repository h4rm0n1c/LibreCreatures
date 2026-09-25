// The Score Kit's page: counters, icon picture, Breeders Score and elapsed
// time, drawn from the kit's art.  The original's behaviour is in
// ../ORIGINAL.md; deviations are marked "Fix (bug N)".

#include "score.hpp"
#include "score_ids.hpp"

#include <algorithm>
#include <string>

namespace score {
namespace {

std::string palette_path() {
    CString directory = c1kitshell::game_directory_setting("Palette Directory");
    if (directory.IsEmpty()) {
        directory = _T("Palettes\\");
    } else if (directory.Right(1) != _T("\\")) {
        directory += _T("\\");
    }
    return std::string(CStringA(directory)) + kPaletteFile;
}

} // namespace

BEGIN_MESSAGE_MAP(ScorePage, CPropertyPage)
    ON_WM_DRAWITEM()
    ON_WM_HSCROLL()
    ON_WM_SIZE()
    ON_BN_CLICKED(kControlClose, &ScorePage::OnClose)
END_MESSAGE_MAP()

ScorePage::ScorePage(ScoreSheet& sheet)
    : CPropertyPage(kDialogScore), sheet_(sheet) {}

// CScorePage::Initialize @ 0x00407f10.
BOOL ScorePage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    load_art();

    anchors_.capture(*this);
    using Anchors = c1kitshell::ControlAnchors;
    anchors_.add(*this, kControlPanel, Anchors::kGrowX | Anchors::kGrowY);
    anchors_.add(*this, kControlPanelScroll, Anchors::kGrowX | Anchors::kMoveY);
    anchors_.add(*this, kLabelBreedersScore, Anchors::kMoveY);
    anchors_.add(*this, kLabelElapsedTime, Anchors::kMoveY);
    anchors_.add(*this, kControlBreedersScore, Anchors::kMoveY);
    anchors_.add(*this, kControlElapsedTime, Anchors::kMoveY);
    anchors_.add(*this, kControlClose, Anchors::kMoveX | Anchors::kMoveY);

    initialized_ = true;
    show(sheet_.state());
    return TRUE;
}

// LoadSprite @ 0x004090c0, LoadNumericAndTimeSprites @ 0x00408db0 and
// LoadScorePaletteData.  Fix (bug 9): one message naming every missing file,
// instead of a generic "Can not create data file." per file (for files it
// only reads) and a page left half-built.
void ScorePage::load_art() {
    CString missing;
    const std::string palette = palette_path();
    if (!palette_.load(palette)) {
        missing += CString(palette.c_str()) + _T("\n");
    }
    if (!digits_.load(kDigitsFile)) {
        missing += CString(kDigitsFile) + _T("\n");
    }
    if (!icons_.load(kIconsFile)) {
        missing += CString(kIconsFile) + _T("\n");
    }
    if (!colon_.load(kColonFile)) {
        missing += CString(kColonFile) + _T("\n");
    }
    if (!missing.IsEmpty()) {
        AfxMessageBox(_T("The Performance Kit could not read:\n\n") + missing +
                      _T("\nThey belong in the game's main directory."));
    }
}

CSize ScorePage::control_size(unsigned control_id) const {
    CRect rect;
    if (const CWnd* control = GetDlgItem(control_id)) {
        control->GetClientRect(&rect);
    }
    return rect.Size();
}

void ScorePage::invalidate_control(unsigned control_id) {
    if (CWnd* control = GetDlgItem(control_id)) {
        control->Invalidate(FALSE);
    }
}

// A number in the digit art over a backdrop, right-aligned and zero-padded
// to `digits`, as RefreshDisplay does (digits 10 px apart).  Fix (bug 5):
// values too large for the box show as all nines instead of silently
// losing their leading digits.
void ScorePage::draw_number(c1kitshell::Canvas& canvas, unsigned control_id,
                            const char* backdrop, int value, int digits) {
    const CSize size = control_size(control_id);
    if (!canvas.create(size.cx, size.cy)) {
        return;
    }
    canvas.fill(palette_.colour(kPanelColourIndex));
    canvas.tile_bitmap_file(backdrop);
    int limit = 1;
    for (int i = 0; i < digits; ++i) {
        limit *= 10;
    }
    value = std::clamp(value, 0, limit - 1);
    const int span = (digits - 1) * kDigitStep + kDigitWidth;
    const int left = (size.cx - span) / 2;
    const int top = (size.cy - kDigitHeight) / 2;
    for (int i = digits - 1; i >= 0; --i) {
        canvas.draw_frame(digits_, value % 10, left + i * kDigitStep, top,
                          palette_);
        value /= 10;
    }
    invalidate_control(control_id);
}

// RenderWorldTime @ 0x00408980 and RenderSelector @ 0x00408130: hours, the
// blinking colon (Time.spr frame 1, or the blank frame 0), minutes.
// Fix (bug 6): four hour digits; the original drew five, the fifth partly
// off the left edge of its box.
void ScorePage::draw_time() {
    const CSize size = control_size(kControlElapsedTime);
    if (!elapsed_time_.create(size.cx, size.cy)) {
        return;
    }
    elapsed_time_.fill(palette_.colour(kPanelColourIndex));
    elapsed_time_.tile_bitmap_file(kScoreBackdrop);
    // Offsets from the original: hours at 8..38, colon at 50, minutes at
    // 60 and 70.
    const int span = 70 + kDigitWidth - 8;
    const int base = (size.cx - span) / 2 - 8;
    const int top = (size.cy - kDigitHeight) / 2;
    int hours = std::clamp(state_.hours, 0, 9999);
    for (int i = 0; i < 4; ++i) {
        elapsed_time_.draw_frame(digits_, hours % 10, base + 38 - i * kDigitStep,
                                 top, palette_);
        hours /= 10;
    }
    elapsed_time_.draw_frame(colon_, colon_visible_ ? 1 : 0, base + 50, top,
                             palette_);
    const int minutes = std::clamp(state_.minutes, 0, 59);
    elapsed_time_.draw_frame(digits_, minutes / 10, base + 60, top, palette_);
    elapsed_time_.draw_frame(digits_, minutes % 10, base + 70, top, palette_);
    invalidate_control(kControlElapsedTime);
}

// The icon picture (RefreshDisplay's second half): one row per counter, an
// icon per egg or norn, 30 px apart, rows 50 px apart.
// Fix (bug 3): every icon is drawn; the original stopped at 20 per row.
// Fix (bug 4): the picture is redrawn whole each time; the original cleared
// it only when the number of living norns fell, so other rows kept stale
// icons.
void ScorePage::draw_panel() {
    const CSize view = control_size(kControlPanel);
    const int counts[4] = {state_.values.hatchery_eggs,
                           state_.values.natural_eggs,
                           state_.values.previous_norns,
                           state_.values.current_norns};
    int widest = 0;
    for (int count : counts) {
        widest = (std::max)(widest, (std::max)(count, 0));
    }
    const int width = (std::max)(static_cast<int>(view.cx), 2 * kIconMargin + widest * kIconStepX);
    const int height = (std::max)(static_cast<int>(view.cy), 2 * kIconMargin + 3 * kIconStepY + 40);
    if (!panel_.create(width, height)) {
        return;
    }
    panel_.fill(palette_.colour(kPanelColourIndex));
    for (int row = 0; row < 4; ++row) {
        for (int i = 0; i < counts[row]; ++i) {
            panel_.draw_frame(icons_, row, kIconMargin + i * kIconStepX,
                              kIconMargin + row * kIconStepY, palette_);
        }
    }
    update_scroll_range();
    invalidate_control(kControlPanel);
}

// Fix (bug 2): the scroll range matches the picture, and the position is
// kept across refreshes; the original reset it to the start every refresh
// and let it scroll a whole box-width past the last icon.
void ScorePage::update_scroll_range() {
    CScrollBar* bar = static_cast<CScrollBar*>(GetDlgItem(kControlPanelScroll));
    if (bar == nullptr) {
        return;
    }
    const int view = control_size(kControlPanel).cx;
    const int maximum = (std::max)(0, panel_.width() - view);
    scroll_x_ = std::clamp(scroll_x_, 0, maximum);
    SCROLLINFO info = {sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    info.nMin = 0;
    info.nMax = panel_.width() - 1;
    info.nPage = static_cast<UINT>(view);
    info.nPos = scroll_x_;
    bar->SetScrollInfo(&info, TRUE);
    bar->EnableScrollBar(maximum > 0 ? ESB_ENABLE_BOTH : ESB_DISABLE_BOTH);
}

void ScorePage::show(const ScoreState& state) {
    state_ = state;
    if (!initialized_) {
        return;
    }
    const unsigned counter_ids[4] = {kControlHatcheryEggs, kControlNaturalEggs,
                                     kControlPreviousNorns,
                                     kControlCurrentNorns};
    const int counter_values[4] = {state.values.hatchery_eggs,
                                   state.values.natural_eggs,
                                   state.values.previous_norns,
                                   state.values.current_norns};
    for (int i = 0; i < 4; ++i) {
        draw_number(counters_[i], counter_ids[i], kCounterBackdrop,
                    counter_values[i], 4);
    }
    draw_number(breeders_score_, kControlBreedersScore, kScoreBackdrop,
                c1kit::breeders_score(state.values), 8);
    draw_time();
    draw_panel();
}

void ScorePage::set_colon_visible(bool visible) {
    colon_visible_ = visible;
    if (initialized_) {
        draw_time();
    }
}

void ScorePage::OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw) {
    CDC* dc = CDC::FromHandle(draw->hDC);
    const CRect rect(draw->rcItem);
    const c1kitshell::Canvas* canvas = nullptr;
    int source_x = 0;
    switch (control_id) {
    case kControlHatcheryEggs: canvas = &counters_[0]; break;
    case kControlNaturalEggs: canvas = &counters_[1]; break;
    case kControlPreviousNorns: canvas = &counters_[2]; break;
    case kControlCurrentNorns: canvas = &counters_[3]; break;
    case kControlBreedersScore: canvas = &breeders_score_; break;
    case kControlElapsedTime: canvas = &elapsed_time_; break;
    case kControlPanel:
        canvas = &panel_;
        source_x = scroll_x_;
        break;
    default:
        CPropertyPage::OnDrawItem(control_id, draw);
        return;
    }
    if (canvas->width() == 0) {
        dc->FillSolidRect(&rect, GetSysColor(COLOR_BTNFACE));
        return;
    }
    canvas->present(*dc, rect.left, rect.top, rect.Width(), rect.Height(),
                    source_x, 0);
}

// OnHScroll @ 0x004092a0: 10 px a step, as the original.
void ScorePage::OnHScroll(UINT code, UINT position, CScrollBar* bar) {
    if (bar == nullptr || bar->GetDlgCtrlID() != kControlPanelScroll) {
        CPropertyPage::OnHScroll(code, position, bar);
        return;
    }
    const int view = control_size(kControlPanel).cx;
    const int maximum = (std::max)(0, panel_.width() - view);
    switch (code) {
    case SB_LINELEFT: scroll_x_ -= kDigitStep; break;
    case SB_LINERIGHT: scroll_x_ += kDigitStep; break;
    case SB_PAGELEFT: scroll_x_ -= view; break;
    case SB_PAGERIGHT: scroll_x_ += view; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: scroll_x_ = static_cast<int>(position); break;
    case SB_LEFT: scroll_x_ = 0; break;
    case SB_RIGHT: scroll_x_ = maximum; break;
    default: break;
    }
    scroll_x_ = std::clamp(scroll_x_, 0, maximum);
    bar->SetScrollPos(scroll_x_, TRUE);
    invalidate_control(kControlPanel);
}

void ScorePage::OnSize(UINT type, int cx, int cy) {
    CPropertyPage::OnSize(type, cx, cy);
    anchors_.apply(*this);
    if (initialized_) {
        draw_panel();
    }
}

void ScorePage::OnClose() {
    sheet_.request_game_quit();
}

} // namespace score
