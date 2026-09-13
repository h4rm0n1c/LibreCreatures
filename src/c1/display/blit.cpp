#include "blit.hpp"

#include <cstddef>

namespace creatures1::display {

void blit_zero_transparent_pixels(const IndexedPixelSurface& source,
                                  std::int32_t source_x,
                                  std::int32_t source_y,
                                  const IndexedPixelSurface& destination,
                                  std::int32_t destination_x,
                                  std::int32_t destination_y,
                                  std::uint32_t width,
                                  std::int32_t height) {
    const std::uint8_t* source_row =
        source.pixels + source_y * source.stride + source_x;
    std::uint8_t* destination_row =
        destination.pixels + destination_y * destination.stride + destination_x;

    for (std::int32_t row = 0; row < height; ++row) {
        for (std::uint32_t column = 0; column < width; ++column) {
            const std::uint8_t pixel = source_row[column];
            if (pixel != 0) {
                destination_row[column] = pixel;
            }
        }
        source_row += source.stride;
        destination_row += destination.stride;
    }
}

} // namespace creatures1::display
