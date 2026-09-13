#include "gdi.hpp"

namespace creatures1::platform {

TextExtent measure_text_extent(const TextMeasurementApi& api,
                               std::string_view text) {
    return api.measure_ascii_text(text);
}

} // namespace creatures1::platform
