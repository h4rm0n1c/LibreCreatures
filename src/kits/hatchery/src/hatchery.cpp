// The Hatchery: the nest page and the sheet.  The original's behaviour is in
// ../ORIGINAL.md; deviations are marked "Fix (bug N)".

#include "hatchery.hpp"
#include "hatchery_ids.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

CSize bitmap_size(HBITMAP bitmap) {
    BITMAP info = {};
    if (bitmap == nullptr || ::GetObject(bitmap, sizeof(info), &info) == 0) {
        return CSize(0, 0);
    }
    return CSize(info.bmWidth, info.bmHeight);
}

// Draws `bitmap` scaled into `target`, leaving its pure-green pixels out
// (a mask, as TransparentBlt would, without needing msimg32).  Scaling is
// nearest-neighbour, so a whole-number scale keeps pixel art crisp.
// `fade_percent` washes it towards `fade_to`.
void draw_keyed(CDC& dc, HBITMAP bitmap, const CRect& target, int fade_percent = 0,
                COLORREF fade_to = RGB(0, 0, 0)) {
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
        for (int y = 0; y < size.cy; ++y) {
            for (int x = 0; x < size.cx; ++x) {
                image_dc.SetPixel(x, y, c1kitshell::blend(image_dc.GetPixel(x, y), fade_to,
                                                          fade_percent));
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

// A game sprite frame as a bitmap whose transparent pixels are the key
// colour, for draw_keyed.
HBITMAP sprite_bitmap(const c1kit::GameSprite& sprite, const c1kit::GamePalette& palette) {
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = sprite.width;
    info.bmiHeader.biHeight = -sprite.height;  // top row first
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = ::CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr || bits == nullptr) {
        return bitmap;
    }
    auto* out = static_cast<std::uint32_t*>(bits);
    for (std::size_t i = 0; i < sprite.pixels.size(); ++i) {
        const std::uint8_t index = sprite.pixels[i];
        const c1kit::PaletteColour c = palette[index];
        out[i] = index == 0 ? 0x0000ff00u  // the key, RGB(0, 255, 0)
                            : (static_cast<std::uint32_t>(c.red) << 16) |
                                  (static_cast<std::uint32_t>(c.green) << 8) | c.blue;
    }
    return bitmap;
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::vector<std::uint8_t> bytes;
    if (FILE* file = std::fopen(path.c_str(), "rb")) {
        std::uint8_t buffer[4096];
        std::size_t got;
        while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
            bytes.insert(bytes.end(), buffer, buffer + got);
        }
        std::fclose(file);
    }
    return bytes;
}

// The panel's palette: a warm incubator dark, lamp light, and the gold of
// the selection and the sex symbols.
struct Rgbf {
    double r, g, b;
};
constexpr Rgbf kPanelTop = {38, 31, 28};
constexpr Rgbf kPanelBottom = {50, 39, 33};
constexpr Rgbf kGround = {30, 25, 22};
constexpr Rgbf kGroundEdge = {70, 58, 48};
constexpr Rgbf kLamp = {255, 190, 110};
constexpr Rgbf kGold = {226, 178, 104};
constexpr COLORREF kLabel = RGB(240, 230, 216);
constexpr COLORREF kLabelTaken = RGB(150, 138, 124);
constexpr COLORREF kPanelMid = RGB(44, 35, 30);  // what a hatched egg fades into

Rgbf mix(Rgbf a, Rgbf b, double t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

double clamp01(double v) {
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

// Distance from p to the segment a-b.
double segment_distance(double px, double py, double ax, double ay, double bx, double by) {
    const double dx = bx - ax, dy = by - ay;
    const double t = clamp01(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy + 1e-9));
    const double ex = ax + t * dx - px, ey = ay + t * dy - py;
    return std::sqrt(ex * ex + ey * ey);
}

// Signed distance from p to a rounded rectangle (negative inside).
double rounded_rect_distance(double px, double py, double left, double top, double right,
                             double bottom, double radius) {
    const double cx = (left + right) / 2, cy = (top + bottom) / 2;
    const double hx = (right - left) / 2 - radius, hy = (bottom - top) / 2 - radius;
    const double qx = std::abs(px - cx) - hx, qy = std::abs(py - cy) - hy;
    const double ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    return std::sqrt(ox * ox + oy * oy) + (std::min)((std::max)(qx, qy), 0.0) - radius;
}

// The strokes of a sex symbol about 11 pixels tall, centred on (x, y):
// the circle, and a cross below it (female) or an arrow off it (male).
double symbol_distance(bool female, double px, double py, double x, double y) {
    const double radius = 3.4;
    if (female) {
        const double cy = y - 2.3;
        double d = std::abs(std::hypot(px - x, py - cy) - radius);
        d = (std::min)(d, segment_distance(px, py, x, cy + radius, x, y + 5.6));
        d = (std::min)(d, segment_distance(px, py, x - 2.4, y + 3.3, x + 2.4, y + 3.3));
        return d;
    }
    const double cx = x - 1.4, cy = y + 1.4;
    const double tip_x = x + 4.2, tip_y = y - 4.2;
    double d = std::abs(std::hypot(px - cx, py - cy) - radius);
    d = (std::min)(d, segment_distance(px, py, cx + 2.4, cy - 2.4, tip_x, tip_y));
    d = (std::min)(d, segment_distance(px, py, tip_x - 3.2, tip_y, tip_x, tip_y));
    d = (std::min)(d, segment_distance(px, py, tip_x, tip_y, tip_x, tip_y + 3.2));
    return d;
}

constexpr int kSymbolWidth = 10;
constexpr int kSymbolGap = 5;

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
    for (auto& looks : egg_art_) {
        for (HBITMAP& bitmap : looks) {
            if (bitmap != nullptr) ::DeleteObject(bitmap);
        }
    }
}

// The eggs from the player's own Images\eggs.spr in the game's palette.
void NestPage::load_art() {
    std::vector<c1kit::GameSprite> sprites;
    c1kit::GamePalette palette;
    if (!c1kit::parse_game_sprites(read_file(sheet_.game_file("Images\\eggs.spr")), sprites) ||
        !c1kit::parse_palette_dta(read_file(sheet_.game_file("Palettes\\palette.dta")),
                                  palette) ||
        sprites.size() <
            static_cast<std::size_t>(c1kit::kEggCount * c1kit::kEggSpriteFramesPerEgg)) {
        return;  // drawn as empty slots
    }
    const int stages[kLooks] = {c1kit::kEggSpriteWhole, c1kit::kEggSpriteIncubating,
                                c1kit::kEggSpriteCracked};
    for (int egg = 0; egg < c1kit::kEggCount; ++egg) {
        for (int look = 0; look < kLooks; ++look) {
            const c1kit::GameSprite& sprite =
                sprites[static_cast<std::size_t>(c1kit::egg_sprite_frame(egg, stages[look]))];
            egg_art_[egg][look] = sprite_bitmap(sprite, palette);
        }
    }
    const c1kit::GameSprite& first =
        sprites[static_cast<std::size_t>(c1kit::egg_sprite_frame(0, c1kit::kEggSpriteWhole))];
    egg_size_ = CSize(first.width, first.height);
}

void NestPage::create_controls() {
    load_art();
    nest_.create(*this, kControlNest, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
    nest_.set_mouse_handler([this](CPoint point, bool clicked) {
        const int egg = egg_at(point);
        if (clicked) {
            selected_ = egg;
            describe_selection();
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
    make(hatch_, _T("BUTTON"), _T("Hatch this egg"), BS_DEFPUSHBUTTON | WS_TABSTOP, kControlHatch);
    make(refill_, _T("BUTTON"), _T("Refill the nest"), BS_PUSHBUTTON | WS_TABSTOP, kControlRefill);
    make(status_, _T("STATIC"), _T(""), SS_LEFT | SS_NOPREFIX, kControlStatus);
    refresh();
}

// The nest across the top, a line about the chosen egg under it, then
// Refill on the left and Hatch beside Close on the right.
void NestPage::layout(int width, int height) {
    const int buttons = height - kMargin - button_height();
    const int status_top = buttons - kMargin - text_height() - 4;
    place(nest_, kMargin, kMargin, width - 2 * kMargin, status_top - 2 * kMargin);
    place(status_, kMargin, status_top, width - 2 * kMargin, text_height() + 4);
    place(refill_, kMargin, buttons, 120, button_height());
    place(hatch_, width - kMargin - 84 - kMargin - 120, buttons, 120, button_height());
    place_close(width, height);
}

void NestPage::describe_selection() {
    const c1kit::Nest& nest = sheet_.nest();
    if (selected_ < 0) {
        status_.SetWindowText(_T("Double-click an egg to hatch it, or pick one and press Hatch."));
        return;
    }
    CString text;
    if (nest.eggs[selected_] == c1kit::EggState::taken) {
        text.Format(_T("Egg %d has hatched."), selected_ + 1);
    } else {
        text.Format(_T("Egg %d: %s.  Press Hatch to put it in the incubator."), selected_ + 1,
                    nest.eggs[selected_] == c1kit::EggState::female ? _T("female") : _T("male"));
    }
    status_.SetWindowText(text);
}

void NestPage::refresh() {
    // (Not created_: that is set after create_controls, which calls this.)
    if (hatch_.GetSafeHwnd() == nullptr) {
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
    } else {
        CString shown;
        status_.GetWindowText(shown);
        if (shown.IsEmpty()) {
            describe_selection();
        }
    }
    nest_.redraw();
}

// The ground runs along the bottom, just deep enough for the labels; the
// eggs stand on it at the largest whole-number scale that fits, lit from
// above.
void NestPage::fit(const CRect& view) {
    const int frame_width = egg_size_.cx > 0 ? egg_size_.cx : 48;
    const int frame_height = egg_size_.cy > 0 ? egg_size_.cy : 60;
    const int slot = view.Width() / c1kit::kEggCount;
    baseline_ = static_cast<int>(view.bottom) - (text_height() + 24);
    const int room = baseline_ - static_cast<int>(view.top) - 20;
    scale_ = (std::max)(1, (std::min)((slot - 8) / frame_width, room / frame_height));
}

CRect NestPage::egg_rect(int slot) const {
    const int frame_width = egg_size_.cx > 0 ? egg_size_.cx : 48;
    const int frame_height = egg_size_.cy > 0 ? egg_size_.cy : 60;
    const int column = view_rect_.Width() / c1kit::kEggCount;
    const int centre = view_rect_.left + slot * column + column / 2;
    const int width = frame_width * scale_, height = frame_height * scale_;
    return CRect(centre - width / 2, baseline_ - height, centre - width / 2 + width, baseline_);
}

int NestPage::egg_at(CPoint point) const {
    const int column = view_rect_.Width() / c1kit::kEggCount;
    if (column <= 0 || !view_rect_.PtInRect(point)) {
        return -1;
    }
    const int slot = (point.x - view_rect_.left) / column;
    return slot < c1kit::kEggCount ? slot : -1;
}

void NestPage::render_panel(const CRect& view) {
    const int width = view.Width(), height = view.Height();
    panel_.assign(static_cast<std::size_t>(width) * height, 0);
    const c1kit::Nest& nest = sheet_.nest();
    const int column = width / c1kit::kEggCount;
    const double base = baseline_ - view.top;
    const double egg_height = (egg_size_.cy > 0 ? egg_size_.cy : 60) * scale_;
    const double lamp_y = base - egg_height * 0.55;
    // The labels' line, and each symbol's centre: on the capitals' middle,
    // from the page font's metrics, so it sits level with the word.
    CClientDC dc(&nest_);
    CFont* previous_font = dc.SelectObject(GetFont());
    TEXTMETRIC metrics = {};
    dc.GetTextMetrics(&metrics);
    const double label_top = base + 12;
    const double symbol_y = label_top + metrics.tmInternalLeading +
                            (metrics.tmAscent - metrics.tmInternalLeading) / 2.0;
    double symbol_x[c1kit::kEggCount] = {};
    for (int i = 0; i < c1kit::kEggCount; ++i) {
        if (nest.eggs[i] == c1kit::EggState::taken) continue;
        const CSize word = dc.GetTextExtent(nest.eggs[i] == c1kit::EggState::female ? _T("Female")
                                                                                   : _T("Male"));
        const double total = kSymbolWidth + kSymbolGap + word.cx;
        symbol_x[i] = column * i + column / 2.0 - total / 2 + kSymbolWidth / 2.0;
    }
    dc.SelectObject(previous_font);

    for (int y = 0; y < height; ++y) {
        const double t = static_cast<double>(y) / (std::max)(1, height - 1);
        for (int x = 0; x < width; ++x) {
            const double px = x + 0.5, py = y + 0.5;
            Rgbf c = mix(kPanelTop, kPanelBottom, t);
            const int slot = (std::min)(c1kit::kEggCount - 1, x / (std::max)(1, column));
            const double cx = column * slot + column / 2.0;
            // Lamp light over each egg.
            const double lx = (px - cx) / (column * 0.62), ly = (py - lamp_y) / (egg_height * 0.95);
            c = mix(c, kLamp, 0.16 * std::exp(-(lx * lx + ly * ly) * 1.6));
            // The ground the eggs stand on.
            if (py > base + 1) {
                c = py < base + 2 ? kGroundEdge : kGround;
            }
            // A soft shadow under each egg.
            const double sx = (px - cx) / (egg_height * 0.3), sy = (py - (base + 1)) / 4.5;
            c = mix(c, {0, 0, 0}, 0.55 * std::exp(-(sx * sx + sy * sy) * 2.2));
            // The egg pointed at, and the one picked.
            if (slot == selected_ || slot == hover_) {
                const double d = rounded_rect_distance(px, py, cx - column / 2.0 + 5, 8,
                                                       cx + column / 2.0 - 5, height - 8, 6);
                if (slot == selected_) {
                    if (d < 0) c = mix(c, kLamp, 0.08);
                    c = mix(c, kGold, clamp01(1.5 - std::abs(d + 1)));
                } else if (nest.eggs[slot] != c1kit::EggState::taken) {
                    c = mix(c, kGold, 0.45 * clamp01(1.0 - std::abs(d + 0.5)));
                }
            }
            // The sex symbol, anti-aliased.
            if (nest.eggs[slot] != c1kit::EggState::taken &&
                std::abs(py - symbol_y) < 8 && std::abs(px - symbol_x[slot]) < 8) {
                const double d = symbol_distance(nest.eggs[slot] == c1kit::EggState::female, px, py,
                                                 symbol_x[slot], symbol_y);
                c = mix(c, kGold, clamp01(1.25 - d * 1.5));
            }
            panel_[static_cast<std::size_t>(y) * width + x] =
                (static_cast<std::uint32_t>(c.r) << 16) | (static_cast<std::uint32_t>(c.g) << 8) |
                static_cast<std::uint32_t>(c.b);
        }
    }
}

void NestPage::draw(CDC& dc, const CRect& rect) {
    view_rect_ = rect;
    fit(rect);
    render_panel(rect);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = rect.Width();
    info.bmiHeader.biHeight = -rect.Height();
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    ::SetDIBitsToDevice(dc.GetSafeHdc(), rect.left, rect.top, rect.Width(), rect.Height(), 0, 0,
                        0, rect.Height(), panel_.data(), &info, DIB_RGB_COLORS);

    const c1kit::Nest& nest = sheet_.nest();
    const int column = rect.Width() / c1kit::kEggCount;
    CFont* previous_font = dc.SelectObject(GetFont());
    dc.SetBkMode(TRANSPARENT);
    for (int i = 0; i < c1kit::kEggCount; ++i) {
        const bool taken = nest.eggs[i] == c1kit::EggState::taken;
        const Look look = taken ? kCracked : i == selected_ ? kIncubating : kWhole;
        draw_keyed(dc, egg_art_[i][look], egg_rect(i), taken ? 45 : 0, kPanelMid);
        // The sex beside its symbol (drawn in the panel), or "Hatched".
        const CString word = taken ? _T("Hatched")
                                   : nest.eggs[i] == c1kit::EggState::female ? _T("Female")
                                                                             : _T("Male");
        const CSize extent = dc.GetTextExtent(word);
        const int total = extent.cx + (taken ? 0 : kSymbolWidth + kSymbolGap);
        const int left = rect.left + column * i + column / 2 - total / 2 +
                         (taken ? 0 : kSymbolWidth + kSymbolGap);
        dc.SetTextColor(taken ? kLabelTaken : kLabel);
        dc.TextOut(left, baseline_ + 12, word);
    }
    dc.SelectObject(previous_font);
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
    // The saved size, but never smaller than the default, which is what the
    // eggs need at twice their size (an older, smaller layout saved one).
    CRect window;
    GetWindowRect(&window);
    WindowSize size = {};
    if (registry_->read_binary(c1kit::SettingsScope::user, "Size", &size, sizeof(size)) &&
        size.width > 0 && size.height > 0) {
        set_window_size(CSize((std::max)(static_cast<int>(size.width), window.Width()),
                              (std::max)(static_cast<int>(size.height), window.Height())));
    }
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
