#pragma once

#include "compound_object.hpp"

#include <cstddef>
#include <cstdint>

namespace creatures1::objects {

class Vehicle;

// Vehicle interaction and creature-registry storage belong to the world
// owner. Vehicle keeps the exact contact predicate and event production here.
class VehicleEventHost {
public:
    virtual ~VehicleEventHost() = default;
    virtual std::size_t creature_count() const = 0;
    virtual Object* creature_at(std::size_t index) const = 0;
    virtual void report_invalid_creature_index() const = 0;
    virtual bool object_is_bound_to_vehicle(const Object& creature,
                                            const Vehicle& vehicle) const = 0;
    virtual void queue_immediate_event(Object& source, Object& target,
                                       ObjectEventId event_id,
                                       std::uint32_t argument) = 0;
};

enum class VehicleCollisionSide : std::uint32_t {
    none = 0,
    right = 1,
    left = 2,
    top = 4,
    bottom = 8,
};

// Vehicle::Tick is still game-owned policy, but it consumes services owned
// by the sound, map, script, registry, and renderer subsystems. Multiple
// inheritance here composes those existing narrow boundaries; it does not
// reproduce any MFC or global-runtime object layout.
class VehicleTickHost : public virtual ObjectSoundPlaybackHost,
                        public virtual ObjectMovementBoundsHost,
                        public virtual ObjectScriptDispatchHost,
                        public virtual ObjectRegistryHost,
                        public virtual EntityImageSequenceRenderHost,
                        public virtual CompoundObjectMoveRedrawHost {
public:
    ~VehicleTickHost() override = default;
    virtual int ground_height_at_x_block(std::size_t block_index) const = 0;
};

class Vehicle : public CompoundObject {
public:
    explicit Vehicle(CompoundObjectLifetimeHost* lifetime_host = nullptr);
    Vehicle(std::uint32_t sprite_file_id, int header_record_index,
            std::uint32_t image_count,
            CompoundObjectConstructionHost& construction);
    ~Vehicle() override = default;

    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    void synchronize_fixed_point_position_and_queue_redraw(
        ObjectMovementBoundsHost& world_host,
        ObjectImmediateEventQueueHost& event_queue);
    void queue_event_4_for_creatures_in_local_bounds(
        VehicleEventHost& event_host);
    // CAOS `cabn` owns this local rectangle on Vehicle.  Keep the field
    // mutation behind a named owner operation so the interpreter does not
    // acquire Vehicle layout knowledge.
    void set_creature_event_bounds_local(const world::WorldRect& bounds);
    void tick(VehicleTickHost& host);
    void serialize(ObjectArchive& archive);

    world::WorldRect creature_event_bounds_local{};
    std::int32_t velocity_x_8_8 = 0;
    std::int32_t velocity_y_8_8 = 0;
    std::int32_t position_x_8_8 = 0;
    std::int32_t position_y_8_8 = 0;
    VehicleCollisionSide collision_side_flags =
        VehicleCollisionSide::none;
};

} // namespace creatures1::objects
