#include "commands.hpp"

namespace creatures1::application {

void dispatch_main_frame_command(const MainFrameWindow& main_frame,
                                 const WindowCommandApi& window_api,
                                 std::uint32_t command_id) {
    window_api.send_command(main_frame.native_handle(), command_id);
}

} // namespace creatures1::application
