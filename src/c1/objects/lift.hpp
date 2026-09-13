#pragma once

#include "vehicle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace creatures1::objects {

class CallButton;
class Lift;

// Lift logic spans the same world-owned services as Vehicle, plus the room
// table and the CallButton registry.  The adapter supplies those services;
// Lift retains only floor selection, movement, and event policy.
class LiftRuntimeHost : public virtual VehicleTickHost,
                        public virtual CompoundObjectEventHost,
                        public virtual ObjectImmediateEventQueueHost {
public:
    ~LiftRuntimeHost() override = default;

    virtual std::size_t room_count() const = 0;
    virtual bool room_contains_point(std::size_t room_index,
                                     int world_x, int world_y) const = 0;
    virtual int room_bottom(std::size_t room_index) const = 0;
    virtual void publish_active_lift(Lift& lift) = 0;

    virtual int call_button_floor(const CallButton& button) const = 0;
    virtual void deactivate_call_button(CallButton& button) = 0;
};

class Lift : public Vehicle {
public:
    static constexpr std::size_t kCallButtonCapacity = 8;

    explicit Lift(CompoundObjectLifetimeHost* lifetime_host = nullptr);

    // `new: lift`, from Macro::ExecuteNewCommand @ 0x0041d130: the Vehicle
    // base is constructed from the same three operands `new: vhcl` takes, and
    // initialize_state runs afterwards through the runtime host.
    Lift(std::uint32_t sprite_file_id, int header_record_index,
         std::uint32_t image_count,
         CompoundObjectConstructionHost& construction);
    ~Lift() override = default;

    void serialize(ObjectArchive& archive);

    void handle_floor_up_event(const QueuedObjectEvent& event,
                               LiftRuntimeHost& host);
    void handle_floor_down_event(const QueuedObjectEvent& event,
                                 LiftRuntimeHost& host);
    void handle_floor_arrival_event(const QueuedObjectEvent& event,
                                    LiftRuntimeHost& host);
    void complete_floor_arrival(LiftRuntimeHost& host);
    void initialize_state(LiftRuntimeHost& host);
    void update_bounds_and_queue_redraw(LiftRuntimeHost& host);
    void tick(LiftRuntimeHost& host);
    void request_move_up(const QueuedObjectEvent& event,
                         LiftRuntimeHost& host);
    void request_move_down(const QueuedObjectEvent& event,
                           LiftRuntimeHost& host);

    std::uint32_t interaction_event() const {
        return current_interaction_event_id();
    }
    void set_interaction_event(std::uint32_t value) {
        set_current_interaction_event_id(value);
    }

    // Implemented by the subsequent CallButton-selection batch. Keeping the
    // declaration here preserves the actual owner boundary and prevents a
    // temporary free-function bucket from being emitted.
    void select_nearest_call_button_and_start_move(LiftRuntimeHost& host);

    std::int32_t floor_count = 0;
    std::int32_t current_floor_index = 0;
    std::int32_t selected_call_button_index = -1;
    std::uint8_t call_selection_cooldown = 0;
    std::array<std::int32_t, kCallButtonCapacity> floor_y_by_index{};
    std::array<CallButton*, kCallButtonCapacity> call_button_refs{};
};

} // namespace creatures1::objects
