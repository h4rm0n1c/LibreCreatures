#pragma once

#include <string_view>

namespace creatures1::ui {

struct ClassifierTipExtent {
    int width = 0;
    int height = 0;
};

// One boundary for the whole classifier tip, not one per operation.  The
// recovered policy owns the text comparison, the +16 screen offset, and the
// 8x4 padding around the measured text; window creation, the device context,
// font selection, and SetWindowPos remain host operations.
class ClassifierTipHost {
public:
    virtual ~ClassifierTipHost() = default;

    virtual bool classifier_tip_visible() const = 0;
    virtual void hide_classifier_tip_window() = 0;
    virtual void clear_classifier_tip_text() = 0;

    virtual bool classifier_tip_text_matches(std::string_view text) const = 0;
    virtual void set_classifier_tip_text(std::string_view text) = 0;
    // Measures with the tip's own DC and stock GUI font, then reports the
    // window size that text needs including the frame.
    virtual ClassifierTipExtent measure_classifier_tip(
        int content_width, int content_height) = 0;
    virtual void move_classifier_tip(int screen_x, int screen_y) = 0;
    virtual void resize_classifier_tip(int screen_x, int screen_y,
                                       int width, int height) = 0;
    virtual ClassifierTipExtent measure_classifier_tip_text() = 0;
    virtual void show_classifier_tip_if_hidden() = 0;
};

void hide_classifier_tip(ClassifierTipHost& host);

// SFCView::OnMouseMove @ 00436b30, classifier-tip branch.  Unchanged text
// only repositions; changed text is re-measured, the window resized around
// it, and the tip repainted.  Either way the tip is shown without activating.
void update_classifier_tip(ClassifierTipHost& host, std::string_view text,
                           int screen_x, int screen_y);

}  // namespace creatures1::ui
