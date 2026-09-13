#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "object.hpp"
#include "../creatures/stimulus.hpp"

namespace creatures1::creatures {
class Creature;
}

namespace creatures1::objects {

struct QueuedObjectEvent {
    Object* source = nullptr;
    Object* target = nullptr;
    ObjectEventId event_id = ObjectEventId::event_0;
    std::uint32_t argument = 0;
    std::uint32_t reserved_zero = 0;
    std::int32_t due_world_tick = 0;
};

struct QueuedCreatureStimulus {
    Object* source_object = nullptr;
    creatures1::creatures::Creature* target_creature = nullptr;
    creatures1::creatures::StimulusDescriptor descriptor{};
    creatures1::creatures::StimulusChemicalIds chemical_ids{};
    creatures1::creatures::StimulusChemicalAmounts chemical_amounts{};
};

static_assert(sizeof(QueuedObjectEvent) == 24);
static_assert(sizeof(QueuedCreatureStimulus) == 20);

inline constexpr std::size_t kObjectEventQueueCapacity = 200;
using ObjectEventQueue = std::array<QueuedObjectEvent, kObjectEventQueueCapacity>;
using CreatureStimulusQueue =
    std::array<QueuedCreatureStimulus, kObjectEventQueueCapacity>;

// Queue-reset helpers retain the queued records and invalidate only their
// due markers.  They are used for both the immediate and delayed storage
// during world-state reset.
void clear_immediate_object_event_queue(ObjectEventQueue& queue);
void clear_delayed_object_event_queue(ObjectEventQueue& queue);

// This interface is the concrete Object/Creature hierarchy boundary.  The
// scheduler owns ordering, capacity, and tick policy; object classes own the
// event behavior selected by each event id.
class ObjectEventRuntime {
public:
    virtual ~ObjectEventRuntime() = default;

    virtual bool object_is_tick_enabled(const Object&) const = 0;
    virtual void handle_event_0(Object&, const QueuedObjectEvent&) = 0;
    virtual void handle_event_1(Object&, const QueuedObjectEvent&) = 0;
    virtual void handle_event_2(Object&, const QueuedObjectEvent&) = 0;
    virtual void handle_event_3(Object&, const QueuedObjectEvent&) = 0;
    virtual void handle_event_4(Object&, const QueuedObjectEvent&) = 0;
    virtual void handle_event_5(Object&, const QueuedObjectEvent&) = 0;
    virtual void handle_event_6(Object&, const QueuedObjectEvent&) = 0;
    virtual void handle_event_7(Object&, const QueuedObjectEvent&) = 0;
    virtual void handle_event_8(Object&) = 0;
    virtual void handle_event_9(Object&) = 0;

    virtual bool creature_is_tick_enabled(
        const creatures1::creatures::Creature&) const = 0;
    virtual void apply_stimulus(
        creatures1::creatures::Creature& target,
        Object* source_object,
        creatures1::creatures::Creature& source_creature,
        const creatures1::creatures::StimulusDescriptor& descriptor,
        const creatures1::creatures::StimulusChemicalIds& chemical_ids,
        const creatures1::creatures::StimulusChemicalAmounts& chemical_amounts,
        std::uint32_t magnitude) = 0;
};

class ObjectEventScheduler {
public:
    void queue_object_event(Object* source,
                            Object* target,
                            ObjectEventId event_id,
                            std::uint32_t argument,
                            std::uint32_t reserved_abi_word,
                            std::int32_t delay_world_ticks,
                            std::int32_t world_tick);

    void queue_creature_stimulus(const QueuedCreatureStimulus& stimulus);

    // Processes immediate events first, then one due delayed event at a time,
    // and finally the queued creature stimuli.  This repeats until all work
    // currently eligible for the tick has been consumed.
    void process_queued_events(ObjectEventRuntime& runtime,
                               std::int32_t world_tick);

    // Permanent object deletion removes matching records while preserving
    // the FIFO order of the live ring and the active count of delayed slots.
    void purge_object_references(const Object& object);

    void clear_due_ticks(ObjectEventQueue& queue);

    // Read-only lifetime inspection used by SFCDoc's save/delete policy.
    // The scheduler remains the sole owner of queue storage and mutation.
    std::size_t immediate_event_count() const;
    const QueuedObjectEvent* immediate_event_at(std::size_t index) const;
    std::size_t delayed_event_count() const {
        return static_cast<std::size_t>(delayed_event_count_);
    }
    const QueuedObjectEvent* delayed_event_at(std::size_t index) const;
    std::size_t queued_stimulus_count() const;
    const QueuedCreatureStimulus* queued_stimulus_at(
        std::size_t index) const;
    bool delayed_event_is_active(const QueuedObjectEvent& event) const {
        return event.due_world_tick != 0;
    }

private:
    bool take_due_delayed_event(QueuedObjectEvent*& event,
                                std::int32_t world_tick);
    void dispatch_object_event(ObjectEventRuntime& runtime,
                               QueuedObjectEvent& event);
    void drain_creature_stimuli(ObjectEventRuntime& runtime);

    ObjectEventQueue immediate_events_{};
    ObjectEventQueue delayed_events_{};
    CreatureStimulusQueue creature_stimuli_{};
    std::size_t immediate_read_ = 0;
    std::size_t immediate_write_ = 0;
    std::size_t stimulus_read_ = 0;
    std::size_t stimulus_write_ = 0;
    std::int32_t delayed_event_count_ = 0;
};

} // namespace creatures1::objects
