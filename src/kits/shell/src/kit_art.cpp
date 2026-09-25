// Kit artwork: palette, kit sprite files and 32-bit canvases.

#include "c1kitshell/kit_art.hpp"

#include "c1kit/c1kit.hpp"

#include <cstdio>
#include <cstring>

namespace c1kitshell {
namespace {

bool read_file(const std::string& path, std::vector<std::uint8_t>& bytes) {
    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr) {
        return false;
    }
    bytes.clear();
    std::uint8_t buffer[4096];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        bytes.insert(bytes.end(), buffer, buffer + read);
    }
    std::fclose(file);
    return true;
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    return static_cast<std::uint32_t>(bytes[at]) |
           (static_cast<std::uint32_t>(bytes[at + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[at + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    return static_cast<std::uint16_t>(bytes[at] | (bytes[at + 1] << 8));
}

} // namespace

// ---------------------------------------------------------------------------
// GamePalette
// ---------------------------------------------------------------------------

bool GamePalette::load(const std::string& path) {
    std::vector<std::uint8_t> bytes;
    if (!read_file(path, bytes) || bytes.size() < 256 * 3) {
        return false;
    }
    for (int index = 0; index < 256; ++index) {
        // Six bits per channel; scale to eight.
        const auto channel = [&](int offset) {
            const unsigned value = bytes[index * 3 + offset] & 0x3f;
            return (value << 2) | (value >> 4);
        };
        colours_[index] = (channel(0) << 16) | (channel(1) << 8) | channel(2);
    }
    loaded_ = true;
    return true;
}

// ---------------------------------------------------------------------------
// KitSprite
// ---------------------------------------------------------------------------

bool KitSprite::load(const std::string& path) {
    frames_.clear();
    std::vector<std::uint8_t> bytes;
    if (!read_file(path, bytes) || bytes.size() < 6) {
        return false;
    }
    // AllNumbers.spr and Time.spr repeat the header; Score.spr does not.
    std::size_t at = 0;
    if (bytes.size() >= 12 && std::memcmp(bytes.data(), bytes.data() + 6, 6) == 0) {
        at = 6;
    }
    const int count = read_u16(bytes, at);
    at += 6;
    for (int index = 0; index < count; ++index) {
        if (at + 10 > bytes.size()) {
            frames_.clear();
            return false;
        }
        const std::uint32_t stride = read_u32(bytes, at);
        const std::uint32_t height = read_u32(bytes, at + 4);
        const std::uint16_t width = read_u16(bytes, at + 8);
        at += 10;
        if (width > stride || height > 4096 || stride > 4096 ||
            at + static_cast<std::size_t>(stride) * height > bytes.size()) {
            frames_.clear();
            return false;
        }
        Frame frame;
        frame.width = width;
        frame.height = static_cast<int>(height);
        frame.pixels.resize(static_cast<std::size_t>(width) * height);
        for (std::uint32_t row = 0; row < height; ++row) {
            // Rows are stored bottom first.
            const std::uint8_t* source = bytes.data() + at + (height - 1 - row) * stride;
            std::memcpy(frame.pixels.data() + static_cast<std::size_t>(row) * width,
                        source, width);
        }
        at += static_cast<std::size_t>(stride) * height;
        frames_.push_back(std::move(frame));
    }
    return true;
}

const KitSprite::Frame* KitSprite::frame(int index) const {
    return index >= 0 && index < frame_count() ? &frames_[index] : nullptr;
}

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------

Canvas::~Canvas() {
    release();
}

void Canvas::release() {
    if (dc_ != nullptr) {
        SelectObject(dc_, previous_);
        DeleteDC(dc_);
        dc_ = nullptr;
    }
    if (bitmap_ != nullptr) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    bits_ = nullptr;
    width_ = height_ = 0;
}

bool Canvas::create(int width, int height) {
    release();
    if (width <= 0 || height <= 0) {
        return false;
    }
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    bitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap_ == nullptr) {
        return false;
    }
    dc_ = CreateCompatibleDC(nullptr);
    previous_ = SelectObject(dc_, bitmap_);
    bits_ = static_cast<std::uint32_t*>(bits);
    width_ = width;
    height_ = height;
    return true;
}

void Canvas::fill(std::uint32_t colour) {
    if (bits_ == nullptr) {
        return;
    }
    GdiFlush();
    for (int index = 0; index < width_ * height_; ++index) {
        bits_[index] = colour;
    }
}

void Canvas::draw_frame(const KitSprite& sprite, int frame_index, int x, int y,
                        const GamePalette& palette) {
    const KitSprite::Frame* frame = sprite.frame(frame_index);
    if (bits_ == nullptr || frame == nullptr) {
        return;
    }
    GdiFlush();
    for (int row = 0; row < frame->height; ++row) {
        const int dy = y + row;
        if (dy < 0 || dy >= height_) {
            continue;
        }
        for (int column = 0; column < frame->width; ++column) {
            const int dx = x + column;
            if (dx < 0 || dx >= width_) {
                continue;
            }
            bits_[dy * width_ + dx] = palette.colour(
                frame->pixels[static_cast<std::size_t>(row) * frame->width + column]);
        }
    }
}

bool Canvas::tile_bitmap_file(const std::string& path) {
    if (dc_ == nullptr) {
        return false;
    }
    auto* bitmap = static_cast<HBITMAP>(LoadImageA(
        nullptr, path.c_str(), IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION));
    if (bitmap == nullptr) {
        return false;
    }
    BITMAP info = {};
    GetObject(bitmap, sizeof(info), &info);
    HDC source = CreateCompatibleDC(dc_);
    HGDIOBJ previous = SelectObject(source, bitmap);
    for (int y = 0; info.bmHeight > 0 && y < height_; y += info.bmHeight) {
        for (int x = 0; info.bmWidth > 0 && x < width_; x += info.bmWidth) {
            BitBlt(dc_, x, y, info.bmWidth, info.bmHeight, source, 0, 0, SRCCOPY);
        }
    }
    SelectObject(source, previous);
    DeleteDC(source);
    DeleteObject(bitmap);
    return true;
}

void Canvas::present(CDC& dc, int dest_x, int dest_y, int width, int height,
                     int source_x, int source_y) const {
    if (dc_ == nullptr) {
        return;
    }
    BitBlt(dc.GetSafeHdc(), dest_x, dest_y, width, height, dc_, source_x,
           source_y, SRCCOPY);
}

// ---------------------------------------------------------------------------

CString game_directory_setting(const char* value_name) {
    CString directory;
    c1kit::KitSettings* settings = c1kit::open_kit_settings(
        "Gameware Development", "Creatures 1", "1.0",
        c1kit::SettingsOpenPolicy::user_key_only);
    if (settings == nullptr) {
        return directory;
    }
    char buffer[MAX_PATH] = {};
    if (settings->read_string(c1kit::SettingsScope::machine, value_name, buffer,
                              sizeof(buffer)) ||
        settings->read_string(c1kit::SettingsScope::user, value_name, buffer,
                              sizeof(buffer))) {
        directory = buffer;
    }
    settings->release();
    return directory;
}

} // namespace c1kitshell
