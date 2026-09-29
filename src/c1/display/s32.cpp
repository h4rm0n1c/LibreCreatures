#include "s32.hpp"

#include <cstring>

namespace creatures1::display {
namespace {

constexpr std::uint32_t kS32ThirtyTwoBitFlag = 0x4;
constexpr std::size_t kFileHeaderBytes = 6;
constexpr std::size_t kImageHeaderBytes = 8;
constexpr std::uint8_t kPngSignature[] = {0x89, 'P', 'N', 'G',
                                          0x0d, 0x0a, 0x1a, 0x0a};
// Signature, then the IHDR chunk: length, "IHDR", width, height, and the
// five one-byte fields and CRC after them.
constexpr std::size_t kPngMinimumBytes = 8 + 4 + 4 + 13 + 4;

std::uint16_t le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint32_t be32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

bool sizes_within_limits(std::uint32_t width, std::uint32_t height) {
    return width >= 1 && height >= 1 && width <= kS32MaxDimension &&
           height <= kS32MaxDimension &&
           static_cast<std::uint64_t>(width) * height <= kS32MaxDecodedPixels;
}

} // namespace

bool parse_s32(const std::uint8_t* bytes, std::size_t size, S32Index& out) {
    out.frames.clear();
    if (bytes == nullptr || size < kFileHeaderBytes ||
        static_cast<std::uint64_t>(size) > kS32MaxFileBytes) {
        return false;
    }
    if ((le32(bytes) & kS32ThirtyTwoBitFlag) == 0) {
        return false;
    }
    const std::size_t count = le16(bytes + 4);
    const std::size_t headers_end =
        kFileHeaderBytes + count * kImageHeaderBytes;
    if (count == 0 || headers_end > size) {
        return false;
    }

    std::vector<S32Frame> frames(count);
    for (std::size_t index = 0; index < count; ++index) {
        frames[index].offset =
            le32(bytes + kFileHeaderBytes + index * kImageHeaderBytes);
    }
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint64_t start = frames[index].offset;
        const std::uint64_t end =
            index + 1 < count ? frames[index + 1].offset : size;
        // In order, after the headers, inside the file, and big enough for
        // a PNG signature and IHDR.
        if (start < headers_end || end > size || end < start ||
            end - start < kPngMinimumBytes) {
            return false;
        }
        const std::uint8_t* png = bytes + start;
        if (std::memcmp(png, kPngSignature, sizeof kPngSignature) != 0 ||
            be32(png + 8) != 13 || std::memcmp(png + 12, "IHDR", 4) != 0) {
            return false;
        }
        const std::uint32_t width = be32(png + 16);
        const std::uint32_t height = be32(png + 20);
        if (!sizes_within_limits(width, height)) {
            return false;
        }
        frames[index].size = static_cast<std::uint32_t>(end - start);
        frames[index].width = width;
        frames[index].height = height;
    }
    out.frames = std::move(frames);
    return true;
}

bool decode_s32_frame(const std::uint8_t* bytes, std::size_t size,
                      const S32Frame& frame, RgbaImage& out) {
    out = {};
    if (bytes == nullptr || frame.offset > size ||
        frame.size > size - frame.offset ||
        !sizes_within_limits(frame.width, frame.height)) {
        return false;
    }
    RgbaImage decoded;
    if (!decode_png_rgba(bytes + frame.offset, frame.size, decoded) ||
        static_cast<std::uint32_t>(decoded.width) != frame.width ||
        static_cast<std::uint32_t>(decoded.height) != frame.height) {
        return false;
    }
    out = std::move(decoded);
    return true;
}

} // namespace creatures1::display
