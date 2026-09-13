#pragma once

#include <cstdint>

namespace creatures1::application {

class MainFrameWindow {
public:
    virtual ~MainFrameWindow() = default;
    virtual std::uintptr_t native_handle() const = 0;
};

// Platform ownership of USER32::SendMessageA is kept behind this boundary.
class WindowCommandApi {
public:
    virtual ~WindowCommandApi() = default;
    virtual void send_command(std::uintptr_t window_handle,
                              std::uint32_t command_id) const = 0;
};

void dispatch_main_frame_command(const MainFrameWindow& main_frame,
                                 const WindowCommandApi& window_api,
                                 std::uint32_t command_id);

} // namespace creatures1::application
