#pragma once

#include <cstdint>

namespace creatures1::platform {

// Win32 RECT is four 32-bit LONG fields on the x86 target.  The platform
// adapter owns the actual SDK type and PtInRect call at the build boundary.
struct Rectangle {
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

class RectangleHitTestApi {
public:
    virtual ~RectangleHitTestApi() = default;

    virtual bool contains(const Rectangle& rectangle,
                          std::int32_t x,
                          std::int32_t y) const = 0;
};

class EditControlApi {
public:
    virtual ~EditControlApi() = default;

    virtual void set_selection(std::intptr_t selection_start,
                               std::intptr_t selection_end) = 0;
    virtual void scroll_caret() = 0;
};

bool point_in_rect_xy(const RectangleHitTestApi& api,
                      const Rectangle& rectangle,
                      std::int32_t x,
                      std::int32_t y);

void set_edit_selection_and_scroll(EditControlApi& api,
                                   std::intptr_t selection_start,
                                   std::intptr_t selection_end);

} // namespace creatures1::platform
