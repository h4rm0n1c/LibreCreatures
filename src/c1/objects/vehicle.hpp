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
                        public virtual CompoundObjectMoveRedrawHost,
                        public virtual ObjectRenderableSetHost {
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

    // Queued events 0, 1 and 2 (vtable slots 5-7, 0042c0b0 / 0042c170 /
    // 0042c230).  Unlike CompoundObject's, starting an interaction snaps the
    // fixed-point position to the object first, and a failed script does not
    // fall back to the creature stimulus.  Lift overrides 0 and 1.
    void handle_queued_event_0(const QueuedObjectEvent& event,
                               CompoundObjectEventHost& host);
    void handle_queued_event_1(const QueuedObjectEvent& event,
                               CompoundObjectEventHost& host);
    void handle_queued_event_2(const QueuedObjectEvent& event,
                               CompoundObjectEventHost& host,
                               CompoundObjectMoveRedrawHost& renderer);
    // 0042c2c0: stop, resynchronise, and if an interaction was running end
    // it with script event 0.  Lift's tick ends each floor stop with it.
    void complete_floor_arrival(CompoundObjectEventHost& host,
                                CompoundObjectMoveRedrawHost& renderer);

    world::WorldRect creature_event_bounds_local{};
    std::int32_t velocity_x_8_8 = 0;
    std::int32_t velocity_y_8_8 = 0;
    std::int32_t position_x_8_8 = 0;
    std::int32_t position_y_8_8 = 0;
    VehicleCollisionSide collision_side_flags =
        VehicleCollisionSide::none;

private:
    void start_interaction(const QueuedObjectEvent& event,
                           ObjectEventId interaction, std::size_t config_index,
                           CompoundObjectEventHost& host);
};

} // namespace creatures1::objects
