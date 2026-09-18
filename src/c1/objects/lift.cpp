#include "lift.hpp"

#include "events.hpp"

#include <algorithm>
#include <cstdlib>

namespace creatures1::objects {

Lift::Lift(std::uint32_t sprite_file_id, int header_record_index,
           std::uint32_t image_count,
           CompoundObjectConstructionHost& construction)
    : Vehicle(sprite_file_id, header_record_index, image_count, construction) {}


namespace {

constexpr Object::BoundsFlags kLiftPreservedBoundsMask = 0x3fu;

void snap_lift_to_sound_source(Lift& lift) {
    lift.position_x_8_8 = lift.sound_source_x() << 8;
    lift.position_y_8_8 = lift.sound_source_y() << 8;
}

void queue_built_in_creature_stimulus(
    Lift& lift, const Object& source, LiftRuntimeHost& host) {
    QueuedCreatureStimulus stimulus{};
    if (host.copy_built_in_stimulus(source, lift, 0, stimulus)) {
        host.queue_creature_stimulus(stimulus);
    }
}

void request_move(Lift& lift, const QueuedObjectEvent& event,
                  bool move_up, LiftRuntimeHost& host) {
    const bool available = lift.interaction_event() == 0 &&
                           lift.object_variable_0() == 0;
    const bool has_destination = move_up
                                     ? lift.current_floor_index <
                                           lift.floor_count - 1
                                     : lift.current_floor_index > 0;
    if (available && has_destination) {
        lift.set_interaction_event(1);
        lift.current_floor_index += move_up ? 1 : -1;
        snap_lift_to_sound_source(lift);
        host.dispatch_script_event(
            lift, event.source,
            move_up ? ObjectEventId::event_1 : ObjectEventId::event_2);
        return;
    }

    if (event.source != nullptr && host.source_is_creature(*event.source)) {
        queue_built_in_creature_stimulus(lift, *event.source, host);
    }
}

} // namespace

Lift::Lift(CompoundObjectLifetimeHost* lifetime_host)
    : Vehicle(lifetime_host) {
    floor_count = 0;
    current_floor_index = 0;
    selected_call_button_index = -1;
    call_selection_cooldown = 0;
    floor_y_by_index.fill(0);
    call_button_refs.fill(nullptr);
}

void Lift::serialize(ObjectArchive& archive) {
    Vehicle::serialize(archive);

    if (archive.is_loading()) {
        floor_count = archive.read_int32();
        current_floor_index = archive.read_int32();
        selected_call_button_index = archive.read_int32();
        call_selection_cooldown = archive.read_byte();
        for (std::size_t index = 0; index < kCallButtonCapacity; ++index) {
            floor_y_by_index[index] = archive.read_int32();
            call_button_refs[index] = static_cast<CallButton*>(
                archive.read_object_reference("CallButton"));
        }
        return;
    }

    archive.write_int32(floor_count);
    archive.write_int32(current_floor_index);
    archive.write_int32(selected_call_button_index);
    archive.write_byte(call_selection_cooldown);
    for (std::size_t index = 0; index < kCallButtonCapacity; ++index) {
        archive.write_int32(floor_y_by_index[index]);
        archive.write_object_reference(call_button_refs[index], "CallButton");
    }
}

void Lift::initialize_state(LiftRuntimeHost& host) {
    set_bounds_flags(static_cast<BoundsFlags>(bounds_flags() &
                                               kLiftPreservedBoundsMask));
    floor_count = 0;
    current_floor_index = 0;
    selected_call_button_index = -1;
    call_selection_cooldown = 0;
    call_button_refs.fill(nullptr);
    host.publish_active_lift(*this);
}

void Lift::update_bounds_and_queue_redraw(LiftRuntimeHost& host) {
    world::WorldRect lift_bounds{};
    if (!get_bounds(&lift_bounds)) {
        return;
    }

    const int center_x = (lift_bounds.max_x - lift_bounds.min_x) / 2 +
                         lift_bounds.min_x;
    const int center_y = (lift_bounds.max_y - lift_bounds.min_y) / 2 +
                         lift_bounds.min_y;
    for (std::size_t room = 0; room < host.room_count(); ++room) {
        if (host.room_contains_point(room, center_x, center_y)) {
            move_to_and_redraw(
                lift_bounds.min_x,
                host.room_bottom(room) - creature_event_bounds_local.max_y,
                host);
        }
    }

    host.clear_edit_object();
    update_movement_bounds(host);
    host.queue_immediate_event(*this, *this, ObjectEventId::event_8, 0);
}

void Lift::tick(LiftRuntimeHost& host) {
    if (interaction_event() != 0) {
        int arrival_divisor = velocity_y_8_8 >> 8;
        if (arrival_divisor == 0) {
            arrival_divisor = 1;
        }
        const int cabin_bottom = creature_event_bounds_local.max_y;
        if ((cabin_bottom + sound_source_y()) / arrival_divisor ==
            floor_y_by_index[static_cast<std::size_t>(current_floor_index)] /
                arrival_divisor) {
            complete_floor_arrival(host, host);
            if (selected_call_button_index != -1) {
                const std::size_t button_index = static_cast<std::size_t>(
                    selected_call_button_index);
                CallButton* button = call_button_refs[button_index];
                if (button != nullptr) {
                    host.deactivate_call_button(*button);
                }
                call_button_refs[button_index] = nullptr;
                selected_call_button_index = -1;
            }
            call_selection_cooldown = 1;
        }
    }

    Vehicle::tick(host);
}

void Lift::request_move_up(const QueuedObjectEvent& event,
                           LiftRuntimeHost& host) {
    request_move(*this, event, true, host);
}

void Lift::request_move_down(const QueuedObjectEvent& event,
                             LiftRuntimeHost& host) {
    request_move(*this, event, false, host);
}

void Lift::select_nearest_call_button_and_start_move(LiftRuntimeHost& host) {
    if (call_selection_cooldown != 0) {
        --call_selection_cooldown;
        return;
    }
    if (interaction_event() != 0 || object_variable_0() != 0) {
        return;
    }

    selected_call_button_index = -1;
    int nearest_floor_distance = 999;
    for (std::size_t index = 0; index < 4; ++index) {
        CallButton* button = call_button_refs[index];
        if (button == nullptr) {
            continue;
        }

        const int floor_delta = host.call_button_floor(*button) -
                                current_floor_index;
        const int floor_distance = std::abs(floor_delta);
        if (floor_distance < nearest_floor_distance) {
            nearest_floor_distance = floor_distance;
            selected_call_button_index = static_cast<std::int32_t>(index);
        }
    }

    if (selected_call_button_index == -1) {
        return;
    }

    const int previous_floor = current_floor_index;
    const std::size_t selected_index =
        static_cast<std::size_t>(selected_call_button_index);
    current_floor_index = host.call_button_floor(
        *call_button_refs[selected_index]);
    const std::uint32_t previous_interaction = interaction_event();
    const bool is_creature_classifier =
        (classifier_base() & 0xff000000u) == 0x04000000u;

    const bool moving_up = previous_floor < current_floor_index;
    const ObjectEventId event_id = moving_up ? ObjectEventId::event_1
                                             : ObjectEventId::event_2;
    const std::size_t config_index = moving_up ? 0 : 1;
    const std::uint32_t direction_event =
        static_cast<std::uint32_t>(event_id);

    if (previous_interaction != direction_event) {
        if (!is_creature_classifier ||
            creature_event_config.event_config_value[config_index] != -1) {
            set_interaction_event(direction_event);
            snap_lift_to_sound_source(*this);
            host.dispatch_script_event(*this, this, event_id);
            return;
        }
    } else if (!is_creature_classifier) {
        return;
    }

    // The binary's shared tail copies the lift's built-in stimulus record and
    // queues it with the lift as source.  The adapter owns the ring storage;
    // this method owns only the decision to use that fallback.
    queue_built_in_creature_stimulus(*this, *this, host);
}

} // namespace creatures1::objects
