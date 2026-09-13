#include "events.hpp"

#include <cstdlib>

namespace creatures1::creatures {
namespace {

bool rectangles_overlap(const world::WorldRect& first,
                        const world::WorldRect& second) {
    return first.min_x < second.max_x && second.min_x < first.max_x &&
           first.min_y < second.max_y && second.min_y < first.max_y;
}

bool target_is_in_tactile_region(
    const objects::Object& source,
    Creature& target,
    const world::WorldRect& source_bounds,
    CreatureEventFanoutHost& host) {
    if (host.source_is_vehicle(source) &&
        host.creature_is_bound_to_source(target, source)) {
        return true;
    }

    world::WorldRect target_bounds{};
    objects::Object& target_object = host.object_for_creature(target);
    target_object.get_bounds(&target_bounds);
    return rectangles_overlap(source_bounds, target_bounds);
}

void queue_stimulus_context(
    const StimulusContext& context,
    CreatureEventFanoutHost& host) {
    objects::QueuedCreatureStimulus queued{};
    queued.source_object = context.target;
    queued.target_creature = context.source_creature;
    queued.descriptor = context.descriptor;
    queued.chemical_ids = context.chemical_ids;
    queued.chemical_amounts = context.chemical_amounts;

    objects::QueuedCreatureStimulus copied{};
    copy_queued_creature_stimulus(copied, queued);
    host.queue_creature_stimulus(copied);
}

} // namespace

void queue_events_in_speech_range(
    objects::Object& source,
    objects::ObjectEventId event_id,
    std::uint32_t event_argument,
    std::int32_t delay_world_ticks,
    CreatureEventFanoutHost& host) {
    const int source_x = host.source_sound_source_x(source);
    const int source_y = host.source_sound_source_y(source);

    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        Creature* target = host.creature_at(index);
        if (target == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }
        if (host.is_same_object(*target, source)) {
            continue;
        }

        const int target_x = host.down_foot_x(*target);
        const int target_y = host.down_foot_y(*target);
        if (std::abs(source_x - target_x) < 500 &&
            source_y > target_y - 200 && source_y < target_y + 32) {
            host.queue_object_event(
                source, host.object_for_creature(*target), event_id,
                event_argument, delay_world_ticks);
        }
    }
}

void queue_events_for_perceiving_creatures(
    objects::Object& source,
    objects::ObjectEventId event_id,
    CreatureEventFanoutHost& host) {
    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        Creature* target = host.creature_at(index);
        if (target == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }
        if (host.is_same_object(*target, source) ||
            !host.can_perceive(*target, source)) {
            continue;
        }

        host.queue_object_event(
            source, host.object_for_creature(*target), event_id, 0, 0);
    }
}

void queue_sign_stimulus_for_perceiving_creatures(
    objects::Object& source,
    std::int32_t stimulus_index,
    CreatureEventFanoutHost& host) {
    // Native code tests only `stimulus_index < 0x24`; retain the signed
    // contract here because negative indices are passed through to the
    // recovered built-in-context address calculation.
    if (stimulus_index >= 0x24) {
        host.report_invalid_stimulus_index(stimulus_index);
        return;
    }

    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        Creature* target = host.creature_at(index);
        if (target == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }
        if (!host.can_perceive(*target, source)) {
            continue;
        }

        objects::QueuedCreatureStimulus queued_stimulus;
        if (host.copy_built_in_stimulus(
                *target, stimulus_index, source, queued_stimulus) &&
            host.is_creature_classifier(*target)) {
            host.queue_creature_stimulus(queued_stimulus);
        }
    }
}

objects::QueuedCreatureStimulus& copy_queued_creature_stimulus(
    objects::QueuedCreatureStimulus& destination,
    const objects::QueuedCreatureStimulus& source) {
    if (&destination != &source) {
        destination = source;
    }
    return destination;
}

void queue_tact_events_for_overlapping_creatures(
    objects::Object& source,
    objects::ObjectEventId event_id,
    CreatureEventFanoutHost& host) {
    world::WorldRect source_bounds{};
    source.get_bounds(&source_bounds);

    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        Creature* target = host.creature_at(index);
        if (target == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }
        if (host.is_same_object(*target, source) ||
            !target_is_in_tactile_region(source, *target, source_bounds,
                                         host)) {
            continue;
        }

        host.queue_object_event(
            source, host.object_for_creature(*target), event_id, 0, 0);
    }
}

void queue_stimulus_for_perceiving_creatures(
    StimulusContext& stimulus_context,
    objects::Object& target,
    CreatureEventFanoutHost& host) {
    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        Creature* perceiving_creature = host.creature_at(index);
        if (perceiving_creature == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }
        if (host.is_same_object(*perceiving_creature, target) ||
            !host.can_perceive(*perceiving_creature, target) ||
            !host.is_creature_classifier(*perceiving_creature)) {
            continue;
        }

        stimulus_context.target = &target;
        stimulus_context.source_creature = perceiving_creature;
        queue_stimulus_context(stimulus_context, host);
    }
}

void queue_stimulus_for_overlapping_creatures(
    StimulusContext& stimulus_context,
    objects::Object& target,
    CreatureEventFanoutHost& host) {
    world::WorldRect target_bounds{};
    target.get_bounds(&target_bounds);

    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        Creature* target_creature = host.creature_at(index);
        if (target_creature == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }
        if (host.is_same_object(*target_creature, target) ||
            !target_is_in_tactile_region(target, *target_creature,
                                         target_bounds, host) ||
            !host.is_creature_classifier(*target_creature)) {
            continue;
        }

        stimulus_context.target = &target;
        stimulus_context.source_creature = target_creature;
        queue_stimulus_context(stimulus_context, host);
    }
}

void queue_tact_stimulus_for_overlapping_creatures(
    objects::Object& source,
    std::int32_t stimulus_index,
    CreatureEventFanoutHost& host) {
    // Native code tests only `stimulus_index < 0x24`; retain the signed
    // contract here for the same reason as the sign helper.
    if (stimulus_index >= 0x24) {
        host.report_invalid_stimulus_index(stimulus_index);
        return;
    }

    for (std::size_t source_index = 0;
         source_index < host.creature_count(); ++source_index) {
        Creature* target_creature = host.creature_at(source_index);
        if (target_creature == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }

        world::WorldRect source_bounds{};
        source.get_bounds(&source_bounds);
        for (std::size_t candidate_index = 0;
             candidate_index < host.creature_count(); ++candidate_index) {
            Creature* candidate = host.creature_at(candidate_index);
            if (candidate == nullptr) {
                host.report_invalid_creature_index();
                continue;
            }
            if (host.is_same_object(*candidate, source) ||
                !target_is_in_tactile_region(source, *candidate,
                                             source_bounds, host)) {
                continue;
            }

            objects::QueuedCreatureStimulus queued{};
            // The outer creature is the native `target_creature`: its
            // indexed built-in context is copied after the inner candidate
            // proves that the source overlaps something.  The candidate is
            // not the stimulus-record owner.
            if (host.copy_built_in_stimulus(
                    *target_creature, stimulus_index, source, queued) &&
                host.is_creature_classifier(*target_creature)) {
                host.queue_creature_stimulus(queued);
            }
            break;
        }
    }
}

namespace {

// The shared speech-range window: |dx| < 500 horizontally, and the source's
// sound-source y strictly inside (foot_y - 200, foot_y + 32).
bool target_is_in_speech_range(int source_x, int source_y,
                               const Creature& target,
                               CreatureEventFanoutHost& host) {
    const int target_x = host.down_foot_x(target);
    const int target_y = host.down_foot_y(target);
    return std::abs(source_x - target_x) < 500 &&
           source_y > target_y - 200 && source_y < target_y + 32;
}

} // namespace

void queue_built_in_stimulus_in_speech_range(
    objects::Object& source,
    std::int32_t stimulus_index,
    CreatureEventFanoutHost& host) {
    // Native tests only the upper bound, exactly as the sign and tact helpers
    // do, and reports through the same diagnostic.
    if (stimulus_index >= 0x24) {
        host.report_invalid_stimulus_index(stimulus_index);
        return;
    }

    const int source_x = host.source_sound_source_x(source);
    const int source_y = host.source_sound_source_y(source);

    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        Creature* target = host.creature_at(index);
        if (target == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }
        if (host.is_same_object(*target, source) ||
            !target_is_in_speech_range(source_x, source_y, *target, host)) {
            continue;
        }

        objects::QueuedCreatureStimulus queued{};
        if (host.copy_built_in_stimulus(*target, stimulus_index, source,
                                        queued) &&
            host.is_creature_classifier(*target)) {
            host.queue_creature_stimulus(queued);
        }
    }
}

void queue_built_in_stimulus_for_creature(
    objects::Object& source,
    Creature& target,
    std::int32_t stimulus_index,
    CreatureEventFanoutHost& host) {
    objects::QueuedCreatureStimulus queued{};
    if (host.copy_built_in_stimulus(target, stimulus_index, source, queued) &&
        host.is_creature_classifier(target)) {
        host.queue_creature_stimulus(queued);
    }
}

void queue_stimulus_in_speech_range(
    StimulusContext& stimulus_context,
    objects::Object& source,
    CreatureEventFanoutHost& host) {
    const int source_x = host.source_sound_source_x(source);
    const int source_y = host.source_sound_source_y(source);

    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        Creature* target = host.creature_at(index);
        if (target == nullptr) {
            host.report_invalid_creature_index();
            continue;
        }
        if (host.is_same_object(*target, source) ||
            !target_is_in_speech_range(source_x, source_y, *target, host) ||
            !host.is_creature_classifier(*target)) {
            continue;
        }

        stimulus_context.target = &source;
        stimulus_context.source_creature = target;
        queue_stimulus_context(stimulus_context, host);
    }
}

void queue_stimulus_for_creature(
    StimulusContext& stimulus_context,
    objects::Object& source,
    Creature& target,
    CreatureEventFanoutHost& host) {
    if (!host.is_creature_classifier(target)) {
        return;
    }
    stimulus_context.target = &source;
    stimulus_context.source_creature = &target;
    queue_stimulus_context(stimulus_context, host);
}

} // namespace creatures1::creatures
