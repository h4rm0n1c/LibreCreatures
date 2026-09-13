#pragma once

#include <memory>
#include "windows_prelude.hpp"

#include "../application/main_frame.hpp"

namespace creatures1::platform {

// Concrete Win32 handle for the frame's main menu bar.
class C1NativeMainMenuHandle final
    : public creatures1::application::MainMenuHandle {
public:
    explicit C1NativeMainMenuHandle(HMENU menu = nullptr) : menu_(menu) {}

    void reset(HMENU menu) { menu_ = menu; }
    HMENU native_menu() const { return menu_; }

private:
    HMENU menu_ = nullptr;
};

class C1MainFrame;

// The closed main frame, held from PostNcDestroy until the message loop has
// unwound.  Resetting it runs ~C1MainFrame.
std::unique_ptr<C1MainFrame>& deferred_main_frame_release();

} // namespace creatures1::platform
