// The Breeder's Kit's classic look: the 1996 Fertility page (dialog 138) with
// its pictures drawn from the game's files.  The Aphrodisiac page is the
// shell's ClassicShopPage.  See breeder.hpp.
//
// CBreedersKitPage8A: every picture control takes its picture's size where
// the template put it.  Once a second the page reads five chemicals; the
// three gauges (Fertility.spr's gauges, their pointer at the level) show sex
// drive, then progesterone for a female or glycogen ("Health") for a male,
// then gonadotrophin ("Fertility"); the graph adds a four-pixel bar of
// oestrogen or testosterone, scrolling left once full; and the silhouette is
// Male.bmp, crossed out, for a male, or Pregnancy.spr's woman for a female,
// with the embryo it shows growing with progesterone (a stage every 12
// pixels of the gauge's reach, as the original counted them).  The original
// fed its middle gauge an average of three readings and gated the embryo on
// the highest level seen so far; here each shows its own reading.

#include "breeder.hpp"
#include "breeder_ids.hpp"

#include <algorithm>

namespace breeder {
namespace {

constexpr UINT kClose = 1123;
constexpr UINT kSilhouette = 1111;
constexpr UINT kGaugeControls[3] = {1020, 1081, 1021};
constexpr UINT kGraph = 1019;
constexpr UINT kMiddleLabel = 1110;
constexpr UINT kGraphLabel = 1109;
constexpr UINT kSexIcon = 1095;
constexpr UINT_PTR kPollTimer = 11;
constexpr int kPointerX = 0x1d;    // the pointer's column on a gauge
constexpr int kEmbryoX = 0x1b, kEmbryoY = 0x44;
constexpr int kBarWidth = 4;

std::string main_file(const char* name) {
    return std::string(CStringA(c1kitshell::game_directory_setting("Main Directory"))) + name;
}

}  // namespace

BEGIN_MESSAGE_MAP(ClassicFertilityPage, CPropertyPage)
    ON_WM_TIMER()
    ON_BN_CLICKED(kClose, &ClassicFertilityPage::OnCloseKit)
END_MESSAGE_MAP()

ClassicFertilityPage::ClassicFertilityPage(BreederSheet& sheet, const c1kitshell::ClassicArt& art)
    : CPropertyPage(kDialogClassicFertility), sheet_(sheet), art_(art) {
    title_ = c1kitshell::load_string(kStringFertilityTab);
    m_psp.dwFlags |= PSP_USETITLE;
    m_psp.pszTitle = title_;
}

void ClassicFertilityPage::place_view(UINT control, c1kitshell::PaintedView& view, int width, int height,
                                      c1kitshell::PaintedView::Painter painter) {
    CWnd* picture = GetDlgItem(control);
    if (picture == nullptr) return;
    CRect area;
    picture->GetWindowRect(&area);
    ScreenToClient(&area);
    picture->ShowWindow(SW_HIDE);
    view.create(*this, 0x7f00 + control, std::move(painter));
    view.ModifyStyle(0, WS_CLIPSIBLINGS);
    view.SetWindowPos(&wndBottom, area.left, area.top, width, height, SWP_NOACTIVATE);
}

BOOL ClassicFertilityPage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    c1kitshell::KitSprite::load_set(main_file("Fertility.spr"), fertility_);
    c1kitshell::KitSprite::load_set(main_file("Pregnancy.spr"), pregnancy_);
    const auto size_of = [](const std::vector<c1kitshell::KitSprite>& set, std::size_t sprite, CSize fallback) {
        const c1kitshell::KitSprite::Frame* frame = sprite < set.size() ? set[sprite].frame(0) : nullptr;
        return frame != nullptr ? CSize(frame->width, frame->height) : fallback;
    };
    const CSize silhouette = size_of(pregnancy_, 0, CSize(84, 141));
    place_view(kSilhouette, silhouette_, silhouette.cx, silhouette.cy,
               [this](CDC& dc, const CRect& rect) { draw_silhouette(dc, rect); });
    for (int i = 0; i < 3; ++i) {
        const CSize gauge = size_of(fertility_, static_cast<std::size_t>(i + 1), CSize(40, 120));
        place_view(kGaugeControls[i], gauges_[i], gauge.cx, gauge.cy,
                   [this, i](CDC& dc, const CRect& rect) { draw_gauge(i, dc, rect); });
    }
    const CSize graph = size_of(fertility_, 0, CSize(241, 112));
    place_view(kGraph, graph_, graph.cx, graph.cy, [this](CDC& dc, const CRect& rect) { draw_graph(dc, rect); });
    // The graph's scale, a bitmap in the original (the template's image
    // static, id -1).
    scale_ = static_cast<HBITMAP>(::LoadImage(art_.module(), MAKEINTRESOURCE(kBitmapGraphScale),
                                              IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    for (CWnd* child = GetWindow(GW_CHILD); child != nullptr; child = child->GetNextWindow()) {
        if (child->GetDlgCtrlID() == 0xffff && (child->GetStyle() & SS_TYPEMASK) == SS_BITMAP) {
            child->SendMessage(STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(scale_));
        }
    }
    return TRUE;
}

BOOL ClassicFertilityPage::OnSetActive() {
    poll();
    SetTimer(kPollTimer, 1000, nullptr);
    return CPropertyPage::OnSetActive();
}

BOOL ClassicFertilityPage::OnKillActive() {
    KillTimer(kPollTimer);
    return CPropertyPage::OnKillActive();
}

void ClassicFertilityPage::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kPollTimer) {
        poll();
        return;
    }
    CPropertyPage::OnTimer(timer_id);
}

void ClassicFertilityPage::OnCloseKit() {
    sheet_.request_game_quit();
}

// RefreshSelectedCreatureBreedingStatus @ 0x00409140: oestrogen (63) or
// testosterone (64), sex drive (13), progesterone (66) or glycogen (59),
// gonadotrophin (65), glycogen (59).
void ClassicFertilityPage::poll() {
    const Subject& subject = sheet_.subject();
    const bool female = subject.sex == 2;
    if (subject.sex != sex_) {
        sex_ = subject.sex;
        columns_.clear();
        SetDlgItemText(kMiddleLabel, c1kitshell::load_string(female ? kStringProgesterone : kStringHealth));
        SetDlgItemText(kGraphLabel, c1kitshell::load_string(female ? kStringOestrogen : kStringTestosterone));
        if (CWnd* icon = GetDlgItem(kSexIcon)) {
            icon->SendMessage(STM_SETICON, reinterpret_cast<WPARAM>(AfxGetApp()->LoadIcon(female ? kIconFemale : kIconMale)));
        }
    }
    std::string reply;
    std::vector<int> values;
    const std::vector<int> chemicals = {female ? 63 : 64, 13, female ? 66 : 59, 65, 59};
    if (!subject.present || !sheet_.query(c1kit::chemical_levels_query(chemicals), reply) ||
        !c1kit::parse_values(reply, chemicals.size(), values)) {
        return;
    }
    const auto clamp = [](int v) { return (std::max)(0, (std::min)(255, v)); };
    levels_[0] = clamp(values[1]);
    levels_[1] = clamp(values[2]);
    levels_[2] = clamp(values[3]);
    // The embryo: InitializeStatusDisplay's frame table over the middle
    // gauge's reach (a new stage every 12 pixels, the eighth from 100 on).
    const c1kitshell::KitSprite::Frame* gauge =
        fertility_.size() > 2 ? fertility_[2].frame(0) : nullptr;
    const int reach = gauge != nullptr ? gauge->height - 10 : 110;
    const int scaled = levels_[1] * reach / 256;
    stage_ = !female || scaled == 0 ? 0 : scaled < 100 ? scaled / 12 + 1 : 8;
    const c1kitshell::KitSprite::Frame* graph = !fertility_.empty() ? fertility_[0].frame(0) : nullptr;
    if (graph != nullptr) {
        columns_.push_back(clamp(values[0]) * graph->height / 256);
        const std::size_t room = static_cast<std::size_t>(graph->width / kBarWidth);
        if (columns_.size() > room) columns_.erase(columns_.begin(), columns_.end() - room);
    }
    silhouette_.redraw();
    for (c1kitshell::PaintedView& view : gauges_) view.redraw();
    graph_.redraw();
}

void ClassicFertilityPage::draw_silhouette(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    const c1kitshell::GamePalette& palette = sheet_.palette();
    if (sex_ != 2) {
        // LoadMaleBitmap @ 0x00409b20: the crossed-out Male.bmp.
        c1kitshell::PaletteBitmap male;
        if (male.load_file(CString(main_file("Male.bmp").c_str()))) male.draw(dc, rect.left, rect.top);
        return;
    }
    if (pregnancy_.empty()) return;
    const c1kitshell::KitSprite& sprite = pregnancy_[0];
    const c1kitshell::KitSprite::Frame* base = sprite.frame(0);
    if (base == nullptr || !canvas_.create(base->width, base->height)) return;
    canvas_.draw_frame(sprite, stage_ > 0 ? 1 : 0, 0, 0, palette);
    if (stage_ > 0) canvas_.draw_frame_keyed(sprite, stage_ + 1, kEmbryoX, kEmbryoY, palette);
    canvas_.present(dc, rect.left, rect.top, base->width, base->height);
}

// UpdateFertilityVisual @ 0x0040a3b0 and its kin: the gauge, and its pointer
// at the level.
void ClassicFertilityPage::draw_gauge(int index, CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    const std::size_t set = static_cast<std::size_t>(index + 1);
    if (set >= fertility_.size()) return;
    const c1kitshell::KitSprite& sprite = fertility_[set];
    const c1kitshell::KitSprite::Frame* gauge = sprite.frame(1);
    const c1kitshell::KitSprite::Frame* pointer = sprite.frame(2);
    if (gauge == nullptr || pointer == nullptr || !canvas_.create(gauge->width, gauge->height)) return;
    const c1kitshell::GamePalette& palette = sheet_.palette();
    canvas_.draw_frame(sprite, 1, 0, 0, palette);
    const int reach = gauge->height - pointer->height;
    canvas_.draw_frame_keyed(sprite, 2, kPointerX, reach - levels_[index] * reach / 256, palette);
    canvas_.present(dc, rect.left, rect.top, gauge->width, gauge->height);
}

// AdvanceBreedingVisualAnimation @ 0x0040a1f0: each bar the background above
// its height and the bar strip below.
void ClassicFertilityPage::draw_graph(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
    if (fertility_.empty()) return;
    const c1kitshell::KitSprite& sprite = fertility_[0];
    const c1kitshell::KitSprite::Frame* back = sprite.frame(0);
    const c1kitshell::KitSprite::Frame* bar = sprite.frame(1);
    if (back == nullptr || bar == nullptr || !canvas_.create(back->width, back->height)) return;
    const c1kitshell::GamePalette& palette = sheet_.palette();
    canvas_.draw_frame(sprite, 0, 0, 0, palette);
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        const int x = static_cast<int>(i) * kBarWidth;
        const int top = back->height - columns_[i];
        for (int y = top; y < back->height && y < bar->height; ++y) {
            canvas_.draw_indexed(bar->pixels.data() + static_cast<std::size_t>(y) * bar->width, bar->width,
                                 1, bar->width, false, x, y, palette);
        }
    }
    canvas_.present(dc, rect.left, rect.top, back->width, back->height);
}

}  // namespace breeder
