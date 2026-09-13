#include "update_timer.hpp"

namespace creatures1::world {

namespace {
constexpr std::uint32_t kUpdateTimerId = 1;
constexpr std::uint32_t kMinimumIntervalMs = 1;
constexpr std::uint32_t kMaximumIntervalMs = 300;
constexpr std::uint32_t kSlowModeIntervalMs = 90;
}

void dispatch_world_update_timer_with_state(
    UpdateTimerStateConsumer& consumer,
    const UpdateTimerState& state) {
    consumer.dispatch_update_timer(state);
}

void set_world_update_paused(WorldUpdateControl& control, bool paused) {
    if (paused) {
        control.service_document_timer();
    } else {
        control.arm_application_timer();
    }
}

void configure_update_timer_interval(UpdateTimerState& state,
                                     std::uint32_t command,
                                     TimerScheduler* scheduler,
                                     std::uintptr_t window_handle) {
    if (command != 0) {
        if (command == static_cast<std::uint32_t>(
                           UpdateTimerCommand::increase_by_five_ms)) {
            state.interval_ms += 5;
        } else if (command == static_cast<std::uint32_t>(
                                  UpdateTimerCommand::decrease_by_five_ms)) {
            if (state.interval_ms < 5) {
                state.interval_ms = kMinimumIntervalMs;
            } else {
                state.interval_ms -= 5;
            }
        } else {
            state.interval_ms = command;
            if (command == static_cast<std::uint32_t>(
                               UpdateTimerCommand::select_slow_mode)) {
                state.interval_ms = kSlowModeIntervalMs;
            }
        }
    }

    if (state.interval_ms == 0) {
        state.interval_ms = kMinimumIntervalMs;
    } else if (state.interval_ms > kMaximumIntervalMs) {
        state.interval_ms = kMaximumIntervalMs;
    }

    if (scheduler != nullptr) {
        scheduler->set_timer(window_handle, kUpdateTimerId, state.interval_ms);
    }
}

} // namespace creatures1::world
