#pragma once

// Not native: the private state of a Lobe that the load equivalence test's
// dump reads (the B column of load_dump_brain.inc).  Lobe names this class
// as a friend; nothing else uses it.

#include "lobe.hpp"

#include <cstddef>
#include <cstdint>

namespace creatures1::brain {

class LobeFieldDump {
public:
    static std::uint8_t threshold(const Lobe& l) { return l.activation_threshold; }
    static std::uint8_t relaxation(const Lobe& l) { return l.activation_relaxation_selector; }
    static std::uint8_t baseline(const Lobe& l) { return l.activation_baseline; }
    static std::uint8_t gain(const Lobe& l) { return l.input_gain; }
    static const std::uint8_t* state_rule(const Lobe& l) { return l.state_svrule.tokens.data(); }
    static std::uint32_t connection_count(const Lobe& l) { return l.total_connection_count; }
    static std::uint32_t migration_candidate(const Lobe& l, std::size_t index) {
        return index == 0 ? l.type0_migration_candidate : l.type1_migration_candidate;
    }
    static std::uint8_t active_fraction(const Lobe& l) { return l.active_fraction; }
};

} // namespace creatures1::brain
