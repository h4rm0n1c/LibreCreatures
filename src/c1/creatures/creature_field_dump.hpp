#pragma once

// Not native: the private state of a Creature that the load equivalence
// test's dump reads (the P column of load_dump_fields.inc).  Creature
// names this class as a friend; nothing else uses it.

#include "creature.hpp"

#include <cstdint>

namespace creatures1::creatures {

class CreatureFieldDump {
public:
    static std::uint8_t active_involuntary(const Creature& c) {
        return c.active_involuntary_action_index_;
    }
    static const Creature::GoalDirectionState& goal(const Creature& c) {
        return c.goal_direction_state_;
    }
    static const objects::Object* sleep_indicator(const Creature& c) {
        return c.sleep_indicator_object_;
    }
};

} // namespace creatures1::creatures
