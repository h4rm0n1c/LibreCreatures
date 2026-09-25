#pragma once

// Kit artwork: the game palette, the kits' own sprite files, and a 32-bit
// canvas they are drawn on.  The 1996 kits drew into 8-bit DIBs and realised
// palettes; drawing into 32-bit surfaces needs neither.

#include <afxwin.h>

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
    // Covers the surface with a BMP file, tiled from the top-left.
    bool tile_bitmap_file(const std::string& path);
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

// A directory the game records in its registry ("Palette Directory",
// "Main Directory", ...): the machine-wide install first, then the per-user
// key.  Empty when neither has it.
CString game_directory_setting(const char* value_name);

} // namespace c1kitshell
