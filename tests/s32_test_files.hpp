// Builds real PNG and S32 files for the S32 tests (tests/c1_s32_test.cpp,
// tests/c1_image_tiers_test.cpp).  Header-only; each test includes it once.
#pragma once

#include <cstdint>
#include <vector>

using Bytes = std::vector<std::uint8_t>;

namespace s32_test_files {

void put_be32(Bytes& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void put_le32(Bytes& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void put_le16(Bytes& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void put_chunk(Bytes& out, const char* type, const Bytes& data) {
    put_be32(out, static_cast<std::uint32_t>(data.size()));
    Bytes typed(type, type + 4);
    typed.insert(typed.end(), data.begin(), data.end());
    out.insert(out.end(), typed.begin(), typed.end());
    put_be32(out, crc32(typed.data(), typed.size()));
}

// A minimal valid RGBA PNG (at most 64 KB of pixels): filter 0 on every
// row, one stored deflate block.
Bytes make_png(std::uint32_t width, std::uint32_t height,
               std::uint32_t rgba_fill) {
    Bytes raw;
    for (std::uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        for (std::uint32_t x = 0; x < width; ++x) {
            raw.push_back(static_cast<std::uint8_t>(rgba_fill >> 24));
            raw.push_back(static_cast<std::uint8_t>(rgba_fill >> 16));
            raw.push_back(static_cast<std::uint8_t>(rgba_fill >> 8));
            raw.push_back(static_cast<std::uint8_t>(rgba_fill));
        }
    }
    Bytes zlib = {0x78, 0x01, 0x01};  // header, final stored block
    const auto len = static_cast<std::uint16_t>(raw.size());
    put_le16(zlib, len);
    put_le16(zlib, static_cast<std::uint16_t>(~len));
    zlib.insert(zlib.end(), raw.begin(), raw.end());
    std::uint32_t a = 1, b = 0;
    for (std::uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    put_be32(zlib, (b << 16) | a);

    Bytes png = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    Bytes ihdr;
    put_be32(ihdr, width);
    put_be32(ihdr, height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA
    put_chunk(png, "IHDR", ihdr);
    put_chunk(png, "IDAT", zlib);
    put_chunk(png, "IEND", {});
    return png;
}

Bytes make_s32(const std::vector<Bytes>& pngs, std::uint32_t flags = 0x4) {
    Bytes out;
    put_le32(out, flags);
    put_le16(out, static_cast<std::uint16_t>(pngs.size()));
    std::uint32_t offset =
        6 + 8 * static_cast<std::uint32_t>(pngs.size());
    for (const Bytes& png : pngs) {
        put_le32(out, offset);
        put_le16(out, 0);  // deprecated width/height, ignored
        put_le16(out, 0);
        offset += static_cast<std::uint32_t>(png.size());
    }
    for (const Bytes& png : pngs) {
        out.insert(out.end(), png.begin(), png.end());
    }
    return out;
}

} // namespace s32_test_files
