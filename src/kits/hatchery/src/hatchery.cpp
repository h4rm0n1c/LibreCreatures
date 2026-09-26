// The Hatchery: the nest page and the sheet.  The original's behaviour is in
// ../ORIGINAL.md; deviations are marked "Fix (bug N)".

#include "hatchery.hpp"
#include "hatchery_ids.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace hatchery {
namespace {

constexpr char kCompany[] = "Gameware Development";
constexpr char kProduct[] = "Creatures 1\\Hatchery";
constexpr char kVersion[] = "1.0";
constexpr char kNestValue[] = "Eggstra";
constexpr COLORREF kTransparent = RGB(0, 255, 0);  // the art's background

struct WindowLocation {
    std::int32_t left;
    std::int32_t top;
};
struct WindowSize {
    std::int32_t width;
    std::int32_t height;
};

HBITMAP load_bitmap(const std::string& path) {
    return static_cast<HBITMAP>(::LoadImageA(nullptr, path.c_str(), IMAGE_BITMAP, 0, 0,
                                             LR_LOADFROMFILE | LR_CREATEDIBSECTION));
}

CSize bitmap_size(HBITMAP bitmap) {
    BITMAP info = {};
    if (bitmap == nullptr || ::GetObject(bitmap, sizeof(info), &info) == 0) {
        return CSize(0, 0);
    }
    return CSize(info.bmWidth, info.bmHeight);
}

// Draws `bitmap` scaled into `target`, leaving its pure-green pixels out
// (a mask, as TransparentBlt would, without needing msimg32).
void draw_keyed(CDC& dc, HBITMAP bitmap, const CRect& target, int fade_percent = 0) {
    const CSize size = bitmap_size(bitmap);
    if (size.cx == 0) {
        return;
    }
    CDC source;
    source.CreateCompatibleDC(&dc);
    HGDIOBJ previous_source = ::SelectObject(source.GetSafeHdc(), bitmap);
    CDC mask_dc;
    mask_dc.CreateCompatibleDC(&dc);
    CBitmap mask;
    mask.CreateBitmap(size.cx, size.cy, 1, 1, nullptr);
    CBitmap* previous_mask = mask_dc.SelectObject(&mask);
    source.SetBkColor(kTransparent);
    mask_dc.BitBlt(0, 0, size.cx, size.cy, &source, 0, 0, SRCCOPY);  // green -> white
    // A copy with the green turned black, to OR over the hole the mask cuts.
    CDC image_dc;
    image_dc.CreateCompatibleDC(&dc);
    CBitmap image;
    image.CreateCompatibleBitmap(&dc, size.cx, size.cy);
    CBitmap* previous_image = image_dc.SelectObject(&image);
    image_dc.BitBlt(0, 0, size.cx, size.cy, &source, 0, 0, SRCCOPY);
    if (fade_percent > 0) {
        // Washed out, for a taken egg's outline.
        for (int y = 0; y < size.cy; ++y) {
            for (int x = 0; x < size.cx; ++x) {
                const COLORREF c = image_dc.GetPixel(x, y);
                image_dc.SetPixel(x, y, c1kitshell::blend(c, RGB(180, 150, 110), fade_percent));
            }
        }
    }
    // Mono to colour: the mask's 1 bits (the key) become the background
    // colour, white, so D & ~S leaves the key black.
    image_dc.SetBkColor(RGB(255, 255, 255));
    image_dc.SetTextColor(RGB(0, 0, 0));
    image_dc.BitBlt(0, 0, size.cx, size.cy, &mask_dc, 0, 0, 0x00220326);  // DSna: clear the key
    dc.SetStretchBltMode(COLORONCOLOR);
    dc.SetBkColor(RGB(255, 255, 255));
    dc.SetTextColor(RGB(0, 0, 0));
    dc.StretchBlt(target.left, target.top, target.Width(), target.Height(), &mask_dc, 0, 0,
                  size.cx, size.cy, SRCAND);
    dc.StretchBlt(target.left, target.top, target.Width(), target.Height(), &image_dc, 0, 0,
                  size.cx, size.cy, SRCPAINT);
    image_dc.SelectObject(previous_image);
    mask_dc.SelectObject(previous_mask);
    ::SelectObject(source.GetSafeHdc(), previous_source);
}

} // namespace

// ===========================================================================
// The nest
// ===========================================================================

BEGIN_MESSAGE_MAP(NestPage, c1kitshell::LayoutPage)
    ON_BN_CLICKED(kControlHatch, &NestPage::OnHatch)
    ON_BN_CLICKED(kControlRefill, &NestPage::OnRefill)
END_MESSAGE_MAP()

NestPage::NestPage(HatcherySheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringNestTab), sheet_(sheet) {}

NestPage::~NestPage() {
    for (HBITMAP& egg : eggs_) {
        if (egg != nullptr) ::DeleteObject(egg);
    }
    if (female_ != nullptr) ::DeleteObject(female_);
    if (male_ != nullptr) ::DeleteObject(male_);
}

// The 1996 kit's pictures, in the game's Hatchery folder.
void NestPage::load_art() {
    for (int i = 0; i < c1kit::kEggCount; ++i) {
        eggs_[i] = load_bitmap(sheet_.game_file("Hatchery\\egg" + std::to_string(i) + ".bmp"));
    }
    female_ = load_bitmap(sheet_.game_file("Hatchery\\female.bmp"));
    male_ = load_bitmap(sheet_.game_file("Hatchery\\male.bmp"));
}

void NestPage::create_controls() {
    load_art();
    nest_.create(*this, kControlNest, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
    nest_.set_mouse_handler([this](CPoint point, bool clicked) {
        const int egg = egg_at(point);
        if (clicked) {
            selected_ = egg;
            status_.SetWindowText(_T(""));
            refresh();
        } else if (egg != hover_) {
            hover_ = egg;
            nest_.redraw();
        }
    });
    // A double-click hatches, as in the 1996 kit.
    nest_.set_double_click_handler([this](CPoint point) {
        selected_ = egg_at(point);
        OnHatch();
    });
    make(hatch_, _T("BUTTON"), _T("Hatch this egg"), BS_PUSHBUTTON | WS_TABSTOP, kControlHatch);
    make(refill_, _T("BUTTON"), _T("Refill the nest"), BS_PUSHBUTTON | WS_TABSTOP, kControlRefill);
    make(status_, _T("STATIC"), _T(""), SS_LEFT, kControlStatus);
    refresh();
}

void NestPage::layout(int width, int height) {
    const int bottom = height - kMargin - button_height();
    place(nest_, kMargin, kMargin, width - 2 * kMargin, bottom - 2 * kMargin - text_height());
    place(status_, kMargin, bottom - kMargin - text_height() + 2, width - 2 * kMargin, text_height());
    place(hatch_, kMargin, bottom, 110, button_height());
    place(refill_, 2 * kMargin + 110, bottom, 110, button_height());
    place_close(width, height);
}

void NestPage::refresh() {
    if (!created_) {
        return;
    }
    const c1kit::Nest& nest = sheet_.nest();
    const bool can_hatch = selected_ >= 0 && nest.eggs[selected_] != c1kit::EggState::taken;
    hatch_.EnableWindow(can_hatch);
    // Fix (bug 1): once the six eggs are gone the nest can be refilled; the
    // original wanted an "Egg Disk" in drive A.
    refill_.EnableWindow(!nest.any_left());
    if (!nest.any_left()) {
        status_.SetWindowText(c1kitshell::load_string(kStringNoMoreEggs) +
                              _T("  Refill the nest for six more."));
    } else if (selected_ < 0) {
        CString shown;
        status_.GetWindowText(shown);
        if (shown.IsEmpty()) {
            status_.SetWindowText(_T("Click an egg to pick it; double-click to hatch it."));
        }
    }
    nest_.redraw();
}

CRect NestPage::egg_rect(const CRect& view, int slot) const {
    const int column = view.Width() / c1kit::kEggCount;
    const int egg_height = (std::min)(view.Height() - 70, column * 3 / 2);
    const int egg_width = egg_height * 2 / 3;
    const int left = view.left + slot * column + (column - egg_width) / 2;
    const int top = view.top + 16 + (view.Height() - 70 - egg_height) / 2;
    return CRect(left, top, left + egg_width, top + egg_height);
}

int NestPage::egg_at(CPoint point) const {
    for (int i = 0; i < c1kit::kEggCount; ++i) {
        CRect slot = egg_rect(view_rect_, i);
        slot.InflateRect(8, 8);
        if (slot.PtInRect(point)) {
            return i;
        }
    }
    return -1;
}

void NestPage::draw(CDC& dc, const CRect& rect) {
    view_rect_ = rect;
    // Straw.
    for (int y = rect.top; y < rect.bottom; ++y) {
        const int t = (y - rect.top) * 100 / (std::max)(1, static_cast<int>(rect.Height()));
        dc.FillSolidRect(rect.left, y, rect.Width(), 1,
                         c1kitshell::blend(RGB(92, 62, 32), RGB(196, 152, 84), t));
    }
    dc.SetBkMode(TRANSPARENT);
    const c1kit::Nest& nest = sheet_.nest();
    for (int i = 0; i < c1kit::kEggCount; ++i) {
        const CRect egg = egg_rect(rect, i);
        const c1kit::EggState state = nest.eggs[i];
        const bool taken = state == c1kit::EggState::taken;
        if (i == selected_ || (i == hover_ && !taken)) {
            CRect ring = egg;
            ring.InflateRect(6, 6);
            CBrush brush(i == selected_ ? RGB(255, 240, 120) : RGB(230, 210, 160));
            dc.FrameRect(ring, &brush);
            ring.DeflateRect(1, 1);
            dc.FrameRect(ring, &brush);
        }
        draw_keyed(dc, eggs_[i], egg, taken ? 75 : 0);
        // Its sex and parents, under it.
        const int column = rect.Width() / c1kit::kEggCount;
        const CRect label(rect.left + i * column, egg.bottom + 8, rect.left + (i + 1) * column,
                          egg.bottom + 26);
        CString sex = taken ? _T("Taken")
                            : state == c1kit::EggState::female ? _T("Female") : _T("Male");
        if (!taken) {
            HBITMAP symbol = state == c1kit::EggState::female ? female_ : male_;
            const CRect mark(label.left + column / 2 - 30, label.top, label.left + column / 2 - 14,
                             label.top + 16);
            draw_keyed(dc, symbol, mark);
        }
        dc.SetTextColor(taken ? RGB(210, 190, 160) : RGB(255, 255, 255));
        dc.DrawText(sex, CRect(label.left + (taken ? 0 : 20), label.top, label.right, label.bottom),
                    DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
        CString parents;
        parents.Format(_T("mum%d x dad%d"), i + 1, i + 1);
        dc.SetTextColor(RGB(235, 220, 190));
        dc.DrawText(parents, CRect(label.left, label.bottom, label.right, label.bottom + 18),
                    DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

void NestPage::OnHatch() {
    if (selected_ < 0) {
        return;
    }
    CString why;
    const int slot = selected_;
    if (sheet_.hatch(slot, why)) {
        CString done;
        done.Format(_T("Egg %d is in the incubator."), slot + 1);
        status_.SetWindowText(done);
        selected_ = -1;
    } else {
        status_.SetWindowText(why);
    }
    refresh();
}

void NestPage::OnRefill() {
    sheet_.refill();
    selected_ = -1;
    status_.SetWindowText(_T("Six new eggs."));
    refresh();
}

// ===========================================================================
// The sheet
// ===========================================================================

BEGIN_MESSAGE_MAP(HatcherySheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

HatcherySheet::HatcherySheet(CFont& default_font)
    : KitSheet(kStringToolName, 0), default_font_(default_font), nest_page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    AddPage(&nest_page_);
}

HatcherySheet::~HatcherySheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

bool HatcherySheet::create_window() {
    return Create(nullptr,
                  WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                      WS_MAXIMIZEBOX | WS_THICKFRAME,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

std::string HatcherySheet::game_file(const std::string& name) const {
    const CString directory = c1kitshell::game_directory_setting("Main Directory");
    return std::string(CStringA(directory)) + name;
}

int HatcherySheet::OnCreate(LPCREATESTRUCT create) {
    if (c1kitshell::KitSheet::OnCreate(create) == -1) {
        return -1;
    }
    SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(default_font_.GetSafeHandle()), 0);
    registry_ = c1kit::open_kit_settings(kCompany, kProduct, kVersion,
                                         c1kit::SettingsOpenPolicy::user_key_only);
    if (registry_ == nullptr || !registry_->is_open()) {
        return -1;
    }
    // The nest: the 1996 value, or six new eggs when there is none
    // (InitializeEggstraPattern @ 0x004045b0 regenerated it then too).
    char pattern[16] = {};
    if (!registry_->read_string(c1kit::SettingsScope::user, kNestValue, pattern, sizeof(pattern)) ||
        !c1kit::parse_nest(pattern, nest_)) {
        refill();
    }
    return 0;
}

BOOL HatcherySheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    SetWindowText(c1kitshell::load_string(kStringToolName));
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->AppendMenu(MF_SEPARATOR);
        menu->AppendMenu(MF_STRING, kSysCommandOnTop, _T("Always on &top"));
    }
    load_preferences();
    return result;
}

void HatcherySheet::load_preferences() {
    WindowSize size = {};
    if (registry_->read_binary(c1kit::SettingsScope::user, "Size", &size, sizeof(size)) &&
        size.width > 0 && size.height > 0) {
        set_window_size(CSize(size.width, size.height));
    }
    CRect window;
    GetWindowRect(&window);
    const int max_left = GetSystemMetrics(SM_CXSCREEN) - window.Width();
    const int max_top = GetSystemMetrics(SM_CYSCREEN) - window.Height();
    WindowLocation location = {0x100, 0x80};
    if (!registry_->read_binary(c1kit::SettingsScope::user, "Location", &location,
                                sizeof(location))) {
        location = {0x100, 0x80};
    }
    registry_->read_dword(c1kit::SettingsScope::user, "On Top", always_on_top_);
    const int left = location.left < max_left ? location.left : max_left;
    const int top = location.top < max_top ? location.top : max_top;
    SetWindowPos(always_on_top_ != 0 ? &wndTopMost : &wndNoTopMost, left < 0 ? 0 : left,
                 top < 0 ? 0 : top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, always_on_top_ != 0 ? MF_CHECKED : MF_UNCHECKED);
    }
    SetTimer(kTimerStartup, kStartupDelayMs, nullptr);
}

void HatcherySheet::save_preferences() {
    if (registry_ == nullptr || GetSafeHwnd() == nullptr) {
        return;
    }
    WINDOWPLACEMENT placement = {sizeof(placement)};
    GetWindowPlacement(&placement);
    const CRect window(placement.rcNormalPosition);
    const WindowLocation location = {window.left < 0 ? 0 : window.left,
                                     window.top < 0 ? 0 : window.top};
    const WindowSize size = {window.Width(), window.Height()};
    registry_->write_binary("Location", &location, sizeof(location));
    registry_->write_binary("Size", &size, sizeof(size));
    registry_->write_dword("On Top", always_on_top_);
}

void HatcherySheet::save_nest() {
    if (registry_ != nullptr) {
        registry_->write_string(kNestValue, c1kit::format_nest(nest_).c_str());
    }
}

void HatcherySheet::refill() {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    nest_ = c1kit::fresh_nest(static_cast<std::uint32_t>(std::rand()));
    save_nest();
}

// HandleEggSlotClick @ 0x00403c30: the egg's script, then the egg is taken.
// Fix (bug 2): the kit stays open; the original asked the game to close it
// after every egg.
bool HatcherySheet::hatch(int slot, CString& why) {
    if (slot < 0 || slot >= c1kit::kEggCount || nest_.eggs[slot] == c1kit::EggState::taken) {
        why = _T("That egg has been taken.");
        return false;
    }
    c1kit::MacroTransport* game = transport();
    if (!connected_ || game == nullptr) {
        why = _T("The Hatchery is not connected to the game.");
        return false;
    }
    const bool female = nest_.eggs[slot] == c1kit::EggState::female;
    if (!c1kit::execute_scheduled(*game, c1kit::hatch_script(slot, female).c_str(), false)) {
        why = _T("The game did not take the egg.");
        return false;
    }
    nest_.eggs[slot] = c1kit::EggState::taken;
    save_nest();
    return true;
}

void HatcherySheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connected_ = connect_to_game(kCommandBufferBytes);
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

void HatcherySheet::set_always_on_top(bool on) {
    always_on_top_ = on ? 1 : 0;
    SetWindowPos(on ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (CMenu* menu = GetSystemMenu(FALSE)) {
        menu->CheckMenuItem(kSysCommandOnTop, on ? MF_CHECKED : MF_UNCHECKED);
    }
}

void HatcherySheet::OnSysCommand(UINT id, LPARAM lparam) {
    if ((id & 0xfff0) == kSysCommandOnTop) {
        set_always_on_top(always_on_top_ == 0);
        return;
    }
    c1kitshell::KitSheet::OnSysCommand(id, lparam);
}

void HatcherySheet::before_game_quit() {
    save_preferences();
}

void HatcherySheet::OnClose() {
    request_game_quit();
}

void HatcherySheet::OnDestroy() {
    save_preferences();
    c1kitshell::KitSheet::OnDestroy();
}

} // namespace hatchery

// ===========================================================================
// Kit definition
// ===========================================================================

namespace {

c1kitshell::KitSheet* create_hatchery_window(CFont& font) {
    auto* sheet = new hatchery::HatcherySheet(font);
    if (!sheet->create_window()) {
        delete sheet;
        return nullptr;
    }
    return sheet;
}

c1kitshell::KitDefinition make_definition() {
    c1kitshell::KitDefinition kit;
    kit.identity.prog_id = "Hatchery.OLE";
    // {F10D5CA1-A8B7-11CF-BBF2-0020AF71E433}
    const GUID clsid = {0xf10d5ca1, 0xa8b7, 0x11cf,
                        {0xbb, 0xf2, 0x00, 0x20, 0xaf, 0x71, 0xe4, 0x33}};
    memcpy(kit.identity.clsid, &clsid, sizeof(clsid));
    kit.tool_slot = 0;
    kit.tool_value_prog_id = "Hatchery.OLE";
    kit.tool_name_string = hatchery::kStringToolName;
    kit.tool_help_string = hatchery::kStringToolHelp;
    kit.ole_init_failed_string = hatchery::kStringOleInitFailed;
    kit.create_main_window = &create_hatchery_window;
    return kit;
}

} // namespace

const c1kitshell::KitDefinition& c1kitshell::kit_definition() {
    static const KitDefinition kit = make_definition();
    return kit;
}
