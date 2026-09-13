#pragma once

#include <cstdint>

namespace creatures1::application {

// C1's command table passes the active document to these handlers.  The
// document implementation remains an MFC boundary; the ordering and timer
// policy are owned by the application layer.
class DocumentSaveTarget {
public:
    virtual ~DocumentSaveTarget() = default;
    virtual void save_document() = 0;
    virtual void save_document_as() = 0;
};

// The CMainFrame lifetime and USER32 timer calls are supplied by the window
// adapter.  This interface keeps those mechanisms out of application policy.
class MainFrameTimerHost {
public:
    virtual ~MainFrameTimerHost() = default;
    virtual std::uintptr_t native_handle() const = 0;
    virtual void kill_timer(std::uint32_t timer_id) = 0;
    virtual void set_timer(std::uint32_t timer_id,
                           std::uint32_t interval_ms) = 0;
};

void save_active_document_and_resume_world_timer(
    DocumentSaveTarget& document,
    MainFrameTimerHost* main_frame,
    std::uint32_t& world_update_timer_interval_ms);

void save_active_document_as_and_resume_world_timer(
    DocumentSaveTarget& document,
    MainFrameTimerHost* main_frame,
    std::uint32_t& world_update_timer_interval_ms);

} // namespace creatures1::application
