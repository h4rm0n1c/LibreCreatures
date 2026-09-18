#include "vehicle.hpp"

#include "events.hpp"

#include "../world/geometry.hpp"

namespace creatures1::objects {

namespace {

// Vehicle::Tick tests the sign bit of the recovered byte-sized bounds flag
// field before using map ground heights. The executable proves the bit test;
// its original symbolic flag name is not present, so keep the evidence-based
// meaning local to this owner rather than inventing a global Object enum.
constexpr Object::BoundsFlags kVehicleTerrainCollisionFlag = 0x80u;

void add_collision_side(VehicleCollisionSide& flags,
                        VehicleCollisionSide side) {
    flags = static_cast<VehicleCollisionSide>(
        static_cast<std::uint32_t>(flags) |
        static_cast<std::uint32_t>(side));
}

int wrap_vehicle_x_once(int x) {
    if (x < 0) {
        return x + world::kWorldWidth;
    }
    if (x >= world::kWorldWidth) {
        return x - world::kWorldWidth;
    }
    return x;
}

std::size_t ground_block_for_x(int x) {
    // x is normalized into the C1 world before this helper is called.
    return static_cast<std::size_t>(x >> 5);
}

} // namespace

Vehicle::Vehicle(CompoundObjectLifetimeHost* lifetime_host)
    : CompoundObject(lifetime_host) {
    set_bounds_flags(static_cast<BoundsFlags>(bounds_flags() | kIsVehicle));
    creature_event_bounds_local = {};
    velocity_x_8_8 = 0;
    velocity_y_8_8 = 0;
    position_x_8_8 = 0;
    position_y_8_8 = 0;
    collision_side_flags = VehicleCollisionSide::none;
}

Vehicle::Vehicle(std::uint32_t sprite_file_id, int header_record_index,
                 std::uint32_t image_count,
                 CompoundObjectConstructionHost& construction)
    : CompoundObject(sprite_file_id, header_record_index, image_count, false,
                     construction) {
    set_bounds_flags(static_cast<BoundsFlags>(bounds_flags() | kIsVehicle));
    creature_event_bounds_local = {};
    velocity_x_8_8 = 0;
    velocity_y_8_8 = 0;
    position_x_8_8 = 0;
    position_y_8_8 = 0;
    collision_side_flags = VehicleCollisionSide::none;
}

void Vehicle::synchronize_fixed_point_position_and_queue_redraw(
    ObjectMovementBoundsHost& world_host,
    ObjectImmediateEventQueueHost& event_queue) {
    position_x_8_8 = sound_source_x() << 8;
    position_y_8_8 = sound_source_y() << 8;
    queue_event_8_after_bounds_update(world_host, event_queue);
}

void Vehicle::queue_event_4_for_creatures_in_local_bounds(
    VehicleEventHost& event_host) {
    if (part_count() == 0 || part(0).entity == nullptr) {
        return;
    }

    const Entity& primary_entity = *part(0).entity;
    world::WorldRect contact_bounds = creature_event_bounds_local;
    contact_bounds.min_x += primary_entity.world_x();
    contact_bounds.max_x += primary_entity.world_x();
    contact_bounds.min_y += primary_entity.world_y();
    contact_bounds.max_y += primary_entity.world_y();

    for (std::size_t index = 0; index < event_host.creature_count();
         ++index) {
        Object* creature = event_host.creature_at(index);
        if (creature == nullptr) {
            event_host.report_invalid_creature_index();
            continue;
        }
        if (event_host.object_is_bound_to_vehicle(*creature, *this)) {
            continue;
        }

        world::WorldRect creature_bounds{};
        if (!creature->get_bounds(&creature_bounds) ||
            !world::wrapped_world_rects_overlap(contact_bounds,
                                                creature_bounds)) {
            continue;
        }
        event_host.queue_immediate_event(
            *this, *creature, ObjectEventId::event_4, 0);
    }
}

void Vehicle::set_creature_event_bounds_local(
    const world::WorldRect& bounds) {
    creature_event_bounds_local = bounds;
}

void Vehicle::serialize(ObjectArchive& archive) {
    CompoundObject::serialize(archive);

    if (archive.is_loading()) {
        velocity_x_8_8 = archive.read_int32();
        velocity_y_8_8 = archive.read_int32();
        position_x_8_8 = archive.read_int32();
        position_y_8_8 = archive.read_int32();
        archive.read_bytes(&creature_event_bounds_local,
                           sizeof(creature_event_bounds_local));
        collision_side_flags = static_cast<VehicleCollisionSide>(
            archive.read_uint32());
        return;
    }

    archive.write_int32(velocity_x_8_8);
    archive.write_int32(velocity_y_8_8);
    archive.write_int32(position_x_8_8);
    archive.write_int32(position_y_8_8);
    archive.write_bytes(&creature_event_bounds_local,
                        sizeof(creature_event_bounds_local));
    archive.write_uint32(static_cast<std::uint32_t>(collision_side_flags));
}

void Vehicle::tick(VehicleTickHost& host) {
    update_sound(host);

    if (timer_period() > 0) {
        const std::int32_t next_countdown = timer_countdown() - 1;
        set_timer_countdown(next_countdown);
        if (next_countdown < 1) {
            dispatch_script_event(ObjectEventId::event_9, this, false, host);
            set_timer_countdown(timer_period());
        }
    }

    if (velocity_x_8_8 == 0 && velocity_y_8_8 == 0) {
        for (int index = 0; index < part_count(); ++index) {
            Entity* entity = part(static_cast<std::size_t>(index)).entity.get();
            if (entity != nullptr) {
                entity->advance_image_sequence(host);
            }
        }
        return;
    }

    for (int index = 0; index < part_count(); ++index) {
        Entity* entity = part(static_cast<std::size_t>(index)).entity.get();
        if (entity != nullptr) {
            entity->advance_image_sequence_for_moving_vehicle();
        }
    }

    position_x_8_8 += velocity_x_8_8;
    position_y_8_8 += velocity_y_8_8;
    const int new_world_x = wrap_vehicle_x_once(position_x_8_8 >> 8);
    const int new_world_y = position_y_8_8 >> 8;
    const int previous_sound_source_x = sound_source_x();
    const int previous_sound_source_y = sound_source_y();

    if (previous_sound_source_x == new_world_x &&
        previous_sound_source_y == new_world_y) {
        return;
    }

    const VehicleCollisionSide old_collision_flags = collision_side_flags;
    const BoundsFlags bounds_flags_snapshot = bounds_flags();
    collision_side_flags = VehicleCollisionSide::none;

    if (!has_bounds_flag(kUseCurrentMapRoom)) {
        if ((bounds_flags_snapshot & kVehicleTerrainCollisionFlag) != 0) {
            const int ground_height_at_position = host.ground_height_at_x_block(
                ground_block_for_x(new_world_x));
            int right_edge_x = new_world_x + current_visual_width();
            right_edge_x = wrap_vehicle_x_once(right_edge_x);
            const int right_ground_height = host.ground_height_at_x_block(
                ground_block_for_x(right_edge_x));
            const int vehicle_bottom_edge_y =
                current_visual_height() + new_world_y;

            if (vehicle_bottom_edge_y < ground_height_at_position) {
                if (vehicle_bottom_edge_y < right_ground_height) {
                    if (new_world_y < 0) {
                        add_collision_side(collision_side_flags,
                                           VehicleCollisionSide::top);
                    }
                } else {
                    add_collision_side(collision_side_flags,
                                       VehicleCollisionSide::left);
                }
            } else if (vehicle_bottom_edge_y < right_ground_height) {
                add_collision_side(collision_side_flags,
                                   VehicleCollisionSide::right);
            } else {
                add_collision_side(collision_side_flags,
                                   VehicleCollisionSide::bottom);
            }
        }
    } else {
        if (new_world_x < movement_bounds().min_x) {
            collision_side_flags = VehicleCollisionSide::right;
        } else if (movement_bounds().max_x <
                   current_visual_width() + new_world_x) {
            add_collision_side(collision_side_flags,
                               VehicleCollisionSide::left);
        }

        if (new_world_y < movement_bounds().min_y) {
            add_collision_side(collision_side_flags,
                               VehicleCollisionSide::top);
        } else if (movement_bounds().max_y <
                   current_visual_height() + new_world_y) {
            add_collision_side(collision_side_flags,
                               VehicleCollisionSide::bottom);
        }
    }

    if (collision_side_flags != VehicleCollisionSide::none &&
        collision_side_flags != old_collision_flags) {
        velocity_x_8_8 = 0;
        velocity_y_8_8 = 0;
        position_x_8_8 = sound_source_x() << 8;
        position_y_8_8 = sound_source_y() << 8;
        dispatch_script_event(ObjectEventId::event_6, this, false, host);
        return;
    }

    if (previous_sound_source_x == new_world_x &&
        previous_sound_source_y == new_world_y) {
        return;
    }

    const int delta_x = new_world_x - previous_sound_source_x;
    const int delta_y = new_world_y - previous_sound_source_y;
    const std::size_t object_count = host.object_count();
    for (std::size_t index = 0; index < object_count; ++index) {
        Object* candidate = host.object_at(index);
        if (candidate == nullptr) {
            host.report_invalid_index();
            continue;
        }
        if (candidate->bounds_reference_object() != this) {
            continue;
        }
        candidate->move_by(delta_x, delta_y);
        if (index >= host.object_count()) {
            host.report_invalid_index();
            continue;
        }
        Object* moved_candidate = host.object_at(index);
        if (moved_candidate != nullptr) {
            moved_candidate->update_movement_bounds(host);
        }
    }

    move_by_and_redraw(delta_x, delta_y, host);
}

namespace {

void synchronize_fixed_point_position(Vehicle& vehicle) {
    vehicle.position_x_8_8 = vehicle.sound_source_x() << 8;
    vehicle.position_y_8_8 = vehicle.sound_source_y() << 8;
}

void queue_built_in_creature_stimulus(Vehicle& vehicle, const Object& source,
                                      CompoundObjectEventHost& host) {
    QueuedCreatureStimulus stimulus{};
    if (host.copy_built_in_stimulus(source, vehicle, 0, stimulus)) {
        host.queue_creature_stimulus(stimulus);
    }
}

} // namespace

// The shared body of queued events 0 and 1: a creature re-triggering the
// running interaction, or asking for one the object has configured as
// creature-proof, gets the object's creature stimulus instead.
void Vehicle::start_interaction(const QueuedObjectEvent& event,
                                ObjectEventId interaction,
                                std::size_t config_index,
                                CompoundObjectEventHost& host) {
    Object* source = event.source;
    if (source == nullptr) {
        return;
    }
    const bool source_is_creature = host.source_is_creature(*source);
    if (current_interaction_event_id() ==
        static_cast<std::uint32_t>(interaction)) {
        if (!source_is_creature) {
            return;
        }
    } else if (!source_is_creature ||
               creature_event_config.event_config_value[config_index] != -1) {
        set_current_interaction_event_id(
            static_cast<std::uint32_t>(interaction));
        synchronize_fixed_point_position(*this);
        host.dispatch_script_event(*this, source, interaction);
        return;
    }
    queue_built_in_creature_stimulus(*this, *source, host);
}

void Vehicle::handle_queued_event_0(const QueuedObjectEvent& event,
                                    CompoundObjectEventHost& host) {
    start_interaction(event, ObjectEventId::event_1, 0, host);
}

void Vehicle::handle_queued_event_1(const QueuedObjectEvent& event,
                                    CompoundObjectEventHost& host) {
    start_interaction(event, ObjectEventId::event_2, 1, host);
}

void Vehicle::handle_queued_event_2(const QueuedObjectEvent& event,
                                    CompoundObjectEventHost& host,
                                    CompoundObjectMoveRedrawHost& renderer) {
    Object* source = event.source;
    if (source == nullptr) {
        return;
    }
    const bool source_is_creature = host.source_is_creature(*source);
    if (current_interaction_event_id() == 0) {
        if (!source_is_creature) {
            return;
        }
    } else if (!source_is_creature ||
               creature_event_config.event_config_value[2] != -1) {
        complete_floor_arrival(host, renderer);
        return;
    }
    queue_built_in_creature_stimulus(*this, *source, host);
}

void Vehicle::complete_floor_arrival(CompoundObjectEventHost& host,
                                     CompoundObjectMoveRedrawHost& renderer) {
    velocity_x_8_8 = 0;
    velocity_y_8_8 = 0;
    synchronize_fixed_point_position(*this);
    if (current_interaction_event_id() == 0) {
        return;
    }
    set_current_interaction_event_id(0);
    host.dispatch_script_event(*this, this, ObjectEventId::event_0);
    queue_primary_part_dirty_rect(renderer);
}

} // namespace creatures1::objects
