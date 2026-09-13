#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../world/geometry.hpp"

namespace creatures1::objects {
class Object;
}

namespace creatures1::creatures {

class Creature;

// The game-owned portion of the non-scenery object contract used by the
// world tick callback. Container storage and the concrete Object hierarchy
// remain outside this leaf module.
class DriveThresholdObject {
public:
    virtual ~DriveThresholdObject() = default;
    virtual bool tick_enabled() const = 0;
    virtual void update_drive_threshold_state() = 0;
};

struct NonSceneryObjectRegistryView {
    DriveThresholdObject* const* objects = nullptr;
    std::size_t object_count = 0;
};

// Updates every tick-enabled non-scenery object. The count is refreshed after
// each callback because an update may add or remove registry entries.
void update_all_creature_drive_threshold_states(
    NonSceneryObjectRegistryView registry);

// Creature::UpdateDriveThresholdState @ 0x00409110 compares each of the
// sixteen goal-direction drive levels against two fixed tables, read here
// from g_drive_lower_thresholds @ 0x004547b0 and g_drive_upper_thresholds
// @ 0x00454770.  Slots 10 and 12..15 are pinned to 0xff, which is above the
// byte range a drive level can reach, so those drives never raise the state.
inline constexpr std::array<std::int32_t, 16> kDriveLowerThresholds{
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0xff, 0x80, 0xff, 0xff, 0xff, 0xff};
inline constexpr std::array<std::int32_t, 16> kDriveUpperThresholds{
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff};

// The native world pass owns the creature registry and dispatches through the
// Object/Creature hierarchy.  These services expose the recovered facts while
// keeping MFC arrays, virtual slots, debug-dialog state, and global SFCView
// storage outside the clean game-policy lane.
class CreatureWorldUpdateHost {
public:
    virtual ~CreatureWorldUpdateHost() = default;

    virtual std::size_t creature_count() const = 0;
    virtual Creature* creature_at(std::size_t index) const = 0;
    virtual bool creature_is_tick_enabled(const Creature& creature) const = 0;
    virtual bool creature_is_alive(const Creature& creature) const = 0;

    virtual objects::Object* motion_link(const Creature& creature) const = 0;
    virtual int motion_target_x(const Creature& creature) const = 0;
    virtual int sound_source_x(const Creature& creature) const = 0;
    virtual bool object_has_interaction_event(
        const objects::Object& object) const = 0;

    virtual bool boundary_correction_pending(
        const Creature& creature) const = 0;
    virtual void clear_boundary_correction(Creature& creature) = 0;
    virtual objects::Object* bounds_reference(
        const Creature& creature) const = 0;
    virtual void trigger_built_in_stimulus(Creature& creature,
                                           std::uint32_t stimulus_id,
                                           std::uint32_t argument) = 0;

    virtual void update_perception(Creature& creature) = 0;
    virtual void update_attention(Creature& creature) = 0;

    virtual world::WorldRect movement_bounds(
        const Creature& creature) const = 0;
    virtual world::WorldRect movement_bounds(
        const objects::Object& object) const = 0;
    virtual bool reference_uses_own_attention_bounds(
        const objects::Object& object) const = 0;
    virtual void object_part_center(const objects::Object& object,
                                    int part_index,
                                    int& out_x,
                                    int& out_y) const = 0;

    virtual bool should_log_attention_loss(
        const Creature& creature) const = 0;
    virtual void log_attention_target_out_of_reach(
        const Creature& creature,
        const objects::Object& target) = 0;
    virtual void log_attention_target_inaccessible(
        const Creature& creature,
        const objects::Object& target) = 0;
};

// Copies the recovered motion, boundary-correction, and 16-byte
// goal-direction drive inputs into the appropriate brain lobes for every
// tick-enabled living creature. Registry size is refreshed after each pass.
void update_all_creature_brain_inputs(CreatureWorldUpdateHost& host);

// Runs perception and attention for every tick-enabled creature, then removes
// attention records whose target centre leaves the creature's effective
// movement bounds. The fixed C1 attention table reserves slot zero, so the
// cleanup pass covers slots 1 through 39.
void update_all_creature_perception_and_attention(
    CreatureWorldUpdateHost& host);

} // namespace creatures1::creatures
