#include "windows_pointer_tool_host.hpp"

#include "windows_creature_hosts.hpp"

namespace creatures1::platform {

// --- PointerToolRuntimeHost ------------------------------------------------

creatures1::creatures::Creature*
WindowsPointerToolRuntimeHost::creature_for_object(
    creatures1::objects::Object& object) const {
    return document_.mutable_creature_for_object(object);
}

std::size_t WindowsPointerToolRuntimeHost::creature_count() const {
    return document_.creature_count();
}

creatures1::creatures::Creature* WindowsPointerToolRuntimeHost::creature_at(
    std::size_t index) const {
    return dynamic_cast<creatures1::creatures::Creature*>(
        document_.creature_at(index));
}

void WindowsPointerToolRuntimeHost::report_invalid_creature_index() const {
    throw std::out_of_range("C1 pointer-tool creature registry index");
}

void WindowsPointerToolRuntimeHost::select_creature(
    creatures1::creatures::Creature& creature, bool return_to_creature) {
    static_cast<void>(return_to_creature);
    document_.set_selected_creature(&creature);
}

bool WindowsPointerToolRuntimeHost::is_creature_object(
    const creatures1::objects::Object& object) const {
    return document_.creature_for_object(object) != nullptr;
}

void WindowsPointerToolRuntimeHost::update_creature_attention(
    creatures1::creatures::Creature& creature) {
    // Creature::update_attention is already translated; the attention host
    // built for the CAOS runtime supplies its services.
    WindowsCreatureAttentionHost attention(document_);
    creature.update_attention(attention);
}

void WindowsPointerToolRuntimeHost::queue_pointer_tool_stimulus(
    creatures1::creatures::Creature& source,
    creatures1::ui::PointerTool& tool) {
    // The recovered stimulus is the pointer tool acting on the creature; the
    // immediate ring carries it as the standard interaction event.
    document_.queue_immediate_object_event(
        tool, document_.object_for_creature(source),
        creatures1::objects::ObjectEventId::event_4, 0);
}

void WindowsPointerToolRuntimeHost::set_edit_object(
    creatures1::objects::Object* object) {
    document_.set_edit_object(object);
}

std::size_t WindowsPointerToolRuntimeHost::scenery_object_count() const {
    return document_.scenery_count();
}

creatures1::objects::Object*
WindowsPointerToolRuntimeHost::scenery_object_at(std::size_t index) const {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    return runtime == nullptr ? nullptr : runtime->scenery_at(index);
}

void WindowsPointerToolRuntimeHost::report_invalid_scenery_index() const {
    throw std::out_of_range("C1 pointer-tool scenery registry index");
}

bool WindowsPointerToolRuntimeHost::point_in_world_rect(
    const creatures1::world::WorldRect& bounds, int world_x,
    int world_y) const {
    return world_x >= bounds.min_x && world_x < bounds.max_x &&
           world_y >= bounds.min_y && world_y < bounds.max_y;
}

std::uint32_t WindowsPointerToolRuntimeHost::pending_input_flags() const {
    return view_ == nullptr ? 0u : view_->view_state().pending_input_flags;
}

void WindowsPointerToolRuntimeHost::finish_pending_input() {
    if (view_ != nullptr) {
        view_->view_state().pending_input_flags = 0;
    }
}

// --- pointer-tool bubbles --------------------------------------------------

creatures1::objects::Bubble*
WindowsPointerToolRuntimeHost::create_persistent_pointer_bubble(
    creatures1::ui::PointerTool& tool, std::string_view text) {
    // The bubble construction path bound earlier this session: the document is
    // the complete BubbleConstructionHost, and the world runtime owns the
    // resulting object.
    return create_pointer_bubble(
        tool, text, 0, creatures1::objects::BubblePlacementMode::persistent);
}

creatures1::objects::Bubble*
WindowsPointerToolRuntimeHost::create_pointer_bubble(
    creatures1::ui::PointerTool& tool, std::string_view text,
    std::uint8_t lifetime_ticks,
    creatures1::objects::BubblePlacementMode placement_mode) {
    const int viewport_center =
        document_.viewport_left() +
        (document_.viewport_right() - document_.viewport_left()) / 2;
    const bool place_on_right = tool.sound_source_x() <= viewport_center;
    auto bubble = std::unique_ptr<creatures1::objects::Bubble>(
        new (std::nothrow) creatures1::objects::Bubble(
            &tool, lifetime_ticks, text, placement_mode, place_on_right,
            document_.bubble_construction()));
    if (bubble == nullptr) {
        return nullptr;
    }
    creatures1::objects::Bubble* raw = bubble.get();
    document_.adopt_speech_bubble(std::move(bubble));
    return raw;
}

creatures1::objects::Bubble*
WindowsPointerToolRuntimeHost::create_transient_pointer_bubble(
    creatures1::ui::PointerTool& tool, std::string_view text) {
    // SetTransientBubbleText @ 004299b0 makes its own bubble with lifetime 10
    // and placement mode 0; it is not the persistent bubble with its lifetime
    // rewritten, and the persistent slot is deliberately not occupied by it.
    return create_pointer_bubble(
        tool, text, 10, creatures1::objects::BubblePlacementMode::speech);
}

void WindowsPointerToolRuntimeHost::dismiss_pointer_bubble(
    creatures1::objects::Bubble& bubble) {
    bubble.destroy_and_redraw(document_);
}

void WindowsPointerToolRuntimeHost::set_pointer_bubble_text(
    creatures1::objects::Bubble& bubble, std::string_view text) {
    bubble.set_text(text, document_);
}

void WindowsPointerToolRuntimeHost::queue_speech_range_event(
    creatures1::objects::Object& source,
    creatures1::objects::ObjectEventId event_id) {
    // SetTransientBubbleText @ 004299b0 and SetSelectedCreatureName both call
    // QueueEventForCreaturesInSpeechRange, which fans the event out over the
    // creature registry.  Queueing it back to the source was a placeholder
    // that stood only while the fan-out host was unbound.
    WindowsCreatureFanoutHost fanout(document_);
    creatures1::creatures::queue_events_in_speech_range(source, event_id, 0, 0,
                                                        fanout);
}

void WindowsPointerToolRuntimeHost::synchronize_creature_selector(
    std::string_view text) {
    static_cast<void>(text);
    // The selector shows the creature list, which the document rebuilds; the
    // native refresh is the same one the selection path already drives.
    document_.rebuild_creature_selection_menu();
}

// --- SimpleObjectInteractionHost -------------------------------------------

creatures1::objects::Object*
WindowsPointerToolRuntimeHost::pointer_tool() const {
    return document_.pointer_tool();
}

int WindowsPointerToolRuntimeHost::pointer_world_x() const {
    return document_.mouse_world_x();
}

int WindowsPointerToolRuntimeHost::pointer_world_y() const {
    return document_.mouse_world_y();
}

std::size_t WindowsPointerToolRuntimeHost::non_scenery_object_count() const {
    return document_.non_scenery_object_count();
}

creatures1::objects::Object*
WindowsPointerToolRuntimeHost::non_scenery_object_at(
    std::size_t index) const {
    return document_.non_scenery_object_at(index);
}

void WindowsPointerToolRuntimeHost::report_invalid_non_scenery_index() const {
    throw std::out_of_range("C1 pointer-tool non-scenery registry index");
}

// --- ObjectRenderableSetHost -----------------------------------------------

bool WindowsPointerToolRuntimeHost::contains(
    const creatures1::objects::Object& object) const {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    return runtime != nullptr && runtime->contains(object);
}

void WindowsPointerToolRuntimeHost::insert(
    creatures1::objects::Object& object) {
    document_.renderables().insert(object);
}

void WindowsPointerToolRuntimeHost::erase(
    creatures1::objects::Object& object) {
    document_.renderables().erase(object);
}

// --- ObjectMovementBoundsHost ----------------------------------------------

void WindowsPointerToolRuntimeHost::clear_edit_object() {
    document_.clear_edit_object();
}

void WindowsPointerToolRuntimeHost::find_nearest_room_bounds_at_point(
    int world_x, int world_y,
    creatures1::world::WorldRect& out_bounds) const {
    document_.find_nearest_room_bounds_at_point(world_x, world_y, out_bounds);
}

creatures1::world::WorldRect
WindowsPointerToolRuntimeHost::vehicle_local_bounds(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_local_bounds(vehicle);
}

int WindowsPointerToolRuntimeHost::vehicle_primary_entity_x(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_primary_entity_x(vehicle);
}

int WindowsPointerToolRuntimeHost::vehicle_primary_entity_y(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_primary_entity_y(vehicle);
}

creatures1::objects::Object* WindowsPointerToolRuntimeHost::edit_object()
    const {
    return document_.edit_object();
}

// --- ObjectOverlapHost -----------------------------------------------------

std::size_t WindowsPointerToolRuntimeHost::object_count() const {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    return runtime == nullptr ? 0 : runtime->object_count();
}

creatures1::objects::Object* WindowsPointerToolRuntimeHost::object_at(
    std::size_t index) const {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    return runtime == nullptr ? nullptr : runtime->object_at(index);
}

void WindowsPointerToolRuntimeHost::report_invalid_index() const {
    throw std::out_of_range("C1 pointer-tool object registry index");
}

bool WindowsPointerToolRuntimeHost::is_pointer_tool(
    const creatures1::objects::Object& object) const {
    return document_.pointer_tool() == &object;
}

creatures1::world::WorldRect
WindowsPointerToolRuntimeHost::pointer_tool_bounds(
    const creatures1::objects::Object& pointer_tool) const {
    // Native FindTopmostOverlappingObject does not use the hand sprite as
    // its hit rectangle. It uses the one-pixel cursor hotspot, anchored at
    // the hand entity plus the serialized hotspot offsets. Using the full
    // sprite here lets the hand select objects that are merely underneath
    // its artwork, and can also select a vehicle while dropping an item.
    if (const auto* pointer = dynamic_cast<const creatures1::ui::PointerTool*>(
            &pointer_tool);
        pointer != nullptr && pointer->entity() != nullptr) {
        const int x = pointer->entity()->world_x() +
                      pointer->cursor_hotspot_offset_x;
        const int y = pointer->entity()->world_y() +
                      pointer->cursor_hotspot_offset_y;
        return {x, y, x + 1, y + 1};
    }

    creatures1::world::WorldRect bounds{};
    pointer_tool.get_bounds(&bounds);
    return bounds;
}

creatures1::world::WorldRect
WindowsPointerToolRuntimeHost::vehicle_interaction_bounds(
    const creatures1::objects::Object& vehicle) const {
    auto bounds = document_.vehicle_local_bounds(vehicle);
    const int x = document_.vehicle_primary_entity_x(vehicle);
    const int y = document_.vehicle_primary_entity_y(vehicle);
    bounds.min_x += x;
    bounds.max_x += x;
    bounds.min_y += y;
    bounds.max_y += y;
    return bounds;
}

// --- event queue, script dispatch, redraw ----------------------------------

void WindowsPointerToolRuntimeHost::queue_immediate_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    document_.queue_immediate_object_event(source, target, event_id, argument);
}

int WindowsPointerToolRuntimeHost::execute_script_for_classifier(
    creatures1::objects::Object& object,
    creatures1::objects::Object* from_object, std::uint32_t classifier,
    bool force_restart) {
    WindowsObjectScriptDispatchHost scripts(document_);
    return scripts.execute_script_for_classifier(object, from_object,
                                                 classifier, force_restart);
}

void WindowsPointerToolRuntimeHost::redraw_after_simple_object_move(
    creatures1::objects::SimpleObject& object,
    const creatures1::world::WorldRect& old_bounds,
    const creatures1::world::WorldRect& new_bounds) {
    document_.redraw_after_simple_object_move(object, old_bounds, new_bounds);
}

} // namespace creatures1::platform
