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

} // namespace

// A mask, as TransparentBlt would make, without needing msimg32.  Scaling is
// nearest-neighbour, so a whole-number scale keeps pixel art crisp.
void draw_keyed(CDC& dc, HBITMAP bitmap, const CRect& target, int fade_percent, COLORREF fade_to,
                COLORREF key) {
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
    source.SetBkColor(key);
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

namespace {

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
            if (egg != selected_) {
                selected_ = egg;
                play_selection_sound();
            }
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
        const CString parents = sheet_.egg_parents_text(selected_);
        text.Format(_T("Egg %d: %s%s%s.  Press Hatch to put it in the incubator."), selected_ + 1,
                    nest.eggs[selected_] == c1kit::EggState::female ? _T("female") : _T("male"),
                    parents.IsEmpty() ? _T("") : _T(", "), static_cast<LPCTSTR>(parents));
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
    // Room for the labels: the sex, and with Scramble Eggs the parents.
    const int label_lines = sheet_.egg_parents_text(0).IsEmpty() ? 1 : 2;
    baseline_ = static_cast<int>(view.bottom) - (label_lines * text_height() + 24);
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
        // Scramble Eggs: the pair it will be crossed from.
        const CString parents = taken ? CString() : sheet_.egg_parents_text(i);
        if (!parents.IsEmpty()) {
            const CSize parents_extent = dc.GetTextExtent(parents);
            dc.TextOut(rect.left + column * i + column / 2 - parents_extent.cx / 2,
                       baseline_ + 12 + extent.cy, parents);
        }
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
        stop_tone();
        sheet_.sound().play("hslt", false);  // the 1996 kit's hatch
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
    stop_tone();
    selected_ = -1;
    status_.SetWindowText(_T("Six new eggs."));
    refresh();
}

// ===========================================================================
// The Designer
// ===========================================================================

namespace {

constexpr const TCHAR* kSliderNames[] = {_T("Red tint"), _T("Green tint"), _T("Blue tint"),
                                         _T("Hue rotation"), _T("Colour swap")};
constexpr const char* kSliderSettings[] = {"Designer Red", "Designer Green", "Designer Blue",
                                           "Designer Hue", "Designer Swap"};
constexpr const TCHAR* kGeneticsNames[] = {
    _T("Eat better: rising drives do not punish (hunger, tiredness, sleepiness, boredom, pleasure, sex drive)"),
    _T("No death from old age"),
    _T("Lock learning genes against mutation"),
    _T("Hardier babies: more birth reserves and immunity to every antigen"),
};
constexpr const char* kGeneticsSettings[] = {"Designer Eat Better", "Designer No Old Age",
                                             "Designer Lock Learning", "Designer Hardier"};

} // namespace

BEGIN_MESSAGE_MAP(DesignerPage, c1kitshell::LayoutPage)
    ON_BN_CLICKED(kControlDesignColour, &DesignerPage::OnColourChanged)
    ON_BN_CLICKED(kControlDesignRandomise, &DesignerPage::OnRandomise)
    ON_BN_CLICKED(kControlDesignMakeEgg, &DesignerPage::OnMakeEgg)
    ON_BN_CLICKED(kControlDesignEatBetter, &DesignerPage::OnChoiceChanged)
    ON_BN_CLICKED(kControlDesignNoOldAge, &DesignerPage::OnChoiceChanged)
    ON_BN_CLICKED(kControlDesignLockLearning, &DesignerPage::OnChoiceChanged)
    ON_BN_CLICKED(kControlDesignHardier, &DesignerPage::OnChoiceChanged)
    ON_CBN_SELCHANGE(kControlDesignSex, &DesignerPage::OnChoiceChanged)
    ON_WM_HSCROLL()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

DesignerPage::DesignerPage(HatcherySheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringDesignerTab), sheet_(sheet) {}

DesignerPage::~DesignerPage() {
    free_faces();
}

void DesignerPage::create_controls() {
    make(labels_[2], _T("STATIC"), _T("Sex"), SS_LEFT, 0);
    load_faces();
    faces_.create(*this, kControlDesignFaces, [this](CDC& dc, const CRect& rect) { draw_faces(dc, rect); });
    faces_.set_mouse_handler([this](CPoint point, bool clicked) { on_faces_mouse(point, clicked); });
    make(sex_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, kControlDesignSex);
    sex_.AddString(_T("Either"));
    sex_.AddString(_T("Male"));
    sex_.AddString(_T("Female"));
    make(colour_, _T("BUTTON"), _T("Colour this creature"), BS_AUTOCHECKBOX | WS_TABSTOP,
         kControlDesignColour);
    for (int i = 0; i < kSliders; ++i) {
        make(labels_[3 + i], _T("STATIC"), kSliderNames[i], SS_LEFT, 0);
        make(sliders_[i], TRACKBAR_CLASS, _T(""), TBS_HORZ | TBS_NOTICKS | WS_TABSTOP,
             kControlDesignTint + i);
        sliders_[i].SetRange(0, 255);
        sliders_[i].SetPos(0x80);
    }
    make(randomise_, _T("BUTTON"), _T("Randomise"), BS_PUSHBUTTON | WS_TABSTOP,
         kControlDesignRandomise);
    make(swatch_, _T("STATIC"), _T(""), SS_LEFT | WS_BORDER, kControlDesignSwatch);
    for (int i = 0; i < 4; ++i) {
        make(genetics_[i], _T("BUTTON"), kGeneticsNames[i], BS_AUTOCHECKBOX | WS_TABSTOP,
             kControlDesignEatBetter + i);
    }
    make(make_egg_, _T("BUTTON"), _T("Make this egg"), BS_DEFPUSHBUTTON | WS_TABSTOP,
         kControlDesignMakeEgg);
    make(status_, _T("STATIC"), _T(""), SS_LEFT | SS_NOPREFIX | SS_EDITCONTROL,
         kControlDesignStatus);
    load_settings();
    OnColourChanged();
}

// The mothers' faces and the fathers' at the top; the sex, the colour
// switch and Randomise on one line; the colour sliders beside the swatch,
// then the genetics switches; status, Make this egg and Close at the bottom.
void DesignerPage::layout(int width, int height) {
    const int line = button_height();
    int y = kMargin;
    place(faces_, kMargin, y, width - 2 * kMargin, 2 * kFaceTile + kFaceGap);
    y += 2 * kFaceTile + kFaceGap + kMargin;
    place(labels_[2], kMargin, y + 4, 30, text_height());
    place(sex_, kMargin + 32, y, 90, line * 8);
    place(colour_, kMargin + 140, y, 200, line);
    place(randomise_, kMargin + 350, y, 90, line);
    y += line + 2;
    const int slider_left = kMargin + 90;
    const int swatch_size = 5 * (line - 2);
    const int slider_width = (std::max)(120, width - slider_left - swatch_size - 3 * kMargin);
    for (int i = 0; i < kSliders; ++i) {
        place(labels_[3 + i], kMargin, y + i * (line - 2) + 3, 88, text_height());
        place(sliders_[i], slider_left, y + i * (line - 2), slider_width, line - 2);
    }
    place(swatch_, slider_left + slider_width + kMargin, y, swatch_size, swatch_size);
    y += kSliders * (line - 2) + kMargin;
    for (int i = 0; i < 4; ++i) {
        place(genetics_[i], kMargin, y, width - 2 * kMargin, line - 4);
        y += line - 4;
    }
    const int buttons = height - kMargin - button_height();
    // Two lines: what was made, and what was changed in its genome.
    place(status_, kMargin, buttons - 2 * text_height() - 8, width - 2 * kMargin,
          2 * text_height() + 4);
    place(make_egg_, width - kMargin - 84 - kMargin - 120, buttons, 120, button_height());
    place_close(width, height);
}

void DesignerPage::load_settings() {
    c1kit::KitSettings* registry = sheet_.registry();
    std::uint32_t value = 0;
    const auto read = [registry](const char* name, std::uint32_t fallback) {
        std::uint32_t found = fallback;
        if (registry != nullptr) {
            registry->read_dword(c1kit::SettingsScope::user, name, found);
        }
        return found;
    };
    // Saved as the parent's number less one (as the old lists' places were).
    picked_[0] = parent_or_first(0, static_cast<int>((std::min)(read("Designer Mother", 0), 8u)) + 1);
    picked_[1] = parent_or_first(1, static_cast<int>((std::min)(read("Designer Father", 0), 8u)) + 1);
    sex_.SetCurSel(static_cast<int>((std::min)(read("Designer Sex", 0), 2u)));
    colour_.SetCheck(read("Designer Colour", 0) != 0 ? BST_CHECKED : BST_UNCHECKED);
    for (int i = 0; i < kSliders; ++i) {
        sliders_[i].SetPos(static_cast<int>((std::min)(read(kSliderSettings[i], 0x80), 255u)));
    }
    for (int i = 0; i < 4; ++i) {
        genetics_[i].SetCheck(read(kGeneticsSettings[i], 0) != 0 ? BST_CHECKED : BST_UNCHECKED);
    }
    static_cast<void>(value);
}

void DesignerPage::save_settings() {
    c1kit::KitSettings* registry = sheet_.registry();
    if (registry == nullptr) {
        return;
    }
    registry->write_dword("Designer Mother", static_cast<std::uint32_t>(picked_[0] - 1));
    registry->write_dword("Designer Father", static_cast<std::uint32_t>(picked_[1] - 1));
    registry->write_dword("Designer Sex", static_cast<std::uint32_t>(sex_.GetCurSel()));
    registry->write_dword("Designer Colour", colour_.GetCheck() == BST_CHECKED ? 1 : 0);
    for (int i = 0; i < kSliders; ++i) {
        registry->write_dword(kSliderSettings[i], static_cast<std::uint32_t>(sliders_[i].GetPos()));
    }
    for (int i = 0; i < 4; ++i) {
        registry->write_dword(kGeneticsSettings[i], genetics_[i].GetCheck() == BST_CHECKED ? 1 : 0);
    }
}

c1kit::EggColours DesignerPage::colours() const {
    c1kit::EggColours colours;
    for (int i = 0; i < 3; ++i) {
        colours.tint[i] = static_cast<std::uint8_t>(sliders_[i].GetPos());
    }
    colours.hue_rotation = static_cast<std::uint8_t>(sliders_[3].GetPos());
    colours.colour_swap = static_cast<std::uint8_t>(sliders_[4].GetPos());
    return colours;
}

c1kit::DesignerChoices DesignerPage::choices() const {
    c1kit::DesignerChoices choices;
    choices.eat_better = genetics_[0].GetCheck() == BST_CHECKED;
    choices.no_old_age_death = genetics_[1].GetCheck() == BST_CHECKED;
    choices.lock_learning = genetics_[2].GetCheck() == BST_CHECKED;
    choices.hardier_babies = genetics_[3].GetCheck() == BST_CHECKED;
    return choices;
}

// The swatch shows the three tints as one colour (0x80 each is no tint, a
// mid grey); hue rotation and colour swap act on the creature's own art.
void DesignerPage::update_swatch() {
    const c1kit::EggColours tint = colours();
    swatch_brush_.DeleteObject();
    swatch_brush_.CreateSolidBrush(RGB(tint.tint[0], tint.tint[1], tint.tint[2]));
    swatch_.Invalidate();
}

HBRUSH DesignerPage::OnCtlColor(CDC* dc, CWnd* control, UINT type) {
    if (control != nullptr && control->GetDlgCtrlID() == static_cast<int>(kControlDesignSwatch) &&
        swatch_brush_.GetSafeHandle() != nullptr) {
        return static_cast<HBRUSH>(swatch_brush_.GetSafeHandle());
    }
    return c1kitshell::LayoutPage::OnCtlColor(dc, control, type);
}

void DesignerPage::OnColourChanged() {
    const BOOL on = colour_.GetCheck() == BST_CHECKED;
    for (int i = 0; i < kSliders; ++i) {
        sliders_[i].EnableWindow(on);
        labels_[3 + i].EnableWindow(on);
    }
    randomise_.EnableWindow(on);
    swatch_.ShowWindow(on ? SW_SHOW : SW_HIDE);
    update_swatch();
    save_settings();
}

void DesignerPage::OnRandomise() {
    const c1kit::EggColours random = c1kit::random_egg_colours([] { return std::rand(); });
    for (int i = 0; i < 3; ++i) {
        sliders_[i].SetPos(random.tint[i]);
    }
    sliders_[3].SetPos(random.hue_rotation);
    sliders_[4].SetPos(random.colour_swap);
    update_swatch();
    save_settings();
}

void DesignerPage::OnHScroll(UINT code, UINT position, CScrollBar* bar) {
    c1kitshell::LayoutPage::OnHScroll(code, position, bar);
    update_swatch();
    if (code == TB_ENDTRACK || code == TB_THUMBPOSITION) {
        save_settings();
    }
}

void DesignerPage::OnChoiceChanged() {
    save_settings();
}

// Each parent's face: its genes' head (c1kit::parent_look), from the game's
// own head sprites in the game's own order, looking out and content.  Mums
// as females, dads as males.  A parent with no genome file has no tile; one
// whose head art cannot be found gets an empty tile.
void DesignerPage::load_faces() {
    free_faces();
    c1kit::GamePalette palette{};
    std::vector<std::string> images;
    const bool have_palette = sheet_.game_art_places(palette, images);
    for (int row = 0; row < 2; ++row) {
        numbers_[row].clear();
        const bool female = row == 0;
        for (int n = 1; n <= c1kit::kMaxParents; ++n) {
            std::vector<std::uint8_t> bytes;
            std::vector<c1kit::Gene> genes;
            if (!sheet_.parent_genome(female, n, bytes)) continue;
            numbers_[row].push_back(n);
            if (!c1kit::parse_genome(bytes, genes)) continue;
            if (!have_palette) continue;
            for (const std::string& name : c1kit::head_sprite_names(c1kit::parent_look(genes, female), female)) {
                std::vector<c1kit::GameSprite> sprites;
                for (const std::string& directory : images) {
                    if (c1kit::parse_game_sprites(read_file(directory + name + ".spr"), sprites) &&
                        !sprites.empty()) {
                        break;
                    }
                    sprites.clear();
                }
                if (sprites.empty()) continue;
                const std::size_t frame = (std::min)(static_cast<std::size_t>(c1kit::kParentFaceFrame),
                                                     sprites.size() - 1);
                face_art_[row][n - 1] = sprite_bitmap(sprites[frame], palette);
                break;
            }
        }
    }
}

void DesignerPage::free_faces() {
    for (auto& row : face_art_) {
        for (HBITMAP& face : row) {
            if (face != nullptr) ::DeleteObject(face);
            face = nullptr;
        }
    }
}

int DesignerPage::parent_or_first(int row, int number) const {
    const std::vector<int>& there = numbers_[row];
    if (std::find(there.begin(), there.end(), number) != there.end()) return number;
    return there.empty() ? 1 : there.front();
}

// A row's tiles start after its label.
CRect DesignerPage::face_tile(int row, int column) const {
    const int left = kFaceLabelWidth + column * (kFaceTileWidth + kFaceGap);
    const int top = row * (kFaceTile + kFaceGap);
    return CRect(left, top, left + kFaceTileWidth, top + kFaceTile);
}

bool DesignerPage::face_at(CPoint point, int& row, int& number) const {
    for (int r = 0; r < 2; ++r) {
        for (std::size_t column = 0; column < numbers_[r].size(); ++column) {
            if (face_tile(r, static_cast<int>(column)).PtInRect(point)) {
                row = r;
                number = numbers_[r][column];
                return true;
            }
        }
    }
    return false;
}

void DesignerPage::on_faces_mouse(CPoint point, bool clicked) {
    int row = -1;
    int number = 0;
    const bool over = face_at(point, row, number);
    if (clicked && over) {
        picked_[row] = number;
        save_settings();
    }
    if (!over) row = -1;
    if (clicked || row != hover_row_ || number != hover_number_) {
        hover_row_ = row;
        hover_number_ = number;
        faces_.redraw();
    }
}

// "Mother" and "Father", then a tile per parent: its face fitted to the
// tile (shrunk if it is larger, enlarged only by whole numbers so it stays
// crisp), and its number in the corner.  The picked
// one is outlined; the one under the pointer gets a darker edge.
void DesignerPage::draw_faces(CDC& dc, const CRect& rect) {
    CFont* old_font = dc.SelectObject(GetFont());
    dc.SetBkMode(TRANSPARENT);
    const TCHAR* labels[2] = {_T("Mother"), _T("Father")};
    for (int row = 0; row < 2; ++row) {
        CRect label(rect.left, row * (kFaceTile + kFaceGap), rect.left + kFaceLabelWidth - 6,
                    row * (kFaceTile + kFaceGap) + kFaceTile);
        dc.SetTextColor(GetSysColor(COLOR_BTNTEXT));
        dc.DrawText(labels[row], &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (numbers_[row].empty()) {
            CRect none(label.right + 6, label.top, rect.right, label.bottom);
            dc.SetTextColor(RGB(110, 104, 96));
            dc.DrawText(row == 0 ? _T("No mum1.gen to mum9.gen in the Genetics folder.")
                                 : _T("No dad1.gen to dad9.gen in the Genetics folder."),
                        &none, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            continue;
        }
        for (std::size_t column = 0; column < numbers_[row].size(); ++column) {
            const int number = numbers_[row][column];
            const CRect tile = face_tile(row, static_cast<int>(column));
            const bool picked = picked_[row] == number;
            const bool hover = hover_row_ == row && hover_number_ == number;
            CBrush fill(picked ? RGB(255, 246, 222) : RGB(255, 255, 255));
            CPen edge(PS_SOLID, picked ? 2 : 1,
                      picked ? RGB(196, 136, 40) : hover ? RGB(150, 144, 136) : RGB(208, 204, 196));
            CBrush* old_brush = dc.SelectObject(&fill);
            CPen* old_pen = dc.SelectObject(&edge);
            dc.RoundRect(tile, CPoint(8, 8));
            dc.SelectObject(old_pen);
            dc.SelectObject(old_brush);
            if (HBITMAP face = face_art_[row][number - 1]) {
                BITMAP info = {};
                ::GetObject(face, sizeof(info), &info);
                const double room_w = kFaceTileWidth - 8;
                const double room_h = kFaceTile - 8;
                double scale = (std::min)(room_w / (std::max)(1L, info.bmWidth),
                                          room_h / (std::max)(1L, info.bmHeight));
                if (scale >= 2.0) scale = std::floor(scale);
                const int w = (std::max)(1, static_cast<int>(info.bmWidth * scale));
                const int h = (std::max)(1, static_cast<int>(info.bmHeight * scale));
                draw_keyed(dc, face, CRect(CPoint(tile.left + (tile.Width() - w) / 2, tile.top + (tile.Height() - h) / 2),
                                           CSize(w, h)));
            }
            CString label_number;
            label_number.Format(_T("%d"), number);
            CRect corner(tile.left + 5, tile.top + 2, tile.right - 4, tile.top + 2 + text_height());
            dc.SetTextColor(RGB(130, 124, 116));
            dc.DrawText(label_number, &corner, DT_LEFT | DT_SINGLELINE);
        }
    }
    dc.SelectObject(old_font);
}

void DesignerPage::OnMakeEgg() {
    save_settings();
    const c1kit::EggColours tint = colours();
    CString why;
    CString done;
    if (sheet_.hatch_designed(picked_[0], picked_[1], sex_.GetCurSel(),
                              colour_.GetCheck() == BST_CHECKED ? &tint : nullptr, choices(), why,
                              done)) {
        sheet_.sound().play("hslt", false);
        status_.SetWindowText(done);
    } else {
        status_.SetWindowText(why);
    }
}

// The 1996 kit's sounds for a chosen egg: a click, then the egg's male or
// female tone until another is chosen (the 1996 machine played them as the
// pointer passed over an egg).
void NestPage::play_selection_sound() {
    stop_tone();
    const c1kit::Nest& nest = sheet_.nest();
    if (selected_ < 0 || nest.eggs[selected_] == c1kit::EggState::taken) {
        return;
    }
    c1kitshell::KitSound& sound = sheet_.sound();
    sound.play("hegg", false);
    tone_channel_ =
        sound.play(nest.eggs[selected_] == c1kit::EggState::female ? "hfml" : "hmle", true);
}

void NestPage::stop_tone() {
    if (tone_channel_ >= 0) {
        sheet_.sound().stop(tone_channel_);
        tone_channel_ = -1;
    }
}

// ===========================================================================
// The sheet
// ===========================================================================

BEGIN_MESSAGE_MAP(HatcherySheet, c1kitshell::KitSheet)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_INITMENUPOPUP()
    ON_WM_WINDOWPOSCHANGING()
    ON_COMMAND(kCommandAbout, &HatcherySheet::OnAbout)
END_MESSAGE_MAP()

HatcherySheet::HatcherySheet(CFont& default_font)
    : KitSheet(kStringToolName, 0), default_font_(default_font), nest_page_(*this),
      designer_page_(*this), machine_page_(*this) {
    m_psh.dwFlags |= PSH_USEHICON;
    m_psh.hIcon = AfxGetApp()->LoadIcon(kIconKit);
    // The classic look, when the player's original Hatchery is beside this
    // one (its pictures are files in the game's Hatchery folder, so they
    // must be there too): the 1996 machine instead of the nest page.
    classic_ = c1kitshell::ClassicArt::find({}, {}, {kIconKit});
    if (classic_ && GetFileAttributesA(game_file("Hatchery\\hatchery.bmp").c_str()) ==
                        INVALID_FILE_ATTRIBUTES) {
        classic_.reset();
    }
    AddPage(classic_ ? static_cast<CPropertyPage*>(&machine_page_) : &nest_page_);
    if (!classic_) {
        AddPage(&designer_page_);
    }
}

HatcherySheet::~HatcherySheet() {
    if (registry_ != nullptr) {
        registry_->release();
    }
}

bool HatcherySheet::create_window() {
    // The classic look is the 1996 fixed-size window.
    const DWORD sizing = classic_ ? 0 : WS_MAXIMIZEBOX | WS_THICKFRAME;
    return Create(nullptr, WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | sizing,
                  WS_EX_DLGMODALFRAME) != FALSE;
}

std::string HatcherySheet::game_file(const std::string& name) const {
    const CString directory = c1kitshell::game_directory_setting("Main Directory");
    return std::string(CStringA(directory)) + name;
}

bool HatcherySheet::game_art_places(c1kit::GamePalette& palette, std::vector<std::string>& images) const {
    CString palettes = c1kitshell::game_directory_setting("Palette Directory");
    if (palettes.IsEmpty()) {
        palettes = CString(game_file("Palettes\\").c_str());
    }
    const bool have_palette =
        c1kit::parse_palette_dta(read_file(std::string(CStringA(palettes)) + "palette.dta"), palette);
    images.clear();
    for (const c1kit::GameDirectory which :
         {c1kit::GameDirectory::world, c1kit::GameDirectory::installation}) {
        const std::string directory(CStringA(c1kitshell::game_directory_setting("Image Directory", which)));
        if (!directory.empty() && std::find(images.begin(), images.end(), directory) == images.end()) {
            images.push_back(directory);
        }
    }
    images.push_back(game_file("Images\\"));  // as before, if no setting
    return have_palette;
}

bool HatcherySheet::parent_genome(bool mum, int number, std::vector<std::uint8_t>& bytes) const {
    const std::string name = std::string(mum ? "mum" : "dad") + std::to_string(number) + ".gen";
    for (const c1kit::GameDirectory which :
         {c1kit::GameDirectory::world, c1kit::GameDirectory::installation}) {
        const std::string directory(
            CStringA(c1kitshell::game_directory_setting("Genetics Directory", which)));
        if (directory.empty()) continue;
        bytes = read_file(directory + name);
        if (!bytes.empty()) return true;
    }
    bytes = read_file(game_file("Genetics\\" + name));
    return !bytes.empty();
}

c1kit::ParentSet HatcherySheet::available_parents() const {
    c1kit::ParentSet set;
    set.mums.clear();
    set.dads.clear();
    std::vector<std::uint8_t> bytes;
    for (int n = 1; n <= c1kit::kMaxParents; ++n) {
        if (parent_genome(true, n, bytes)) set.mums.push_back(n);
        if (parent_genome(false, n, bytes)) set.dads.push_back(n);
    }
    return set;
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
    // New eggs' sexes, Scramble Eggs' parents and Colourful's colours.
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    registry_->read_dword(c1kit::SettingsScope::user, "Scramble Eggs", scramble_eggs_);
    registry_->read_dword(c1kit::SettingsScope::user, "Colourful Eggs", colourful_eggs_);
    // The nest: the 1996 value, or six new eggs when there is none
    // (InitializeEggstraPattern @ 0x004045b0 regenerated it then too).
    char pattern[16] = {};
    if (!registry_->read_string(c1kit::SettingsScope::user, kNestValue, pattern, sizeof(pattern)) ||
        !c1kit::parse_nest(pattern, nest_)) {
        refill();
    }
    char parents[32] = {};
    if (!registry_->read_string(c1kit::SettingsScope::user, "Egg Parents", parents,
                                sizeof(parents)) ||
        !c1kit::parse_parents(parents, parents_)) {
        pick_parents();
    }
    ensure_scrambled();
    return 0;
}

BOOL HatcherySheet::OnInitDialog() {
    const BOOL result = c1kitshell::KitSheet::OnInitDialog();
    if (classic_) {
        set_up_classic_window();
    } else {
        enable_resizing(CSize(kDefaultPageWidthDlu, kDefaultPageHeightDlu));
    }
    SetWindowText(c1kitshell::load_string(kStringToolName));
    install_options(registry_, classic_ != nullptr);
    sound_.open(GetSafeHwnd());
    for (const char* name : kHatcherySounds) {
        sound_.load(name, game_file(std::string("Sounds\\") + name + ".wav"));
    }
    if (!classic_) {
        enable_ambience("hfan", kLibreFanVolume);  // the classic machine runs its own fan
    }
    load_preferences();
    if (classic_) fit_classic_window();  // after anything that sized the sheet
    return result;
}

void HatcherySheet::load_preferences() {
    // The saved size, but never smaller than the default, which is what the
    // eggs need at twice their size (an older, smaller layout saved one).
    CRect window;
    GetWindowRect(&window);
    WindowSize size = {};
    if (!classic_ &&
        registry_->read_binary(c1kit::SettingsScope::user, "Size", &size, sizeof(size)) &&
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
    const int left = location.left < max_left ? location.left : max_left;
    const int top = location.top < max_top ? location.top : max_top;
    SetWindowPos(always_on_top() ? &wndTopMost : &wndNoTopMost, left < 0 ? 0 : left,
                 top < 0 ? 0 : top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
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
    if (!classic_) {
        registry_->write_binary("Size", &size, sizeof(size));  // the classic window is fixed
    }
}

void HatcherySheet::save_nest() {
    if (registry_ != nullptr) {
        registry_->write_string(kNestValue, c1kit::format_nest(nest_).c_str());
    }
}

void HatcherySheet::refill() {
    nest_ = c1kit::fresh_nest(static_cast<std::uint32_t>(std::rand()));
    save_nest();
    pick_parents();
}

void HatcherySheet::save_parents() {
    if (registry_ != nullptr) {
        registry_->write_string("Egg Parents", c1kit::format_parents(parents_).c_str());
    }
}

void HatcherySheet::pick_parents() {
    parents_ = scramble_eggs_ != 0
                   ? c1kit::scrambled_nest_parents([] { return std::rand(); }, available_parents())
                   : c1kit::NestParents();
    save_parents();
}

void HatcherySheet::ensure_scrambled() {
    if (scramble_eggs_ != 0 &&
        c1kit::scramble_own_pairs(parents_, [] { return std::rand(); }, available_parents())) {
        save_parents();
    }
}

CString HatcherySheet::egg_parents_text(int slot) const {
    CString text;
    if (scramble_eggs_ != 0 && slot >= 0 && slot < c1kit::kEggCount) {
        text.Format(_T("mum%d x dad%d"), parents_.mum[slot], parents_.dad[slot]);
    }
    return text;
}

bool HatcherySheet::colourful_genome(c1kit::MacroTransport& game, int mum, int dad,
                                     std::int32_t& genome) {
    c1kit::MacroConversation conversation(game);
    std::string reply;
    const bool crossed = conversation.query_reusing_holder(
        c1kit::kMacroModeQuery, c1kit::cross_script(mum, dad).c_str(), reply);
    conversation.close();
    char* end = nullptr;
    const long value = crossed ? std::strtol(reply.c_str(), &end, 10) : 0;
    if (!crossed || end == reply.c_str() || value == 0) {
        return false;
    }
    genome = static_cast<std::int32_t>(value);
    // The game writes the crossed genome into the world's Genetics folder.
    const std::string path =
        std::string(CStringA(c1kitshell::game_directory_setting(
            "Genetics Directory", c1kit::GameDirectory::world))) +
        c1kit::genome_file_for(genome);
    std::vector<std::uint8_t> bytes = read_file(path);
    if (!c1kit::append_colour_genes(
            bytes, c1kit::random_egg_colours([] { return std::rand(); }))) {
        return true;  // made, but left as crossed
    }
    if (FILE* file = std::fopen(path.c_str(), "wb")) {
        std::fwrite(bytes.data(), 1, bytes.size(), file);
        std::fclose(file);
    }
    return true;
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
    // The pair the egg shows (its own unless Scramble Eggs picked another).
    ensure_scrambled();
    const int mum = scramble_eggs_ != 0 ? parents_.mum[slot] : slot + 1;
    const int dad = scramble_eggs_ != 0 ? parents_.dad[slot] : slot + 1;
    std::int32_t genome = 0;
    const std::string script = colourful_eggs_ != 0 && colourful_genome(*game, mum, dad, genome)
                                   ? c1kit::hatch_script_for_genome(slot, female, genome)
                                   : c1kit::hatch_script(slot, female, mum, dad);
    if (!c1kit::execute_scheduled(*game, script.c_str(), false)) {
        why = _T("The game did not take the egg.");
        return false;
    }
    nest_.eggs[slot] = c1kit::EggState::taken;
    save_nest();
    return true;
}

bool HatcherySheet::designed_genome(c1kit::MacroTransport& game, int mum, int dad,
                                    const c1kit::EggColours* colours,
                                    const c1kit::DesignerChoices& choices,
                                    std::int32_t& genome, std::string& summary) {
    c1kit::MacroConversation conversation(game);
    std::string reply;
    const bool crossed = conversation.query_reusing_holder(
        c1kit::kMacroModeQuery, c1kit::cross_script(mum, dad).c_str(), reply);
    conversation.close();
    char* end = nullptr;
    const long value = crossed ? std::strtol(reply.c_str(), &end, 10) : 0;
    if (!crossed || end == reply.c_str() || value == 0) {
        return false;
    }
    genome = static_cast<std::int32_t>(value);
    const std::string path =
        std::string(CStringA(c1kitshell::game_directory_setting(
            "Genetics Directory", c1kit::GameDirectory::world))) +
        c1kit::genome_file_for(genome);
    std::vector<std::uint8_t> bytes = read_file(path);
    bool changed = false;
    if (colours != nullptr && c1kit::append_colour_genes(bytes, *colours)) {
        changed = true;
        summary = "coloured";
    }
    std::string edits;
    if (c1kit::apply_designer_choices(bytes, choices, &edits) && !edits.empty()) {
        changed = true;
        summary += (summary.empty() ? "" : "; ") + edits;
    }
    if (changed) {
        if (FILE* file = std::fopen(path.c_str(), "wb")) {
            std::fwrite(bytes.data(), 1, bytes.size(), file);
            std::fclose(file);
        }
    }
    return true;
}

bool HatcherySheet::hatch_designed(int mum, int dad, int sex, const c1kit::EggColours* colours,
                                   const c1kit::DesignerChoices& choices, CString& why,
                                   CString& done) {
    c1kit::MacroTransport* game = transport();
    if (!connected_ || game == nullptr) {
        why = _T("The Hatchery is not connected to the game.");
        return false;
    }
    int slot = -1;
    for (int i = 0; i < c1kit::kEggCount && slot < 0; ++i) {
        slot = nest_.eggs[i] != c1kit::EggState::taken ? i : -1;
    }
    if (slot < 0) {
        why = _T("The nest is empty: refill it on the Nest page first.");
        return false;
    }
    const bool female = sex == 0 ? (std::rand() & 1) != 0 : sex == 2;
    std::int32_t genome = 0;
    std::string summary;
    if (!designed_genome(*game, mum, dad, colours, choices, genome, summary)) {
        why = _T("The game did not cross the parents' genomes.");
        return false;
    }
    if (!c1kit::execute_scheduled(
            *game, c1kit::hatch_script_for_genome(slot, female, genome).c_str(), false)) {
        why = _T("The game did not take the egg.");
        return false;
    }
    nest_.eggs[slot] = c1kit::EggState::taken;
    save_nest();
    nest_page_.refresh();
    done.Format(_T("A %s egg (mum%d x dad%d) is in the incubator%s%s."),
                female ? _T("female") : _T("male"), mum, dad,
                summary.empty() ? _T("") : _T(": "), CString(summary.c_str()).GetString());
    return true;
}

void HatcherySheet::OnTimer(UINT_PTR timer_id) {
    if (timer_id == kTimerStartup) {
        KillTimer(kTimerStartup);
        connected_ = connect_to_game(kCommandBufferBytes);
        if (classic_) {
            fit_classic_window();
            machine_page_.refresh();
        }
    }
    c1kitshell::KitSheet::OnTimer(timer_id);
}

// The 1996 window: no tabs or buttons, the machine filling it (320 x 240),
// and its menu, Help > About Hatchery.  Options (Refill the nest, for when
// it is empty, in place of the 1996 Egg Disk, and every kit's) are on the
// system menu.
void HatcherySheet::set_up_classic_window() {
    if (CWnd* tabs = GetTabControl()) {
        tabs->ShowWindow(SW_HIDE);
    }
    classic_menu_.CreateMenu();
    CMenu help;
    help.CreatePopupMenu();
    help.AppendMenu(MF_STRING, kCommandAbout, _T("&About Hatchery..."));
    classic_menu_.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(help.Detach()), _T("&Help"));
    SetMenu(&classic_menu_);
    fit_classic_window();
}

// The frame for a 320 x 240 client under the menu, from the styles rather
// than from the current client rectangle, which the property sheet's own
// sizing leaves stale during start-up.
CSize HatcherySheet::classic_window_size() const {
    CRect frame(0, 0, kMachineWidth, kMachineHeight);
    ::AdjustWindowRectEx(&frame, GetStyle(), TRUE, GetExStyle());
    return frame.Size() + classic_correction_;
}

void HatcherySheet::fit_classic_window() {
    const CSize size = classic_window_size();
    SetWindowPos(nullptr, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    // AdjustWindowRectEx counts the menu bar a pixel short of the drawn one;
    // measure once and make up the difference.
    CRect client;
    GetClientRect(&client);
    const CSize missing(kMachineWidth - client.Width(), kMachineHeight - client.Height());
    if (missing != CSize(0, 0)) {
        classic_correction_ += missing;
        const CSize corrected = classic_window_size();
        SetWindowPos(nullptr, 0, 0, corrected.cx, corrected.cy,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    machine_page_.SetWindowPos(nullptr, 0, 0, kMachineWidth, kMachineHeight, SWP_NOZORDER);
}

// The classic window is fixed: whatever else sizes the sheet gets the
// machine's size.
void HatcherySheet::OnWindowPosChanging(WINDOWPOS* position) {
    c1kitshell::KitSheet::OnWindowPosChanging(position);
    if (classic_ && (position->flags & SWP_NOSIZE) == 0) {
        const CSize size = classic_window_size();
        position->cx = size.cx;
        position->cy = size.cy;
    }
}

void HatcherySheet::OnInitMenuPopup(CMenu* menu, UINT index, BOOL system_menu) {
    c1kitshell::KitSheet::OnInitMenuPopup(menu, index, system_menu);
    enable_kit_option(kOptionRefill, !nest_.any_left());
}

void HatcherySheet::add_kit_options(CMenu& options) {
    options.AppendMenu(MF_STRING, kOptionRefill, _T("&Refill the nest"));
    // Not in the 1996 kit: each egg crosses a random mum and dad, never the
    // pair with the same number.
    options.AppendMenu(MF_STRING | (scramble_eggs_ != 0 ? MF_CHECKED : 0), kOptionScramble,
                       _T("&Scramble Eggs"));
    // Not in the 1996 kit: each egg's genome gets colour genes.
    options.AppendMenu(MF_STRING | (colourful_eggs_ != 0 ? MF_CHECKED : 0), kOptionColourful,
                       _T("Make My Creatures &Colourful"));
}

bool HatcherySheet::on_kit_option(UINT id) {
    if (id == kOptionScramble) {
        scramble_eggs_ = scramble_eggs_ != 0 ? 0 : 1;
        registry_->write_dword("Scramble Eggs", scramble_eggs_);
        check_kit_option(kOptionScramble, scramble_eggs_ != 0);
        // The eggs still in the nest take new pairs (or their own back).
        pick_parents();
        nest_page_.refresh();
        machine_page_.refresh();
        return true;
    }
    if (id == kOptionColourful) {
        colourful_eggs_ = colourful_eggs_ != 0 ? 0 : 1;
        registry_->write_dword("Colourful Eggs", colourful_eggs_);
        check_kit_option(kOptionColourful, colourful_eggs_ != 0);
        return true;
    }
    if (id != kOptionRefill) {
        return false;
    }
    if (!nest_.any_left()) {
        refill();
        machine_page_.refresh();
    }
    return true;
}

void HatcherySheet::on_sounds_muted_changed() {
    machine_page_.refresh();
}

void HatcherySheet::OnAbout() {
    show_about();
}

void HatcherySheet::before_skin_change() {
    save_preferences();
}

void HatcherySheet::before_game_quit() {
    save_preferences();
}

void HatcherySheet::OnClose() {
    request_game_quit();
}

void HatcherySheet::OnDestroy() {
    save_preferences();
    sound_.stop_all();
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
    kit.original_file_name = "Hatchery";
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
