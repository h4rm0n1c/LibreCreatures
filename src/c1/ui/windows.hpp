#pragma once

namespace creatures1::ui {

struct WindowPolicyState {
    bool always_on_top = false;
};

class WindowPlatform {
public:
    virtual ~WindowPlatform() = default;
    virtual void set_window_always_on_top(bool enabled) = 0;
    virtual void flash_window() = 0;
    virtual void forward_default_activation(int activation_code) = 0;
};

void toggle_window_always_on_top(WindowPolicyState& state,
                                 WindowPlatform& platform);

void on_activate_flash_window(WindowPlatform& platform,
                              int activation_code);

} // namespace creatures1::ui
