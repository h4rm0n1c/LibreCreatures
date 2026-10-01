// The Hatchery's classic look: the 1996 machine.  See hatchery.hpp and
// ../ORIGINAL.md.  The pictures and sounds are the game's own files, read at
// run time (Hatchery\*.bmp, Sounds\h*.wav); the composition, positions and
// timings are the 1996 view's (CHatcheryView), over this kit's nest and
// hatching.

#include "hatchery.hpp"
#include "hatchery_ids.hpp"

#include <ctime>

namespace hatchery {
namespace {

// CHatcheryView::Constructor @ 0x00402d90: where each egg sits in the nest.
const CPoint kEggAt[c1kit::kEggCount] = {{60, 158}, {90, 149}, {122, 162},
                                        {150, 159}, {186, 152}, {205, 155}};

// RenderInvalidatedRegion @ 0x004035e0: the fans' frames, the lamp
// (lit one frame in ten), the nest front over the eggs, the scanner (while
// the kit connects), and the sex symbol spinning over the egg pointed at.
const CPoint kFansAt(229, 54);
const CPoint kLampAt(109, 0);
const CPoint kFrontAt(0, 169);
const CPoint kScannerAt(120, 80);
const CSize kSpinFrame(28, 24);  // six across, 29 apart; female above, male below
constexpr COLORREF kFrontKey = RGB(255, 0, 128);

// The 1996 sound manager's pan for an egg: ((x * 5 - 800) * 1000) / 160.
int egg_pan(int slot) {
    return ((kEggAt[slot].x * 5 - 800) * 1000) / 160;
}

HBITMAP load_picture(const std::string& path) {
    return static_cast<HBITMAP>(LoadImageA(nullptr, path.c_str(), IMAGE_BITMAP, 0, 0,
                                           LR_LOADFROMFILE | LR_CREATEDIBSECTION));
}

CSize picture_size(HBITMAP bitmap) {
    BITMAP info = {};
    if (bitmap == nullptr || GetObject(bitmap, sizeof(info), &info) == 0) {
        return CSize(0, 0);
    }
    return CSize(info.bmWidth, info.bmHeight);
}

void draw_opaque(CDC& dc, HBITMAP bitmap, CPoint at) {
    if (bitmap == nullptr) return;
    CDC source;
    source.CreateCompatibleDC(&dc);
    HGDIOBJ previous = SelectObject(source.GetSafeHdc(), bitmap);
    const CSize size = picture_size(bitmap);
    dc.BitBlt(at.x, at.y, size.cx, size.cy, &source, 0, 0, SRCCOPY);
    SelectObject(source.GetSafeHdc(), previous);
}

// A part of `bitmap`, keyed.
void draw_keyed_part(CDC& dc, HBITMAP bitmap, CPoint at, CRect part, COLORREF key) {
    if (bitmap == nullptr) return;
    CDC sheet;
    sheet.CreateCompatibleDC(&dc);
    HGDIOBJ previous = SelectObject(sheet.GetSafeHdc(), bitmap);
    CBitmap piece;
    piece.CreateCompatibleBitmap(&dc, part.Width(), part.Height());
    CDC piece_dc;
    piece_dc.CreateCompatibleDC(&dc);
    CBitmap* old_piece = piece_dc.SelectObject(&piece);
    piece_dc.BitBlt(0, 0, part.Width(), part.Height(), &sheet, part.left, part.top, SRCCOPY);
    piece_dc.SelectObject(old_piece);
    SelectObject(sheet.GetSafeHdc(), previous);
    draw_keyed(dc, static_cast<HBITMAP>(piece.GetSafeHandle()),
               CRect(at, part.Size()), 0, RGB(0, 0, 0), key);
}


} // namespace

BEGIN_MESSAGE_MAP(MachinePage, CPropertyPage)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_TIMER()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_DESTROY()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, &MachinePage::OnEggTip)
END_MESSAGE_MAP()

MachinePage::MachinePage(HatcherySheet& sheet)
    : CPropertyPage(kDialogPage, kStringNestTab), sheet_(sheet) {}

MachinePage::~MachinePage() {
    for (HBITMAP bitmap : {background_, front_, genspin_, lamp_off_}) {
        if (bitmap != nullptr) DeleteObject(bitmap);
    }
    for (HBITMAP bitmap : eggs_) {
        if (bitmap != nullptr) DeleteObject(bitmap);
    }
    for (HBITMAP bitmap : fans_) {
        if (bitmap != nullptr) DeleteObject(bitmap);
    }
    for (HBITMAP bitmap : scans_) {
        if (bitmap != nullptr) DeleteObject(bitmap);
    }
}

void MachinePage::load_art() {
    const auto picture = [&](const std::string& name) {
        return load_picture(sheet_.game_file("Hatchery\\" + name));
    };
    background_ = picture("hatchery.bmp");
    front_ = picture("htchmask.bmp");
    genspin_ = picture("genspin.bmp");
    lamp_off_ = picture("lightoff.bmp");
    for (int i = 0; i < c1kit::kEggCount; ++i) eggs_[i] = picture("egg" + std::to_string(i) + ".bmp");
    for (int i = 0; i < 4; ++i) fans_[i] = picture("fan" + std::to_string(i) + ".bmp");
    for (int i = 0; i < 6; ++i) scans_[i] = picture("scan" + std::to_string(i) + ".bmp");
}

BOOL MachinePage::OnInitDialog() {
    CPropertyPage::OnInitDialog();
    load_art();
    // Not in the 1996 kit: with Scramble Eggs on, an egg's tip names the
    // pair it will be crossed from (the 1996 picture stays as it was).
    if (tips_.Create(this)) {
        for (int i = 0; i < c1kit::kEggCount; ++i) {
            tips_.AddTool(this, LPSTR_TEXTCALLBACK, CRect(kEggAt[i], picture_size(eggs_[i])),
                          static_cast<UINT_PTR>(i + 1));
        }
        tips_.Activate(TRUE);
    }
    SetTimer(kTimerMachine, kMachineTickMs, nullptr);
    return TRUE;
}

BOOL MachinePage::PreTranslateMessage(MSG* message) {
    if (tips_.GetSafeHwnd() != nullptr) {
        tips_.RelayEvent(message);
    }
    return CPropertyPage::PreTranslateMessage(message);
}

BOOL MachinePage::OnEggTip(UINT /*id*/, NMHDR* header, LRESULT* result) {
    auto* text = reinterpret_cast<TOOLTIPTEXT*>(header);
    const int slot = static_cast<int>(header->idFrom) - 1;
    tip_text_ = slot >= 0 && slot < c1kit::kEggCount &&
                        sheet_.nest().eggs[slot] != c1kit::EggState::taken
                    ? sheet_.egg_parents_text(slot)
                    : CString();
    text->lpszText = const_cast<LPTSTR>(static_cast<LPCTSTR>(tip_text_));
    *result = 0;
    return TRUE;
}

void MachinePage::refresh() {
    if (GetSafeHwnd() == nullptr) return;
    if (pointed_ >= 0 && sheet_.nest().eggs[pointed_] == c1kit::EggState::taken) {
        pointed_ = -1;
    }
    keep_ambience();
    Invalidate(FALSE);
}

// The continuous sounds: the fans always, the scanner while connecting, the
// pointed-at egg's male or female tone -- all stopped while muted.
void MachinePage::keep_ambience() {
    c1kitshell::KitSound& sound = sheet_.sound();
    const bool muted = sheet_.muted();
    const auto keep = [&](int& channel, bool wanted, const char* name, int volume, int pan) {
        if (wanted && channel < 0) {
            channel = sound.play(name, true, volume, pan);
        } else if (!wanted && channel >= 0) {
            sound.stop(channel);
            channel = -1;
        }
    };
    keep(fan_channel_, !muted, "hfan", 0, 3000);
    keep(scanner_channel_, !muted && !sheet_.connected(), "hdsk", -1000, 0);
    if (muted || pointed_ < 0) {
        keep(egg_channel_, false, "", 0, 0);
    }
}

// CHatcheryView's timer (75 ms): the wobbles count down, the animations step.
void MachinePage::OnTimer(UINT_PTR timer_id) {
    if (timer_id != kTimerMachine) {
        CPropertyPage::OnTimer(timer_id);
        return;
    }
    for (int& wobble : wobble_) {
        if (wobble > 0) --wobble;
    }
    ++counter_;
    keep_ambience();
    CClientDC dc(this);
    compose(dc);
}

BOOL MachinePage::OnEraseBkgnd(CDC*) {
    return TRUE;
}

void MachinePage::OnPaint() {
    CPaintDC dc(this);
    compose(dc);
}

void MachinePage::compose(CDC& target) {
    CBitmap frame;
    frame.CreateCompatibleBitmap(&target, kMachineWidth, kMachineHeight);
    CDC dc;
    dc.CreateCompatibleDC(&target);
    CBitmap* previous = dc.SelectObject(&frame);
    dc.FillSolidRect(0, 0, kMachineWidth, kMachineHeight, RGB(0, 0, 0));
    draw_opaque(dc, background_, CPoint(0, 0));
    draw_opaque(dc, fans_[counter_ % 4], kFansAt);
    // The lamp: out nine frames in ten; the tenth it flickers on, buzzing.
    if (std::rand() % 10 < 9) {
        draw_opaque(dc, lamp_off_, kLampAt);
    } else if (!sheet_.muted()) {
        sheet_.sound().play("hlgt", false);
    }
    const c1kit::Nest& nest = sheet_.nest();
    for (int i = 0; i < c1kit::kEggCount; ++i) {
        if (nest.eggs[i] == c1kit::EggState::taken || eggs_[i] == nullptr) continue;
        CPoint at = kEggAt[i];
        if (wobble_[i] > 0) {
            at += CPoint(wobble_[i] % 3 - 2, (wobble_[i] & 1) * 2);
        }
        draw_keyed(dc, eggs_[i], CRect(at, picture_size(eggs_[i])));
    }
    if (front_ != nullptr) {
        draw_keyed(dc, front_, CRect(kFrontAt, picture_size(front_)), 0, RGB(0, 0, 0), kFrontKey);
    }
    if (pointed_ >= 0 && nest.eggs[pointed_] != c1kit::EggState::taken) {
        const CSize egg = picture_size(eggs_[pointed_]);
        const int row = nest.eggs[pointed_] == c1kit::EggState::female ? 0 : 1;
        const int column = (counter_ / 2) % 6;
        draw_keyed_part(dc, genspin_,
                        CPoint(kEggAt[pointed_].x + (egg.cx - kSpinFrame.cx) / 2,
                               kEggAt[pointed_].y - 32),
                        CRect(CPoint(column * 29, row * 25), kSpinFrame), RGB(0, 255, 0));
    }
    if (!sheet_.connected() && scans_[counter_ % 6] != nullptr) {
        draw_keyed(dc, scans_[counter_ % 6], CRect(kScannerAt, picture_size(scans_[counter_ % 6])));
    }
    target.BitBlt(0, 0, kMachineWidth, kMachineHeight, &dc, 0, 0, SRCCOPY);
    dc.SelectObject(previous);
}

int MachinePage::egg_at(CPoint point) const {
    const c1kit::Nest& nest = sheet_.nest();
    for (int i = c1kit::kEggCount - 1; i >= 0; --i) {
        if (nest.eggs[i] == c1kit::EggState::taken) continue;
        if (CRect(kEggAt[i], picture_size(eggs_[i])).PtInRect(point)) return i;
    }
    return -1;
}

// The mouse-move handler at 0x00404b90: an egg pointed at, and not already
// wobbling, wobbles for five to nine steps and clicks, and becomes the egg
// whose symbol spins and whose tone plays.
void MachinePage::OnMouseMove(UINT flags, CPoint point) {
    const int egg = egg_at(point);
    if (egg >= 0 && wobble_[egg] == 0) {
        pointed_ = egg;
        wobble_[egg] = static_cast<int>(std::time(nullptr) % 5) + 5;
        c1kitshell::KitSound& sound = sheet_.sound();
        sound.play("hegg", false, 0, egg_pan(egg));
        if (egg_channel_ >= 0) {
            sound.stop(egg_channel_);
            egg_channel_ = -1;
        }
        if (!sheet_.muted()) {
            const bool female = sheet_.nest().eggs[egg] == c1kit::EggState::female;
            egg_channel_ = sound.play(female ? "hfml" : "hmle", true, 0, egg_pan(egg));
        }
    }
    CPropertyPage::OnMouseMove(flags, point);
}

// A page's window class may not ask for double-clicks, so two clicks on
// the same egg within the double-click time count as one too.
void MachinePage::OnLButtonDown(UINT flags, CPoint point) {
    const int egg = egg_at(point);
    const DWORD now = GetMessageTime();
    if (egg >= 0 && egg == last_click_egg_ && now - last_click_time_ <= GetDoubleClickTime()) {
        last_click_egg_ = -1;
        hatch_at(point);
    } else {
        last_click_egg_ = egg;
        last_click_time_ = now;
    }
    CPropertyPage::OnLButtonDown(flags, point);
}

void MachinePage::OnLButtonDblClk(UINT flags, CPoint point) {
    last_click_egg_ = -1;
    hatch_at(point);
    CPropertyPage::OnLButtonDblClk(flags, point);
}

// HandleEggSlotClick @ 0x00403c30: a double-click hatches the egg (with the
// 1996 click, panned to it).  Fix (bug 2): the kit stays open.
void MachinePage::hatch_at(CPoint point) {
    const int egg = egg_at(point);
    if (egg >= 0) {
        CString why;
        if (sheet_.hatch(egg, why)) {
            sheet_.sound().play("hslt", false, 0, egg_pan(egg));
        } else {
            MessageBox(why, _T("The Hatchery"), MB_OK | MB_ICONINFORMATION);
        }
        refresh();
    }
}

void MachinePage::OnDestroy() {
    KillTimer(kTimerMachine);
    sheet_.sound().stop_all();
    CPropertyPage::OnDestroy();
}

} // namespace hatchery
