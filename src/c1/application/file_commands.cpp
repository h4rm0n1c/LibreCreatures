#include "file_commands.hpp"

namespace creatures1::application {

namespace {
constexpr std::uint32_t kWorldUpdateTimerId = 1;
constexpr std::uint32_t kMinimumWorldUpdateIntervalMs = 1;
constexpr std::uint32_t kMaximumWorldUpdateIntervalMs = 300;

void resume_world_timer_after_document_save(
    MainFrameTimerHost* main_frame,
    std::uint32_t& world_update_timer_interval_ms) {
    if (world_update_timer_interval_ms == 0) {
        world_update_timer_interval_ms = kMinimumWorldUpdateIntervalMs;
    } else if (world_update_timer_interval_ms >
               kMaximumWorldUpdateIntervalMs) {
        world_update_timer_interval_ms = kMaximumWorldUpdateIntervalMs;
    }

    if (main_frame != nullptr) {
        main_frame->set_timer(kWorldUpdateTimerId,
                              world_update_timer_interval_ms);
    }
}

template <typename SaveOperation>
void save_document_and_resume_world_timer(
    DocumentSaveTarget& document,
    MainFrameTimerHost* main_frame,
    std::uint32_t& world_update_timer_interval_ms,
    SaveOperation save_operation) {
    if (main_frame != nullptr) {
        main_frame->kill_timer(kWorldUpdateTimerId);
    }

    save_operation(document);
    resume_world_timer_after_document_save(
        main_frame, world_update_timer_interval_ms);
}
} // namespace

void save_active_document_and_resume_world_timer(
    DocumentSaveTarget& document,
    MainFrameTimerHost* main_frame,
    std::uint32_t& world_update_timer_interval_ms) {
    save_document_and_resume_world_timer(
        document, main_frame, world_update_timer_interval_ms,
        [](DocumentSaveTarget& target) { target.save_document(); });
}

void save_active_document_as_and_resume_world_timer(
    DocumentSaveTarget& document,
    MainFrameTimerHost* main_frame,
    std::uint32_t& world_update_timer_interval_ms) {
    save_document_and_resume_world_timer(
        document, main_frame, world_update_timer_interval_ms,
        [](DocumentSaveTarget& target) { target.save_document_as(); });
}

} // namespace creatures1::application
