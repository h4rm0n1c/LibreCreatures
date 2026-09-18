#include "windows_object_event_host.hpp"

#include "../objects/object.hpp"
#include "../world/map.hpp"
#include "../world/runtime.hpp"
#include "windows_creature_hosts.hpp"
#include "windows_macro_host.hpp"
#include "windows_pointer_tool_host.hpp"
#include "windows_shell.hpp"

namespace creatures1::platform {

// --- WindowsCompoundObjectEventHost ----------------------------------------

bool WindowsCompoundObjectEventHost::source_is_creature(
    const creatures1::objects::Object& source) const {
    return ((source.classifier_base() >> 24) & 0xffu) == 4u;
}

int WindowsCompoundObjectEventHost::dispatch_script_event(
    creatures1::objects::CompoundObject& target,
    creatures1::objects::Object* source,
    creatures1::objects::ObjectEventId event_id) {
    WindowsObjectScriptDispatchHost scripts(document_);
    // CompoundObject's interaction path does not force a restart; the native
    // passes zero for that argument.
    return target.dispatch_script_event(event_id, source, false, scripts);
}

bool WindowsCompoundObjectEventHost::copy_built_in_stimulus(
    const creatures1::objects::Object& source,
    creatures1::objects::Object& target, std::uint32_t stimulus_index,
    creatures1::objects::QueuedCreatureStimulus& out) const {
    const creatures1::creatures::Creature* creature =
        document_.creature_for_object(source);
    if (creature == nullptr) {
        return false;
    }
    WindowsCreatureFanoutHost fanout(document_);
    return fanout.copy_built_in_stimulus(
        *creature, static_cast<std::int32_t>(stimulus_index), target, out);
}

void WindowsCompoundObjectEventHost::queue_creature_stimulus(
    const creatures1::objects::QueuedCreatureStimulus& stimulus) {
    document_.queue_creature_stimulus(stimulus);
}

// --- WindowsCreatureScriptEventHost ----------------------------------------

void WindowsCreatureScriptEventHost::dispatch_script_event(
    creatures1::creatures::Creature& creature,
    creatures1::objects::Object* source, std::uint32_t event_id,
    std::uint32_t argument) {
    // Creature's queued interaction handlers pass a non-zero force-restart
    // argument; the native calls slot 34 with that value directly.
    WindowsCreatureAttentionHost attention(document_);
    // CreatureAttentionHost already carries CreatureScriptDispatchHost.
    creature.dispatch_script_event(event_id, source, argument != 0, attention,
                                   attention);
}

// --- WindowsCallButtonRuntimeHost ------------------------------------------

std::size_t WindowsCallButtonRuntimeHost::room_count() const {
    return static_cast<std::size_t>(document_.map_room_count());
}

namespace {

const creatures1::world::MapRoom* map_room(C1WindowsDocument& document,
                                           std::size_t index) {
    creatures1::world::WorldRuntime* runtime = document.world_runtime();
    if (runtime == nullptr) {
        return nullptr;
    }
    const creatures1::world::MapRoomTable table =
        runtime->map_data().room_table();
    if (index >= table.room_count) {
        return nullptr;
    }
    return table.rooms + index;
}

} // namespace

bool WindowsCallButtonRuntimeHost::room_contains_point(
    std::size_t room_index, int world_x, int world_y) const {
    const creatures1::world::MapRoom* room = map_room(document_, room_index);
    if (room == nullptr) {
        return false;
    }
    // Native room hit tests use the Win32 PtInRect convention: the right and
    // bottom edges are outside.
    return room->bounds.left <= world_x && world_x < room->bounds.right &&
           room->bounds.top <= world_y && world_y < room->bounds.bottom;
}

int WindowsCallButtonRuntimeHost::room_bottom(std::size_t room_index) const {
    const creatures1::world::MapRoom* room = map_room(document_, room_index);
    return room == nullptr ? 0 : room->bounds.bottom;
}

void WindowsCallButtonRuntimeHost::clear_edit_object() {
    document_.clear_edit_object();
}

void WindowsCallButtonRuntimeHost::find_nearest_room_bounds_at_point(
    int world_x, int world_y,
    creatures1::world::WorldRect& out_bounds) const {
    document_.find_nearest_room_bounds_at_point(world_x, world_y, out_bounds);
}

creatures1::world::WorldRect
WindowsCallButtonRuntimeHost::vehicle_local_bounds(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_local_bounds(vehicle);
}

int WindowsCallButtonRuntimeHost::vehicle_primary_entity_x(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_primary_entity_x(vehicle);
}

int WindowsCallButtonRuntimeHost::vehicle_primary_entity_y(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_primary_entity_y(vehicle);
}

creatures1::objects::Object* WindowsCallButtonRuntimeHost::edit_object()
    const {
    return document_.edit_object();
}

void WindowsCallButtonRuntimeHost::queue_immediate_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    document_.queue_immediate_object_event(source, target, event_id, argument);
}

int WindowsCallButtonRuntimeHost::execute_script_for_classifier(
    creatures1::objects::Object& object,
    creatures1::objects::Object* from_object, std::uint32_t classifier,
    bool force_restart) {
    WindowsObjectScriptDispatchHost scripts(document_);
    return scripts.execute_script_for_classifier(object, from_object,
                                                 classifier, force_restart);
}

bool WindowsCallButtonRuntimeHost::source_is_creature(
    const creatures1::objects::Object& source) const {
    return compound_.source_is_creature(source);
}

int WindowsCallButtonRuntimeHost::dispatch_script_event(
    creatures1::objects::CompoundObject& target,
    creatures1::objects::Object* source,
    creatures1::objects::ObjectEventId event_id) {
    return compound_.dispatch_script_event(target, source, event_id);
}

bool WindowsCallButtonRuntimeHost::copy_built_in_stimulus(
    const creatures1::objects::Object& source,
    creatures1::objects::Object& target, std::uint32_t stimulus_index,
    creatures1::objects::QueuedCreatureStimulus& out) const {
    return compound_.copy_built_in_stimulus(source, target, stimulus_index,
                                            out);
}

void WindowsCallButtonRuntimeHost::queue_creature_stimulus(
    const creatures1::objects::QueuedCreatureStimulus& stimulus) {
    compound_.queue_creature_stimulus(stimulus);
}

void WindowsCallButtonRuntimeHost::publish_active_lift(
    creatures1::objects::Lift& lift) {
    document_.set_active_lift(&lift);
}

int WindowsCallButtonRuntimeHost::call_button_floor(
    const creatures1::objects::CallButton& button) const {
    return button.floor_index;
}

void WindowsCallButtonRuntimeHost::deactivate_call_button(
    creatures1::objects::CallButton& button) {
    // Recovered from Lift::Tick @ 0x0042c4f0: the arrival path clears the
    // button's interaction event id and dispatches EVENT_0 on it.  CallButton
    // owns that pair; this supplies the script runtime.
    WindowsObjectScriptDispatchHost scripts(document_);
    button.deactivate(scripts);
}

int WindowsCallButtonRuntimeHost::ground_height_at_x_block(
    std::size_t block_index) const {
    return document_.map_ground_height(
        static_cast<std::uint32_t>(block_index));
}

creatures1::world::ViewportBounds
WindowsCallButtonRuntimeHost::sound_viewport() const {
    return document_.sound_viewport();
}

bool WindowsCallButtonRuntimeHost::sounds_muted() const {
    return document_.sounds_muted();
}

creatures1::sound::SoundManager&
WindowsCallButtonRuntimeHost::sound_manager() {
    return document_.sound_manager();
}

bool WindowsCallButtonRuntimeHost::debug_console_available() const {
    return document_.debug_console_available();
}

void WindowsCallButtonRuntimeHost::log_sound_event(
    creatures1::objects::ObjectSoundPlaybackHost::LogEvent event, creatures1::sound::SoundId sound_id,
    int value) {
    document_.log_sound_event(event, sound_id, value);
}

void WindowsCallButtonRuntimeHost::add_object(
    creatures1::objects::Object* object) {
    document_.object_registry().add_object(object);
}

std::size_t WindowsCallButtonRuntimeHost::object_count() const {
    return document_.object_registry().object_count();
}

creatures1::objects::Object* WindowsCallButtonRuntimeHost::object_at(
    std::size_t index) const {
    return document_.object_registry().object_at(index);
}

void WindowsCallButtonRuntimeHost::remove_object_at(std::size_t index) {
    document_.object_registry().remove_object_at(index);
}

void WindowsCallButtonRuntimeHost::report_invalid_index() const {
    document_.object_registry().report_invalid_index();
}

void WindowsCallButtonRuntimeHost::redraw_image_sequence_change(
    creatures1::objects::Entity& entity,
    const creatures1::world::WorldRect& old_bounds,
    const creatures1::world::WorldRect& new_bounds) {
    WindowsEntityImageSequenceRenderHost render(document_);
    render.redraw_image_sequence_change(entity, old_bounds, new_bounds);
}

void WindowsCallButtonRuntimeHost::dispatch_timer_event(
    creatures1::objects::CompoundObject& object) {
    // CompoundObject::Tick @ 0042b6c0 runs the object's own event-9
    // classifier script when the timer reaches zero, the same event the
    // SimpleObject timer path fires.
    object.dispatch_script_event(creatures1::objects::ObjectEventId::event_9,
                                 &object, false, *this);
}

void WindowsCallButtonRuntimeHost::advance_image_sequence(
    creatures1::objects::Entity& entity) {
    entity.advance_image_sequence(*this);
}

void WindowsCallButtonRuntimeHost::redraw_after_compound_object_move(
    creatures1::objects::CompoundObject& object,
    const creatures1::world::WorldRect& old_bounds,
    const creatures1::world::WorldRect& new_bounds) {
    document_.redraw_after_compound_object_move(object, old_bounds,
                                                new_bounds);
}

void WindowsCallButtonRuntimeHost::queue_compound_object_dirty_rect(
    creatures1::objects::CompoundObject& object,
    const creatures1::world::WorldRect& primary_bounds) {
    document_.queue_compound_object_dirty_rect(object, primary_bounds);
}

// --- WindowsObjectEventDispatchHost ----------------------------------------

void WindowsObjectEventDispatchHost::dispatch_event_8(
    creatures1::objects::Object& object) {
    // Object::DispatchEvent8 @ 00425c20 runs the object's own EVENT_8
    // classifier with itself as the from-object and no restart.
    WindowsObjectScriptDispatchHost scripts(document_);
    object.dispatch_script_event(creatures1::objects::ObjectEventId::event_8,
                                 &object, false, scripts);
}

// --- WindowsSimpleObjectInteractionHost ------------------------------------

WindowsSimpleObjectInteractionHost::WindowsSimpleObjectInteractionHost(
    C1WindowsDocument& document)
    : document_(document),
      pointer_(document, active_main_frame() == nullptr
                             ? nullptr
                             : active_c1_view(*active_main_frame())) {}

creatures1::objects::SimpleObject*
WindowsSimpleObjectInteractionHost::pointer_tool() const {
    return dynamic_cast<creatures1::objects::SimpleObject*>(
        document_.pointer_tool());
}

bool WindowsSimpleObjectInteractionHost::pointer_input_pending() const {
    // SimpleObject::UpdateUnboundedPositionAndRedraw @ 00427fd0 tests the
    // view's pending_input_flags before handing the pointer tool its input.
    return pointer_.pending_input_flags() != 0;
}

void WindowsSimpleObjectInteractionHost::process_pending_pointer_input(
    creatures1::objects::SimpleObject& receiver) {
    // Native dispatch enters PointerTool::ProcessPendingInput through the
    // currently ticking SimpleObject. Once an object is held, that receiver
    // is the held object; the pointer tool still owns the implementation.
    auto* tool = dynamic_cast<creatures1::ui::PointerTool*>(
        document_.pointer_tool());
    if (tool != nullptr) {
        tool->process_pending_input(receiver, pointer_);
    }
}

int WindowsSimpleObjectInteractionHost::pointer_mouse_world_x() const {
    return document_.mouse_world_x();
}

int WindowsSimpleObjectInteractionHost::pointer_mouse_world_y() const {
    return document_.mouse_world_y();
}

void WindowsSimpleObjectInteractionHost::present_or_queue_dirty_world_rect(
    const creatures1::world::WorldRect& dirty_rect) {
    // WorldRenderer::queue_dirty_world_rect already is the present-or-queue
    // decision: it presents immediately unless deferred dirty-rect rendering
    // is on, which is the same `deferred_dirty_rect_rendering == 0` test the
    // native makes here.
    document_.queue_renderer_dirty_world_rect(dirty_rect);
}

int WindowsSimpleObjectInteractionHost::carried_object_render_plane_offset(
    const creatures1::objects::CompoundObject& reference) const {
    // UpdateEntityForExplicitRectBoundsAndRedraw @0x00428d30 computes a
    // carried object's plane as the carrier's plane plus an offset taken from
    // a four-entry table at 0x0045abb8, {-1, 1, 1, -1}:
    //
    //   MOVZX EAX, byte ptr [ECX + 0x78]          ; low byte of parts[3].entity
    //   MOV   ESI, dword ptr [EAX*0x4 + 0x45abb8] ; table[that byte]
    //
    // +0x78 is `parts[3].entity`, a pointer, and the index is its low byte --
    // so the native read is only in bounds when that pointer is null, and is
    // otherwise an out-of-bounds load keyed on a heap address.  Past the four
    // entries lies unrelated pose-string data, so the offsets it produces
    // there are arbitrary.
    //
    // This is not reproducible: a port's heap addresses differ from the
    // original's, and the original is not self-consistent between runs either.
    // Exactly one object in a shipped world takes this path with a non-null
    // parts[3] -- the incubator (classifier 3.4.1, part_count 4, parts[3] an
    // Entity at 1929,680) -- and an egg placed in it is the visible case.
    //
    // The incubator's own part planes decide what is correct here.  From its
    // record in a shipped World.sfc:
    //
    //   part[0] plane    0   body, and what GetRenderPlane returns
    //   part[1] plane    1   the door, animated by its event 1/2 scripts
    //   part[2] plane 4000   the front cover
    //   part[3] plane    2   the dial
    //
    // so a carried object's plane is 0 + offset.  The table's two values give
    // -1, which puts it *behind the body* -- out of sight entirely, and the
    // reason cheese dropped into the incubator vanished behind it -- or +1,
    // which puts it in front of the body and far behind the front cover,
    // which is where an egg belongs.
    //
    // +1 it is: a real entry in the native table, deterministic, and the only
    // one of the two that leaves the carried object visible.  An earlier
    // revision of this returned -1 on the grounds that index 0 is the table's
    // only in-bounds entry; that reasoning ignored what the planes actually
    // are and regressed every carried object into the carrier's body.
    static_cast<void>(reference);
    return 1;
}

int WindowsSimpleObjectInteractionHost::privilege_level() const {
    return static_cast<int>(document_.privilege_level());
}

void WindowsSimpleObjectInteractionHost::move_bubble_and_redraw(
    creatures1::objects::Bubble& bubble, int world_x, int world_y) {
    // Bubble::Tick @ 0042a4c0 moves through the object's MoveToAndRedraw
    // slot, which for a SimpleObject is move_to_and_redraw.
    bubble.move_to_and_redraw(world_x, world_y, *this);
}

void WindowsSimpleObjectInteractionHost::expire_bubble(
    creatures1::objects::Bubble& bubble) {
    bubble.destroy_and_redraw(document_);
}

void WindowsSimpleObjectInteractionHost::redraw_image_sequence_change(
    creatures1::objects::Entity& entity,
    const creatures1::world::WorldRect& old_bounds,
    const creatures1::world::WorldRect& new_bounds) {
    WindowsEntityImageSequenceRenderHost render(document_);
    render.redraw_image_sequence_change(entity, old_bounds, new_bounds);
}

creatures1::world::ViewportBounds
WindowsSimpleObjectInteractionHost::sound_viewport() const {
    return document_.sound_viewport();
}

bool WindowsSimpleObjectInteractionHost::sounds_muted() const {
    return document_.sounds_muted();
}

creatures1::sound::SoundManager&
WindowsSimpleObjectInteractionHost::sound_manager() {
    return document_.sound_manager();
}

bool WindowsSimpleObjectInteractionHost::debug_console_available() const {
    return document_.debug_console_available();
}

void WindowsSimpleObjectInteractionHost::log_sound_event(
    creatures1::objects::ObjectSoundPlaybackHost::LogEvent event,
    creatures1::sound::SoundId sound_id, int value) {
    document_.log_sound_event(event, sound_id, value);
}

int WindowsSimpleObjectInteractionHost::pointer_world_x() const {
    return pointer_.pointer_world_x();
}

int WindowsSimpleObjectInteractionHost::pointer_world_y() const {
    return pointer_.pointer_world_y();
}

std::size_t WindowsSimpleObjectInteractionHost::non_scenery_object_count()
    const {
    return document_.non_scenery_object_count();
}

creatures1::objects::Object*
WindowsSimpleObjectInteractionHost::non_scenery_object_at(
    std::size_t index) const {
    return document_.non_scenery_object_at(index);
}

void WindowsSimpleObjectInteractionHost::report_invalid_non_scenery_index()
    const {
    OutputDebugStringA(
        "Non-scenery registry index out of range in a queued object event\n");
}

bool WindowsSimpleObjectInteractionHost::contains(
    const creatures1::objects::Object& object) const {
    return document_.renderables().contains(object);
}

void WindowsSimpleObjectInteractionHost::insert(
    creatures1::objects::Object& object) {
    document_.renderables().insert(object);
}

void WindowsSimpleObjectInteractionHost::erase(
    creatures1::objects::Object& object) {
    document_.renderables().erase(object);
}

void WindowsSimpleObjectInteractionHost::clear_edit_object() {
    document_.clear_edit_object();
}

void WindowsSimpleObjectInteractionHost::find_nearest_room_bounds_at_point(
    int world_x, int world_y,
    creatures1::world::WorldRect& out_bounds) const {
    document_.find_nearest_room_bounds_at_point(world_x, world_y, out_bounds);
}

creatures1::world::WorldRect
WindowsSimpleObjectInteractionHost::vehicle_local_bounds(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_local_bounds(vehicle);
}

int WindowsSimpleObjectInteractionHost::vehicle_primary_entity_x(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_primary_entity_x(vehicle);
}

int WindowsSimpleObjectInteractionHost::vehicle_primary_entity_y(
    const creatures1::objects::Object& vehicle) const {
    return document_.vehicle_primary_entity_y(vehicle);
}

creatures1::objects::Object*
WindowsSimpleObjectInteractionHost::edit_object() const {
    return document_.edit_object();
}

std::size_t WindowsSimpleObjectInteractionHost::object_count() const {
    return pointer_.object_count();
}

creatures1::objects::Object* WindowsSimpleObjectInteractionHost::object_at(
    std::size_t index) const {
    return pointer_.object_at(index);
}

void WindowsSimpleObjectInteractionHost::report_invalid_index() const {
    pointer_.report_invalid_index();
}

bool WindowsSimpleObjectInteractionHost::is_pointer_tool(
    const creatures1::objects::Object& object) const {
    return pointer_.is_pointer_tool(object);
}

creatures1::world::WorldRect
WindowsSimpleObjectInteractionHost::pointer_tool_bounds(
    const creatures1::objects::Object& pointer_tool) const {
    return pointer_.pointer_tool_bounds(pointer_tool);
}

creatures1::world::WorldRect
WindowsSimpleObjectInteractionHost::vehicle_interaction_bounds(
    const creatures1::objects::Object& vehicle) const {
    return pointer_.vehicle_interaction_bounds(vehicle);
}

void WindowsSimpleObjectInteractionHost::queue_immediate_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    document_.queue_immediate_object_event(source, target, event_id, argument);
}

int WindowsSimpleObjectInteractionHost::execute_script_for_classifier(
    creatures1::objects::Object& object,
    creatures1::objects::Object* from_object, std::uint32_t classifier,
    bool force_restart) {
    WindowsObjectScriptDispatchHost scripts(document_);
    return scripts.execute_script_for_classifier(object, from_object,
                                                 classifier, force_restart);
}

void WindowsSimpleObjectInteractionHost::redraw_after_simple_object_move(
    creatures1::objects::SimpleObject& object,
    const creatures1::world::WorldRect& old_bounds,
    const creatures1::world::WorldRect& new_bounds) {
    document_.redraw_after_simple_object_move(object, old_bounds, new_bounds);
}

// --- WindowsObjectEventRuntime ---------------------------------------------

creatures1::creatures::Creature* WindowsObjectEventRuntime::as_creature(
    creatures1::objects::Object& target) const {
    return document_.mutable_creature_for_object(target);
}

bool WindowsObjectEventRuntime::object_is_tick_enabled(
    const creatures1::objects::Object& object) const {
    return object.tick_enabled();
}

bool WindowsObjectEventRuntime::creature_is_tick_enabled(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().tick_enabled();
}

void WindowsObjectEventRuntime::apply_stimulus(
    creatures1::creatures::Creature& target,
    creatures1::objects::Object* source_object,
    creatures1::creatures::Creature& source_creature,
    const creatures1::creatures::StimulusDescriptor& descriptor,
    const creatures1::creatures::StimulusChemicalIds& chemical_ids,
    const creatures1::creatures::StimulusChemicalAmounts& chemical_amounts,
    std::uint32_t magnitude) {
    WindowsStimulusSourceHost source(document_);
    target.apply_stimulus(source_object, &source_creature, descriptor,
                          chemical_ids, chemical_amounts, magnitude, source);
}

// --- WindowsCreaturePickupDropHost -----------------------------------------

creatures1::objects::ObjectRenderableSetHost&
WindowsCreaturePickupDropHost::renderables() {
    return document_.renderables();
}

creatures1::objects::ObjectMovementBoundsHost&
WindowsCreaturePickupDropHost::movement_bounds_host() {
    return document_;
}

creatures1::objects::ObjectSoundPlaybackHost&
WindowsCreaturePickupDropHost::sound_host() {
    return document_;
}

creatures1::objects::Object* WindowsCreaturePickupDropHost::pointer_tool()
    const {
    return document_.pointer_tool();
}

void WindowsCreaturePickupDropHost::queue_dirty_world_rect(
    const creatures1::world::WorldRect& bounds) {
    document_.queue_renderer_dirty_world_rect(bounds);
}

bool WindowsCreaturePickupDropHost::pointer_pickup_is_privileged(
    const creatures1::objects::Object& /*source*/) const {
    // Creature::HandlePickupEvent has two independent native gates: the
    // application must be privileged, and the pointer tool's immediately
    // preceding input must have been right-button-with-shift.  The latter is
    // stored in SFCView::previous_input_flags by PointerTool after it queues
    // EVENT_4; reading pending_input_flags here would be too late because it
    // has already been consumed.
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    if (view == nullptr || document_.privilege_level() <= 0) {
        return false;
    }
    constexpr std::uint32_t kRightWithShift = static_cast<std::uint32_t>(
        creatures1::ui::SfcViewPendingInputFlag::right_with_shift);
    return (view->view_state().previous_input_flags & kRightWithShift) != 0;
}

int WindowsCreaturePickupDropHost::vehicle_attachment_render_plane(
    const creatures1::objects::Object& vehicle) const {
    // Creature::HandlePickupEvent @0x004098e0, the vehicle branch:
    //
    //   MOV    EAX, [ECX + 0x54]   ; vehicle parts[0].entity
    //   MOV    EDX, [EAX + 0xc]    ; its render_plane            -- the base
    //   MOV    EAX, [ECX + 0x60]   ; vehicle parts[1].entity
    //   MOV    ECX, 0x5
    //   CMP    EDX, [EAX + 0xc]    ; against parts[1] render_plane
    //   CMOVGE ECX, [ESP + 0x10]   ; -5 when base >= parts[1]
    //   ADD    ECX, EDX
    //   MOV    [EAX + 0xc], ECX    ; body->render_plane = base +/- 5
    //
    // +0x54 and +0x60 are parts[0].entity and parts[1].entity -- `parts` is
    // CompoundPart[10] at +0x54, twelve bytes each -- and +0xc on an Entity is
    // its render_plane.  Vehicle extends CompoundObject, so both are valid.
    //
    // The creature is placed five planes off the car's own plane, on whichever
    // side keeps it between the car and its facing panel: behind when part 0
    // already draws at or in front of part 1, in front otherwise.  This
    // previously returned the vehicle's plane unchanged, which left a creature
    // in a lift at exactly the car's plane, where a stable sort keyed only on
    // plane falls back to entity-registry order -- so whether the norn drew
    // inside or behind the lift was arbitrary.
    auto& mutable_vehicle = const_cast<creatures1::objects::Object&>(vehicle);
    const int base_plane = mutable_vehicle.render_plane();

    const auto* compound =
        dynamic_cast<const creatures1::objects::CompoundObject*>(&vehicle);
    if (compound == nullptr) {
        return base_plane;
    }
    const creatures1::objects::Entity* facing_part =
        compound->part(1).entity.get();
    if (facing_part == nullptr) {
        return base_plane;
    }
    return base_plane +
           (base_plane >= facing_part->render_plane() ? -5 : 5);
}

void WindowsCreaturePickupDropHost::select_creature(
    creatures1::creatures::Creature& creature) {
    document_.set_selected_creature(&creature);
}

creatures1::objects::Object*
WindowsCreaturePickupDropHost::find_topmost_vehicle_overlap(
    creatures1::creatures::Creature& creature) const {
    // The drop path looks for the topmost object carrying the vehicle bounds
    // flag underneath the creature.
    return document_.object_for_creature(creature)
        .find_topmost_overlapping_object(
            creatures1::objects::Object::kIsVehicle,
            creatures1::objects::Object::kIsVehicle, interaction_);
}

bool WindowsCreaturePickupDropHost::read_view_input_and_clear_pending_flag(
    creatures1::creatures::UnboundedWorldPositionInput& out) {
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    if (view == nullptr) {
        return false;
    }
    creatures1::ui::SfcViewState& state = view->view_state();
    out.mouse_client_x = state.mouse_client_x;
    out.mouse_client_y = state.mouse_client_y;
    out.viewport_left = document_.viewport_left();
    out.viewport_top = document_.renderer_viewport_top();
    const bool had_pending = state.pending_input_flags != 0;
    state.pending_input_flags = 0;
    return had_pending;
}

creatures1::objects::Object&
WindowsCreaturePickupDropHost::object_for_creature(
    creatures1::creatures::Creature& creature) const {
    return document_.object_for_creature(creature);
}

void WindowsCreaturePickupDropHost::move_to_and_redraw(
    creatures1::objects::Object& object, int world_x, int world_y) {
    document_.move_to_and_redraw(object, world_x, world_y);
}

void WindowsCreaturePickupDropHost::
    update_pointer_tool_unbounded_position_and_redraw() {
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    if (view != nullptr) {
        view->update_pointer_tool_unbounded_position_and_redraw();
    }
}

void WindowsCreaturePickupDropHost::queue_immediate_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    document_.queue_immediate_object_event(source, target, event_id, argument);
}

void WindowsCreaturePickupDropHost::dispatch_script_event(
    creatures1::creatures::Creature& creature,
    creatures1::objects::Object* source, std::uint32_t event_id,
    std::uint32_t argument) {
    events_.dispatch_script_event(creature, source, event_id, argument);
}

// --- WindowsCreatureUpdateHost ----------------------------------------------

bool WindowsCreatureUpdateHost::read_view_input_and_clear_pending_flag(
    creatures1::creatures::UnboundedWorldPositionInput& out) {
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    if (view == nullptr) {
        return false;
    }
    creatures1::ui::SfcViewState& state = view->view_state();
    out.mouse_client_x = state.mouse_client_x;
    out.mouse_client_y = state.mouse_client_y;
    out.viewport_left = document_.viewport_left();
    out.viewport_top = document_.renderer_viewport_top();
    const bool had_pending = state.pending_input_flags != 0;
    state.pending_input_flags = 0;
    return had_pending;
}

creatures1::objects::Object& WindowsCreatureUpdateHost::object_for_creature(
    creatures1::creatures::Creature& creature) const {
    return document_.object_for_creature(creature);
}

void WindowsCreatureUpdateHost::move_to_and_redraw(
    creatures1::objects::Object& object, int world_x, int world_y) {
    document_.move_to_and_redraw(object, world_x, world_y);
}

void WindowsCreatureUpdateHost::
    update_pointer_tool_unbounded_position_and_redraw() {
    C1MainFrame* frame = active_main_frame();
    C1WindowsView* view = frame == nullptr ? nullptr : active_c1_view(*frame);
    if (view != nullptr) {
        view->update_pointer_tool_unbounded_position_and_redraw();
    }
}

void WindowsCreatureUpdateHost::queue_immediate_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument) {
    document_.queue_immediate_object_event(source, target, event_id, argument);
}

int WindowsCreatureUpdateHost::render_plane(
    const creatures1::objects::Object& object) const {
    return const_cast<creatures1::objects::Object&>(object).render_plane();
}

void WindowsCreatureUpdateHost::queue_dirty_world_rect(
    const creatures1::world::WorldRect& bounds) {
    document_.queue_renderer_dirty_world_rect(bounds);
}

void WindowsCreatureUpdateHost::dispatch_sleep_indicator_event(
    creatures1::objects::Object& indicator,
    creatures1::objects::ObjectEventId event_id,
    creatures1::objects::Object* source, std::uint32_t /*argument*/) {
    // SetSleepIndicator @ 0040da80 calls Object::DispatchScriptEvent (vtable
    // +0x88) directly, so the indicator's event script (1/2: `sndl zzzz` /
    // `sndl gsnr`, 0: `fade`) runs before InitializeRuntimeState purges the
    // object's macros.  Queuing it let the purge win: `fade` never ran and
    // every sleep left its snore loop playing.
    WindowsObjectScriptDispatchHost scripts(document_);
    indicator.dispatch_script_event(event_id, source, false, scripts);
}

void WindowsCreatureUpdateHost::initialize_sleep_indicator(
    creatures1::objects::Object& indicator) {
    indicator.initialize_runtime_state(document_);
}

// --- WindowsCreatureWordsHost ----------------------------------------------

char* WindowsCreatureWordsHost::mutable_words_for_event(
    const creatures1::objects::QueuedObjectEvent& event) {
    // HandleHeardWordsEvent @ 00409da0 accepts two source layouts: the
    // pointer tool's own text buffer, or the indexed recognised word of a
    // speaking creature.
    creatures1::objects::Object* source = event.source;
    if (source == nullptr) {
        return nullptr;
    }
    if (source == document_.pointer_tool()) {
        auto* tool = dynamic_cast<creatures1::ui::PointerTool*>(source);
        return tool == nullptr ? nullptr : tool->text_buffer.data();
    }
    creatures1::creatures::Creature* speaker =
        document_.mutable_creature_for_object(*source);
    if (speaker == nullptr ||
        event.argument >=
            creatures1::creatures::Creature::kLearnedWordRecordCount) {
        return nullptr;
    }
    return speaker->learned_word_record(event.argument).recognized_word;
}

bool WindowsCreatureWordsHost::is_pointer_tool(
    const creatures1::objects::Object& object) const {
    return &object == document_.pointer_tool();
}

creatures1::creatures::PointerAttentionApi&
WindowsCreatureWordsHost::pointer_attention_api() {
    return *this;
}

creatures1::creatures::Creature::CreatureSpeechPhraseHost&
WindowsCreatureWordsHost::speech_phrase_host() {
    return phrase_;
}

creatures1::creatures::CreatureGoalDirectionHost&
WindowsCreatureWordsHost::goal_direction_host() {
    return goal_direction_;
}

bool WindowsCreatureWordsHost::resolve_word_learning_event(
    const creatures1::objects::QueuedObjectEvent& event, char*& heard_word,
    std::size_t& record_index) const {
    // HandleWordLearningEvent @ 00409e00 accepts a family-3 genus-3
    // Blackboard, whose selected slot supplies both the text and the record
    // index, or a family-4 Creature, where the event argument is the index.
    creatures1::objects::Object* source = event.source;
    if (source == nullptr) {
        return false;
    }

    if ((source->classifier_base() & 0xffff0000u) == 0x03030000u) {
        auto* blackboard =
            dynamic_cast<creatures1::brain::Blackboard*>(source);
        if (blackboard == nullptr) {
            return false;
        }
        const std::size_t slot = static_cast<std::size_t>(
            source->object_variable_0());
        if (slot >= creatures1::brain::Blackboard::kWordCount) {
            return false;
        }
        heard_word = blackboard->word(slot).text.data();
        record_index =
            static_cast<std::size_t>(blackboard->word_value(slot));
        return true;
    }

    if (((source->classifier_base() >> 24) & 0xffu) == 4u) {
        creatures1::creatures::Creature* speaker =
            document_.mutable_creature_for_object(*source);
        if (speaker == nullptr ||
            event.argument >=
                creatures1::creatures::Creature::kLearnedWordRecordCount) {
            return false;
        }
        heard_word =
            speaker->learned_word_record(event.argument).recognized_word;
        record_index = event.argument;
        return true;
    }
    return false;
}

int WindowsCreatureWordsHost::creature_sound_source_x() const {
    return creature_.skeleton().sound_source_x();
}

int WindowsCreatureWordsHost::pointer_tool_sound_source_x() const {
    creatures1::objects::Object* tool = document_.pointer_tool();
    return tool == nullptr ? 0 : tool->sound_source_x();
}

creatures1::world::WorldRect
WindowsCreatureWordsHost::creature_movement_bounds() const {
    return creature_.skeleton().movement_bounds();
}

std::size_t WindowsCreatureWordsHost::non_scenery_object_count() const {
    return document_.non_scenery_object_count();
}

bool WindowsCreatureWordsHost::read_non_scenery_object(
    std::size_t index,
    creatures1::creatures::PointerAttentionCandidate& out) const {
    creatures1::objects::Object* object =
        document_.non_scenery_object_at(index);
    if (object == nullptr) {
        return false;
    }
    object->get_bounds(&out.bounds);
    WindowsStimulusSourceHost classifier(document_);
    out.classifier = classifier.classify(*object);
    out.is_pointer_tool = object == document_.pointer_tool();
    out.is_this_creature = object == &creature_.skeleton();
    out.blocks_pointer_attention = object->has_bounds_flag(0x10);
    return true;
}

creatures1::creatures::AttentionClassifier
WindowsCreatureWordsHost::pointer_tool_classifier() const {
    creatures1::objects::Object* tool = document_.pointer_tool();
    if (tool == nullptr) {
        return {};
    }
    WindowsStimulusSourceHost classifier(document_);
    return classifier.classify(*tool);
}

void WindowsCreatureWordsHost::clear_pointer_tool_lobe_neuron(
    std::uint32_t neuron_index) {
    creatures1::brain::Brain* brain = creature_.brain();
    if (brain == nullptr) {
        return;
    }
    creatures1::brain::Lobe& lobe = brain->lobe(7);
    if (neuron_index < lobe.neuron_count_value()) {
        creatures1::brain::LobeNeuron& neuron = lobe.neuron(neuron_index);
        neuron.firing_strength = 0;
        neuron.activation = 0;
    }
}

void WindowsCreatureWordsHost::report_registry_bounds_failure() {
    OutputDebugStringA(
        "Non-scenery registry index out of range in pointer attention\n");
}

namespace {

// The four interaction events share one resolution order, which reproduces
// the native vtable's most-derived-wins dispatch.
enum class EventTargetKind {
    creature,
    call_button,
    pointer_tool,
    simple_object,
    lift,
    compound_object,
    base_object,
};

EventTargetKind classify_event_target(
    creatures1::objects::Object& target,
    creatures1::creatures::Creature* creature) {
    if (creature != nullptr) {
        return EventTargetKind::creature;
    }
    if (dynamic_cast<creatures1::objects::CallButton*>(&target) != nullptr) {
        return EventTargetKind::call_button;
    }
    if (dynamic_cast<creatures1::ui::PointerTool*>(&target) != nullptr) {
        return EventTargetKind::pointer_tool;
    }
    if (dynamic_cast<creatures1::objects::SimpleObject*>(&target) != nullptr) {
        return EventTargetKind::simple_object;
    }
    if (dynamic_cast<creatures1::objects::Lift*>(&target) != nullptr) {
        return EventTargetKind::lift;
    }
    if (dynamic_cast<creatures1::objects::CompoundObject*>(&target) !=
        nullptr) {
        return EventTargetKind::compound_object;
    }
    return EventTargetKind::base_object;
}

} // namespace

void WindowsObjectEventRuntime::handle_event_0(
    creatures1::objects::Object& target,
    const creatures1::objects::QueuedObjectEvent& event) {
    creatures1::creatures::Creature* creature = as_creature(target);
    WindowsCreatureFanoutHost fanout(document_);
    switch (classify_event_target(target, creature)) {
    case EventTargetKind::creature: {
        WindowsCreatureScriptEventHost events(document_);
        creature->handle_queued_event_slot5(event, events);
        return;
    }
    case EventTargetKind::call_button: {
        WindowsCallButtonRuntimeHost host(document_);
        static_cast<creatures1::objects::CallButton&>(target)
            .request_lift_call(event, host);
        return;
    }
    case EventTargetKind::pointer_tool:
    case EventTargetKind::simple_object: {
        WindowsObjectScriptDispatchHost scripts(document_);
        static_cast<creatures1::objects::SimpleObject&>(target)
            .handle_queued_event_0(event, fanout, scripts);
        return;
    }
    case EventTargetKind::lift: {
        WindowsCallButtonRuntimeHost host(document_);
        static_cast<creatures1::objects::Lift&>(target)
            .request_move_up(event, host);
        return;
    }
    case EventTargetKind::compound_object: {
        WindowsCompoundObjectEventHost host(document_);
        static_cast<creatures1::objects::CompoundObject&>(target)
            .handle_queued_event_0(event, host);
        return;
    }
    case EventTargetKind::base_object:
        target.handle_queued_creature_event(event, fanout);
        return;
    }
}

void WindowsObjectEventRuntime::handle_event_1(
    creatures1::objects::Object& target,
    const creatures1::objects::QueuedObjectEvent& event) {
    creatures1::creatures::Creature* creature = as_creature(target);
    WindowsCreatureFanoutHost fanout(document_);
    switch (classify_event_target(target, creature)) {
    case EventTargetKind::creature: {
        WindowsCreatureScriptEventHost events(document_);
        creature->handle_queued_event_slot6(event, events);
        return;
    }
    case EventTargetKind::call_button: {
        // CallButton::HandleQueuedEvent1 @ 00429d50 is a tail jump back to
        // slot 5, so queued event 1 runs the same call request.
        WindowsCallButtonRuntimeHost host(document_);
        static_cast<creatures1::objects::CallButton&>(target)
            .request_lift_call(event, host);
        return;
    }
    case EventTargetKind::pointer_tool:
    case EventTargetKind::simple_object: {
        WindowsObjectScriptDispatchHost scripts(document_);
        static_cast<creatures1::objects::SimpleObject&>(target)
            .handle_queued_event_1(event, fanout, scripts);
        return;
    }
    case EventTargetKind::lift: {
        WindowsCallButtonRuntimeHost host(document_);
        static_cast<creatures1::objects::Lift&>(target)
            .request_move_down(event, host);
        return;
    }
    case EventTargetKind::compound_object: {
        WindowsCompoundObjectEventHost host(document_);
        static_cast<creatures1::objects::CompoundObject&>(target)
            .handle_queued_event_1(event, host);
        return;
    }
    case EventTargetKind::base_object:
        target.handle_queued_creature_event(event, fanout);
        return;
    }
}

void WindowsObjectEventRuntime::handle_event_2(
    creatures1::objects::Object& target,
    const creatures1::objects::QueuedObjectEvent& event) {
    creatures1::creatures::Creature* creature = as_creature(target);
    WindowsCreatureFanoutHost fanout(document_);
    switch (classify_event_target(target, creature)) {
    case EventTargetKind::creature: {
        WindowsCreatureScriptEventHost events(document_);
        creature->handle_queued_event_slot7(event, events);
        return;
    }
    case EventTargetKind::call_button:
    case EventTargetKind::pointer_tool:
    case EventTargetKind::simple_object: {
        // CallButton does not override slot 7; it inherits SimpleObject's.
        WindowsObjectScriptDispatchHost scripts(document_);
        static_cast<creatures1::objects::SimpleObject&>(target)
            .handle_queued_event_2(event, fanout, scripts);
        return;
    }
    case EventTargetKind::lift:
        // Lift's vtable slot 7 is Object::HandleQueuedEvent3, not
        // CompoundObject::HandleQueuedEvent2.
        target.handle_queued_event_3(event, fanout);
        return;
    case EventTargetKind::compound_object: {
        WindowsCompoundObjectEventHost host(document_);
        static_cast<creatures1::objects::CompoundObject&>(target)
            .handle_queued_event_2(event, host);
        return;
    }
    case EventTargetKind::base_object:
        target.handle_queued_creature_event(event, fanout);
        return;
    }
}

void WindowsObjectEventRuntime::handle_event_3(
    creatures1::objects::Object& target,
    const creatures1::objects::QueuedObjectEvent& event) {
    // Creature's slot 10 is NoOpVirtualMethod.  Every other class reaches a
    // body identical to handle_queued_creature_event.
    if (as_creature(target) != nullptr) {
        return;
    }
    WindowsCreatureFanoutHost fanout(document_);
    target.handle_queued_event_3(event, fanout);
}

void WindowsObjectEventRuntime::handle_event_4(
    creatures1::objects::Object& target,
    const creatures1::objects::QueuedObjectEvent& event) {
    creatures1::creatures::Creature* creature = as_creature(target);
    WindowsCreatureFanoutHost fanout(document_);
    switch (classify_event_target(target, creature)) {
    case EventTargetKind::creature: {
        WindowsCreaturePickupDropHost host(document_);
        creature->handle_pickup_event(event, host);
        return;
    }
    case EventTargetKind::pointer_tool: {
        C1MainFrame* frame = active_main_frame();
        WindowsPointerToolRuntimeHost runtime(
            document_, frame == nullptr ? nullptr : active_c1_view(*frame));
        static_cast<creatures1::ui::PointerTool&>(target)
            .handle_queued_event_4(event, fanout, runtime);
        return;
    }
    case EventTargetKind::call_button:
    case EventTargetKind::simple_object: {
        WindowsSimpleObjectInteractionHost host(document_);
        static_cast<creatures1::objects::SimpleObject&>(target)
            .handle_queued_event_4(event, fanout, host);
        return;
    }
    case EventTargetKind::compound_object:
    case EventTargetKind::base_object:
        // CompoundObject's slot 8 is the inherited creature-event body.
        target.handle_queued_creature_event(event, fanout);
        return;
    }
}

void WindowsObjectEventRuntime::handle_event_5(
    creatures1::objects::Object& target,
    const creatures1::objects::QueuedObjectEvent& event) {
    creatures1::creatures::Creature* creature = as_creature(target);
    switch (classify_event_target(target, creature)) {
    case EventTargetKind::creature: {
        WindowsCreaturePickupDropHost host(document_);
        creature->handle_drop_event(event, host);
        return;
    }
    case EventTargetKind::pointer_tool: {
        C1MainFrame* frame = active_main_frame();
        WindowsPointerToolRuntimeHost runtime(
            document_, frame == nullptr ? nullptr : active_c1_view(*frame));
        static_cast<creatures1::ui::PointerTool&>(target)
            .handle_queued_event_5(event, runtime);
        return;
    }
    case EventTargetKind::call_button:
    case EventTargetKind::simple_object: {
        WindowsSimpleObjectInteractionHost host(document_);
        static_cast<creatures1::objects::SimpleObject&>(target)
            .handle_queued_event_5(event, host);
        return;
    }
    case EventTargetKind::compound_object:
    case EventTargetKind::base_object:
        // Slot 9 is NoOpVirtualMethod for both.
        return;
    }
}

void WindowsObjectEventRuntime::handle_event_6(
    creatures1::objects::Object& target,
    const creatures1::objects::QueuedObjectEvent& event) {
    // Only Creature overrides slot 11; every other class holds the no-op.
    creatures1::creatures::Creature* creature = as_creature(target);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureWordsHost words(document_, *creature);
    WindowsStimulusSourceHost source(document_);
    WindowsCreatureSpeechHost speech(document_);
    creature->handle_heard_words_event(event, source, document_, words,
                                       speech);
}

void WindowsObjectEventRuntime::handle_event_7(
    creatures1::objects::Object& target,
    const creatures1::objects::QueuedObjectEvent& event) {
    // Only Creature overrides slot 12.
    creatures1::creatures::Creature* creature = as_creature(target);
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureWordsHost words(document_, *creature);
    WindowsStimulusSourceHost source(document_);
    WindowsCreatureSpeechHost speech(document_);
    creature->handle_word_learning_event(event, source, document_, words,
                                         speech);
}

void WindowsObjectEventRuntime::handle_event_8(
    creatures1::objects::Object& target) {
    WindowsObjectScriptDispatchHost scripts(document_);
    target.dispatch_event_7_after_bounds_update(document_, scripts);
}

void WindowsObjectEventRuntime::handle_event_9(
    creatures1::objects::Object& target) {
    WindowsObjectEventDispatchHost dispatcher(document_);
    target.dispatch_event_8(dispatcher);
}

} // namespace creatures1::platform
