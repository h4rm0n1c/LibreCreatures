#pragma once

#include <array>
#include <string>
#include <string_view>

#include "lobe.hpp"

namespace creatures1::brain {

class Brain;

struct LobeDebugLabelTables {
    std::array<std::string_view, 112> connection_labels;
    std::array<std::string_view, 16> neuron_labels;
};

std::string format_connection_debug_description(
    const LobeNeuron& neuron, const LobeDebugLabelTables& labels);

void migrate_rule_connections(LobeNeuron& neuron, Brain& owner_brain,
                              Lobe& source_lobe, int connection_rule_index);

} // namespace creatures1::brain
