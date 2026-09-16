#include "brain.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace creatures1::brain {

namespace {

void append_activity_entry(char*& output,
                           std::uint32_t lobe_x,
                           std::uint32_t lobe_y,
                           const LobeNeuron& neuron,
                           std::uint8_t value) {
    if (value == 0) {
        return;
    }
    *output++ = static_cast<char>('0' + lobe_x + neuron.grid_x);
    *output++ = static_cast<char>('0' + lobe_y + neuron.grid_y);
    *output++ = static_cast<char>('0' + (value >> 4));
}

std::uint32_t average_connection_value(const LobeNeuron& neuron,
                                       int rule_index,
                                       bool dendrite_state) {
    if (rule_index < 0 || rule_index > 1) {
        return 0;
    }

    const std::uint8_t count =
        rule_index == 0 ? neuron.rule0_connection_count
                        : neuron.rule1_connection_count;
    const LobeConnection* connections =
        rule_index == 0 ? neuron.rule0_connections_begin
                        : neuron.rule1_connections_begin;
    if (count == 0 || connections == nullptr) {
        return 0;
    }

    std::uint32_t total = 0;
    for (std::uint8_t index = 0; index < count; ++index) {
        total += dendrite_state ? connections[index].dendrite_state
                                : connections[index].target_weight;
    }
    return total / count;
}

std::uint8_t maximum_current_weight(const LobeNeuron& neuron,
                                    int rule_index) {
    if (rule_index < 0 || rule_index > 1) {
        return 0;
    }

    const std::uint8_t count =
        rule_index == 0 ? neuron.rule0_connection_count
                        : neuron.rule1_connection_count;
    const LobeConnection* connections =
        rule_index == 0 ? neuron.rule0_connections_begin
                        : neuron.rule1_connections_begin;
    if (count == 0 || connections == nullptr) {
        return 0;
    }

    std::uint8_t maximum = 0;
    for (std::uint8_t index = 0; index < count; ++index) {
        if (maximum < connections[index].current_weight) {
            maximum = connections[index].current_weight;
        }
    }
    return maximum;
}

std::uint8_t random_byte_inclusive(std::uint8_t minimum,
                                   std::uint8_t maximum) {
    const int range = static_cast<int>(maximum) -
                          static_cast<int>(minimum) +
                      1;
    return static_cast<std::uint8_t>(
        static_cast<int>(minimum) + std::rand() % range);
}

int clamp_coordinate(int coordinate, std::uint32_t extent) {
    if (coordinate < 0) {
        return 0;
    }
    const int maximum = static_cast<int>(extent) - 1;
    return coordinate > maximum ? maximum : coordinate;
}

void serialize_expression(BrainArchive& archive, LobeRuleExpression& expression) {
    if (archive.is_loading()) {
        for (std::uint8_t& token : expression.tokens) {
            token = archive.read_byte();
        }
        return;
    }
    for (const std::uint8_t token : expression.tokens) {
        archive.write_byte(token);
    }
}

void serialize_connection_rule(BrainArchive& archive,
                               LobeConnectionRule& rule) {
    if (archive.is_loading()) {
        rule.target_lobe_index = archive.read_u32();
        rule.connection_count_min = archive.read_byte();
        rule.connection_count_max = archive.read_byte();
        rule.count_distribution = static_cast<ConnectionCountDistribution>(
            archive.read_byte());
        rule.target_cell_spread_radius = archive.read_byte();
        rule.baseline_weight_min = archive.read_byte();
        rule.baseline_weight_max = archive.read_byte();
        rule.dendrite_state_min = archive.read_byte();
        rule.dendrite_state_max = archive.read_byte();
        rule.connection_mode =
            static_cast<ConnectionMode>(archive.read_byte());
        rule.current_weight_decay_selector = archive.read_byte();
        rule.target_weight_convergence_selector = archive.read_byte();
        rule.baseline_weight_step_interval = archive.read_byte();
        // Native CBrain::Serialize archives both intervals before the
        // expressions: ... growth interval (+16), decay interval (+27), then
        // growth expr (+17..+26), decay expr (+28..+37), current-weight and
        // target-weight exprs.  Reading them interleaved shifted every field
        // after the growth interval by one byte.
        rule.dendrite_growth_interval = archive.read_byte();
        rule.dendrite_decay_interval = archive.read_byte();
        serialize_expression(archive, rule.dendrite_growth_expression);
        serialize_expression(archive, rule.dendrite_decay_expression);
        serialize_expression(archive, rule.current_weight_expression);
        serialize_expression(archive, rule.target_weight_expression);
        return;
    }

    archive.write_u32(rule.target_lobe_index);
    archive.write_byte(rule.connection_count_min);
    archive.write_byte(rule.connection_count_max);
    archive.write_byte(static_cast<std::uint8_t>(rule.count_distribution));
    archive.write_byte(rule.target_cell_spread_radius);
    archive.write_byte(rule.baseline_weight_min);
    archive.write_byte(rule.baseline_weight_max);
    archive.write_byte(rule.dendrite_state_min);
    archive.write_byte(rule.dendrite_state_max);
    archive.write_byte(static_cast<std::uint8_t>(rule.connection_mode));
    archive.write_byte(rule.current_weight_decay_selector);
    archive.write_byte(rule.target_weight_convergence_selector);
    archive.write_byte(rule.baseline_weight_step_interval);
    archive.write_byte(rule.dendrite_growth_interval);
    archive.write_byte(rule.dendrite_decay_interval);
    serialize_expression(archive, rule.dendrite_growth_expression);
    serialize_expression(archive, rule.dendrite_decay_expression);
    serialize_expression(archive, rule.current_weight_expression);
    serialize_expression(archive, rule.target_weight_expression);
}

struct PendingConnectionTarget {
    LobeConnection* connection = nullptr;
    std::uint32_t target_lobe_index = 0;
    std::int32_t target_neuron_index = -1;
};

} // namespace

void Brain::serialize(BrainArchive& archive) {
    if (!archive.is_loading()) {
        archive.write_u32(lobe_count_);
        const std::size_t count = std::min<std::size_t>(
            static_cast<std::size_t>(lobe_count_), lobes_.size());

        for (std::size_t lobe_index = 0; lobe_index < count; ++lobe_index) {
            Lobe& lobe = lobes_[lobe_index];
            archive.write_u32(lobe.grid_x_offset);
            archive.write_u32(lobe.grid_y_offset);
            archive.write_u32(lobe.grid_width);
            archive.write_u32(lobe.grid_height);
            archive.write_u32(static_cast<std::uint32_t>(lobe.percept_state));
            archive.write_byte(lobe.active_fraction);
            for (const std::uint8_t state :
                 lobe.late_phase_runtime_state.token_evaluation_state) {
                archive.write_byte(state);
            }
            archive.write_byte(lobe.activation_threshold);
            archive.write_byte(lobe.activation_relaxation_selector);
            archive.write_byte(lobe.activation_baseline);
            archive.write_byte(lobe.input_gain);
            serialize_expression(archive, lobe.lobe_expression);
            archive.write_byte(lobe.winner_take_all_flags);
            for (LobeConnectionRule& rule : lobe.connection_rules) {
                serialize_connection_rule(archive, rule);
            }
            archive.write_u32(lobe.neuron_count);
            archive.write_u32(lobe.total_connection_count);
        }

        for (std::size_t lobe_index = 0; lobe_index < count; ++lobe_index) {
            const Lobe& lobe = lobes_[lobe_index];
            for (std::uint32_t neuron_index = 0;
                 neuron_index < lobe.neuron_count; ++neuron_index) {
                const LobeNeuron& neuron = lobe.neurons[neuron_index];
                archive.write_byte(neuron.grid_x);
                archive.write_byte(neuron.grid_y);
                archive.write_byte(neuron.firing_strength);
                archive.write_byte(neuron.activation);
                archive.write_byte(neuron.winner_take_all_excluded);
                archive.write_byte(neuron.source_lobe_index);

                const LobeConnection* groups[2] = {
                    neuron.rule0_connections_begin,
                    neuron.rule1_connections_begin,
                };
                const std::uint8_t counts[2] = {
                    neuron.rule0_connection_count,
                    neuron.rule1_connection_count,
                };
                for (std::size_t rule_index = 0; rule_index < 2;
                     ++rule_index) {
                    archive.write_byte(counts[rule_index]);
                    const std::ptrdiff_t begin =
                        groups[rule_index] == nullptr
                            ? 0
                            : groups[rule_index] - lobe.connections.get();
                    archive.write_u32(static_cast<std::uint32_t>(begin));
                    for (std::uint8_t connection_index = 0;
                         connection_index < counts[rule_index];
                         ++connection_index) {
                        const LobeConnection& connection =
                            groups[rule_index][connection_index];
                        const LobeConnectionRule& rule =
                            lobe.connection_rules[rule_index];
                        const LobeNeuron* target = connection.target_neuron;
                        const Lobe* target_lobe =
                            rule.target_lobe_index < count
                                ? &lobes_[rule.target_lobe_index]
                                : nullptr;
                        std::int32_t target_index = 0;
                        if (target_lobe != nullptr && target != nullptr &&
                            target >= target_lobe->neurons.get() &&
                            target < target_lobe->neurons.get() +
                                        target_lobe->neuron_count) {
                            target_index = static_cast<std::int32_t>(
                                target - target_lobe->neurons.get());
                        }
                        archive.write_u32(
                            static_cast<std::uint32_t>(target_index));
                        archive.write_byte(connection.target_grid_x);
                        archive.write_byte(connection.target_grid_y);
                        archive.write_byte(connection.current_weight);
                        archive.write_byte(connection.target_weight);
                        archive.write_byte(connection.baseline_weight);
                        archive.write_byte(connection.dendrite_state);
                    }
                }
            }
        }
        return;
    }

    const std::uint32_t archived_lobe_count = archive.read_u32();
    lobe_count_ = std::min<std::uint32_t>(archived_lobe_count,
                                          lobes_.size());
    for (Lobe& lobe : lobes_) {
        lobe.neurons.reset();
        lobe.connections.reset();
        lobe.active_neurons.reset();
        lobe.grid_x_offset = 0;
        lobe.grid_y_offset = 0;
        lobe.grid_width = 0;
        lobe.grid_height = 0;
        lobe.activation_threshold = 0;
        lobe.activation_relaxation_selector = 0;
        lobe.activation_baseline = 0;
        lobe.input_gain = 0;
        lobe.lobe_expression = {};
        lobe.winner_take_all_flags = 0;
        lobe.padding = 0;
        lobe.connection_rules = {};
        lobe.neuron_count = 0;
        lobe.total_connection_count = 0;
        lobe.active_neuron_count_ = 0;
        lobe.percept_state = PerceptionCopyState::not_copied;
        lobe.rule0_migration_candidate_neuron_index = 99999;
        lobe.rule1_migration_candidate_neuron_index = 99999;
        lobe.active_fraction = 0;
        lobe.late_phase_runtime_state = {};
    }

    for (std::size_t lobe_index = 0; lobe_index < lobe_count_; ++lobe_index) {
        Lobe& lobe = lobes_[lobe_index];
        lobe.grid_x_offset = archive.read_u32();
        lobe.grid_y_offset = archive.read_u32();
        lobe.grid_width = archive.read_u32();
        lobe.grid_height = archive.read_u32();
        lobe.percept_state = static_cast<PerceptionCopyState>(
            archive.read_u32());
        lobe.active_fraction = archive.read_byte();
        for (std::uint8_t& state :
             lobe.late_phase_runtime_state.token_evaluation_state) {
            state = archive.read_byte();
        }
        lobe.activation_threshold = archive.read_byte();
        lobe.activation_relaxation_selector = archive.read_byte();
        lobe.activation_baseline = archive.read_byte();
        lobe.input_gain = archive.read_byte();
        serialize_expression(archive, lobe.lobe_expression);
        lobe.winner_take_all_flags = archive.read_byte();
        for (LobeConnectionRule& rule : lobe.connection_rules) {
            serialize_connection_rule(archive, rule);
        }
        lobe.neuron_count = archive.read_u32();
        lobe.total_connection_count = archive.read_u32();
        lobe.neurons = std::make_unique<LobeNeuron[]>(lobe.neuron_count);
        lobe.active_neurons =
            std::make_unique<LobeNeuron*[]>(lobe.neuron_count);
        if (lobe.total_connection_count != 0) {
            lobe.connections = std::make_unique<LobeConnection[]>(
                lobe.total_connection_count);
        }
    }

    std::vector<PendingConnectionTarget> pending_targets;
    for (std::size_t lobe_index = 0; lobe_index < lobe_count_; ++lobe_index) {
        Lobe& lobe = lobes_[lobe_index];
        for (std::uint32_t neuron_index = 0;
             neuron_index < lobe.neuron_count; ++neuron_index) {
            LobeNeuron& neuron = lobe.neurons[neuron_index];
            neuron.grid_x = archive.read_byte();
            neuron.grid_y = archive.read_byte();
            neuron.firing_strength = archive.read_byte();
            neuron.activation = archive.read_byte();
            neuron.winner_take_all_excluded = archive.read_byte();
            neuron.source_lobe_index = archive.read_byte();

            LobeConnection** groups[2] = {
                &neuron.rule0_connections_begin,
                &neuron.rule1_connections_begin,
            };
            std::uint8_t* counts[2] = {
                &neuron.rule0_connection_count,
                &neuron.rule1_connection_count,
            };
            for (std::size_t rule_index = 0; rule_index < 2;
                 ++rule_index) {
                *counts[rule_index] = archive.read_byte();
                const std::uint32_t begin = archive.read_u32();
                const bool valid_range =
                    lobe.connections != nullptr &&
                    begin <= lobe.total_connection_count &&
                    *counts[rule_index] <=
                        lobe.total_connection_count - begin;
                *groups[rule_index] = valid_range
                    ? lobe.connections.get() + begin
                    : nullptr;
                const std::uint32_t target_lobe_index =
                    lobe.connection_rules[rule_index].target_lobe_index;
                for (std::uint8_t connection_index = 0;
                     connection_index < *counts[rule_index];
                     ++connection_index) {
                    const std::int32_t target_neuron_index =
                        static_cast<std::int32_t>(archive.read_u32());
                    LobeConnection decoded{};
                    decoded.target_grid_x = archive.read_byte();
                    decoded.target_grid_y = archive.read_byte();
                    decoded.current_weight = archive.read_byte();
                    decoded.target_weight = archive.read_byte();
                    decoded.baseline_weight = archive.read_byte();
                    decoded.dendrite_state = archive.read_byte();
                    if (!valid_range) {
                        continue;
                    }
                    LobeConnection& connection =
                        (*groups[rule_index])[connection_index];
                    connection = decoded;
                    pending_targets.push_back(
                        {&connection, target_lobe_index, target_neuron_index});
                }
            }
        }
    }

    for (const PendingConnectionTarget& pending : pending_targets) {
        if (pending.target_lobe_index >= lobe_count_ ||
            pending.target_neuron_index < 0) {
            continue;
        }
        Lobe& target_lobe = lobes_[pending.target_lobe_index];
        if (static_cast<std::uint32_t>(pending.target_neuron_index) >=
            target_lobe.neuron_count) {
            continue;
        }
        pending.connection->target_neuron =
            &target_lobe.neurons[pending.target_neuron_index];
    }
}

void Brain::load_genome(creatures1::creatures::Genome& genome) {
    if (genome.life_stage() !=
        creatures1::creatures::GenomeLifeStage::stage_zero) {
        return;
    }

    lobe_count_ = 0;

    // C1 makes two complete passes over the stage-zero brain-lobe genes.
    // Pass zero loads records with load-pass 0; pass one loads the remaining
    // records. The lobe ordinal is positional, not stored in the gene.
    for (std::uint8_t load_pass = 0; load_pass < 2; ++load_pass) {
        genome.set_cursor(0);
        while (lobe_count_ < lobes_.size() &&
               genome.find_next_matching_gene(
                   0, 0, 1,
                   creatures1::creatures::GenomeStageFilter::match_stage_zero)) {
            const bool belongs_to_pass =
                load_pass == 0 ? genome.current_gene_load_pass() == 0
                                : genome.current_gene_load_pass() != 0;
            if (!belongs_to_pass) {
                continue;
            }

            const auto lobe_index = static_cast<StandardLobeIndex>(lobe_count_);
            lobes_[lobe_count_].load_genome(genome, lobe_index);
            ++lobe_count_;
        }
    }

    const std::size_t count =
        std::min<std::size_t>(lobe_count_, lobes_.size());
    if (count == 0) {
        return;
    }

    // Genome bytes may name a lobe beyond the number actually selected for
    // this sex/stage. C1 folds each target into the loaded lobe range before
    // allocating runtime connections.
    for (std::size_t lobe_index = 0; lobe_index < count; ++lobe_index) {
        for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
            lobes_[lobe_index].connection_rule(rule_index).target_lobe_index
                %= static_cast<std::uint32_t>(count);
        }
    }

    for (std::size_t lobe_index = 0; lobe_index < count; ++lobe_index) {
        lobes_[lobe_index].allocate_runtime_state();
    }
    initialize_connections();

    // Perception neurons retain the source-lobe ordinal for mutually
    // exclusive perception copies. All perception-copying lobes consume a
    // consecutive range, matching the binary's global cursor.
    Lobe& perception = lobes_[0];
    std::size_t perception_index = 0;
    for (std::size_t source_index = 1;
         source_index < count &&
         perception_index < perception.neuron_count_value();
         ++source_index) {
        const Lobe& source = lobes_[source_index];
        if (!source.copies_to_perception()) {
            continue;
        }

        for (std::uint32_t neuron_index = 0;
             neuron_index < source.neuron_count_value() &&
             perception_index < perception.neuron_count_value();
             ++neuron_index, ++perception_index) {
            if (source.perception_copy_state() ==
                PerceptionCopyState::copy_to_perception_mutually_exclusive) {
                perception.neuron(static_cast<std::uint32_t>(perception_index))
                    .source_lobe_index = static_cast<std::uint8_t>(source_index);
            }
        }
    }
}

void Brain::update(std::uint32_t tick, BrainUpdateHost* host) {
    const std::size_t count = std::min<std::size_t>(lobe_count_, lobes_.size());

    // C1 updates the drive through general-sensory lobes before assembling
    // their perceptual input. The perception lobe itself is updated after
    // that merge, followed by the decision and attention-side late phase.
    for (std::size_t index = 1; index <= 5 && index < count; ++index) {
        lobes_[index].update_early_phase(this, tick);
    }

    Lobe& perception = lobes_[0];
    const std::uint32_t perception_count = perception.neuron_count_value();
    std::uint32_t perception_index = 0;
    bool overrun = false;

    for (std::size_t source_index = 1;
         source_index < count && !overrun;
         ++source_index) {
        const Lobe& source = lobes_[source_index];
        if (!source.copies_to_perception()) {
            continue;
        }

        for (std::uint32_t source_neuron_index = 0;
             source_neuron_index < source.neuron_count_value();
             ++source_neuron_index) {
            if (perception_index >= perception_count) {
                overrun = true;
                break;
            }

            // The source lobes' early phase has already run, and it leaves
            // `activation` relaxed towards the lobe's baseline -- for a
            // selector-0 lobe such as Drive that means zeroed.  What survives
            // the early phase, and what the native merge reads (neuron offset
            // 2), is the firing strength.
            LobeNeuron& destination = perception.neuron(perception_index);
            const LobeNeuron& source_neuron =
                source.neuron(source_neuron_index);
            destination.activation = std::max(destination.activation,
                                              source_neuron.firing_strength);
            ++perception_index;
        }

        if (perception_index >= perception_count) {
            overrun = true;
        }
    }

    if (overrun && host != nullptr) {
        host->report_perception_overrun();
    }

    perception.update_early_phase(this, tick);
    for (std::size_t index = 6; index < count; ++index) {
        lobes_[index].update_late_phase(this, tick);
    }
}

void Brain::add_lobe_neuron_activation(std::uint32_t lobe_index,
                                       std::uint32_t neuron_index,
                                       std::uint8_t activation_delta) {
    if (lobe_index >= lobes_.size()) {
        return;
    }
    lobes_[lobe_index].add_neuron_activation(neuron_index, activation_delta);
}

void Brain::set_lobe_neuron_activation(std::uint32_t lobe_index,
                                       std::uint32_t neuron_index,
                                       std::uint8_t activation) {
    if (lobe_index >= lobes_.size()) {
        return;
    }
    lobes_[lobe_index].set_neuron_activation(neuron_index, activation);
}

void Brain::fire_neuron_at_global_position(std::int32_t global_x,
                                           std::int32_t global_y,
                                           std::int32_t activation) {
    // The native command walks the active lobe count, then each lobe's
    // sixteen-byte neuron records from index zero.  It stops at the first
    // matching global coordinate, even if a later lobe contains the same
    // coordinate.
    const std::size_t count = std::min<std::size_t>(lobe_count_, lobes_.size());
    for (std::size_t lobe_index = 0; lobe_index < count; ++lobe_index) {
        Lobe& current_lobe = lobes_[lobe_index];
        const std::uint32_t neuron_count = current_lobe.neuron_count_value();
        for (std::uint32_t neuron_index = 0;
             neuron_index < neuron_count; ++neuron_index) {
            LobeNeuron& current_neuron = current_lobe.neuron(neuron_index);
            const std::int32_t neuron_global_x =
                static_cast<std::int32_t>(
                    current_lobe.grid_x_offset_value()) +
                static_cast<std::int32_t>(current_neuron.grid_x);
            const std::int32_t neuron_global_y =
                static_cast<std::int32_t>(
                    current_lobe.grid_y_offset_value()) +
                static_cast<std::int32_t>(current_neuron.grid_y);
            if (neuron_global_x != global_x || neuron_global_y != global_y) {
                continue;
            }

            const std::int32_t clamped_activation =
                activation < 0 ? 0 : activation > 0xff ? 0xff : activation;
            current_neuron.activation =
                static_cast<std::uint8_t>(clamped_activation);
            return;
        }
    }
}

void Brain::initialize_connections() {
    const std::size_t count = lobe_count_ < lobes_.size()
                                  ? static_cast<std::size_t>(lobe_count_)
                                  : lobes_.size();

    for (std::size_t source_lobe_index = 0; source_lobe_index < count;
         ++source_lobe_index) {
        Lobe& source_lobe = lobes_[source_lobe_index];
        const std::uint32_t source_neuron_count =
            source_lobe.neuron_count_value();
        if (source_neuron_count == 0) {
            continue;
        }

        for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
            const LobeConnectionRule& rule =
                source_lobe.connection_rule(rule_index);
            Lobe& target_lobe = lobes_[rule.target_lobe_index];
            const std::uint32_t target_width = target_lobe.grid_width_value();
            const std::uint32_t target_height =
                target_lobe.grid_height_value();
            const std::uint32_t target_area = target_width * target_height;
            if (target_area == 0) {
                continue;
            }

            const int spread_radius =
                static_cast<int>(rule.target_cell_spread_radius);
            const int spread_range = spread_radius * 2 + 1;
            for (std::uint32_t source_neuron_index = 0;
                 source_neuron_index < source_neuron_count;
                 ++source_neuron_index) {
                LobeNeuron& source_neuron =
                    source_lobe.neuron(source_neuron_index);
                const std::uint8_t connection_count =
                    rule_index == 0 ? source_neuron.rule0_connection_count
                                    : source_neuron.rule1_connection_count;
                LobeConnection* connections =
                    rule_index == 0 ? source_neuron.rule0_connections_begin
                                    : source_neuron.rule1_connections_begin;
                if (connection_count == 0 || connections == nullptr) {
                    continue;
                }

                const std::uint32_t canonical_target_index =
                    (source_neuron_index * target_area) / source_neuron_count;
                for (std::uint32_t connection_index = 0;
                     connection_index < connection_count; ++connection_index) {
                    int target_x;
                    int target_y;
                    if (connection_index == 0) {
                        target_x = static_cast<int>(canonical_target_index %
                                                    target_width);
                        target_y = static_cast<int>(canonical_target_index /
                                                    target_width);
                    } else {
                        const int canonical_x = static_cast<int>(
                            canonical_target_index % target_width);
                        const int canonical_y = static_cast<int>(
                            canonical_target_index / target_width);
                        target_x = canonical_x +
                                   (std::rand() % spread_range -
                                    spread_radius);
                        target_y = canonical_y +
                                   (std::rand() % spread_range -
                                    spread_radius);
                    }

                    target_x = clamp_coordinate(target_x, target_width);
                    target_y = clamp_coordinate(target_y, target_height);
                    LobeConnection& connection = connections[connection_index];
                    connection.target_grid_x =
                        static_cast<std::uint8_t>(target_x);
                    connection.target_grid_y =
                        static_cast<std::uint8_t>(target_y);
                    connection.target_neuron = &target_lobe.neuron(
                        static_cast<std::uint32_t>(target_y) * target_width +
                        static_cast<std::uint32_t>(target_x));
                    connection.baseline_weight = random_byte_inclusive(
                        rule.baseline_weight_min, rule.baseline_weight_max);
                    connection.target_weight = connection.baseline_weight;
                    connection.dendrite_state = random_byte_inclusive(
                        rule.dendrite_state_min, rule.dendrite_state_max);
                }
            }
        }
    }
}

void Brain::reset_runtime_activity() {
    const std::size_t count = lobe_count_ < lobes_.size()
                                  ? static_cast<std::size_t>(lobe_count_)
                                  : lobes_.size();
    for (std::size_t index = 0; index < count; ++index) {
        lobes_[index].reset_runtime_activity();
    }
}

void Brain::normalize_dream_connection_weights() {
    const std::size_t lobe_count =
        std::min<std::size_t>(lobe_count_, lobes_.size());
    for (std::size_t lobe_index = 0; lobe_index < lobe_count; ++lobe_index) {
        Lobe& current_lobe = lobes_[lobe_index];
        for (std::uint32_t neuron_index = 0;
             neuron_index < current_lobe.neuron_count_value();
             ++neuron_index) {
            LobeNeuron& neuron = current_lobe.neuron(neuron_index);
            for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
                LobeConnection* connections =
                    rule_index == 0 ? neuron.rule0_connections_begin
                                    : neuron.rule1_connections_begin;
                const std::uint8_t count =
                    rule_index == 0 ? neuron.rule0_connection_count
                                    : neuron.rule1_connection_count;
                for (std::uint8_t connection_index = 0;
                     connection_index < count; ++connection_index) {
                    LobeConnection& connection = connections[connection_index];
                    if (connection.baseline_weight < connection.target_weight) {
                        connection.baseline_weight = connection.target_weight;
                    }
                }
            }
        }
    }
}

std::size_t Brain::format_activity_report(char* output,
                                          ActivityReportMode mode,
                                          int rule_index) const {
    char* cursor = output;
    const std::size_t count = lobe_count_ < lobes_.size()
                                  ? static_cast<std::size_t>(lobe_count_)
                                  : lobes_.size();
    for (std::size_t lobe_index = 0; lobe_index < count; ++lobe_index) {
        const Lobe& current_lobe = lobes_[lobe_index];
        const std::uint32_t neuron_count = current_lobe.neuron_count_value();
        for (std::uint32_t neuron_index = 0;
             neuron_index < neuron_count; ++neuron_index) {
            const LobeNeuron& neuron = current_lobe.neuron(neuron_index);
            std::uint8_t report_value = 0;
            switch (mode) {
            case ActivityReportMode::firing_strength:
                report_value = neuron.firing_strength;
                break;
            case ActivityReportMode::activation:
                report_value = neuron.activation;
                break;
            case ActivityReportMode::maximum_current_weight:
                report_value = maximum_current_weight(neuron, rule_index);
                break;
            case ActivityReportMode::average_target_weight:
                report_value = static_cast<std::uint8_t>(
                    average_connection_value(neuron, rule_index, false));
                break;
            case ActivityReportMode::average_dendrite_state:
                report_value = static_cast<std::uint8_t>(
                    average_connection_value(neuron, rule_index, true));
                break;
            }
            append_activity_entry(cursor, current_lobe.grid_x_offset_value(),
                                  current_lobe.grid_y_offset_value(), neuron,
                                  report_value);
        }
    }
    *cursor = '\0';
    return static_cast<std::size_t>(cursor - output) + 1;
}

} // namespace creatures1::brain
