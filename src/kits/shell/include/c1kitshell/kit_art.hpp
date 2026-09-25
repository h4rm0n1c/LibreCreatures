#pragma once

// Kit artwork: the game palette, the kits' own sprite files, and a 32-bit
// canvas they are drawn on.  The 1996 kits drew into 8-bit DIBs and realised
// palettes; drawing into 32-bit surfaces needs neither.

#include <afxwin.h>

#include "c1kit/c1kit.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace c1kitshell {

// The game's 256-colour palette: palette.dta holds 256 RGB triples with six
// bits per channel.
class GamePalette {
public:
    bool load(const std::string& path);
    bool loaded() const { return loaded_; }
    // 0x00RRGGBB, as a 32-bit DIB stores it.
    std::uint32_t colour(std::uint8_t index) const { return colours_[index]; }

private:
    std::array<std::uint32_t, 256> colours_{};
    bool loaded_ = false;
};

// A kit sprite file (Score Kit AllNumbers.spr, Score.spr, Time.spr; read by
// CPhasedSprite::SerializeArchive @ 0x00404610).  Not the game's .spr format:
// an optional repeated six-byte header {u16 frame count, u32 data size}, then
// the same header, then per frame {u32 row stride, u32 height, u16 width}
// followed by height rows of palette indices, stored bottom row first.
class KitSprite {
public:
    struct Frame {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> pixels;  // top row first, width per row
    };

    bool load(const std::string& path);
    int frame_count() const { return static_cast<int>(frames_.size()); }
    const Frame* frame(int index) const;

private:
    std::vector<Frame> frames_;
};

// A 32-bit drawing surface.
class Canvas {
public:
    Canvas() = default;
    ~Canvas();
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    // (Re)creates the surface; its contents are undefined until drawn.
    bool create(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }

    void fill(std::uint32_t colour);
    // Draws a sprite frame with every pixel opaque (the kits' art carries its
    // own background colour).
    void draw_frame(const KitSprite& sprite, int frame, int x, int y,
                    const GamePalette& palette);
    // Draws palette-indexed pixels (a photograph), `bottom_up` when rows are
    // stored bottom row first, as the kits' bitmaps are.
    void draw_indexed(const std::uint8_t* pixels, int width, int height,
                      int stride, bool bottom_up, int x, int y,
                      const GamePalette& palette);
    // Covers the surface with a BMP file, tiled from the top-left.
    bool tile_bitmap_file(const std::string& path);
    // Draws a BMP file once with its top-left at (x, y).
    bool draw_bitmap_file(const std::string& path, int x, int y);
    // Copies part of the surface to a device context.
    void present(CDC& dc, int dest_x, int dest_y, int width, int height,
                 int source_x = 0, int source_y = 0) const;

private:
    void release();

    HBITMAP bitmap_ = nullptr;
    HDC dc_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    std::uint32_t* bits_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

// Writes palette-indexed pixels as an 8-bit BMP with the game palette.
bool save_indexed_bmp(const std::string& path, const std::uint8_t* pixels,
                      int width, int height, int stride, bool bottom_up,
                      const GamePalette& palette);

// A directory the game records in its registry ("Palette Directory",
// "Main Directory", ...), with a trailing backslash; empty when the game has
// none.  See c1kit::read_game_directory.
CString game_directory_setting(
    const char* value_name,
    c1kit::GameDirectory which = c1kit::GameDirectory::installation);

} // namespace c1kitshell
