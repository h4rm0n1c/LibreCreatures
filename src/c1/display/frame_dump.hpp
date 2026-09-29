#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace creatures1::display {

// A whole rendered frame as 32-bit colour, independent of how it was drawn.
// Not in the 1996 game: this is the neutral form the renderer back-ends are
// compared in (the 8-bit software DIB expanded through the game palette on
// one side, a GPU frame read back on the other), and what a developer frame
// dump writes out.
struct RgbFrame {
    int width = 0;
    int height = 0;
    // Row-major from the top; 0x00RRGGBB.
    std::vector<std::uint32_t> pixels;
};

struct FrameColour {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

// An 8-bit top-down frame (rows `stride` bytes apart) through a palette.
RgbFrame expand_indexed_frame(const std::uint8_t* pixels, int width,
                              int height, int stride,
                              const std::array<FrameColour, 256>& palette);

// A complete .bmp file (BITMAPFILEHEADER + BITMAPINFOHEADER, 24-bit,
// bottom-up rows padded to four bytes).
std::vector<std::uint8_t> encode_bmp24(const RgbFrame& frame);

// The top-left width x height of a frame (clamped to its size).
RgbFrame crop_frame(const RgbFrame& frame, int width, int height);

// Pixels that differ between two frames of the same size, or -1 when the
// sizes differ.
std::ptrdiff_t count_differing_pixels(const RgbFrame& left,
                                      const RgbFrame& right);

} // namespace creatures1::display
