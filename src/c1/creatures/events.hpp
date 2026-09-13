#pragma once

#include "../objects/events.hpp"
#include "stimulus.hpp"

#include <cstddef>
#include <cstdint>

namespace creatures1::creatures {

class Creature;

// Registry lookup, perception, built-in stimulus storage, and queue storage
// are concrete runtime boundaries.  These producers own only the C1 fan-out
// predicates and event/stimulus submission policy.
class CreatureEventFanoutHost {
public:
    virtual ~CreatureEventFanoutHost() = default;

    virtual std::size_t creature_count() const = 0;
    virtual Creature* creature_at(std::size_t index) const = 0;
    virtual void report_invalid_creature_index() const = 0;

    virtual int source_sound_source_x(
        const objects::Object& source) const = 0;
    virtual int source_sound_source_y(
        const objects::Object& source) const = 0;
    virtual int down_foot_x(const Creature& creature) const = 0;
    virtual int down_foot_y(const Creature& creature) const = 0;
    virtual bool is_same_object(const Creature& creature,
                                const objects::Object& object) const = 0;
    virtual bool can_perceive(const Creature& creature,
                              const objects::Object& source) const = 0;
    virtual bool source_is_vehicle(const objects::Object& source) const = 0;
    virtual bool creature_is_bound_to_source(
        const Creature& creature,
        const objects::Object& source) const = 0;
    virtual bool is_creature_classifier(const Creature& creature) const = 0;
    virtual objects::Object& object_for_creature(Creature& creature) const = 0;

    virtual void queue_object_event(
        objects::Object& source,
        objects::Object& target,
        objects::ObjectEventId event_id,
        std::uint32_t argument,
        std::int32_t delay_world_ticks) = 0;

    virtual bool copy_built_in_stimulus(
        const Creature& creature,
        std::int32_t stimulus_index,
        objects::Object& source,
        objects::QueuedCreatureStimulus& out) const = 0;
    virtual void queue_creature_stimulus(
        const objects::QueuedCreatureStimulus& stimulus) = 0;
    virtual void report_invalid_stimulus_index(
        std::int32_t stimulus_index) const = 0;
};

void queue_events_in_speech_range(
    objects::Object& source,
    objects::ObjectEventId event_id,
    std::uint32_t event_argument,
    std::int32_t delay_world_ticks,
    CreatureEventFanoutHost& host);

void queue_events_for_perceiving_creatures(
    objects::Object& source,
    objects::ObjectEventId event_id,
    CreatureEventFanoutHost& host);

void queue_sign_stimulus_for_perceiving_creatures(
    objects::Object& source,
    std::int32_t stimulus_index,
    CreatureEventFanoutHost& host);

objects::QueuedCreatureStimulus& copy_queued_creature_stimulus(
    objects::QueuedCreatureStimulus& destination,
    const objects::QueuedCreatureStimulus& source);

void queue_tact_events_for_overlapping_creatures(
    objects::Object& source,
    objects::ObjectEventId event_id,
    CreatureEventFanoutHost& host);

void queue_stimulus_for_perceiving_creatures(
    StimulusContext& stimulus_context,
    objects::Object& target,
    CreatureEventFanoutHost& host);

void queue_stimulus_for_overlapping_creatures(
    StimulusContext& stimulus_context,
    objects::Object& target,
    CreatureEventFanoutHost& host);

void queue_tact_stimulus_for_overlapping_creatures(
    objects::Object& source,
    std::int32_t stimulus_index,
    CreatureEventFanoutHost& host);

// `stm# shou` is an inline walk in the native interpreter rather than a call
// to one of the shared helpers, but its predicate is the same speech-range
// window the event fan-out uses.  Each creature inside the window contributes
// its OWN indexed built-in context; the script owner is only the source.
void queue_built_in_stimulus_in_speech_range(
    objects::Object& source,
    std::int32_t stimulus_index,
    CreatureEventFanoutHost& host);

// `stm# writ` delivers one creature's indexed built-in context to that same
// creature, with the script owner as the source object.
void queue_built_in_stimulus_for_creature(
    objects::Object& source,
    Creature& target,
    std::int32_t stimulus_index,
    CreatureEventFanoutHost& host);

// `stim shou` is the descriptor-carrying counterpart of the walk above: the
// caller supplies the whole stimulus record instead of an index into the
// target's built-in table.
void queue_stimulus_in_speech_range(
    StimulusContext& stimulus_context,
    objects::Object& source,
    CreatureEventFanoutHost& host);

// `stim writ` and `stim from` deliver the supplied record to one creature.
void queue_stimulus_for_creature(
    StimulusContext& stimulus_context,
    objects::Object& source,
    Creature& target,
    CreatureEventFanoutHost& host);

} // namespace creatures1::creatures
