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

void Canvas::draw_indexed(const std::uint8_t* pixels, int width, int height,
                          int stride, bool bottom_up, int x, int y,
                          const GamePalette& palette) {
    if (bits_ == nullptr || pixels == nullptr) {
        return;
    }
    GdiFlush();
    for (int row = 0; row < height; ++row) {
        const int dy = y + row;
        if (dy < 0 || dy >= height_) {
            continue;
        }
        const std::uint8_t* source =
            pixels + static_cast<std::size_t>(bottom_up ? height - 1 - row : row) * stride;
        for (int column = 0; column < width; ++column) {
            const int dx = x + column;
            if (dx >= 0 && dx < width_) {
                bits_[dy * width_ + dx] = palette.colour(source[column]);
            }
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

bool Canvas::draw_bitmap_file(const std::string& path, int x, int y) {
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
    BitBlt(dc_, x, y, info.bmWidth, info.bmHeight, source, 0, 0, SRCCOPY);
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

bool save_indexed_bmp(const std::string& path, const std::uint8_t* pixels,
                      int width, int height, int stride, bool bottom_up,
                      const GamePalette& palette) {
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    const int row_bytes = (width + 3) & ~3;
    BITMAPFILEHEADER file = {};
    BITMAPINFOHEADER info = {};
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = height;  // bottom-up
    info.biPlanes = 1;
    info.biBitCount = 8;
    info.biCompression = BI_RGB;
    info.biSizeImage = static_cast<DWORD>(row_bytes) * height;
    info.biClrUsed = 256;
    const DWORD header_bytes =
        sizeof(file) + sizeof(info) + 256 * sizeof(RGBQUAD);
    file.bfType = 0x4d42;  // "BM"
    file.bfOffBits = header_bytes;
    file.bfSize = header_bytes + info.biSizeImage;
    FILE* out = nullptr;
    if (fopen_s(&out, path.c_str(), "wb") != 0 || out == nullptr) {
        return false;
    }
    bool ok = std::fwrite(&file, sizeof(file), 1, out) == 1 &&
              std::fwrite(&info, sizeof(info), 1, out) == 1;
    for (int index = 0; ok && index < 256; ++index) {
        const std::uint32_t colour = palette.colour(static_cast<std::uint8_t>(index));
        const RGBQUAD quad = {static_cast<BYTE>(colour),
                              static_cast<BYTE>(colour >> 8),
                              static_cast<BYTE>(colour >> 16), 0};
        ok = std::fwrite(&quad, sizeof(quad), 1, out) == 1;
    }
    std::vector<std::uint8_t> row(static_cast<std::size_t>(row_bytes), 0);
    for (int y = 0; ok && y < height; ++y) {
        // The file stores the bottom row first.
        const int source_row = bottom_up ? y : height - 1 - y;
        std::memcpy(row.data(), pixels + static_cast<std::size_t>(source_row) * stride,
                    static_cast<std::size_t>(width));
        ok = std::fwrite(row.data(), row.size(), 1, out) == 1;
    }
    std::fclose(out);
    return ok;
}

// ---------------------------------------------------------------------------

CString game_directory_setting(const char* value_name,
                               c1kit::GameDirectory which) {
    char buffer[MAX_PATH] = {};
    if (!c1kit::read_game_directory(value_name, which, buffer, sizeof(buffer))) {
        return CString();
    }
    CString directory(buffer);
    if (directory.Right(1) != _T("\\")) {
        directory += _T("\\");
    }
    return directory;
}

} // namespace c1kitshell
