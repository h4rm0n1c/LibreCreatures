// The Ecology Kit's map page: the world's picture made small, its rooms
// coloured by the chosen layer, and every creature as a dot.

#include "ecology.hpp"
#include "ecology_ids.hpp"

#include "c1kitshell/kit_art.hpp"
#include "c1kit/game_sprite.hpp"

#include <algorithm>
#include <cstdio>

namespace ecology {
namespace {

// back.spr: 58 columns of 8 tiles, 144 x 150 each, frame = column * 8 + row.
constexpr int kTileColumns = 58;
constexpr int kTileRows = 8;
constexpr int kTileWidth = 144;
constexpr int kTileHeight = 150;
// The picture is kept at a sixth of the world's size (1392 x 200).
constexpr int kShrink = 6;

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

struct Rgb {
    int r, g, b;
};

// A room's colour and strength (0 to 1) for a layer value.
bool layer_colour(c1kit::EcologyLayer layer, const c1kit::EcologyRoom& room, int value,
                  int largest, Rgb& colour, double& strength) {
    if (layer == c1kit::EcologyLayer::temperature) {
        if (value < 0) {
            colour = {60, 120, 255};
            strength = 0.45;
        } else if (value > 0) {
            colour = {255, 80, 40};
            strength = 0.45;
        } else if (room.type == 1) {
            colour = {220, 220, 220};  // outdoor, mild now
            strength = 0.18;
        } else {
            return false;
        }
        return true;
    }
    if (value <= 0 || largest <= 0) {
        return false;
    }
    switch (layer) {
    case c1kit::EcologyLayer::crowding: colour = {255, 160, 0}; break;
    case c1kit::EcologyLayer::food: colour = {80, 220, 60}; break;
    case c1kit::EcologyLayer::toys: colour = {190, 110, 255}; break;
    case c1kit::EcologyLayer::disease: colour = {230, 40, 150}; break;
    default: return false;
    }
    strength = 0.15 + 0.55 * value / largest;
    return true;
}

// A creature's dot: norns green, grendels brown-red, others yellow, the
// dead grey.
COLORREF dot_colour(const c1kit::EcologyCreature& creature) {
    if (creature.dead) {
        return RGB(150, 150, 150);
    }
    switch (creature.genus) {
    case 1: return RGB(60, 230, 60);
    case 2: return RGB(200, 90, 40);
    default: return RGB(240, 220, 60);
    }
}

// The dot sits a little above the creature's feet.
constexpr int kDotAboveFeet = 20;

} // namespace

BEGIN_MESSAGE_MAP(MapPage, c1kitshell::LayoutPage)
    ON_CBN_SELCHANGE(kControlLayer, &MapPage::OnLayerChanged)
END_MESSAGE_MAP()

MapPage::MapPage(EcologySheet& sheet)
    : LayoutPage(sheet, kDialogPage, kStringMapTab), sheet_(sheet) {}

MapPage::~MapPage() = default;

void MapPage::set_layer(c1kit::EcologyLayer layer) {
    layer_ = layer;
    if (layer_box_.GetSafeHwnd() != nullptr) {
        layer_box_.SetCurSel(static_cast<int>(layer));
        refresh();
    }
}

void MapPage::create_controls() {
    load_world_picture();
    map_.create(*this, kControlMap, [this](CDC& dc, const CRect& rect) { draw(dc, rect); });
    map_.set_mouse_handler([this](CPoint point, bool clicked) {
        if (clicked) {
            on_click(point);
        }
    });
    make(layer_label_, _T("STATIC"), _T("Show:"), SS_LEFT, kControlLayerLabel);
    make(layer_box_, _T("COMBOBOX"), _T(""), CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
         kControlLayer);
    for (int layer = 0; layer < static_cast<int>(c1kit::EcologyLayer::count); ++layer) {
        layer_box_.AddString(CString(c1kit::layer_name(static_cast<c1kit::EcologyLayer>(layer))));
    }
    layer_box_.SetCurSel(static_cast<int>(layer_));
    make(legend_, _T("STATIC"), _T(""), SS_LEFT | SS_NOPREFIX, kControlLegend);
    describe();
}

void MapPage::layout(int width, int height) {
    const int row = button_height();
    const int controls_top = height - kMargin - row;
    place(map_, kMargin, kMargin, width - 2 * kMargin, controls_top - 2 * kMargin);
    place(layer_label_, kMargin, controls_top + 4, 40, text_height());
    place(layer_box_, kMargin + 42, controls_top, 170, row + 200);
    place(legend_, kMargin + 222, controls_top + 4, width - kMargin - 222 - 90, text_height() * 2);
    place_close(width, height);
}

void MapPage::OnLayerChanged() {
    const int selection = layer_box_.GetCurSel();
    if (selection >= 0) {
        layer_ = static_cast<c1kit::EcologyLayer>(selection);
        sheet_.save_layer();
        refresh();
    }
}

void MapPage::refresh() {
    describe();
    map_.redraw();
}

// back.spr from the game's Images folders, each 6 x 6 block averaged into
// one pixel.
void MapPage::load_world_picture() {
    CString palettes = c1kitshell::game_directory_setting("Palette Directory");
    if (palettes.IsEmpty()) {
        palettes = CString(sheet_.game_file("Palettes\\").c_str());
    }
    c1kit::GamePalette palette{};
    if (!c1kit::parse_palette_dta(
            read_file(std::string(CStringA(palettes)) + "palette.dta"), palette)) {
        return;
    }
    std::vector<c1kit::GameSprite> tiles;
    bool found = false;
    for (const std::string& directory : sheet_.image_directories()) {
        if (c1kit::parse_game_sprites(read_file(directory + "back.spr"), tiles)) {
            found = true;
            break;
        }
    }
    if (!found || tiles.size() < static_cast<std::size_t>(kTileColumns * kTileRows)) {
        return;
    }
    const int tile_width = kTileWidth / kShrink;    // 24
    const int tile_height = kTileHeight / kShrink;  // 25
    picture_width_ = kTileColumns * tile_width;
    picture_height_ = kTileRows * tile_height;
    picture_.assign(static_cast<std::size_t>(picture_width_) * picture_height_, 0xff000000u);
    for (int column = 0; column < kTileColumns; ++column) {
        for (int row = 0; row < kTileRows; ++row) {
            const c1kit::GameSprite& tile =
                tiles[static_cast<std::size_t>(column * kTileRows + row)];
            if (tile.width < kTileWidth || tile.height < kTileHeight) {
                continue;
            }
            for (int y = 0; y < tile_height; ++y) {
                for (int x = 0; x < tile_width; ++x) {
                    unsigned r = 0, g = 0, b = 0;
                    for (int dy = 0; dy < kShrink; ++dy) {
                        for (int dx = 0; dx < kShrink; ++dx) {
                            const c1kit::PaletteColour& colour =
                                palette[tile.pixels[static_cast<std::size_t>(
                                    (y * kShrink + dy) * tile.width + x * kShrink + dx)]];
                            r += colour.red;
                            g += colour.green;
                            b += colour.blue;
                        }
                    }
                    const unsigned n = kShrink * kShrink;
                    picture_[static_cast<std::size_t>((row * tile_height + y) * picture_width_ +
                                                      column * tile_width + x)] =
                        0xff000000u | ((r / n) << 16) | ((g / n) << 8) | (b / n);
                }
            }
        }
    }
}

CRect MapPage::world_rect(const CRect& rect) const {
    const double scale = (std::min)(static_cast<double>(rect.Width()) / c1kit::kWorldWidth,
                                    static_cast<double>(rect.Height()) / c1kit::kWorldHeight);
    const int width = static_cast<int>(c1kit::kWorldWidth * scale);
    const int height = static_cast<int>(c1kit::kWorldHeight * scale);
    const int left = rect.left + (rect.Width() - width) / 2;
    const int top = rect.top + (rect.Height() - height) / 2;
    return CRect(left, top, left + width, top + height);
}

CPoint MapPage::to_view(int world_x, int world_y) const {
    world_x %= c1kit::kWorldWidth;
    if (world_x < 0) {
        world_x += c1kit::kWorldWidth;
    }
    return CPoint(view_world_.left + world_x * view_world_.Width() / c1kit::kWorldWidth,
                  view_world_.top + world_y * view_world_.Height() / c1kit::kWorldHeight);
}

void MapPage::draw(CDC& dc, const CRect& rect) {
    dc.FillSolidRect(rect, RGB(16, 16, 20));
    view_world_ = world_rect(rect);
    const int width = view_world_.Width();
    const int height = view_world_.Height();
    if (width <= 0 || height <= 0) {
        return;
    }
    // The picture, scaled to the view (nearest).
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * height, 0xff202028u);
    if (!picture_.empty()) {
        for (int y = 0; y < height; ++y) {
            const int source_y = y * picture_height_ / height;
            for (int x = 0; x < width; ++x) {
                pixels[static_cast<std::size_t>(y * width + x)] =
                    picture_[static_cast<std::size_t>(source_y * picture_width_ +
                                                      x * picture_width_ / width)];
            }
        }
    }
    // The layer, over the rooms.
    const c1kit::EcologySnapshot& world = sheet_.world();
    const std::vector<int> values =
        c1kit::layer_values(world, layer_, sheet_.food_per_room(), sheet_.toys_per_room());
    const int largest = values.empty() ? 0 : *std::max_element(values.begin(), values.end());
    for (std::size_t i = 0; i < world.rooms.size(); ++i) {
        const c1kit::EcologyRoom& room = world.rooms[i];
        Rgb colour{};
        double strength = 0;
        if (room.left < 0 || room.left >= c1kit::kWorldWidth ||
            !layer_colour(layer_, room, values[i], largest, colour, strength)) {
            continue;
        }
        const int x0 = (std::max)(0, room.left * width / c1kit::kWorldWidth);
        const int x1 = (std::min)(width, (room.right + 1) * width / c1kit::kWorldWidth);
        const int y0 = (std::max)(0, room.top * height / c1kit::kWorldHeight);
        const int y1 = (std::min)(height, (room.bottom + 1) * height / c1kit::kWorldHeight);
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                std::uint32_t& p = pixels[static_cast<std::size_t>(y * width + x)];
                const int r = static_cast<int>(((p >> 16) & 0xff) * (1 - strength) + colour.r * strength);
                const int g = static_cast<int>(((p >> 8) & 0xff) * (1 - strength) + colour.g * strength);
                const int b = static_cast<int>((p & 0xff) * (1 - strength) + colour.b * strength);
                p = 0xff000000u | (r << 16) | (g << 8) | b;
            }
        }
    }
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    ::SetDIBitsToDevice(dc.GetSafeHdc(), view_world_.left, view_world_.top, width, height, 0, 0,
                        0, height, pixels.data(), &info, DIB_RGB_COLORS);

    // Room outlines: outdoor rooms lighter.
    CPen indoor(PS_SOLID, 1, RGB(90, 90, 100));
    CPen outdoor(PS_SOLID, 1, RGB(170, 200, 230));
    CGdiObject* old_pen = dc.SelectObject(&indoor);
    CGdiObject* old_brush = dc.SelectStockObject(NULL_BRUSH);
    for (const c1kit::EcologyRoom& room : world.rooms) {
        if (room.left < 0 || room.left >= c1kit::kWorldWidth) {
            continue;
        }
        dc.SelectObject(room.type == 1 ? &outdoor : &indoor);
        const CPoint top_left = to_view(room.left, room.top);
        dc.Rectangle(top_left.x, top_left.y,
                     view_world_.left + (room.right + 1) * width / c1kit::kWorldWidth + 1,
                     view_world_.top + (room.bottom + 1) * height / c1kit::kWorldHeight + 1);
    }

    // The creatures.  Infected ones get a magenta ring, the selected one a
    // white one.
    const int radius = (std::max)(3, width / 300);
    for (const c1kit::EcologyCreature& creature : world.creatures) {
        const CPoint at = to_view(creature.x, creature.y - kDotAboveFeet);
        CBrush fill(dot_colour(creature));
        CPen edge(PS_SOLID, 1, RGB(0, 0, 0));
        dc.SelectObject(&fill);
        dc.SelectObject(&edge);
        dc.Ellipse(at.x - radius, at.y - radius, at.x + radius + 1, at.y + radius + 1);
        dc.SelectStockObject(NULL_BRUSH);
        if (creature.infected && !creature.dead) {
            CPen ring(PS_SOLID, 2, RGB(230, 40, 150));
            dc.SelectObject(&ring);
            dc.Ellipse(at.x - radius - 2, at.y - radius - 2, at.x + radius + 3, at.y + radius + 3);
            dc.SelectObject(&edge);
        }
        if (creature.selected) {
            CPen ring(PS_SOLID, 1, RGB(255, 255, 255));
            dc.SelectObject(&ring);
            dc.Ellipse(at.x - radius - 4, at.y - radius - 4, at.x + radius + 5, at.y + radius + 5);
            dc.SelectObject(&edge);
        }
    }
    dc.SelectObject(old_pen);
    dc.SelectObject(old_brush);
}

// A dot selects its creature and moves the camera to it.
void MapPage::on_click(CPoint point) {
    const int radius = (std::max)(3, view_world_.Width() / 300) + 3;
    const c1kit::EcologyCreature* nearest = nullptr;
    int nearest_distance = radius * radius + 1;
    for (const c1kit::EcologyCreature& creature : sheet_.world().creatures) {
        const CPoint at = to_view(creature.x, creature.y - kDotAboveFeet);
        const int dx = at.x - point.x;
        const int dy = at.y - point.y;
        if (dx * dx + dy * dy < nearest_distance) {
            nearest_distance = dx * dx + dy * dy;
            nearest = &creature;
        }
    }
    if (nearest != nullptr) {
        sheet_.select_creature(nearest->handle);
    }
}

// The line under the map: what the colours mean now.
void MapPage::describe() {
    if (legend_.GetSafeHwnd() == nullptr) {
        return;
    }
    CString text;
    if (!sheet_.connected()) {
        text = _T("Waiting for the game.");
    } else if (!sheet_.game_reports_ecology()) {
        text = _T("This game does not report its rooms and creatures to kits (the Ecology Kit needs "
                  "LibreCreatures).  Only the world is shown.");
    } else {
        const c1kit::EcologySnapshot& world = sheet_.world();
        const std::vector<int> values =
            c1kit::layer_values(world, layer_, sheet_.food_per_room(), sheet_.toys_per_room());
        int total = 0;
        int rooms = 0;
        for (const int value : values) {
            total += value > 0 ? value : 0;
            rooms += value > 0 ? 1 : 0;
        }
        int living = 0;
        for (const c1kit::EcologyCreature& creature : world.creatures) {
            living += creature.dead ? 0 : 1;
        }
        switch (layer_) {
        case c1kit::EcologyLayer::temperature: {
            int outdoor = 0;
            for (const c1kit::EcologyRoom& room : world.rooms) {
                if (room.type == 1) {
                    outdoor = room.temperature;
                    break;
                }
            }
            text.Format(_T("Outdoor rooms (light outlines) are %s now; they follow the season and "
                           "the time of day.  Indoor rooms do not change."),
                        outdoor < 0 ? _T("cold") : outdoor > 0 ? _T("warm") : _T("mild"));
            break;
        }
        case c1kit::EcologyLayer::crowding:
            text.Format(_T("%d creatures alive in %d rooms.  Darker: more creatures."), living, rooms);
            break;
        case c1kit::EcologyLayer::food:
            text.Format(_T("%d food and drink in %d rooms.  Darker: more."), total, rooms);
            break;
        case c1kit::EcologyLayer::toys:
            text.Format(_T("%d toys and instruments in %d rooms.  Darker: more."), total, rooms);
            break;
        case c1kit::EcologyLayer::disease:
            text.Format(_T("%d infected creatures (ringed) in %d rooms."), total, rooms);
            break;
        default:
            text.Format(_T("%d rooms, %d creatures alive.  Click a creature to select it."),
                        static_cast<int>(world.rooms.size()), living);
            break;
        }
    }
    legend_.SetWindowText(text);
}

} // namespace ecology
