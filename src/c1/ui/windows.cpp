#include "windows.hpp"

namespace creatures1::ui {

void toggle_window_always_on_top(WindowPolicyState& state,
                                 WindowPlatform& platform) {
    state.always_on_top = !state.always_on_top;
    platform.set_window_always_on_top(state.always_on_top);
}

void on_activate_flash_window(WindowPlatform& platform,
                              int activation_code) {
    if (activation_code == 1) {
        platform.flash_window();
    }
    platform.forward_default_activation(activation_code);
}

} // namespace creatures1::ui
