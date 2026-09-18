#include "events.hpp"

namespace creatures1::objects {

void ObjectEventScheduler::queue_object_event(
    Object* source,
    Object* target,
    ObjectEventId event_id,
    std::uint32_t argument,
    std::uint32_t reserved_abi_word,
    std::int32_t delay_world_ticks,
    std::int32_t world_tick) {
    // The original producer silently drops malformed or full submissions.
    if (source == nullptr || target == nullptr) {
        return;
    }

    // This fifth word is preserved by the original call ABI but is not part
    // of the queued record; the producer writes a zero into that field.
    static_cast<void>(reserved_abi_word);

    if (delay_world_ticks == 0) {
        const std::size_t next_write =
            (immediate_write_ + 1) % kObjectEventQueueCapacity;
        if (next_write == immediate_read_) {
            return;
        }

        QueuedObjectEvent& slot = immediate_events_[immediate_write_];
        slot.source = source;
        slot.target = target;
        slot.event_id = event_id;
        slot.argument = argument;
        slot.reserved_zero = 0;
        slot.due_world_tick = 0;
        immediate_write_ = next_write;
        return;
    }

    if (delayed_event_count_ ==
        static_cast<std::int32_t>(kObjectEventQueueCapacity)) {
        return;
    }

    for (std::size_t index = 0; index < kObjectEventQueueCapacity; ++index) {
        QueuedObjectEvent& slot = delayed_events_[index];
        if (slot.due_world_tick != 0) {
            continue;
        }

        slot.source = source;
        slot.target = target;
        slot.event_id = event_id;
        slot.argument = argument;
        slot.reserved_zero = 0;
        slot.due_world_tick = world_tick + delay_world_ticks;
        ++delayed_event_count_;
        return;
    }
}

void ObjectEventScheduler::queue_creature_stimulus(
    const QueuedCreatureStimulus& stimulus) {
    const std::size_t next_write =
        (stimulus_write_ + 1) % kObjectEventQueueCapacity;
    if (next_write == stimulus_read_) {
        return;
    }
    creature_stimuli_[stimulus_write_] = stimulus;
    stimulus_write_ = next_write;
}

bool ObjectEventScheduler::take_due_delayed_event(
    QueuedObjectEvent*& event,
    std::int32_t world_tick) {
    std::int32_t remaining = delayed_event_count_;
    for (std::size_t index = 0;
         index < kObjectEventQueueCapacity && remaining > 0;
         ++index) {
        QueuedObjectEvent& candidate = delayed_events_[index];
        const auto due_tick = static_cast<std::uint32_t>(candidate.due_world_tick);
        if (candidate.due_world_tick == 0 ||
            static_cast<std::uint32_t>(world_tick) < due_tick) {
            --remaining;
            continue;
        }

        --delayed_event_count_;
        candidate.due_world_tick = 0;
        event = &candidate;
        return true;
    }
    return false;
}

void ObjectEventScheduler::dispatch_object_event(
    ObjectEventRuntime& runtime,
    QueuedObjectEvent& event) {
    if (event.target == nullptr ||
        !runtime.object_is_tick_enabled(*event.target)) {
        return;
    }

    switch (event.event_id) {
    case ObjectEventId::event_0:
        runtime.handle_event_0(*event.target, event);
        break;
    case ObjectEventId::event_1:
        runtime.handle_event_1(*event.target, event);
        break;
    case ObjectEventId::event_2:
        runtime.handle_event_2(*event.target, event);
        break;
    case ObjectEventId::event_3:
        runtime.handle_event_3(*event.target, event);
        break;
    case ObjectEventId::event_4:
        runtime.handle_event_4(*event.target, event);
        break;
    case ObjectEventId::event_5:
        runtime.handle_event_5(*event.target, event);
        break;
    case ObjectEventId::event_6:
        runtime.handle_event_6(*event.target, event);
        break;
    case ObjectEventId::event_7:
        runtime.handle_event_7(*event.target, event);
        break;
    case ObjectEventId::event_8:
        runtime.handle_event_8(*event.target);
        break;
    case ObjectEventId::event_9:
        runtime.handle_event_9(*event.target);
        break;
    }
}

void ObjectEventScheduler::drain_creature_stimuli(
    ObjectEventRuntime& runtime) {
    while (stimulus_read_ != stimulus_write_) {
        QueuedCreatureStimulus& stimulus = creature_stimuli_[stimulus_read_];
        stimulus_read_ = (stimulus_read_ + 1) % kObjectEventQueueCapacity;

        if (stimulus.target_creature == nullptr ||
            !runtime.creature_is_tick_enabled(*stimulus.target_creature)) {
            continue;
        }

        runtime.apply_stimulus(
            *stimulus.target_creature,
            stimulus.source_object,
            *stimulus.target_creature,
            stimulus.descriptor,
            stimulus.chemical_ids,
            stimulus.chemical_amounts,
            0);
    }
}

void ObjectEventScheduler::process_queued_events(
    ObjectEventRuntime& runtime,
    std::int32_t world_tick) {
    while (true) {
        if (immediate_read_ != immediate_write_) {
            QueuedObjectEvent& event = immediate_events_[immediate_read_];
            immediate_read_ =
                (immediate_read_ + 1) % kObjectEventQueueCapacity;
            dispatch_object_event(runtime, event);
            continue;
        }

        QueuedObjectEvent* delayed_event = nullptr;
        if (take_due_delayed_event(delayed_event, world_tick)) {
            dispatch_object_event(runtime, *delayed_event);
            continue;
        }

        drain_creature_stimuli(runtime);
        return;
    }
}

void ObjectEventScheduler::purge_object_references(
    const Object& object, const creatures1::creatures::Creature* creature) {
    ObjectEventQueue retained_immediate{};
    std::size_t retained_count = 0;
    for (std::size_t cursor = immediate_read_; cursor != immediate_write_;
         cursor = (cursor + 1) % kObjectEventQueueCapacity) {
        const QueuedObjectEvent& event = immediate_events_[cursor];
        if (event.source == &object || event.target == &object) {
            continue;
        }
        retained_immediate[retained_count++] = event;
    }

    for (std::size_t index = 0; index < retained_count; ++index) {
        immediate_events_[(immediate_read_ + index) %
                          kObjectEventQueueCapacity] =
            retained_immediate[index];
    }
    immediate_write_ = (immediate_read_ + retained_count) %
                       kObjectEventQueueCapacity;

    for (QueuedObjectEvent& event : delayed_events_) {
        if (event.due_world_tick == 0 ||
            (event.source != &object && event.target != &object)) {
            continue;
        }
        event.due_world_tick = 0;
        --delayed_event_count_;
    }

    CreatureStimulusQueue retained_stimuli{};
    std::size_t retained_stimulus_count = 0;
    for (std::size_t cursor = stimulus_read_; cursor != stimulus_write_;
         cursor = (cursor + 1) % kObjectEventQueueCapacity) {
        const QueuedCreatureStimulus& stimulus = creature_stimuli_[cursor];
        const bool targets_deleted_object =
            creature != nullptr && stimulus.target_creature == creature;
        if (stimulus.source_object == &object || targets_deleted_object) {
            continue;
        }
        retained_stimuli[retained_stimulus_count++] = stimulus;
    }

    for (std::size_t index = 0; index < retained_stimulus_count; ++index) {
        creature_stimuli_[(stimulus_read_ + index) %
                          kObjectEventQueueCapacity] =
            retained_stimuli[index];
    }
    stimulus_write_ = (stimulus_read_ + retained_stimulus_count) %
                      kObjectEventQueueCapacity;
}

void clear_immediate_object_event_queue(ObjectEventQueue& queue) {
    for (QueuedObjectEvent& event : queue) {
        event.due_world_tick = 0;
    }
}

void clear_delayed_object_event_queue(ObjectEventQueue& queue) {
    clear_immediate_object_event_queue(queue);
}

std::size_t ObjectEventScheduler::immediate_event_count() const {
    if (immediate_write_ >= immediate_read_) {
        return immediate_write_ - immediate_read_;
    }
    return kObjectEventQueueCapacity - immediate_read_ + immediate_write_;
}

const QueuedObjectEvent* ObjectEventScheduler::immediate_event_at(
    std::size_t index) const {
    if (index >= immediate_event_count()) {
        return nullptr;
    }
    return &immediate_events_[(immediate_read_ + index) %
                              kObjectEventQueueCapacity];
}

const QueuedObjectEvent* ObjectEventScheduler::delayed_event_at(
    std::size_t index) const {
    if (index >= delayed_event_count()) {
        return nullptr;
    }
    std::size_t live_index = 0;
    for (const QueuedObjectEvent& event : delayed_events_) {
        if (!delayed_event_is_active(event)) {
            continue;
        }
        if (live_index++ == index) {
            return &event;
        }
    }
    return nullptr;
}

const QueuedCreatureStimulus* ObjectEventScheduler::queued_stimulus_at(
    std::size_t index) const {
    if (index >= queued_stimulus_count()) {
        return nullptr;
    }
    return &creature_stimuli_[(stimulus_read_ + index) %
                              kObjectEventQueueCapacity];
}

std::size_t ObjectEventScheduler::queued_stimulus_count() const {
    if (stimulus_write_ >= stimulus_read_) {
        return stimulus_write_ - stimulus_read_;
    }
    return kObjectEventQueueCapacity - stimulus_read_ + stimulus_write_;
}

} // namespace creatures1::objects
