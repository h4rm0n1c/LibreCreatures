#include "mfc_adapters.hpp"

namespace creatures1::platform {

bool point_in_rect_xy(const RectangleHitTestApi& api,
                      const Rectangle& rectangle,
                      std::int32_t x,
                      std::int32_t y) {
    return api.contains(rectangle, x, y);
}

void set_edit_selection_and_scroll(EditControlApi& api,
                                   std::intptr_t selection_start,
                                   std::intptr_t selection_end) {
    api.set_selection(selection_start, selection_end);
    api.scroll_caret();
}

} // namespace creatures1::platform
