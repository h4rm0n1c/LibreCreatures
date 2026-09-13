#include "kit_processes.hpp"

namespace creatures1::application {

void terminate_launched_kit_processes(KitProcessHost& host,
                                      std::uint32_t initial_exit_code) {
    constexpr std::size_t kLaunchedKitProcessSlots = 20;
    constexpr std::uint32_t kStillActiveExitCode = 0x103;

    if (!host.main_frame_exists()) {
        return;
    }

    std::uint32_t exit_code = initial_exit_code;
    for (std::size_t slot = 0; slot < kLaunchedKitProcessSlots; ++slot) {
        if (!host.take_launched_kit_process(slot)) {
            continue;
        }

        const bool exit_code_was_queried =
            host.query_taken_process_exit_code(exit_code);
        if (exit_code_was_queried &&
            exit_code == kStillActiveExitCode) {
            if (host.debug_console_exists()) {
                host.log_kit_termination(slot);
            }
            host.terminate_taken_kit_process(0);
        }
        host.close_taken_kit_process();
    }
}

} // namespace creatures1::application
