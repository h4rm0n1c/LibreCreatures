#pragma once

#include <cstdint>

namespace creatures1::display {

struct IndexedPixelSurface {
    std::uint8_t* pixels = nullptr;
    std::int32_t stride = 0;
};

// Copies an indexed-colour rectangle while treating palette index zero as
// transparent. Coordinates are relative to each surface's pixel origin.
void blit_zero_transparent_pixels(const IndexedPixelSurface& source,
                                  std::int32_t source_x,
                                  std::int32_t source_y,
                                  const IndexedPixelSurface& destination,
                                  std::int32_t destination_x,
                                  std::int32_t destination_y,
                                  std::uint32_t width,
                                  std::int32_t height);

} // namespace creatures1::display
