// display/frame_dump: palette expansion, the .bmp encoder, and the frame
// comparison the renderer back-ends are checked with.
// Build: c++ -std=c++17 -I src/c1 tests/c1_frame_dump_test.cpp
//            src/c1/display/frame_dump.cpp

#include "display/frame_dump.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace display = creatures1::display;

namespace {

std::uint32_t u32_at(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    return bytes[at] | (bytes[at + 1] << 8) | (bytes[at + 2] << 16) |
           (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
}

} // namespace

int main() {
    std::array<display::FrameColour, 256> palette{};
    palette[1] = {0x11, 0x22, 0x33};
    palette[2] = {0xaa, 0xbb, 0xcc};

    // 3x2, rows padded to a stride of 4 (the padding byte must be ignored).
    const std::uint8_t pixels[] = {1, 2, 0, 9,
                                   2, 1, 1, 9};
    const display::RgbFrame frame =
        display::expand_indexed_frame(pixels, 3, 2, 4, palette);
    assert(frame.width == 3 && frame.height == 2);
    assert(frame.pixels[0] == 0x112233u);
    assert(frame.pixels[1] == 0xaabbccu);
    assert(frame.pixels[2] == 0x000000u);
    assert(frame.pixels[3] == 0xaabbccu);
    assert(frame.pixels[5] == 0x112233u);

    // Bad input gives an empty frame, not a crash.
    assert(display::expand_indexed_frame(nullptr, 3, 2, 4, palette).width == 0);
    assert(display::expand_indexed_frame(pixels, 3, 2, 2, palette).width == 0);

    // .bmp: 24-bit, bottom-up, rows padded to four bytes (3 px * 3 = 9 -> 12).
    const std::vector<std::uint8_t> bmp = display::encode_bmp24(frame);
    assert(bmp[0] == 'B' && bmp[1] == 'M');
    assert(u32_at(bmp, 2) == bmp.size());
    assert(bmp.size() == 54 + 12 * 2);
    assert(u32_at(bmp, 10) == 54);
    assert(u32_at(bmp, 18) == 3 && u32_at(bmp, 22) == 2);
    assert(bmp[28] == 24);
    // The first stored row is the frame's bottom row: 0xaabbcc as B, G, R.
    assert(bmp[54] == 0xcc && bmp[55] == 0xbb && bmp[56] == 0xaa);
    // The top row's first pixel, 0x112233, starts the second stored row.
    assert(bmp[66] == 0x33 && bmp[67] == 0x22 && bmp[68] == 0x11);

    // Comparison.
    display::RgbFrame other = frame;
    assert(display::count_differing_pixels(frame, other) == 0);
    other.pixels[4] ^= 1;
    other.pixels[0] ^= 0x10000;
    assert(display::count_differing_pixels(frame, other) == 2);
    display::RgbFrame smaller = frame;
    smaller.width = 2;
    assert(display::count_differing_pixels(frame, smaller) == -1);

    std::puts("c1_frame_dump_test: ok");
    return 0;
}
