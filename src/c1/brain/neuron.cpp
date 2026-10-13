#include "neuron.hpp"

#include "brain.hpp"
#include "brain_recorder.hpp"
#include "../common/lab_trace.hpp"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace creatures1::brain {

namespace {
std::string_view connection_label(const LobeNeuron& neuron,
                                  const LobeDebugLabelTables& labels,
                                  std::size_t connection_index) {
    const Dendrite& connection =
        neuron.type0_dendrites[connection_index];
    const LobeNeuron& target = *connection.source;
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
    const Dendrite& connection =
        neuron.type1_dendrites[connection_index];
    const LobeNeuron& target = *connection.source;
    const std::size_t label_index =
        static_cast<std::size_t>(target.grid_x) +
        static_cast<std::size_t>(target.grid_y) * 7;
    return label_index < labels.neuron_labels.size()
               ? labels.neuron_labels[label_index]
               : std::string_view("UNKNOWN");
}
} // namespace

namespace {

// MigrateRuleConnections @ 00405da0 packs its duplicate test as
// (sum << 16) | (product & 0xffff), which throws away the high half of the
// pointer sum, so unrelated dendrite sets can pass for duplicates.
// LibreCreatures deviation: compare the whole sum, the closer reading of
// "is this set already wired".
struct ConnectionSetSignature {
    std::uint32_t pointer_sum = 0;
    std::uint16_t pointer_product_low = 0;

    bool operator==(const ConnectionSetSignature& other) const {
        return pointer_sum == other.pointer_sum &&
               pointer_product_low == other.pointer_product_low;
    }
};

// LibreCreatures deviation.  The signature is only a quick filter: with
// neurons 16 bytes apart in one array, the product of four or five target
// pointers has no low 16 bits left, so it reduces to the pointer sum, and
// different target sets with the same sum looked like duplicates.  A
// matching signature is confirmed by comparing the target sets themselves,
// order ignored and repeats counted.
bool same_target_set(const Dendrite* first, const Dendrite* second,
                     std::uint8_t count) {
    std::array<const LobeNeuron*, 256> first_targets{};
    std::array<const LobeNeuron*, 256> second_targets{};
    for (std::uint8_t index = 0; index < count; ++index) {
        first_targets[index] = first[index].source;
        second_targets[index] = second[index].source;
    }
    std::sort(first_targets.begin(), first_targets.begin() + count);
    std::sort(second_targets.begin(), second_targets.begin() + count);
    return std::equal(first_targets.begin(), first_targets.begin() + count,
                      second_targets.begin());
}

std::uint32_t original_pointer_value(const LobeNeuron* neuron) {
    // Creatures 1 is a 32-bit process.  Keep the recovered arithmetic's
    // width explicit when the clean source is compiled on a 64-bit host.
    return static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(neuron));
}

ConnectionSetSignature connection_set_signature(const Dendrite* begin,
                                                 std::uint8_t count) {
    std::uint32_t sum = 0;
    std::uint32_t product = 1;
    for (std::uint8_t index = 0; index < count; ++index) {
        const std::uint32_t target =
            original_pointer_value(begin[index].source);
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
    for (std::size_t index = 0; index < neuron.type0_dendrite_count; ++index) {
        description.append(connection_label(neuron, labels, index));
        description.push_back(',');
    }
    for (std::size_t index = 0; index < neuron.type1_dendrite_count; ++index) {
        description.append(" - ");
        description.append(neuron_label(neuron, labels, index));
        description.append(" - ");
    }
    return description;
}

void migrate_rule_connections(LobeNeuron& neuron, Brain& owner_brain,
                              Lobe& source_lobe, int dtype) {
    if (dtype < 0 || dtype > 1) {
        return;
    }

    const std::size_t rule_index = static_cast<std::size_t>(dtype);
    const std::uint8_t old_connection_count =
        rule_index == 0 ? neuron.type0_dendrite_count
                        : neuron.type1_dendrite_count;
    Dendrite* connections =
        rule_index == 0 ? neuron.type0_dendrites
                        : neuron.type1_dendrites;
    const DendriteType& rule = source_lobe.dendrite_type(rule_index);
    const Lobe& target_lobe = owner_brain.lobe(rule.source_lobe);
    const std::uint32_t target_count = target_lobe.active_neuron_count();

    if (old_connection_count > target_count) {
        return;
    }

    std::array<LobeNeuron*, 256> saved_targets{};
    for (std::uint8_t index = 0; index < old_connection_count; ++index) {
        saved_targets[index] = connections[index].source;
    }
    // The dendrites as they were, for the brain recorder's events: a
    // migration may try several candidate sets before it settles, and only
    // where it ends is an event.
    std::array<Dendrite, 256> entry_state{};
    if (g_brain_events_wanted) {
        for (std::uint8_t index = 0; index < old_connection_count; ++index) {
            entry_state[index] = connections[index];
        }
    }
    const auto record_outcome = [&](DendriteEvent kind) {
        if (!g_brain_events_wanted) {
            return;
        }
        for (std::uint8_t index = 0; index < old_connection_count; ++index) {
            record_dendrite_event(&owner_brain, connections[index], kind,
                                  entry_state[index]);
        }
    };

    // This is the original in-place rand()%N shuffle, retained as a named
    // operation because it is part of the C1 migration behavior.
    Lobe& mutable_target_lobe = owner_brain.lobe(rule.source_lobe);
    for (std::uint32_t index = 0; index < target_count; ++index) {
        const std::uint32_t swap_index =
            static_cast<std::uint32_t>(std::rand() % target_count);
        mutable_target_lobe.swap_active_neurons(index, swap_index);
    }

    const std::uint32_t candidate_limit = target_count - old_connection_count;
    // Lab trace counts (category "dendrites"): candidates rejected as real
    // duplicates, and signature matches the target-set check overturned.
    unsigned duplicate_rejects = 0;
    unsigned false_matches = 0;
    for (std::uint32_t start = 0; start <= candidate_limit; ++start) {
        if (has_repeated_nonzero_source_lobe(mutable_target_lobe, start,
                                             old_connection_count)) {
            continue;
        }

        for (std::uint8_t index = 0; index < old_connection_count; ++index) {
            Dendrite& connection = connections[index];
            connection.source =
                mutable_target_lobe.active_neuron(start + index);
            const std::uint8_t baseline = random_inclusive(
                rule.min_ltw, rule.max_ltw);
            connection.ltw = baseline;
            connection.stw = baseline;
            connection.strength = random_inclusive(
                rule.min_strength, rule.max_strength);
        }

        const ConnectionSetSignature candidate_signature =
            connection_set_signature(connections, old_connection_count);
        bool duplicate = false;
        for (std::uint32_t index = 0;
             index < source_lobe.active_neuron_count(); ++index) {
            const LobeNeuron* existing = source_lobe.active_neuron(index);
            // LibreCreatures deviation.  MigrateRuleConnections @00405da0
            // writes each candidate into this neuron's own connections, then
            // checks every active neuron for the same wiring -- this one
            // included.  An active neuron always matched itself, so every
            // candidate was rejected and the fallback below zeroed the
            // neuron's weights instead of moving its dendrites.  Only a
            // different neuron can make the candidate a duplicate.
            if (existing == &neuron) {
                continue;
            }
            const std::uint8_t existing_count =
                rule_index == 0 ? existing->type0_dendrite_count
                                : existing->type1_dendrite_count;
            if (existing_count != old_connection_count) {
                continue;
            }
            const Dendrite* existing_connections =
                rule_index == 0 ? existing->type0_dendrites
                                : existing->type1_dendrites;
            if (connection_set_signature(existing_connections, existing_count) ==
                candidate_signature) {
                if (!same_target_set(existing_connections, connections,
                                     existing_count)) {
                    ++false_matches;
                    continue;
                }
                duplicate = true;
                ++duplicate_rejects;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        std::uint8_t& loose_count =
            source_lobe.late_phase_state().per_type_loose_dendrite_count[
                rule_index];
        if (loose_count != 0) {
            --loose_count;
        }
        common::lab_trace(common::LabTrace::dendrites,
                          "migrate ok rule=%d duplicates=%u false_matches=%u",
                          dtype, duplicate_rejects,
                          false_matches);
        record_outcome(DendriteEvent::migrate);
        return;
    }
    common::lab_trace(common::LabTrace::dendrites,
                      "migrate fail rule=%d duplicates=%u false_matches=%u",
                      dtype, duplicate_rejects, false_matches);

    for (std::uint8_t index = 0; index < old_connection_count; ++index) {
        connections[index].source = saved_targets[index];
        connections[index].stw = 0;
        connections[index].ltw = 0;
        connections[index].strength = 0;
    }
    record_outcome(DendriteEvent::migrate_failed);
}

} // namespace creatures1::brain
