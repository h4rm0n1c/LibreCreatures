#include "mfc_adapters.hpp"

namespace creatures1::platform {

bool point_in_rect_xy(const RectangleHitTestApi& api,
                      const Rectangle& rectangle,
                      std::int32_t x,
                      std::int32_t y) {
    return api.contains(rectangle, x, y);
}

} // namespace creatures1::platform
