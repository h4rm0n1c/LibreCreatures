#include "windows_shell.hpp"

namespace creatures1::platform {

void set_world_view_safe_frame(C1WindowsView* view,
                               std::uint32_t frame_count) {
    if (view != nullptr) {
        view->set_manual_navigation_safe_frame_count_for_world_tick(
            frame_count);
    }
}

C1WindowsView* active_c1_view(C1MainFrame& frame) {
    return DYNAMIC_DOWNCAST(C1WindowsView, frame.GetActiveView());
}

void toggle_native_mute(C1WindowsDocument& document) {
    if (document.semantic_document_mutable() == nullptr) {
        return;
    }
    C1NativeMuteControl control(document);
    creatures1::application::toggle_mute_and_stop_sounds(control);
}

} // namespace creatures1::platform
