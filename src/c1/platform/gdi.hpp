#pragma once

#include <cstdint>
#include <string_view>

namespace creatures1::platform {

struct TextExtent {
    std::int32_t width;
    std::int32_t height;
};

// The concrete implementation owns CDC/HDC and GetTextExtentPoint32A.  The
// C1-facing contract needs only the measured extent and the byte sequence.
class TextMeasurementApi {
public:
    virtual ~TextMeasurementApi() = default;

    virtual TextExtent measure_ascii_text(std::string_view text) const = 0;
};

TextExtent measure_text_extent(const TextMeasurementApi& api,
                               std::string_view text);

} // namespace creatures1::platform
