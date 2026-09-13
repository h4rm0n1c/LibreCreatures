#include "neuron.hpp"

#include "brain.hpp"

#include <cstdint>
#include <cstdlib>

namespace creatures1::brain {

namespace {
std::string_view connection_label(const LobeNeuron& neuron,
                                  const LobeDebugLabelTables& labels,
                                  std::size_t connection_index) {
    const LobeConnection& connection =
        neuron.rule0_connections_begin[connection_index];
    const LobeNeuron& target = *connection.target_neuron;
    const std::size_t label_index =
        static_cast<std::size_t>(target.grid_x) +
        static_cast<std::size_t>(target.grid_y) * 7;
    return label_index < labels.connection_labels.size()
               ? labels.connection_labels[label_index]
               : std::string_view("UNKNOWN");
}

std::string_view neuron_label(const LobeNeuron& neuron,
                              const LobeDebugLabelTables& labels,
                              std::size_t connection_index) {
    const LobeConnection& connection =
        neuron.rule1_connections_begin[connection_index];
    const LobeNeuron& target = *connection.target_neuron;
    const std::size_t label_index =
        static_cast<std::size_t>(target.grid_x) +
        static_cast<std::size_t>(target.grid_y) * 7;
    return label_index < labels.neuron_labels.size()
               ? labels.neuron_labels[label_index]
               : std::string_view("UNKNOWN");
}
} // namespace

namespace {

struct ConnectionSetSignature {
    std::uint32_t pointer_sum = 0;
    std::uint16_t pointer_product_low = 0;

    bool operator==(const ConnectionSetSignature& other) const {
        return pointer_sum == other.pointer_sum &&
               pointer_product_low == other.pointer_product_low;
    }
};

std::uint32_t original_pointer_value(const LobeNeuron* neuron) {
    // Creatures 1 is a 32-bit process.  Keep the recovered arithmetic's
    // width explicit when the clean source is compiled on a 64-bit host.
    return static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(neuron));
}

ConnectionSetSignature connection_set_signature(const LobeConnection* begin,
                                                 std::uint8_t count) {
    std::uint32_t sum = 0;
    std::uint32_t product = 1;
    for (std::uint8_t index = 0; index < count; ++index) {
        const std::uint32_t target =
            original_pointer_value(begin[index].target_neuron);
        sum += target;
        product *= target;
    }
    return {sum, static_cast<std::uint16_t>(product)};
}

bool has_repeated_nonzero_source_lobe(const Lobe& target_lobe,
                                      std::uint32_t start,
                                      std::uint8_t count) {
    for (std::uint8_t first = 0; first < count; ++first) {
        const std::uint8_t source_lobe = target_lobe.active_neuron(
            start + first)->source_lobe_index;
        if (source_lobe == 0) {
            continue;
        }
        for (std::uint8_t next = static_cast<std::uint8_t>(first + 1);
             next < count; ++next) {
            if (target_lobe.active_neuron(start + next)->source_lobe_index ==
                source_lobe) {
                return true;
            }
        }
    }
    return false;
}

std::uint8_t random_inclusive(std::uint8_t minimum, std::uint8_t maximum) {
    const int range = static_cast<int>(maximum) - minimum + 1;
    return static_cast<std::uint8_t>(minimum + std::rand() % range);
}

} // namespace

std::string format_connection_debug_description(
    const LobeNeuron& neuron, const LobeDebugLabelTables& labels) {
    std::string description;
    for (std::size_t index = 0; index < neuron.rule0_connection_count; ++index) {
        description.append(connection_label(neuron, labels, index));
        description.push_back(',');
    }
    for (std::size_t index = 0; index < neuron.rule1_connection_count; ++index) {
        description.append(" - ");
        description.append(neuron_label(neuron, labels, index));
        description.append(" - ");
    }
    return description;
}

void migrate_rule_connections(LobeNeuron& neuron, Brain& owner_brain,
                              Lobe& source_lobe, int connection_rule_index) {
    if (connection_rule_index < 0 || connection_rule_index > 1) {
        return;
    }

    const std::size_t rule_index = static_cast<std::size_t>(connection_rule_index);
    const std::uint8_t old_connection_count =
        rule_index == 0 ? neuron.rule0_connection_count
                        : neuron.rule1_connection_count;
    LobeConnection* connections =
        rule_index == 0 ? neuron.rule0_connections_begin
                        : neuron.rule1_connections_begin;
    const LobeConnectionRule& rule = source_lobe.connection_rule(rule_index);
    const Lobe& target_lobe = owner_brain.lobe(rule.target_lobe_index);
    const std::uint32_t target_count = target_lobe.active_neuron_count();

    if (old_connection_count > target_count) {
        return;
    }

    std::array<LobeNeuron*, 256> saved_targets{};
    for (std::uint8_t index = 0; index < old_connection_count; ++index) {
        saved_targets[index] = connections[index].target_neuron;
    }

    // This is the original in-place rand()%N shuffle, retained as a named
    // operation because it is part of the C1 migration behavior.
    Lobe& mutable_target_lobe = owner_brain.lobe(rule.target_lobe_index);
    for (std::uint32_t index = 0; index < target_count; ++index) {
        const std::uint32_t swap_index =
            static_cast<std::uint32_t>(std::rand() % target_count);
        mutable_target_lobe.swap_active_neurons(index, swap_index);
    }

    const std::uint32_t candidate_limit = target_count - old_connection_count;
    for (std::uint32_t start = 0; start <= candidate_limit; ++start) {
        if (has_repeated_nonzero_source_lobe(mutable_target_lobe, start,
                                             old_connection_count)) {
            continue;
        }

        for (std::uint8_t index = 0; index < old_connection_count; ++index) {
            LobeConnection& connection = connections[index];
            connection.target_neuron =
                mutable_target_lobe.active_neuron(start + index);
            const std::uint8_t baseline = random_inclusive(
                rule.baseline_weight_min, rule.baseline_weight_max);
            connection.baseline_weight = baseline;
            connection.target_weight = baseline;
            connection.dendrite_state = random_inclusive(
                rule.dendrite_state_min, rule.dendrite_state_max);
        }

        const ConnectionSetSignature candidate_signature =
            connection_set_signature(connections, old_connection_count);
        bool duplicate = false;
        for (std::uint32_t index = 0;
             index < source_lobe.active_neuron_count(); ++index) {
            const LobeNeuron* existing = source_lobe.active_neuron(index);
            const std::uint8_t existing_count =
                rule_index == 0 ? existing->rule0_connection_count
                                : existing->rule1_connection_count;
            if (existing_count != old_connection_count) {
                continue;
            }
            const LobeConnection* existing_connections =
                rule_index == 0 ? existing->rule0_connections_begin
                                : existing->rule1_connections_begin;
            if (connection_set_signature(existing_connections, existing_count) ==
                candidate_signature) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        std::uint8_t& loose_count =
            source_lobe.late_phase_state().per_rule_loose_dendrite_count[
                rule_index];
        if (loose_count != 0) {
            --loose_count;
        }
        return;
    }

    for (std::uint8_t index = 0; index < old_connection_count; ++index) {
        connections[index].target_neuron = saved_targets[index];
        connections[index].target_weight = 0;
        connections[index].baseline_weight = 0;
        connections[index].dendrite_state = 0;
    }
}

} // namespace creatures1::brain
