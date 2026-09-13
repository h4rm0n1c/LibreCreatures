#pragma once

#include <cstddef>
#include <cstdint>

namespace creatures1::application {

class KitProcessHost {
public:
    virtual ~KitProcessHost() = default;

    // CMainFrame's embedded-record layout and the native process-handle
    // operations are platform/UI concerns. The application policy only owns
    // the bounded sweep and the STILL_ACTIVE decision.
    virtual bool main_frame_exists() const = 0;
    virtual bool take_launched_kit_process(std::size_t slot) = 0;
    virtual bool query_taken_process_exit_code(
        std::uint32_t& exit_code) const = 0;
    virtual bool debug_console_exists() const = 0;
    virtual void log_kit_termination(std::size_t slot) = 0;
    virtual void terminate_taken_kit_process(std::uint32_t exit_code) = 0;
    virtual void close_taken_kit_process() = 0;
};

void terminate_launched_kit_processes(KitProcessHost& host,
                                      std::uint32_t initial_exit_code);

} // namespace creatures1::application
