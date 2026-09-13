#include "classifier_tip.hpp"

namespace creatures1::ui {
namespace {

// The native offsets the tip 16 pixels down and right of the cursor, and pads
// the measured text by 8 horizontally and 4 vertically before adjusting for
// the window frame.
constexpr int kCursorOffset = 0x10;
constexpr int kHorizontalPadding = 8;
constexpr int kVerticalPadding = 4;

}  // namespace

void hide_classifier_tip(ClassifierTipHost& host) {
    if (host.classifier_tip_visible()) {
        host.hide_classifier_tip_window();
    }
    host.clear_classifier_tip_text();
}

void update_classifier_tip(ClassifierTipHost& host, std::string_view text,
                           int screen_x, int screen_y) {
    const int tip_x = screen_x + kCursorOffset;
    const int tip_y = screen_y + kCursorOffset;

    if (host.classifier_tip_text_matches(text)) {
        host.move_classifier_tip(tip_x, tip_y);
    } else {
        host.set_classifier_tip_text(text);
        const ClassifierTipExtent text_extent =
            host.measure_classifier_tip_text();
        const ClassifierTipExtent window_extent = host.measure_classifier_tip(
            text_extent.width + kHorizontalPadding,
            text_extent.height + kVerticalPadding);
        host.resize_classifier_tip(tip_x, tip_y, window_extent.width,
                                   window_extent.height);
    }

    host.show_classifier_tip_if_hidden();
}

}  // namespace creatures1::ui
