// PNG decoding for .s32 sprite files, through the vendored stb_image
// (third_party/stb, public domain / MIT).  PNG only; nothing else in stb is
// compiled in.

#include "s32.hpp"

#include <climits>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
// Refuse anything the S32 limits already refuse, inside the decoder as well.
#define STBI_MAX_DIMENSIONS 16384
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "../../../third_party/stb/stb_image.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace creatures1::display {

bool decode_png_rgba(const std::uint8_t* bytes, std::size_t size,
                     RgbaImage& out) {
    out = {};
    if (bytes == nullptr || size == 0 || size > static_cast<std::size_t>(INT_MAX)) {
        return false;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    // Check the size first, from the header alone, so a lying IHDR cannot
    // make the decoder allocate past the S32 pixel limit.
    if (!stbi_info_from_memory(bytes, static_cast<int>(size), &width, &height,
                               &channels) ||
        width <= 0 || height <= 0 ||
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) >
            kS32MaxDecodedPixels) {
        return false;
    }
    stbi_uc* pixels = stbi_load_from_memory(bytes, static_cast<int>(size),
                                            &width, &height, &channels, 4);
    if (pixels == nullptr) {
        return false;
    }
    out.width = width;
    out.height = height;
    out.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) *
                                         static_cast<std::size_t>(height) * 4u);
    stbi_image_free(pixels);
    return true;
}

} // namespace creatures1::display
