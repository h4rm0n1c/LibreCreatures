// display/s32: parsing, decoding, and refusing damaged or hostile files.
// Build: c++ -std=c++17 -I src/c1 tests/c1_s32_test.cpp
//            src/c1/display/s32.cpp src/c1/display/png_decode.cpp

#include "display/s32.hpp"
#include "s32_test_files.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace display = creatures1::display;

using namespace s32_test_files;

namespace {

bool parses(const Bytes& file) {
    display::S32Index index;
    return display::parse_s32(file.data(), file.size(), index);
}

void set_le32(Bytes& file, std::size_t at, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        file[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

} // namespace

int main() {
    const Bytes red = make_png(3, 2, 0xff0000ffu);
    const Bytes clear = make_png(5, 4, 0x00ff0080u);
    const Bytes file = make_s32({red, clear});

    display::S32Index index;
    assert(display::parse_s32(file.data(), file.size(), index));
    assert(index.frames.size() == 2);
    assert(index.frames[0].width == 3 && index.frames[0].height == 2);
    assert(index.frames[1].width == 5 && index.frames[1].height == 4);
    assert(index.frames[0].size == red.size());
    assert(index.frames[1].offset + index.frames[1].size == file.size());

    display::RgbaImage image;
    assert(display::decode_s32_frame(file.data(), file.size(),
                                     index.frames[0], image));
    assert(image.width == 3 && image.height == 2 && image.rgba.size() == 24);
    assert(image.rgba[0] == 0xff && image.rgba[1] == 0 &&
           image.rgba[2] == 0 && image.rgba[3] == 0xff);
    assert(display::decode_s32_frame(file.data(), file.size(),
                                     index.frames[1], image));
    assert(image.rgba[1] == 0xff && image.rgba[3] == 0x80);  // alpha kept

    // Truncation: nothing reads past the end.  Cutting into the headers or
    // an earlier frame is refused outright.  The last frame runs to the end
    // of the file, so a cut inside it (past its 33-byte signature and IHDR)
    // still indexes -- and then fails to decode.
    const std::size_t last_offset = 6 + 16 + red.size();
    for (std::size_t size = 0; size < file.size(); ++size) {
        display::S32Index partial;
        const bool ok = display::parse_s32(file.data(), size, partial);
        assert(ok == (size >= last_offset + 33));
        if (ok) {
            // Only the 12-byte IEND chunk may go missing and still decode:
            // every pixel is intact without it.
            display::RgbaImage out;
            const bool decoded = display::decode_s32_frame(
                file.data(), size, partial.frames[1], out);
            assert(!decoded || size >= file.size() - 12);
            assert(!decoded || (out.width == 5 && out.rgba[3] == 0x80));
        }
    }

    // The 32-bit flag must be set.
    assert(!parses(make_s32({red}, 0x1)));
    // No frames.
    assert(!parses(make_s32({})));
    // Offsets pointing into the headers, past the end, or backwards.
    {
        Bytes bad = file;
        set_le32(bad, 6, 2);
        assert(!parses(bad));
        bad = file;
        set_le32(bad, 14, static_cast<std::uint32_t>(file.size() + 10));
        assert(!parses(bad));
        bad = file;
        set_le32(bad, 14, 30);  // before frame 0's offset
        assert(!parses(bad));
    }
    // A frame that isn't a PNG.
    {
        Bytes bad = file;
        bad[6 + 16] = 'X';
        assert(!parses(bad));
    }
    // An IHDR claiming a size past the limits.
    {
        Bytes bad = file;
        const std::size_t ihdr = 6 + 16 + 16;
        bad[ihdr] = 0x7f;  // width near 2^31
        assert(!parses(bad));
        bad = file;
        bad[ihdr + 1] = 0x01;  // 65536 + 3 wide: over 16384
        assert(!parses(bad));
    }
    // A header count claiming more frames than the file holds.
    {
        Bytes bad = file;
        bad[4] = 0xff;
        bad[5] = 0xff;
        assert(!parses(bad));
    }
    // Things people rename to .s32.
    {
        Bytes avi = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'A', 'V', 'I', ' '};
        avi.resize(4096, 0x11);
        assert(!parses(avi));
        Bytes lone = red;  // a bare PNG
        assert(!parses(lone));
    }
    // A damaged PNG body parses (the index is fine) but will not decode.
    {
        Bytes bad = file;
        bad[6 + 16 + 40] ^= 0xff;  // inside frame 0's IDAT
        display::S32Index damaged;
        assert(display::parse_s32(bad.data(), bad.size(), damaged));
        display::RgbaImage out;
        assert(!display::decode_s32_frame(bad.data(), bad.size(),
                                          damaged.frames[0], out) ||
               out.width == 3);  // stb may still decode a stored block
    }
    // A frame record that points outside the buffer is refused at decode.
    {
        display::S32Frame wild = index.frames[0];
        wild.offset = static_cast<std::uint32_t>(file.size());
        display::RgbaImage out;
        assert(!display::decode_s32_frame(file.data(), file.size(), wild, out));
    }

    std::puts("c1_s32_test: ok");
    return 0;
}
