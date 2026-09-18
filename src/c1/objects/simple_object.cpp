#include "simple_object.hpp"

#include "bubble.hpp"
#include "compound_object.hpp"
#include "debug.hpp"
#include "events.hpp"
#include "../creatures/events.hpp"
#include "../scripting/tables.hpp"
#include "../world/map.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace creatures1::objects {

namespace {
constexpr int kWorldWidth = 0x20a0;

char* skip_image_sequence_text(char* sequence_text) {
    char* read_cursor = sequence_text + 1;
    while (*read_cursor != ']') {
        ++read_cursor;
    }
    return read_cursor + 2;
}

// Recovered from Creatures.exe .rdata at 0x004579a4.  Each row is the exact
// three-byte selector record consumed by BHVR's index*3 addressing.
constexpr std::array<std::array<std::int8_t, 3>, 5>
    kSimpleObjectBehaviorSelectors{{
        {{-1, -1, -1}},
        {{0, -1, -1}},
        {{0, 0, -1}},
        {{0, 2, -1}},
        {{0, 1, 2}},
    }};

world::WorldRect union_object_and_pointer_bounds(
    const world::WorldRect& object_bounds,
    const world::WorldRect& pointer_bounds) {
    world::WorldRect best = object_bounds;
    best.min_y = std::min(object_bounds.min_y, pointer_bounds.min_y);
    best.max_y = std::max(object_bounds.max_y, pointer_bounds.max_y);
    int best_width = kWorldWidth + 1;

    // UpdateUnboundedPositionAndRedraw computes this union before handing it
    // to the renderer.  Try the three equivalent world placements of the
    // pointer rectangle and retain the shortest contiguous representation.
    // In particular, [0,100) plus [8300,8350) becomes [8300,8452), not the
    // almost-world-wide [0,8350) span produced by the generic union helper.
    for (const int world_shift : {-kWorldWidth, 0, kWorldWidth}) {
        const int shifted_min_x = pointer_bounds.min_x + world_shift;
        const int shifted_max_x = pointer_bounds.max_x + world_shift;
        int candidate_min_x = std::min(object_bounds.min_x, shifted_min_x);
        int candidate_max_x = std::max(object_bounds.max_x, shifted_max_x);
        while (candidate_min_x < 0) {
            candidate_min_x += kWorldWidth;
            candidate_max_x += kWorldWidth;
        }
        while (candidate_min_x >= kWorldWidth) {
            candidate_min_x -= kWorldWidth;
            candidate_max_x -= kWorldWidth;
        }
        if (candidate_max_x - candidate_min_x < best_width) {
            best.min_x = candidate_min_x;
            best.max_x = candidate_max_x;
            best_width = candidate_max_x - candidate_min_x;
        }
    }
    return best;
}

creatures1::creatures::Creature* find_creature_source(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout);
void queue_creature_context_zero(
    SimpleObject& target, const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout);
}

SimpleObject::SimpleObject()
    : Object(),
      click_event_for_interaction_state{-1, -1, -1},
      interaction_event_flags(0),
      saved_entity_render_plane(0),
      entity_(nullptr) {
    set_classifier_components(
        static_cast<std::uint8_t>(scripting::ScriptEvent::deactivate), 0, 0,
        static_cast<std::uint8_t>(ClassifierFamily::simple_object));

    // Object's source-level constructor establishes the same default-world
    // movement rectangle produced by the native UpdateMovementBounds call.
}

SimpleObject::~SimpleObject() {
    // Native destruction deletes the owned Entity before Object performs its
    // renderable-set, sound, gallery, and registry teardown. Resetting the
    // unique owner expresses that ordering without reproducing ABI wrappers.
    entity_.reset();
}

std::unique_ptr<Bubble> SimpleObject::create_bubble(
    std::string_view bubble_text, std::uint8_t lifetime_ticks,
    BubblePlacementMode placement_mode, SimpleObjectBubbleHost& host) {
    const int viewport_left = host.viewport_left();
    const int viewport_right = host.viewport_right();
    const int viewport_center =
        viewport_left + (viewport_right - viewport_left) / 2;
    const bool place_on_right = sound_source_x() <= viewport_center;

    return std::unique_ptr<Bubble>(new (std::nothrow) Bubble(
        this, lifetime_ticks, bubble_text, placement_mode, place_on_right,
        host.bubble_construction()));
}

void SimpleObject::tick(SimpleObjectTickHost& host) {
    update_sound(host);

    switch (bounds_mode()) {
    case BoundsMode::unbounded_1:
        update_unbounded_position_and_redraw(host);
        break;

    case BoundsMode::explicit_rectangle:
        if (bounds_reference_object() == nullptr) {
            set_bounds_mode(
                static_cast<std::uint32_t>(BoundsMode::default_world), host);
        } else {
            update_entity_for_explicit_rect_bounds_and_redraw(host);
        }
        break;

    default:
        if (entity_ != nullptr) {
            entity_->advance_image_sequence(host);
        }
        break;
    }

    if (timer_period() > 0) {
        const std::int32_t next_countdown = timer_countdown() - 1;
        set_timer_countdown(next_countdown);
        if (next_countdown < 1) {
            dispatch_script_event(ObjectEventId::event_9, this, false, host);
            set_timer_countdown(timer_period());
        }
    }
}

void SimpleObject::update_unbounded_position_and_redraw(
    SimpleObjectTickHost& host) {
    if (host.pointer_input_pending()) {
        // The native method invokes PointerTool::ProcessPendingInput through
        // the current SimpleObject receiver. Preserve that call as an
        // explicit runtime operation; do not make the renderer impersonate
        // this method.
        host.process_pending_pointer_input(*this);
    }

    world::WorldRect old_object_bounds{};
    get_bounds(&old_object_bounds);

    int world_x = host.pointer_mouse_world_x();
    if (world_x >= kWorldWidth) {
        world_x -= kWorldWidth;
    }
    const int world_y = host.pointer_mouse_world_y();

    if (entity_ != nullptr) {
        entity_->advance_image_sequence_for_moving_vehicle();
    }

    SimpleObject* pointer_tool = host.pointer_tool();
    if (pointer_tool == this) {
        // The native call lands in the object's virtual MoveToAndRedraw
        // slot.  The clean host exposes only the resulting presentation
        // effect, so retain the same old/new bounds ordering without making
        // SimpleObjectTickHost inherit the unrelated movement facade.
        move_to(world_x, world_y);
        world::WorldRect new_object_bounds{};
        get_bounds(&new_object_bounds);
        host.present_or_queue_dirty_world_rect(old_object_bounds);
        host.present_or_queue_dirty_world_rect(new_object_bounds);
        return;
    }

    if (pointer_tool == nullptr) {
        return;
    }

    world::WorldRect old_pointer_bounds{};
    pointer_tool->get_bounds(&old_pointer_bounds);
    const world::WorldRect old_union = union_object_and_pointer_bounds(
        old_object_bounds, old_pointer_bounds);

    move_to(world_x, world_y);
    pointer_tool->move_to(world_x, world_y - 0x10);

    world::WorldRect new_object_bounds{};
    world::WorldRect new_pointer_bounds{};
    get_bounds(&new_object_bounds);
    pointer_tool->get_bounds(&new_pointer_bounds);
    const world::WorldRect new_union = union_object_and_pointer_bounds(
        new_object_bounds, new_pointer_bounds);

    host.present_or_queue_dirty_world_rect(old_union);
    host.present_or_queue_dirty_world_rect(new_union);
}

void SimpleObject::update_entity_for_explicit_rect_bounds_and_redraw(
    SimpleObjectTickHost& host) {
    world::WorldRect old_bounds{};
    get_bounds(&old_bounds);

    if (entity_ != nullptr) {
        entity_->advance_image_sequence_for_moving_vehicle();
    }

    // The native function has already established this relationship as a
    // CompoundObject before reaching this path. Keep the cast explicit and
    // confine the unresolved low-byte compatibility operation to the host.
    const CompoundObject* reference = static_cast<const CompoundObject*>(
        bounds_reference_object());
    const int center_x = reference->part_bounds(1).min_x -
                         entity_->current_image_width() / 2;
    const int center_y = reference->part_bounds(2).max_x -
                         entity_->current_image_height() / 2;

    int wrapped_x = wrap_world_x_once(center_x);
    wrapped_x = wrap_world_x_once(wrapped_x);
    entity_->set_world_x(wrapped_x);
    entity_->set_world_y(center_y);
    entity_->set_render_plane(
        reference->render_plane() +
        host.carried_object_render_plane_offset(*reference));

    world::WorldRect new_bounds{};
    get_bounds(&new_bounds);
    host.present_or_queue_dirty_world_rect(old_bounds);
    host.present_or_queue_dirty_world_rect(new_bounds);
}

void SimpleObject::handle_queued_event_4(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout,
    SimpleObjectInteractionHost& host) {
    Object* pointer_tool = host.pointer_tool();
    if (event.source == pointer_tool &&
        has_bounds_flag(0x02u)) {
        for (std::size_t index = 0; index < host.non_scenery_object_count();
             ++index) {
            Object* candidate = host.non_scenery_object_at(index);
            if (candidate == nullptr) {
                host.report_invalid_non_scenery_index();
                return;
            }
            if (candidate != pointer_tool &&
                candidate->uses_unbounded_world_position()) {
                return;
            }
        }

        set_bounds_mode(static_cast<std::uint32_t>(BoundsMode::unbounded_1),
                        host);
        set_bounds_reference_object(nullptr);
        entity_->set_render_plane(9999);
        move_to_and_redraw(host.pointer_world_x(), host.pointer_world_y(),
                           host);
        if (pointer_tool != nullptr) {
            host.queue_immediate_event(*this, *pointer_tool,
                                       ObjectEventId::event_5, 0);
        }
        dispatch_script_event(ObjectEventId::event_4, event.source, false,
                              host);
        return;
    }

    if (find_creature_source(event, fanout) == nullptr) {
        return;
    }

    if (has_bounds_flag(0x01u)) {
        if (bounds_mode() == BoundsMode::unbounded_1 &&
            pointer_tool != nullptr) {
            host.queue_immediate_event(*this, *pointer_tool,
                                       ObjectEventId::event_4, 0);
        }

        for (std::size_t index = 0; index < host.non_scenery_object_count();
             ++index) {
            Object* candidate = host.non_scenery_object_at(index);
            if (candidate == nullptr) {
                host.report_invalid_non_scenery_index();
                return;
            }
            if (candidate != this && candidate->bounds_mode() ==
                                         BoundsMode::explicit_rectangle &&
                candidate->bounds_reference_object() == event.source &&
                event.source != nullptr) {
                host.queue_immediate_event(*event.source, *candidate,
                                           ObjectEventId::event_5, 0);
            }
        }

        set_bounds_mode(
            static_cast<std::uint32_t>(BoundsMode::explicit_rectangle), host);
        set_bounds_reference_object(event.source);
        dispatch_script_event(ObjectEventId::event_4, event.source, false,
                              host);
        return;
    }

    queue_creature_context_zero(*this, event, fanout);
}

void SimpleObject::handle_queued_event_5(
    const QueuedObjectEvent& event, SimpleObjectInteractionHost& host) {
    switch (bounds_mode()) {
    case BoundsMode::unbounded_1:
        end_interaction_with_source(event.source, host);
        if (Object* pointer_tool = host.pointer_tool(); pointer_tool != nullptr) {
            host.queue_immediate_event(*this, *pointer_tool,
                                       ObjectEventId::event_4, 0);
        }
        break;

    case BoundsMode::explicit_rectangle:
        if (bounds_reference_object() == event.source) {
            end_interaction_with_source(event.source, host);
        }
        break;

    default:
        break;
    }
}

namespace {

// Resting-Y probe.  Enabled by default: see log_placement in
// windows_document_host.cpp for the C1_TRACE_CREATURE contract.
void log_object_resting_y(const Object& object, int old_x, int old_y,
                          int target_x, int target_y) {
    static const char* setting = std::getenv("C1_TRACE_CREATURE");
    if (setting != nullptr &&
        (std::strcmp(setting, "0") == 0 || std::strcmp(setting, "off") == 0)) {
        return;
    }
    static std::size_t rows = 0;
    if (rows >= 400) {
        return;
    }
    ++rows;
    const std::uint32_t classifier = object.classifier_base();
    const auto bounds = object.movement_bounds();
    if (FILE* log = std::fopen("Creatures.place.log", "a")) {
        std::fprintf(log,
                     "drop family=%u genus=%u species=%u old=%d,%d "
                     "rest=%d,%d floor=%d bottom_on_floor=%d\n",
                     (classifier >> 24) & 0xffu, (classifier >> 16) & 0xffu,
                     (classifier >> 8) & 0xffu, old_x, old_y, target_x,
                     target_y, bounds.max_y, 1);
        std::fclose(log);
    }
}

}  // namespace

void SimpleObject::end_interaction_with_source(
    Object* source_object, SimpleObjectInteractionHost& host) {
    const int old_world_x = entity_->world_x();
    const int old_world_y = entity_->world_y();
    set_bounds_reference_object(nullptr);

    if (!has_bounds_flag(0x20u)) {
        Object* vehicle = find_topmost_overlapping_object(
            kIsVehicle, kIsVehicle, host);
        int target_world_x = old_world_x;
        if (vehicle == nullptr) {
            set_bounds_mode(
                static_cast<std::uint32_t>(BoundsMode::default_world), host);
            entity_->set_render_plane(saved_entity_render_plane);
            update_movement_bounds(host);
        } else {
            set_bounds_mode(
                static_cast<std::uint32_t>(BoundsMode::vehicle_local), host);
            set_bounds_reference_object(vehicle);

            const int vehicle_world_y =
                host.vehicle_primary_entity_y(*vehicle);
            const int plane_offset = movement_bounds().max_y <= vehicle_world_y
                                         ? -5
                                         : 5;
            entity_->set_render_plane(vehicle_world_y + plane_offset);
            update_movement_bounds(host);

            target_world_x = movement_bounds().min_x;
            if (target_world_x <= old_world_x) {
                target_world_x = old_world_x;
                if (movement_bounds().max_x <
                    old_world_x + entity_->current_image_width()) {
                    target_world_x =
                        movement_bounds().max_x - entity_->current_image_width();
                }
            }
        }

        dispatch_script_event(ObjectEventId::event_5, source_object, false,
                              host);

        // Native EndInteractionWithSource @00428bb0 has exactly one resting-Y
        // formula, read AFTER EVENT_5 so a stateful drop script (the carrot
        // changes pose, and therefore sprite height, in its own drop script)
        // is reflected:
        //     movement_bounds.max_y - current_image.height
        // This places the object's bottom edge exactly on the room floor.
        // An earlier port revision re-derived a separate "release room" from
        // the pre-move position and added a no-room fallback; both diverged
        // from the native contract, and an egg's resting Y is what the hatch
        // script hands a newborn as its down foot.
        const int target_world_y =
            movement_bounds().max_y - entity_->current_image_height();

        log_object_resting_y(*this, old_world_x, old_world_y, target_world_x,
                             target_world_y);

        move_to_and_redraw(target_world_x, target_world_y, host);
        return;
    }

    set_bounds_mode(static_cast<std::uint32_t>(BoundsMode::unbounded_2), host);
    update_movement_bounds(host);
    entity_->set_render_plane(saved_entity_render_plane);
    move_by_and_redraw(0, 0, host);
}

SimpleObject::SimpleObject(
    std::uint32_t sprite_file_id, int header_record_index,
    std::uint32_t image_count, bool cache_protected, int initial_world_x,
    int initial_world_y, int render_plane, std::uint8_t bounds_flags,
    std::uint8_t classifier_event, std::uint8_t classifier_species,
    std::uint8_t classifier_genus, std::uint8_t classifier_family,
    std::uint8_t click_event_selector_0, std::uint32_t reserved_word_0,
    std::uint32_t reserved_word_1, std::uint8_t interaction_event_flags,
    SimpleObjectConstructionHost& construction)
    : Object(),
      click_event_for_interaction_state{
          static_cast<std::int8_t>(click_event_selector_0), -1, -1},
      interaction_event_flags(interaction_event_flags) {
    // These words are present in the native call but the live body does not
    // consume them.
    (void)reserved_word_0;
    (void)reserved_word_1;

    display::Gallery* gallery = construction.acquire_gallery(
        sprite_file_id, header_record_index, image_count, cache_protected);

    // Native order: establish the SimpleObject defaults, allocate/register
    // the Entity, copy its placement, then apply caller-supplied state.
    set_classifier_components(0, 0, 0, 2);
    entity_ = Entity::create_object(&construction.entity_registry());
    if (entity_ != nullptr) {
        entity_->set_render_plane(render_plane);
        entity_->move_to(initial_world_x, initial_world_y);
        entity_->set_gallery(gallery);
        entity_->set_image_index(0);
        entity_->set_image_index_base(0);
    }
    saved_entity_render_plane = render_plane;
    set_bounds_flags(bounds_flags);
    set_classifier_components(classifier_event, classifier_species,
                              classifier_genus, classifier_family);
    construction.update_movement_bounds(*this);
}

void SimpleObject::serialize(ObjectArchive& archive) {
    Object::serialize(archive);

    if (archive.is_loading()) {
        entity_.reset(static_cast<Entity*>(
            archive.read_object_reference("Entity")));
        saved_entity_render_plane = archive.read_int32();
        for (std::int8_t& selector : click_event_for_interaction_state) {
            selector = static_cast<std::int8_t>(archive.read_byte());
        }
        interaction_event_flags = archive.read_byte();
        return;
    }

    archive.write_object_reference(entity_.get(), "Entity");
    archive.write_int32(saved_entity_render_plane);
    for (const std::int8_t selector : click_event_for_interaction_state) {
        archive.write_byte(static_cast<std::uint8_t>(selector));
    }
    archive.write_byte(interaction_event_flags);
}

void SimpleObject::configure_interaction_behavior(
    std::uint32_t behavior_index, std::uint32_t interaction_flags) {
    if (behavior_index >= kSimpleObjectBehaviorSelectors.size()) {
        return;
    }

    click_event_for_interaction_state =
        kSimpleObjectBehaviorSelectors[behavior_index];
    interaction_event_flags = static_cast<std::uint8_t>(interaction_flags);
}

void SimpleObject::initialize_unbounded_object_placement(
    SimpleObjectPlacementHost& placement) {
    set_bounds_mode(static_cast<std::uint32_t>(BoundsMode::unbounded_1),
                    placement.renderables());
    set_bounds_reference_object(nullptr);
    entity_->set_render_plane(9999);

    int world_x = placement.mouse_world_x();
    if (world_x >= 0x20a0) {
        world_x -= 0x20a0;
    }
    move_to_and_redraw(world_x, placement.mouse_world_y(), placement);
}

void SimpleObject::finalize_object_edit(SimpleObjectEditHost& edit_host) {
    edit_host.clear_edit_object();

    const bool restricted_editor = edit_host.privilege_level() < 2;
    if (restricted_editor) {
        set_bounds_mode(static_cast<std::uint32_t>(BoundsMode::default_world),
                        edit_host);
    }

    update_movement_bounds(edit_host);
    edit_host.queue_immediate_event(*this, *this, ObjectEventId::event_8, 0);

    if (restricted_editor) {
        move_to_and_redraw(
            entity_->world_x(),
            movement_bounds().max_y - entity_->current_image_height(),
            edit_host);
    }
}

namespace {

creatures1::creatures::Creature* find_creature_source(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout) {
    if (event.source == nullptr) {
        return nullptr;
    }

    for (std::size_t index = 0; index < fanout.creature_count(); ++index) {
        creatures1::creatures::Creature* candidate =
            fanout.creature_at(index);
        if (candidate == nullptr) {
            fanout.report_invalid_creature_index();
            continue;
        }
        if (fanout.is_same_object(*candidate, *event.source) &&
            fanout.is_creature_classifier(*candidate)) {
            return candidate;
        }
    }
    return nullptr;
}

void queue_creature_context_zero(
    SimpleObject& target, const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout) {
    creatures1::creatures::Creature* source =
        find_creature_source(event, fanout);
    if (source == nullptr) {
        return;
    }

    QueuedCreatureStimulus stimulus{};
    if (fanout.copy_built_in_stimulus(*source, 0, target, stimulus)) {
        fanout.queue_creature_stimulus(stimulus);
    }
}

} // namespace

void SimpleObject::handle_queued_event(
    const QueuedObjectEvent& event,
    std::uint32_t expected_interaction_state,
    std::uint8_t required_event_flag, ObjectEventId script_event,
    creatures1::creatures::CreatureEventFanoutHost& fanout,
    ObjectScriptDispatchHost& scripts) {
    const bool source_is_creature = find_creature_source(event, fanout) != nullptr;
    const bool should_queue_creature_stimulus =
        source_is_creature &&
        (current_interaction_event_id() ==
             expected_interaction_state ||
         (interaction_event_flags & required_event_flag) == 0);

    if (should_queue_creature_stimulus) {
        queue_creature_context_zero(*this, event, fanout);
        return;
    }

    set_current_interaction_event_id(
        static_cast<std::uint32_t>(script_event));
    dispatch_script_event(script_event, event.source, false, scripts);
}

void SimpleObject::handle_queued_event_0(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout,
    ObjectScriptDispatchHost& scripts) {
    handle_queued_event(event, 1,
                        static_cast<std::uint8_t>(InteractionEventFlag::event_1_enabled),
                        ObjectEventId::event_1, fanout, scripts);
}

void SimpleObject::handle_queued_event_1(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout,
    ObjectScriptDispatchHost& scripts) {
    handle_queued_event(event, 2,
                        static_cast<std::uint8_t>(InteractionEventFlag::event_2_enabled),
                        ObjectEventId::event_2, fanout, scripts);
}

void SimpleObject::handle_queued_event_2(
    const QueuedObjectEvent& event,
    creatures1::creatures::CreatureEventFanoutHost& fanout,
    ObjectScriptDispatchHost& scripts) {
    handle_queued_event(event, 0,
                        static_cast<std::uint8_t>(InteractionEventFlag::event_0_enabled),
                        ObjectEventId::event_0, fanout, scripts);
}

int SimpleObject::sound_source_x() const {
    return entity_ == nullptr ? 0 : entity_->world_x();
}

int SimpleObject::sound_source_y() const {
    return entity_ == nullptr ? 0 : entity_->world_y();
}

int SimpleObject::current_visual_width() const {
    return entity_ == nullptr ? 0 : entity_->current_image_width();
}

int SimpleObject::current_visual_height() const {
    return entity_ == nullptr ? 0 : entity_->current_image_height();
}

int SimpleObject::wrap_world_x_once(int x) {
    if (x < 0) {
        return x + kWorldWidth;
    }
    if (x >= kWorldWidth) {
        return x - kWorldWidth;
    }
    return x;
}

void SimpleObject::move_to(int world_x, int world_y) {
    if (entity_ == nullptr) {
        return;
    }
    entity_->set_world_x(wrap_world_x_once(world_x));
    entity_->set_world_y(world_y);
}

void SimpleObject::move_by(int delta_x, int delta_y) {
    if (entity_ == nullptr) {
        return;
    }
    entity_->set_world_x(
        wrap_world_x_once(entity_->world_x() + delta_x));
    entity_->set_world_y(entity_->world_y() + delta_y);
}

void SimpleObject::move_to_and_redraw(
    int world_x, int world_y, SimpleObjectMoveRedrawHost& renderer) {
    world::WorldRect old_bounds{};
    world::WorldRect new_bounds{};
    get_bounds(&old_bounds);
    move_to(world_x, world_y);
    get_bounds(&new_bounds);
    renderer.redraw_after_simple_object_move(*this, old_bounds, new_bounds);
}

void SimpleObject::move_by_and_redraw(
    int delta_x, int delta_y, SimpleObjectMoveRedrawHost& renderer) {
    world::WorldRect old_bounds{};
    world::WorldRect new_bounds{};
    get_bounds(&old_bounds);
    move_by(delta_x, delta_y);
    get_bounds(&new_bounds);
    renderer.redraw_after_simple_object_move(*this, old_bounds, new_bounds);
}

bool SimpleObject::get_bounds(world::WorldRect* out_bounds) const {
    if (out_bounds == nullptr) {
        return false;
    }
    const Entity* object_entity = entity_.get();
    if (object_entity == nullptr || !object_entity->has_current_image()) {
        *out_bounds = {};
        return true;
    }

    out_bounds->min_x = object_entity->world_x();
    out_bounds->min_y = object_entity->world_y();
    out_bounds->max_x = out_bounds->min_x + object_entity->current_image_width();
    out_bounds->max_y = out_bounds->min_y + object_entity->current_image_height();
    return true;
}

int SimpleObject::render_plane() const {
    return entity_ == nullptr ? 0 : entity_->render_plane();
}

ObjectEventId SimpleObject::click_event_id_at_world_position(
    int world_x, int world_y) const {
    (void)world_x;
    (void)world_y;
    const std::size_t state = static_cast<std::size_t>(
        current_interaction_event_id());
    if (state >= click_event_for_interaction_state.size()) {
        return ObjectEventId::event_0;
    }
    return static_cast<ObjectEventId>(
        static_cast<std::int32_t>(click_event_for_interaction_state[state]));
}

void SimpleObject::get_part_center(int* out_x, int* out_y,
                                   std::int32_t part_index) const {
    (void)part_index;
    if (out_x == nullptr || out_y == nullptr || entity_ == nullptr) {
        return;
    }
    const int half_width = entity_->current_image_width() / 2;
    const int half_height = entity_->current_image_height() / 2;
    *out_x = wrap_world_x_once(entity_->world_x() + half_width);
    *out_y = entity_->world_y() + half_height;
}

char* SimpleObject::parse_image_sequence(char* sequence_text,
                                          int part_index) {
    (void)part_index;
    return entity_ == nullptr
               ? Object::parse_image_sequence(sequence_text, part_index)
               : entity_->parse_image_sequence(sequence_text);
}

bool SimpleObject::image_sequence_is_empty(int part_index) const {
    (void)part_index;
    return entity_ == nullptr || entity_->image_sequence_is_empty();
}

int SimpleObject::relative_image_index(int part_index) const {
    (void)part_index;
    return entity_ == nullptr ? 0 : entity_->relative_image_index();
}

char* SimpleObject::preload_image_sequence(
    char* sequence_text, int part_index,
    ImagePreloadHost& preload_host) const {
    (void)part_index;
    return entity_ == nullptr
               ? skip_image_sequence_text(sequence_text)
               : entity_->preload_image_sequence(sequence_text, preload_host);
}

bool SimpleObject::set_relative_image_index(
    CaosValue relative_index, int part_index,
    EntityImageSequenceRenderHost& redraw_host) {
    (void)part_index;
    if (entity_ == nullptr || entity_->gallery() == nullptr) {
        return false;
    }
    const display::Gallery* object_gallery = entity_->gallery();
    if (object_gallery->images == nullptr ||
        relative_index >= object_gallery->image_count) {
        return false;
    }

    entity_->set_relative_image_index_and_redraw(
        static_cast<std::int32_t>(relative_index), redraw_host);
    return true;
}

void SimpleObject::set_image_index(
    std::uint8_t image_index, EntityImageSequenceRenderHost& redraw_host) {
    if (entity_ != nullptr) {
        entity_->set_image_index_and_redraw(image_index, redraw_host);
    }
}

} // namespace creatures1::objects
