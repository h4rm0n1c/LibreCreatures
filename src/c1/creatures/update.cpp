#include "update.hpp"

#include "../brain/lobe.hpp"
#include "../objects/object.hpp"
#include "creature.hpp"

#include <algorithm>
#include <cstdlib>

namespace creatures1::creatures {

void update_all_creature_drive_threshold_states(
    NonSceneryObjectRegistryView registry) {
    std::size_t object_index = 0;
    std::size_t object_count_snapshot = registry.object_count;

    while (object_index < object_count_snapshot) {
        DriveThresholdObject& object = *registry.objects[object_index];
        if (object.tick_enabled()) {
            object.update_drive_threshold_state();
            object_count_snapshot = registry.object_count;
        }
        ++object_index;
    }
}

namespace {

constexpr std::uint32_t kBoundaryCorrectionStimulus = 7;
constexpr std::uint32_t kAttentionPartIndex = 0;
constexpr std::uint32_t kMotionEventNeuronIndex = 13;
constexpr std::uint32_t kMotionDistanceNeuronIndex = 12;
constexpr std::uint32_t kAttentionNeuronLobeIndex = 7;
constexpr std::uint32_t kGeneralSensoryLobeIndex = 5;
constexpr std::uint32_t kDriveLobeIndex = 1;

bool contains_point(const world::WorldRect& bounds, int x, int y) {
    // This is the Win32 PtInRect convention used by the executable: right
    // and bottom edges are outside the rectangle.
    return bounds.min_x <= x && x < bounds.max_x &&
           bounds.min_y <= y && y < bounds.max_y;
}

} // namespace

void update_all_creature_brain_inputs(CreatureWorldUpdateHost& host) {
    std::size_t creature_index = 0;
    std::size_t creature_count_snapshot = host.creature_count();

    while (creature_index < creature_count_snapshot) {
        Creature* creature = host.creature_at(creature_index);
        if (creature != nullptr &&
            host.creature_is_tick_enabled(*creature) &&
            host.creature_is_alive(*creature)) {
            brain::Brain* creature_brain = creature->brain();
            if (creature_brain != nullptr) {
                brain::Lobe& general_sensory = creature_brain->lobe(
                    kGeneralSensoryLobeIndex);
                objects::Object* linked_object = host.motion_link(*creature);
                if (linked_object != nullptr) {
                    if (general_sensory.neuron_count_value() >
                        kMotionEventNeuronIndex) {
                        general_sensory.neuron(kMotionEventNeuronIndex)
                            .activation = static_cast<std::uint8_t>(
                                host.object_has_interaction_event(
                                    *linked_object)
                                    ? 0xff
                                    : 0);
                    }

                    const int horizontal_distance = std::abs(
                        host.sound_source_x(*creature) -
                        host.motion_target_x(*creature));
                    if (horizontal_distance < 0x80 &&
                        general_sensory.neuron_count_value() >
                            kMotionDistanceNeuronIndex) {
                        const int activation = std::clamp(
                            0xff - (horizontal_distance * 2), 0, 0xff);
                        general_sensory.neuron(kMotionDistanceNeuronIndex)
                            .activation = static_cast<std::uint8_t>(
                                activation);
                    }
                }

                if (host.boundary_correction_pending(*creature)) {
                    host.clear_boundary_correction(*creature);
                    if (host.bounds_reference(*creature) == nullptr) {
                        host.trigger_built_in_stimulus(
                            *creature, kBoundaryCorrectionStimulus, 0);
                    }
                }

                brain::Lobe& drive_lobe =
                    creature_brain->lobe(kDriveLobeIndex);
                const auto& goal_levels =
                    creature->control_state().goal_direction_drive_levels;
                for (std::size_t neuron_index = 0;
                     neuron_index < goal_levels.size(); ++neuron_index) {
                    if (drive_lobe.neuron_count_value() > neuron_index) {
                        drive_lobe.neuron(static_cast<std::uint32_t>(
                                             neuron_index))
                            .activation = goal_levels[neuron_index];
                    }
                }
            }
        }

        ++creature_index;
        creature_count_snapshot = host.creature_count();
    }
}

void update_all_creature_perception_and_attention(
    CreatureWorldUpdateHost& host) {
    std::size_t creature_index = 0;
    std::size_t creature_count_snapshot = host.creature_count();

    while (creature_index < creature_count_snapshot) {
        Creature* creature = host.creature_at(creature_index);
        if (creature != nullptr &&
            host.creature_is_tick_enabled(*creature)) {
            host.update_perception(*creature);
            host.update_attention(*creature);
            creature_count_snapshot = host.creature_count();

            if (host.creature_is_alive(*creature)) {
                world::WorldRect attention_bounds =
                    host.movement_bounds(*creature);
                objects::Object* bounds_object =
                    host.bounds_reference(*creature);
                if (bounds_object != nullptr &&
                    host.reference_uses_own_attention_bounds(*bounds_object)) {
                    attention_bounds = host.movement_bounds(*bounds_object);
                }

                // Attention slot zero is reserved for the pointer/tool path;
                // the native cleanup loop begins at slot one.
                auto& attention_records = creature->attention_records();
                for (std::size_t record_index = 1;
                     record_index < attention_records.size(); ++record_index) {
                    Creature::AttentionRecord& record =
                        attention_records[record_index];
                    objects::Object* target = record.target;
                    if (target != nullptr) {
                        int target_x = 0;
                        int target_y = 0;
                        host.object_part_center(*target,
                                                kAttentionPartIndex,
                                                target_x,
                                                target_y);
                        if (!contains_point(attention_bounds, target_x,
                                            target_y)) {
                            if (host.should_log_attention_loss(*creature)) {
                                host.log_attention_target_out_of_reach(
                                    *creature, *target);
                                host.log_attention_target_inaccessible(
                                    *creature, *target);
                            }

                            record.target = nullptr;
                            record.visible = false;
                            brain::Brain* creature_brain = creature->brain();
                            if (creature_brain != nullptr) {
                                brain::Lobe& attention_lobe =
                                    creature_brain->lobe(
                                        kAttentionNeuronLobeIndex);
                                if (attention_lobe.neuron_count_value() >
                                    record_index) {
                                    brain::LobeNeuron& neuron =
                                        attention_lobe.neuron(
                                            static_cast<std::uint32_t>(
                                                record_index));
                                    neuron.firing_strength = 0;
                                    neuron.activation = 0;
                                }
                            }
                        }
                    }
                    creature_count_snapshot = host.creature_count();
                }
            }
        }

        ++creature_index;
        creature_count_snapshot = host.creature_count();
    }
}

} // namespace creatures1::creatures
