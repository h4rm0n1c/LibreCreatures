#include "call_button.hpp"

#include "lift.hpp"

#include <cstddef>

namespace creatures1::objects {

CallButton::CallButton(std::uint32_t object_file_id, int header_record_index,
                       std::uint32_t image_count, int render_plane,
                       Lift* active_lift,
                       SimpleObjectConstructionHost& construction)
    : SimpleObject(object_file_id, header_record_index, image_count,
                   /*cache_protected=*/false, /*initial_world_x=*/0,
                   /*initial_world_y=*/0, render_plane,
                   /*bounds_flags=*/4,
                   /*classifier_event=*/0, /*classifier_species=*/0,
                   /*classifier_genus=*/2, /*classifier_family=*/2,
                   /*click_event_selector_0=*/0,
                   /*reserved_word_0=*/0, /*reserved_word_1=*/0,
                   static_cast<std::uint8_t>(
                       InteractionEventFlag::event_1_enabled),
                   construction),
      lift_(active_lift) {
    floor_index = 0xff;
}


void CallButton::serialize(ObjectArchive& archive) {
    SimpleObject::serialize(archive);

    if (archive.is_loading()) {
        lift_ = static_cast<Lift*>(archive.read_object_reference("Lift"));
        floor_index = archive.read_byte();
        return;
    }

    archive.write_object_reference(lift_, "Lift");
    archive.write_byte(floor_index);
}

namespace {

int find_room_containing_point(const CallButtonRuntimeHost& host,
                               int world_x, int world_y) {
    for (std::size_t room = 0; room < host.room_count(); ++room) {
        if (host.room_contains_point(room, world_x, world_y)) {
            return static_cast<int>(room);
        }
    }
    return -1;
}

void queue_lift_stimulus(CallButton& button, const Object& source,
                         CallButtonRuntimeHost& host) {
    QueuedCreatureStimulus stimulus{};
    if (host.copy_built_in_stimulus(source, button, 0, stimulus)) {
        host.queue_creature_stimulus(stimulus);
    }
}

} // namespace

void CallButton::update_lift_state_and_queue_redraw(
    CallButtonRuntimeHost& host) {
    Lift& lift = *lift_;

    if (floor_index == 0xff) {
        const int button_x = sound_source_x();
        const int button_y = sound_source_y();
        const int button_room =
            find_room_containing_point(host, button_x, button_y);
        lift.floor_y_by_index[static_cast<std::size_t>(lift.floor_count)] =
            host.room_bottom(static_cast<std::size_t>(button_room));

        const int lift_x = lift.sound_source_x() +
                           lift.current_visual_width() / 2;
        const int lift_y = lift.sound_source_y() +
                           lift.current_visual_height() / 2;
        const int lift_room = find_room_containing_point(host, lift_x, lift_y);
        if (button_room == lift_room) {
            lift.current_floor_index = lift.floor_count;
        }

        floor_index = static_cast<std::uint8_t>(lift.floor_count);
        ++lift.floor_count;
    }

    host.clear_edit_object();
    update_movement_bounds(host);
    host.queue_immediate_event(*this, *this, ObjectEventId::event_8, 0);
}

void CallButton::deactivate(ObjectScriptDispatchHost& scripts) {
    set_current_interaction_event_id(0);
    dispatch_script_event(ObjectEventId::event_0, this, false, scripts);
}

void CallButton::request_lift_call(const QueuedObjectEvent& event,
                                   CallButtonRuntimeHost& host) {
    Lift& lift = *lift_;
    Object* source = event.source;

    if (current_interaction_event_id() == 0) {
        const std::uint32_t lift_event = lift.interaction_event();
        const bool lift_is_moving = lift_event == 1 || lift_event == 2;
        const bool lift_is_elsewhere =
            lift.current_floor_index != static_cast<int>(floor_index);
        if (!lift_is_moving && !lift_is_elsewhere) {
            return;
        }

        const bool source_is_creature =
            source != nullptr && host.source_is_creature(*source);
        if (source_is_creature &&
            (interaction_event_flags &
             static_cast<std::uint8_t>(InteractionEventFlag::event_1_enabled)) ==
                0) {
            queue_lift_stimulus(*this, *source, host);
        } else {
            set_current_interaction_event_id(1);
            dispatch_script_event(ObjectEventId::event_1, source, false, host);
        }

        std::size_t slot = 0;
        while (slot < Lift::kCallButtonCapacity &&
               lift.call_button_refs[slot] != nullptr) {
            ++slot;
        }
        if (slot == Lift::kCallButtonCapacity) {
            set_current_interaction_event_id(0);
            dispatch_script_event(ObjectEventId::event_0, this, false, host);
            return;
        }
        lift.call_button_refs[slot] = this;
        return;
    }

    if (source != nullptr && host.source_is_creature(*source)) {
        queue_lift_stimulus(*this, *source, host);
    }
}

} // namespace creatures1::objects
