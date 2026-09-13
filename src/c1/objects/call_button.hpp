#pragma once

#include "simple_object.hpp"
#include "compound_object.hpp"

#include <cstddef>

namespace creatures1::objects {

class Lift;

// CallButton's two runtime methods touch the room table, Object movement
// bounds, the immediate event ring, and the Creature/CAOS boundaries.  Those
// owners are composed here so the source method does not depend on the native
// global arrays or MFC object layout.
class CallButtonRuntimeHost : public virtual ObjectMovementBoundsHost,
                              public virtual ObjectImmediateEventQueueHost,
                              public virtual ObjectScriptDispatchHost,
                              public virtual CompoundObjectEventHost {
public:
    ~CallButtonRuntimeHost() override = default;

    virtual std::size_t room_count() const = 0;
    virtual bool room_contains_point(std::size_t room_index,
                                     int world_x, int world_y) const = 0;
    virtual int room_bottom(std::size_t room_index) const = 0;
};

// A CallButton is a SimpleObject with one Lift relationship and the compact
// floor slot assigned when it is inserted into that Lift's call table.
class CallButton : public SimpleObject {
public:
    CallButton() = default;

    // `new: cbtn`, from Macro::ExecuteNewCommand @ 0x0041d130.  The recovered
    // SimpleObject arguments are literal: origin position, bounds flags 4,
    // classifier 0x02020000 (family 2, genus 2), click selector 0, and the
    // event-1 interaction flag.  The button then starts unassigned to a floor
    // and adopts whatever Lift is currently active.
    CallButton(std::uint32_t object_file_id, int header_record_index,
               std::uint32_t image_count, int render_plane, Lift* active_lift,
               SimpleObjectConstructionHost& construction);

    ~CallButton() override = default;

    void serialize(ObjectArchive& archive);
    void update_lift_state_and_queue_redraw(CallButtonRuntimeHost& host);
    void request_lift_call(const QueuedObjectEvent& event,
                           CallButtonRuntimeHost& host);

    // Lift::Tick @ 0x0042c4f0 releases the button it answered: it clears the
    // interaction event id and then dispatches script EVENT_0 on the button
    // with the button itself as the from-object and no restart.  Both steps
    // are the button's own state transition, so they live here rather than in
    // the lift's runtime host, which cannot reach the protected setter.
    void deactivate(ObjectScriptDispatchHost& scripts);

    Lift* lift() const { return lift_; }
    void set_lift(Lift* lift) { lift_ = lift; }

    std::uint8_t floor_index = 0xff;

private:
    Lift* lift_ = nullptr;
};

} // namespace creatures1::objects
