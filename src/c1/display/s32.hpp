#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace creatures1::display {

// S32 sprite files (not a 1996 format): the true-colour gallery format the
// Creatures 3 / Docking Station community builds adopted.  All little-endian:
//
//   u32 flags            bit 0x4 set (32-bit); other bits ignored
//   u16 image count
//   per image: u32 offset of its PNG, u16 width, u16 height (deprecated)
//   the PNGs; each runs from its offset to the next one's, the last to EOF
//
// The PNG defines the frame's real size; transparency is its alpha channel.
// Here an S32 sits beside a C1 .spr as a higher-quality way to draw the same
// frames (NAME.s32 at the same size, NAME@2x.s32 at twice the size).

// The spec's limit on either dimension.
constexpr std::uint32_t kS32MaxDimension = 16384;
// A decode limit of our own: 16M pixels (64 MB as RGBA).  A 32-bit process
// cannot afford the 1 GB a 16384 x 16384 frame would take.
constexpr std::uint64_t kS32MaxDecodedPixels = 16u * 1024u * 1024u;
// Files larger than this are refused before anything is read into memory.
constexpr std::uint64_t kS32MaxFileBytes = 256u * 1024u * 1024u;

struct S32Frame {
    std::uint32_t offset = 0;  // of the PNG, from the start of the file
    std::uint32_t size = 0;    // bytes of PNG
    std::uint32_t width = 0;   // from the PNG's IHDR, not the S32 header
    std::uint32_t height = 0;
};

struct S32Index {
    std::vector<S32Frame> frames;
};

// Straight (non-premultiplied) RGBA, four bytes a pixel, rows top-down.
struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

// Checks and indexes a whole S32 file without decoding any image: the flags
// bit, the count, every offset in order and inside the file, each PNG's
// signature and IHDR, and the size limits.  Nothing in the file is trusted
// until it has been checked against the file's real length.
bool parse_s32(const std::uint8_t* bytes, std::size_t size, S32Index& out);

// Decodes one frame of a parsed file to RGBA.  Fails if the PNG is damaged
// or decodes to a size other than its IHDR's.
bool decode_s32_frame(const std::uint8_t* bytes, std::size_t size,
                      const S32Frame& frame, RgbaImage& out);

// PNG bytes to RGBA (src/c1/display/png_decode.cpp, stb_image).
bool decode_png_rgba(const std::uint8_t* bytes, std::size_t size,
                     RgbaImage& out);

} // namespace creatures1::display
