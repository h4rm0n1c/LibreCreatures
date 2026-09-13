#pragma once

#include "windows_macro_host.hpp"
#include "windows_shell.hpp"

namespace creatures1::platform {

// Concrete PointerToolRuntimeHost.
//
// Of its forty-three methods across six base interfaces, twenty-six already
// had a platform implementation on the document, the view, the frame or one
// of the hosts built earlier: object and creature registries, movement
// bounds, script dispatch, the immediate event queue, edit-object state and
// the world-rect hit test.  This binds those through and supplies the rest,
// which are the pointer tool's own bubble, stimulus and input-flag services.
class WindowsPointerToolRuntimeHost final
    : public creatures1::ui::PointerToolRuntimeHost {
public:
    WindowsPointerToolRuntimeHost(C1WindowsDocument& document,
                                  C1WindowsView* view)
        : document_(document), view_(view) {}

    // PointerToolRuntimeHost.
    creatures1::creatures::Creature* creature_for_object(
        creatures1::objects::Object& object) const override;
    std::size_t creature_count() const override;
    creatures1::creatures::Creature* creature_at(
        std::size_t index) const override;
    void report_invalid_creature_index() const override;
    void select_creature(creatures1::creatures::Creature& creature,
                         bool return_to_creature) override;
    bool is_creature_object(
        const creatures1::objects::Object& object) const override;
    void update_creature_attention(
        creatures1::creatures::Creature& creature) override;
    void queue_pointer_tool_stimulus(
        creatures1::creatures::Creature& source,
        creatures1::ui::PointerTool& tool) override;
    void set_edit_object(creatures1::objects::Object* object) override;
    std::size_t scenery_object_count() const override;
    creatures1::objects::Object* scenery_object_at(
        std::size_t index) const override;
    void report_invalid_scenery_index() const override;
    bool point_in_world_rect(const creatures1::world::WorldRect& bounds,
                             int world_x, int world_y) const override;
    std::uint32_t pending_input_flags() const override;
    void finish_pending_input() override;
    void dismiss_pointer_bubble(creatures1::objects::Bubble& bubble) override;
    // Shared construction for both pointer bubbles; the two native members
    // differ only in lifetime and placement mode.
    creatures1::objects::Bubble* create_pointer_bubble(
        creatures1::ui::PointerTool& tool, std::string_view text,
        std::uint8_t lifetime_ticks,
        creatures1::objects::BubblePlacementMode placement_mode);

    creatures1::objects::Bubble* create_persistent_pointer_bubble(
        creatures1::ui::PointerTool& tool, std::string_view text) override;
    creatures1::objects::Bubble* create_transient_pointer_bubble(
        creatures1::ui::PointerTool& tool, std::string_view text) override;
    void set_pointer_bubble_text(creatures1::objects::Bubble& bubble,
                                 std::string_view text) override;
    void queue_speech_range_event(
        creatures1::objects::Object& source,
        creatures1::objects::ObjectEventId event_id) override;
    void synchronize_creature_selector(std::string_view text) override;

    // SimpleObjectInteractionHost.
    creatures1::objects::Object* pointer_tool() const override;
    int pointer_world_x() const override;
    int pointer_world_y() const override;
    std::size_t non_scenery_object_count() const override;
    creatures1::objects::Object* non_scenery_object_at(
        std::size_t index) const override;
    void report_invalid_non_scenery_index() const override;

    // ObjectRenderableSetHost.
    bool contains(const creatures1::objects::Object& object) const override;
    void insert(creatures1::objects::Object& object) override;
    void erase(creatures1::objects::Object& object) override;

    // ObjectMovementBoundsHost.
    void clear_edit_object() override;
    void find_nearest_room_bounds_at_point(
        int world_x, int world_y,
        creatures1::world::WorldRect& out_bounds) const override;
    creatures1::world::WorldRect vehicle_local_bounds(
        const creatures1::objects::Object& vehicle) const override;
    int vehicle_primary_entity_x(
        const creatures1::objects::Object& vehicle) const override;
    int vehicle_primary_entity_y(
        const creatures1::objects::Object& vehicle) const override;
    creatures1::objects::Object* edit_object() const override;

    // ObjectOverlapHost.
    std::size_t object_count() const override;
    creatures1::objects::Object* object_at(std::size_t index) const override;
    void report_invalid_index() const override;
    bool is_pointer_tool(
        const creatures1::objects::Object& object) const override;
    creatures1::world::WorldRect pointer_tool_bounds(
        const creatures1::objects::Object& pointer_tool) const override;
    creatures1::world::WorldRect vehicle_interaction_bounds(
        const creatures1::objects::Object& vehicle) const override;

    // ObjectImmediateEventQueueHost / ObjectScriptDispatchHost /
    // SimpleObjectMoveRedrawHost.
    void queue_immediate_event(
        creatures1::objects::Object& source,
        creatures1::objects::Object& target,
        creatures1::objects::ObjectEventId event_id,
        std::uint32_t argument) override;
    int execute_script_for_classifier(
        creatures1::objects::Object& object,
        creatures1::objects::Object* from_object, std::uint32_t classifier,
        bool force_restart) override;
    void redraw_after_simple_object_move(
        creatures1::objects::SimpleObject& object,
        const creatures1::world::WorldRect& old_bounds,
        const creatures1::world::WorldRect& new_bounds) override;

private:
    C1WindowsDocument& document_;
    C1WindowsView* view_ = nullptr;
};

} // namespace creatures1::platform
