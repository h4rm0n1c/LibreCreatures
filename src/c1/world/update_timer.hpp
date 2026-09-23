#pragma once

#include <cstdint>

namespace creatures1::world {

enum class UpdateTimerCommand : std::uint32_t {
    increase_by_five_ms = 500000,
    decrease_by_five_ms = 500001,
    select_slow_mode = 900000,
};

struct UpdateTimerState {
    std::uint32_t interval_ms = 1;
};

// The USER32 SetTimer call belongs to the platform adapter, not the world
// policy. C1 supplies the existing main-window handle and timer identifier.
class TimerScheduler {
public:
    virtual ~TimerScheduler() = default;
    virtual void set_timer(std::uintptr_t window_handle,
                           std::uint32_t timer_id,
                           std::uint32_t interval_ms) = 0;
};

void configure_update_timer_interval(UpdateTimerState& state,
                                     std::uint32_t command,
                                     TimerScheduler* scheduler = nullptr,
                                     std::uintptr_t window_handle = 0);

} // namespace creatures1::world
