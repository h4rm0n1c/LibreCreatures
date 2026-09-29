#include "frame_dump.hpp"

#include <algorithm>

namespace creatures1::display {
namespace {

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

} // namespace

RgbFrame expand_indexed_frame(const std::uint8_t* pixels, int width,
                              int height, int stride,
                              const std::array<FrameColour, 256>& palette) {
    RgbFrame frame;
    if (pixels == nullptr || width <= 0 || height <= 0 || stride < width) {
        return frame;
    }
    frame.width = width;
    frame.height = height;
    frame.pixels.resize(static_cast<std::size_t>(width) *
                        static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* row =
            pixels + static_cast<std::ptrdiff_t>(y) * stride;
        std::uint32_t* out =
            frame.pixels.data() + static_cast<std::size_t>(y) * width;
        for (int x = 0; x < width; ++x) {
            const FrameColour& colour = palette[row[x]];
            out[x] = (static_cast<std::uint32_t>(colour.red) << 16) |
                     (static_cast<std::uint32_t>(colour.green) << 8) |
                     colour.blue;
        }
    }
    return frame;
}

std::vector<std::uint8_t> encode_bmp24(const RgbFrame& frame) {
    std::vector<std::uint8_t> out;
    if (frame.width <= 0 || frame.height <= 0) {
        return out;
    }
    const std::uint32_t row_bytes =
        (static_cast<std::uint32_t>(frame.width) * 3u + 3u) & ~3u;
    const std::uint32_t image_bytes =
        row_bytes * static_cast<std::uint32_t>(frame.height);
    constexpr std::uint32_t kHeaderBytes = 14 + 40;
    out.reserve(kHeaderBytes + image_bytes);

    out.push_back('B');
    out.push_back('M');
    put_u32(out, kHeaderBytes + image_bytes);
    put_u32(out, 0);
    put_u32(out, kHeaderBytes);

    put_u32(out, 40);
    put_u32(out, static_cast<std::uint32_t>(frame.width));
    put_u32(out, static_cast<std::uint32_t>(frame.height));  // bottom-up
    put_u16(out, 1);
    put_u16(out, 24);
    put_u32(out, 0);  // BI_RGB
    put_u32(out, image_bytes);
    put_u32(out, 2835);
    put_u32(out, 2835);
    put_u32(out, 0);
    put_u32(out, 0);

    for (int y = frame.height - 1; y >= 0; --y) {
        const std::uint32_t* row =
            frame.pixels.data() + static_cast<std::size_t>(y) * frame.width;
        std::uint32_t written = 0;
        for (int x = 0; x < frame.width; ++x) {
            out.push_back(static_cast<std::uint8_t>(row[x]));        // blue
            out.push_back(static_cast<std::uint8_t>(row[x] >> 8));   // green
            out.push_back(static_cast<std::uint8_t>(row[x] >> 16));  // red
            written += 3;
        }
        for (; written < row_bytes; ++written) {
            out.push_back(0);
        }
    }
    return out;
}

RgbFrame crop_frame(const RgbFrame& frame, int width, int height) {
    RgbFrame out;
    out.width = std::max(0, std::min(width, frame.width));
    out.height = std::max(0, std::min(height, frame.height));
    out.pixels.reserve(static_cast<std::size_t>(out.width) * out.height);
    for (int y = 0; y < out.height; ++y) {
        const std::uint32_t* row =
            frame.pixels.data() + static_cast<std::size_t>(y) * frame.width;
        out.pixels.insert(out.pixels.end(), row, row + out.width);
    }
    return out;
}

std::ptrdiff_t count_differing_pixels(const RgbFrame& left,
                                      const RgbFrame& right) {
    if (left.width != right.width || left.height != right.height ||
        left.pixels.size() != right.pixels.size()) {
        return -1;
    }
    std::ptrdiff_t differing = 0;
    for (std::size_t index = 0; index < left.pixels.size(); ++index) {
        if (left.pixels[index] != right.pixels[index]) {
            ++differing;
        }
    }
    return differing;
}

} // namespace creatures1::display
