#pragma once

#include "../objects/object.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::ui {

struct StatusBarPaneInfo {
    std::uint32_t command_id = 0;
    std::uint32_t style_flags = 0;
    std::int32_t width = 0;
};

struct StatusBarClientRect {
    std::int32_t right = 0;
};

// CStatusBar, USER32, and the native window procedure are supplied by this
// adapter. EventBar owns the pane redistribution policy around those calls.
class EventBarWindowApi {
public:
    virtual ~EventBarWindowApi() = default;

    virtual StatusBarPaneInfo pane_info(std::uint32_t pane_index) const = 0;
    virtual StatusBarClientRect client_rect() const = 0;
    virtual bool has_parent_window() const = 0;
    virtual bool parent_is_zoomed() const = 0;
    virtual std::uint32_t window_style() const = 0;
    virtual std::int32_t system_metric(std::int32_t metric) const = 0;
    virtual void def_window_proc(std::uint32_t message,
                                 std::uintptr_t pane_count,
                                 std::intptr_t pane_right_edges) = 0;
};

// The two words are part of the historical archive record, but whole-binary
// reads and save specimens show no application meaning for them. Keeping
// them with the archive compatibility record prevents them becoming fake
// EventBar runtime fields while preserving the record shape for later byte
// verification.
struct EventBarLegacyArchiveWords {
    std::int32_t first = 0;
    std::int32_t second = 0;
};

class EventBarArchiveApi {
public:
    virtual ~EventBarArchiveApi() = default;

    virtual bool is_loading() const = 0;
    virtual std::int32_t read_displayed_object_count() = 0;
    virtual objects::Object* read_object() = 0;
    virtual void write_displayed_object_count(std::int32_t count) = 0;
    virtual void write_object(objects::Object* object) = 0;
    virtual std::int32_t read_legacy_state_word() = 0;
    virtual void write_legacy_state_word(std::int32_t value) = 0;
};

// The EventBar owns list membership and ordering.  Document-state storage,
// Funeral Kit dispatch, and viewport navigation are owned by their existing
// application/world subsystems and enter through this narrow policy boundary.
class EventBarObjectPolicyApi {
public:
    virtual ~EventBarObjectPolicyApi() = default;

    virtual bool is_creature(const objects::Object& object) const = 0;
    virtual bool creature_is_dead(const objects::Object& object) const = 0;
    virtual std::uint32_t genome_filename_id(
        const objects::Object& object) const = 0;
    virtual std::size_t funeral_state_word_count() const = 0;
    virtual void append_funeral_state_word(std::uint32_t value) = 0;
    virtual void flush_funeral_kit_state() = 0;
    virtual void disable_viewport_navigation() = 0;
    virtual void refresh_display_panes() = 0;
};

struct EventBarScoreSnapshot {
    std::int32_t natural_eggs_laid = 0;
    std::int32_t population_time_accumulator = 0;
};

struct StatusBarTextMetrics {
    std::int32_t width = 0;
};

// Status-bar controls, localized resources, and the selected-creature/score
// owners are supplied by the UI/application adapter. EventBar retains the C1
// pane numbering, formatting, clamping, and update ordering.
class EventBarStatusApi {
public:
    virtual ~EventBarStatusApi() = default;

    virtual std::string load_string(std::uint32_t resource_id) const = 0;
    virtual std::string object_pane_text(const objects::Object& object,
                                         std::uint32_t resource_id) const = 0;
    virtual StatusBarTextMetrics measure_text(std::string_view text) const = 0;
    virtual void set_pane_text(std::uint32_t pane_index,
                               std::string_view text) = 0;
    virtual void set_pane_width(std::uint32_t pane_index,
                                std::int32_t width) = 0;
    virtual void set_pane_disabled(std::uint32_t pane_index,
                                   bool disabled) = 0;
    virtual void invalidate_status_bar() = 0;

    virtual bool selected_creature_exists() const = 0;
    virtual bool selected_creature_is_dead() const = 0;
    virtual std::uint8_t selected_creature_glycogen() const = 0;
    virtual EventBarScoreSnapshot score_snapshot() const = 0;
    virtual std::uint32_t world_tick_count() const = 0;
};

// Mouse hit testing, viewport navigation, selection, and embedded-kit
// dispatch are owned by their existing subsystems. This boundary keeps the
// recovered click policy on EventBar without importing HWND/COM/EH details.
class EventBarInteractionApi {
public:
    virtual ~EventBarInteractionApi() = default;

    virtual bool point_is_inside_pane(std::uint32_t pane_index,
                                      long x,
                                      long y) const = 0;
    virtual int viewport_width() const = 0;
    virtual int viewport_height() const = 0;
    virtual void request_viewport_origin(int x, int y) = 0;
    virtual bool viewport_navigation_is_disabled() const = 0;
    virtual void disable_viewport_navigation() = 0;

    virtual bool is_creature(const objects::Object& object) const = 0;
    virtual void select_creature(objects::Object& object) = 0;
    virtual bool creature_is_dead(const objects::Object& object) const = 0;
    virtual void notify_embedded_kit_of_death(
        objects::Object& object) = 0;
    virtual void flush_funeral_kit_document_state() = 0;
    virtual std::string object_display_name(
        const objects::Object& object) const = 0;
    virtual std::string unnamed_creature_label() const = 0;
    virtual bool embedded_kit_is_connected(std::size_t tool_index) const = 0;
    virtual void execute_embedded_kit_tool(std::size_t tool_index) = 0;
};

class EventBar {
public:
    static constexpr std::size_t kMaximumDisplayedObjects = 10;

    void window_proc(EventBarWindowApi& window,
                     std::uint32_t message,
                     std::uint32_t pane_count,
                     std::int32_t* pane_right_edges) const;

    void serialize(EventBarArchiveApi& archive,
                   EventBarLegacyArchiveWords& legacy_words);

    void add_object(objects::Object* object,
                    EventBarObjectPolicyApi& policy);
    void remove_object(objects::Object* object,
                       bool record_auxiliary_state,
                       EventBarObjectPolicyApi& policy);

    void refresh_object_display_panes(EventBarStatusApi& status,
                                      const EventBarObjectPolicyApi& policy) const;
    void update_status_panes(EventBarStatusApi& status) const;
    void on_left_button_down(std::uint32_t mouse_flags,
                             long mouse_x,
                             long mouse_y,
                             EventBarInteractionApi& interaction,
                             EventBarObjectPolicyApi& policy);

    std::size_t displayed_object_count() const { return displayed_count_; }
    objects::Object* displayed_object(std::size_t index) const {
        return displayed_objects_[index];
    }

private:
    std::array<objects::Object*, kMaximumDisplayedObjects> displayed_objects_{};
    std::size_t displayed_count_ = 0;
};

// The application-level entry point owns the process-wide EventBar instance
// in the original UI flow.  The instance operation remains on EventBar so
// list invariants have one owner, while the caller supplies the application
// policy boundary for creature/document/viewport side effects.
void add_object_to_event_bar_display_list(
    EventBar& event_bar,
    objects::Object* object,
    EventBarObjectPolicyApi& policy);

} // namespace creatures1::ui
