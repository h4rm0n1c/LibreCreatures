#include "lobe.hpp"
#include "neuron.hpp"
#include "rules.hpp"
#include "brain.hpp"
#include "../../../include/C1BiochemistrySelectorData.hpp"
#include <algorithm>
#include <cstdlib>
#include <utility>

namespace creatures1::brain {

namespace {

std::uint32_t normalize_grid_offset(std::uint8_t encoded) {
    return encoded < 0x40u ? encoded : encoded & 0x3fu;
}

std::uint32_t normalize_grid_extent(std::uint8_t encoded) {
    // Genome dimensions occupy the inclusive domain 1..63. The machine code
    // implements ((encoded - 1) % 63) + 1, including the encoded-zero case.
    return ((static_cast<std::uint32_t>(encoded) + 62u) % 63u) + 1u;
}

std::uint32_t minimum_grid_area(StandardLobeIndex lobe_index) {
    switch (lobe_index) {
    case StandardLobeIndex::drive:
    case StandardLobeIndex::verb:
    case StandardLobeIndex::decision:
        return 0x10u;
    case StandardLobeIndex::stimulus_source:
    case StandardLobeIndex::noun:
    case StandardLobeIndex::attention:
        return 0x28u;
    case StandardLobeIndex::general_sensory:
        return 0x20u;
    case StandardLobeIndex::perception:
    case StandardLobeIndex::concept_lobe:
        return 0u;
    }
    return 0u;
}

bool applies_full_grid_fallback(StandardLobeIndex lobe_index) {
    return minimum_grid_area(lobe_index) != 0u;
}

std::uint8_t sample_connection_count(
    const LobeConnectionRule& rule) {
    const int range = static_cast<int>(rule.connection_count_max) -
                      static_cast<int>(rule.connection_count_min);
    const int sample_limit = range + 1;
    const int first_sample = std::rand() % sample_limit;

    int offset = first_sample;
    switch (rule.count_distribution) {
    case ConnectionCountDistribution::uniform:
        break;
    case ConnectionCountDistribution::two_sample_average:
        offset = (first_sample + std::rand() % sample_limit) / 2;
        break;
    case ConnectionCountDistribution::two_sample_absolute_difference:
        offset = first_sample - std::rand() % sample_limit;
        offset = offset < 0 ? -offset : offset;
        break;
    case ConnectionCountDistribution::two_sample_complement_difference:
        offset = first_sample - std::rand() % sample_limit;
        offset = offset < 0 ? -offset : offset;
        offset = range - offset;
        break;
    }

    return static_cast<std::uint8_t>(
        static_cast<int>(rule.connection_count_min) + offset);
}

constexpr std::size_t kRuleRegisterCount = 22;
constexpr std::uint8_t kFirstOperator =
    static_cast<std::uint8_t>(RuleToken::stop_if_zero);

using RuleRegisters = std::array<std::uint8_t, kRuleRegisterCount>;

std::uint8_t evaluate_rule_expression(const LobeRuleExpression& expression,
                                      const RuleRegisters& registers) {
    std::uint8_t accumulator = 0;
    std::size_t token_index = 0;
    while (token_index < expression.tokens.size()) {
        const std::uint8_t token = expression.tokens[token_index];
        if (token == static_cast<std::uint8_t>(RuleToken::end)) {
            break;
        }

        if (token < kFirstOperator) {
            if (token < registers.size()) {
                accumulator = registers[token];
            }
            ++token_index;
            continue;
        }

        switch (static_cast<RuleToken>(token)) {
        case RuleToken::stop_if_zero:
            if (accumulator == 0) {
                return accumulator;
            }
            ++token_index;
            break;
        case RuleToken::saturating_add:
        case RuleToken::saturating_subtract:
        case RuleToken::q8_multiply: {
            if (token_index + 1 >= expression.tokens.size()) {
                return accumulator;
            }
            const std::uint8_t operand_token =
                expression.tokens[token_index + 1];
            const std::uint8_t operand =
                operand_token < registers.size() ? registers[operand_token] : 0;
            if (static_cast<RuleToken>(token) == RuleToken::saturating_add) {
                const unsigned int sum = accumulator + operand;
                accumulator = static_cast<std::uint8_t>(
                    std::min(sum, static_cast<unsigned int>(0xff)));
            } else if (static_cast<RuleToken>(token) ==
                       RuleToken::saturating_subtract) {
                accumulator = accumulator < operand
                                  ? 0
                                  : static_cast<std::uint8_t>(accumulator - operand);
            } else {
                accumulator = static_cast<std::uint8_t>(
                    (static_cast<unsigned int>(accumulator) * operand) >> 8);
            }
            token_index += 2;
            break;
        }
        case RuleToken::saturating_increment:
            if (accumulator != 0xff) {
                ++accumulator;
            }
            ++token_index;
            break;
        case RuleToken::saturating_decrement:
            if (accumulator != 0) {
                --accumulator;
            }
            ++token_index;
            break;
        case RuleToken::reserved_28:
        case RuleToken::reserved_29:
        default:
            // The native switch has no arm for 0x1c or 0x1d; both simply
            // advance the cursor.
            ++token_index;
            break;
        }
    }
    return accumulator;
}

bool selector_matches_tick(std::uint8_t selector, std::uint32_t tick) {
    const std::size_t index = selector >> 3;
    return (g_biochemistry_tick_selector_masks[index] & tick) ==
           g_biochemistry_tick_selector_masks[index];
}

std::uint8_t decay_byte(std::uint8_t value, std::uint8_t selector) {
    const std::size_t index = selector >> 3;
    return static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(value) *
         g_biochemistry_tick_selector_q16_multipliers[index]) >> 16);
}

std::uint8_t random_inclusive(std::uint8_t minimum, std::uint8_t maximum) {
    const int range = static_cast<int>(maximum) -
                      static_cast<int>(minimum) + 1;
    return static_cast<std::uint8_t>(
        static_cast<int>(minimum) + std::rand() % range);
}

} // namespace

Lobe::Lobe() = default;

Lobe::~Lobe() = default;

void Lobe::load_genome(Genome& genome, StandardLobeIndex lobe_index) {
    grid_x_offset = normalize_grid_offset(genome.read_next_gene_byte());
    grid_y_offset = normalize_grid_offset(genome.read_next_gene_byte());

    grid_width = normalize_grid_extent(genome.read_next_gene_byte());
    grid_height = normalize_grid_extent(genome.read_next_gene_byte());

    if (grid_x_offset + grid_width > 0x40u) {
        grid_x_offset = 0x40u - grid_width;
    }
    if (grid_y_offset + grid_height > 0x40u) {
        grid_y_offset = 0x40u - grid_height;
    }

    const std::uint32_t minimum_area = minimum_grid_area(lobe_index);
    while (minimum_area != 0u && grid_width * grid_height < minimum_area) {
        ++grid_width;
    }

    const std::uint32_t grid_area = grid_width * grid_height;
    if (applies_full_grid_fallback(lobe_index) && grid_area < 0x400u) {
        grid_width = 0x20u;
        grid_height = 0x20u;
    }

    auto encoded_perception_state = genome.read_next_gene_byte();
    if (encoded_perception_state > 2u) {
        encoded_perception_state = static_cast<std::uint8_t>(
            encoded_perception_state % 3u);
    }
    percept_state = static_cast<PerceptionCopyState>(encoded_perception_state);

    activation_threshold = genome.read_next_gene_byte();
    activation_relaxation_selector = genome.read_next_gene_byte();
    activation_baseline = genome.read_next_gene_byte();
    input_gain = genome.read_next_gene_byte();
    lobe_expression.load_from_genome(genome);
    winner_take_all_flags = genome.read_next_gene_byte();

    for (LobeConnectionRule& rule : connection_rules) {
        rule.load_from_genome(genome);
    }
}

void Lobe::allocate_runtime_state() {
    neuron_count = grid_width * grid_height;
    neurons = std::make_unique<LobeNeuron[]>(neuron_count);
    active_neurons = std::make_unique<LobeNeuron*[]>(neuron_count);
    connections.reset();
    total_connection_count = 0;

    for (std::uint32_t y = 0; y < grid_height; ++y) {
        for (std::uint32_t x = 0; x < grid_width; ++x) {
            LobeNeuron& neuron = neurons[y * grid_width + x];
            neuron.grid_x = static_cast<std::uint8_t>(x);
            neuron.grid_y = static_cast<std::uint8_t>(y);
            neuron.rule0_connection_count = sample_connection_count(
                connection_rules[0]);
            neuron.rule1_connection_count = sample_connection_count(
                connection_rules[1]);
            total_connection_count += neuron.rule0_connection_count;
            total_connection_count += neuron.rule1_connection_count;
        }
    }

    if (total_connection_count != 0) {
        connections = std::make_unique<LobeConnection[]>(
            total_connection_count);
    }

    LobeConnection* connection_cursor = connections.get();
    for (std::uint32_t index = 0; index < neuron_count; ++index) {
        LobeNeuron& neuron = neurons[index];
        neuron.rule0_connections_begin = connection_cursor;
        connection_cursor += neuron.rule0_connection_count;
        neuron.rule1_connections_begin = connection_cursor;
        connection_cursor += neuron.rule1_connection_count;
    }
}

void Lobe::update_early_phase(const Brain* brain, std::uint32_t tick) {
    (void)brain;

    // A lobe is updated only after its runtime neuron array has been
    // allocated; the original path therefore has a non-zero neuron count.
    const std::uint32_t count = neuron_count;
    std::uint32_t active_count = 0;
    active_neuron_count_ = 0;

    for (std::uint32_t index = 0; index < count; ++index) {
        LobeNeuron& neuron = neurons[index];
        const int activation_minus_threshold =
            static_cast<int>(neuron.activation) -
            static_cast<int>(activation_threshold);

        if (activation_minus_threshold < 1) {
            neuron.firing_strength = 0;
        } else {
            neuron.firing_strength =
                static_cast<std::uint8_t>(activation_minus_threshold);
            ++active_count;
            active_neurons[active_neuron_count_] = &neuron;
            ++active_neuron_count_;
        }

        std::uint8_t current_activation = neuron.activation;
        if (current_activation != activation_baseline) {
            const std::uint8_t selector = activation_relaxation_selector >> 3;
            const std::uint32_t selector_mask =
                g_biochemistry_tick_selector_masks[selector];
            if ((selector_mask & tick) == selector_mask) {
                const int difference =
                    static_cast<int>(current_activation) -
                    static_cast<int>(activation_baseline);
                const std::uint32_t magnitude =
                    static_cast<std::uint32_t>(difference < 0 ? -difference : difference);
                const auto relaxation_delta = static_cast<std::int8_t>(
                    (magnitude *
                     g_biochemistry_tick_selector_q16_multipliers[selector]) >> 16);
                if (activation_baseline < current_activation) {
                    current_activation = static_cast<std::uint8_t>(
                        static_cast<int>(activation_baseline) + relaxation_delta);
                } else {
                    current_activation = static_cast<std::uint8_t>(
                        static_cast<int>(activation_baseline) - relaxation_delta);
                }
            }
        }
        neuron.activation = current_activation;
    }

    const int active_fraction_raw =
        static_cast<int>(active_count << 8) / static_cast<int>(count);
    active_fraction = static_cast<std::uint8_t>(active_fraction_raw > 0xff
                                                    ? 0xff
                                                    : active_fraction_raw);

    if (active_neuron_count_ != 0 &&
        (winner_take_all_flags &
         static_cast<std::uint8_t>(WinnerTakeAllFlag::enabled)) != 0) {
        LobeNeuron* winner = &neurons[0];
        std::uint8_t winner_strength = 0;
        for (std::uint32_t index = 0; index < count; ++index) {
            LobeNeuron& neuron = neurons[index];
            if (neuron.winner_take_all_excluded == 0 &&
                winner_strength < neuron.firing_strength) {
                winner = &neuron;
                winner_strength = neuron.firing_strength;
            }
            neuron.firing_strength = 0;
        }
        if (winner_strength != 0) {
            winner->firing_strength = winner_strength;
        }
    }
}

void Lobe::update_late_phase(Brain* brain, std::uint32_t tick) {
    RuleRegisters registers{};
    registers[5] = late_phase_runtime_state.token_evaluation_state[0];
    registers[6] = late_phase_runtime_state.token_evaluation_state[1];
    registers[7] = late_phase_runtime_state.token_evaluation_state[2];
    registers[8] = late_phase_runtime_state.token_evaluation_state[3];
    registers[0] = 0;
    registers[1] = 0;
    registers[2] = 1;
    registers[3] = 0x40;
    registers[4] = 0xff;
    registers[9] = 0;
    registers[10] = 0;
    registers[11] = 0;
    registers[12] = 0;
    registers[13] = 0;
    registers[14] = 0;
    registers[15] = 0;

    active_neuron_count_ = 0;
    std::array<std::uint32_t, 2> migration_candidates{0xffffffffu,
                                                       0xffffffffu};
    for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
        late_phase_runtime_state.per_rule_loose_dendrite_count[rule_index] =
            connection_rules[rule_index].connection_mode ==
                    ConnectionMode::attach_loose_dendrites
                ? 0xff
                : 0;
    }

    for (std::uint32_t neuron_index = 0; neuron_index < neuron_count;
         ++neuron_index) {
        LobeNeuron& neuron = neurons[neuron_index];
        registers[9] = neuron.activation;
        registers[10] = neuron.firing_strength;
        registers[11] = activation_threshold;

        // One input-register pair per connection rule: register 12+i is the
        // gain-scaled weighted sum of that rule's firing targets, and 14+i is
        // the same value gated on *every* target firing.  The native loop
        // stops at the first rule with no connections rather than skipping it,
        // so an empty rule leaves its pair holding the previous neuron's
        // values instead of zeroes.
        for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
            LobeConnection* connection_array =
                rule_index == 0 ? neuron.rule0_connections_begin
                                : neuron.rule1_connections_begin;
            const std::uint8_t connection_count =
                rule_index == 0 ? neuron.rule0_connection_count
                                : neuron.rule1_connection_count;
            if (connection_count == 0) {
                break;
            }

            int weighted_sum = 0;
            std::uint8_t all_targets_firing = 0xff;
            for (std::uint8_t index = 0; index < connection_count; ++index) {
                const LobeConnection& input = connection_array[index];
                const std::uint8_t target_strength =
                    input.target_neuron == nullptr
                        ? 0
                        : input.target_neuron->firing_strength;
                if (target_strength == 0) {
                    all_targets_firing = 0;
                } else {
                    weighted_sum +=
                        (input.target_weight * target_strength) >> 8;
                }
            }
            weighted_sum = std::clamp(weighted_sum, 0, 0xff);
            const std::uint8_t scaled_sum = static_cast<std::uint8_t>(
                (static_cast<unsigned int>(input_gain) * weighted_sum) >> 8);
            registers[12 + rule_index] = scaled_sum;
            registers[14 + rule_index] =
                static_cast<std::uint8_t>(all_targets_firing & scaled_sum);
        }

        registers[9] = evaluate_rule_expression(lobe_expression, registers);
        neuron.activation = registers[9];
        const int strength = static_cast<int>(registers[9]) -
                             static_cast<int>(activation_threshold);
        if (strength < 1) {
            neuron.firing_strength = 0;
        } else {
            neuron.firing_strength = static_cast<std::uint8_t>(strength);
            if (active_neurons != nullptr) {
                active_neurons[active_neuron_count_] = &neuron;
            }
            ++active_neuron_count_;
        }

        if (neuron.activation != activation_baseline &&
            selector_matches_tick(activation_relaxation_selector, tick)) {
            const int difference = static_cast<int>(neuron.activation) -
                                   static_cast<int>(activation_baseline);
            const std::uint32_t magnitude =
                static_cast<std::uint32_t>(difference < 0 ? -difference
                                                          : difference);
            const std::size_t selector = activation_relaxation_selector >> 3;
            const std::uint8_t delta = static_cast<std::uint8_t>(
                (magnitude * g_biochemistry_tick_selector_q16_multipliers[
                                  selector]) >>
                16);
            neuron.activation = activation_baseline < neuron.activation
                                    ? static_cast<std::uint8_t>(
                                          activation_baseline + delta)
                                    : static_cast<std::uint8_t>(
                                          activation_baseline - delta);
        }

        for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
            LobeConnectionRule& rule = connection_rules[rule_index];
            LobeConnection* connection_array =
                rule_index == 0 ? neuron.rule0_connections_begin
                                : neuron.rule1_connections_begin;
            const std::uint8_t connection_count =
                rule_index == 0 ? neuron.rule0_connection_count
                                : neuron.rule1_connection_count;
            int loose_connection_count = 0;
            for (std::uint8_t connection_index = 0;
                 connection_index < connection_count; ++connection_index) {
                LobeConnection& connection = connection_array[connection_index];
                std::uint8_t signal = evaluate_rule_expression(
                    rule.current_weight_expression, registers);
                if (connection.current_weight < signal) {
                    const std::uint8_t gap = static_cast<std::uint8_t>(
                        signal - connection.current_weight);
                    connection.current_weight = static_cast<std::uint8_t>(
                        connection.current_weight + (gap >> 3));
                } else if (connection.current_weight != 0 &&
                           selector_matches_tick(rule.current_weight_decay_selector,
                                                 tick)) {
                    connection.current_weight = decay_byte(
                        connection.current_weight,
                        rule.current_weight_decay_selector);
                }

                if (connection.current_weight != 0) {
                    const std::uint8_t target_signal = evaluate_rule_expression(
                        rule.target_weight_expression, registers);
                    const std::uint32_t weighted_signal =
                        (static_cast<std::uint32_t>(connection.current_weight) *
                         target_signal) >>
                        8;
                    const int headroom = static_cast<int>(connection.target_weight) -
                                         static_cast<int>(connection.baseline_weight);
                    if (headroom < static_cast<int>(weighted_signal)) {
                        const std::uint32_t raised =
                            static_cast<std::uint32_t>(connection.target_weight) +
                            weighted_signal - static_cast<int>(headroom);
                        connection.target_weight = static_cast<std::uint8_t>(
                            std::min(raised, static_cast<std::uint32_t>(0xff)));
                    }
                }

                if (connection.target_weight != connection.baseline_weight) {
                    if (selector_matches_tick(rule.target_weight_convergence_selector,
                                               tick)) {
                        const int difference =
                            static_cast<int>(connection.target_weight) -
                            static_cast<int>(connection.baseline_weight);
                        const std::uint32_t magnitude =
                            static_cast<std::uint32_t>(difference < 0 ? -difference
                                                                      : difference);
                        const std::size_t selector =
                            rule.target_weight_convergence_selector >> 3;
                        const std::uint8_t delta = static_cast<std::uint8_t>(
                            (magnitude *
                             g_biochemistry_tick_selector_q16_multipliers[
                                 selector]) >>
                            16);
                        connection.target_weight =
                            connection.baseline_weight < connection.target_weight
                                ? static_cast<std::uint8_t>(
                                      connection.baseline_weight + delta)
                                : static_cast<std::uint8_t>(
                                      connection.baseline_weight - delta);
                    }
                    if (connection.target_weight != connection.baseline_weight &&
                        rule.baseline_weight_step_interval != 0 &&
                        tick % rule.baseline_weight_step_interval == 0) {
                        if (connection.baseline_weight < connection.target_weight) {
                            ++connection.baseline_weight;
                        } else {
                            --connection.baseline_weight;
                        }
                    }
                }

                if (rule.dendrite_growth_interval != 0 &&
                    connection.dendrite_state != 0xff &&
                    tick % rule.dendrite_growth_interval == 0) {
                    const std::uint8_t growth = evaluate_rule_expression(
                        rule.dendrite_growth_expression, registers);
                    if (connection.dendrite_state < growth) {
                        connection.dendrite_state = growth;
                    }
                }
                if (rule.dendrite_decay_interval != 0 &&
                    connection.dendrite_state != 0 &&
                    tick % rule.dendrite_decay_interval == 0 &&
                    evaluate_rule_expression(rule.dendrite_decay_expression,
                                             registers) != 0) {
                    --connection.dendrite_state;
                    if (connection.dendrite_state == 0) {
                        ++loose_connection_count;
                        connection.current_weight = 0;
                        connection.target_weight = 0;
                        connection.baseline_weight = 0;
                        neuron.firing_strength = 0;
                        neuron.activation = 0;
                    }
                }
            }

            if (rule.connection_mode == ConnectionMode::attach_loose_dendrites) {
                if (loose_connection_count <
                    late_phase_runtime_state.per_rule_loose_dendrite_count[
                        rule_index]) {
                    late_phase_runtime_state.per_rule_loose_dendrite_count[
                        rule_index] = static_cast<std::uint8_t>(
                        loose_connection_count);
                }
            } else if (loose_connection_count != 0) {
                auto& loose_count =
                    late_phase_runtime_state.per_rule_loose_dendrite_count[
                        rule_index];
                if (loose_count != 0xff) {
                    ++loose_count;
                }
                if (neuron_index <
                    (rule_index == 0 ? rule0_migration_candidate_neuron_index
                                     : rule1_migration_candidate_neuron_index)) {
                    if (rule_index == 0) {
                        rule0_migration_candidate_neuron_index = neuron_index;
                    } else {
                        rule1_migration_candidate_neuron_index = neuron_index;
                    }
                    migration_candidates[rule_index] = neuron_index;
                }
            }
        }
    }

    active_fraction = neuron_count == 0
                          ? 0
                          : static_cast<std::uint8_t>(std::min(
                                (active_neuron_count_ << 8) / neuron_count,
                                static_cast<std::uint32_t>(0xff)));

    if (active_neuron_count_ != 0 &&
        (winner_take_all_flags & static_cast<std::uint8_t>(
             WinnerTakeAllFlag::enabled)) != 0) {
        LobeNeuron* winner = nullptr;
        std::uint8_t winner_strength = 0;
        for (std::uint32_t index = 0; index < neuron_count; ++index) {
            LobeNeuron& neuron = neurons[index];
            if (neuron.winner_take_all_excluded == 0 &&
                winner_strength < neuron.firing_strength) {
                winner = &neuron;
                winner_strength = neuron.firing_strength;
            }
            neuron.firing_strength = 0;
        }
        if (winner != nullptr && winner_strength != 0) {
            winner->firing_strength = winner_strength;
        }
    }

    if (brain == nullptr) {
        return;
    }
    for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
        LobeConnectionRule& rule = connection_rules[rule_index];
        if (rule.connection_mode == ConnectionMode::attach_loose_dendrites &&
            rule.target_lobe_index < 32) {
            Lobe& target_lobe = brain->lobe(rule.target_lobe_index);
            if (target_lobe.active_neuron_count() != 0) {
                for (std::uint32_t neuron_index = 0;
                     neuron_index < neuron_count; ++neuron_index) {
                    LobeNeuron& neuron = neurons[neuron_index];
                    LobeConnection* connection_array = rule_index == 0
                                                       ? neuron.rule0_connections_begin
                                                       : neuron.rule1_connections_begin;
                    const std::uint8_t count = rule_index == 0
                                                   ? neuron.rule0_connection_count
                                                   : neuron.rule1_connection_count;
                    if (count == 0 || connection_array == nullptr) {
                        continue;
                    }
                    // A dendrite at state 0 is only a CANDIDATE; attaching it
                    // also costs from this rule's loose-dendrite budget, which
                    // is replenished only when a dendrite actually retires this
                    // tick.  Native's failure branch says so in as many words --
                    // DebugLog("Migrate failed - no loose dens") -- and the
                    // budget is what "loose dens" counts.
                    //
                    // Without the budget check the port attached one dendrite
                    // per neuron per tick for as long as any zero-state
                    // connection remained, steadily filling every historically
                    // retired dendrite in a loaded world.  dork's decision lobe
                    // has 67 of its 128 rule-0 dendrites at zero, and the port
                    // was working through them at ~3 dendrite_state apiece
                    // while the shipped binary attached none.
                    if (late_phase_runtime_state
                            .per_rule_loose_dendrite_count[rule_index] == 0) {
                        continue;
                    }
                    LobeConnection* loose = nullptr;
                    for (std::uint8_t index = 0; index < count; ++index) {
                        if (connection_array[index].dendrite_state == 0) {
                            loose = &connection_array[index];
                            break;
                        }
                    }
                    if (loose == nullptr) {
                        continue;
                    }
                    LobeNeuron* selected = target_lobe.active_neuron(
                        static_cast<std::uint32_t>(std::rand() %
                                                    target_lobe.active_neuron_count()));
                    bool duplicate = false;
                    for (std::uint8_t index = 0; index < count; ++index) {
                        if (connection_array[index].target_neuron == selected) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (duplicate) {
                        continue;
                    }
                    loose->target_neuron = selected;
                    loose->baseline_weight = random_inclusive(
                        rule.baseline_weight_min, rule.baseline_weight_max);
                    loose->target_weight = loose->baseline_weight;
                    loose->dendrite_state = random_inclusive(
                        rule.dendrite_state_min, rule.dendrite_state_max);
                    auto& remaining =
                        late_phase_runtime_state.per_rule_loose_dendrite_count[
                            rule_index];
                    if (remaining != 0) {
                        --remaining;
                    }
                }
            }
        }
    }

    for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
        LobeConnectionRule& rule = connection_rules[rule_index];
        if (rule.connection_mode != ConnectionMode::migrate_connections) {
            continue;
        }
        const std::uint32_t candidate = migration_candidates[rule_index];
        if (candidate != 0xffffffffu && candidate < neuron_count) {
            migrate_rule_connections(neurons[candidate], *brain, *this,
                                     static_cast<int>(rule_index));
        }
    }
}

const LobeConnectionRule& Lobe::connection_rule(std::size_t index) const {
    return connection_rules[index];
}

LobeConnectionRule& Lobe::connection_rule(std::size_t index) {
    return connection_rules[index];
}

LobeLatePhaseRuntimeState& Lobe::late_phase_state() {
    return late_phase_runtime_state;
}

std::uint32_t Lobe::active_neuron_count() const {
    return active_neuron_count_;
}

LobeNeuron* Lobe::active_neuron(std::uint32_t index) const {
    return active_neurons[index];
}

void Lobe::swap_active_neurons(std::uint32_t first, std::uint32_t second) {
    std::swap(active_neurons[first], active_neurons[second]);
}

std::uint32_t Lobe::neuron_count_value() const {
    return neuron_count;
}

std::uint32_t Lobe::grid_width_value() const {
    return grid_width;
}

std::uint32_t Lobe::grid_height_value() const {
    return grid_height;
}

LobeNeuron& Lobe::neuron(std::uint32_t index) {
    return neurons[index];
}

const LobeNeuron& Lobe::neuron(std::uint32_t index) const {
    return neurons[index];
}

std::uint32_t Lobe::grid_x_offset_value() const {
    return grid_x_offset;
}

std::uint32_t Lobe::grid_y_offset_value() const {
    return grid_y_offset;
}

std::uint8_t* Lobe::resolve_genome_locus(GenomeLocusKind kind,
                                         std::uint8_t locus_index) {
    if (kind == GenomeLocusKind::receptor) {
        switch (locus_index) {
        case 0:
            return &activation_threshold;
        case 1:
            return &activation_relaxation_selector;
        case 2:
            return &activation_baseline;
        case 3:
            return &connection_rules[0].current_weight_decay_selector;
        case 4:
            return &connection_rules[0].target_weight_convergence_selector;
        case 5:
            return &connection_rules[0].baseline_weight_step_interval;
        case 6:
            return &connection_rules[0].dendrite_growth_interval;
        case 7:
            return &connection_rules[0].dendrite_decay_interval;
        case 8:
            return &connection_rules[1].current_weight_decay_selector;
        case 9:
            return &connection_rules[1].target_weight_convergence_selector;
        case 10:
            return &connection_rules[1].baseline_weight_step_interval;
        case 11:
            return &connection_rules[1].dendrite_growth_interval;
        case 12:
            return &connection_rules[1].dendrite_decay_interval;
        case 13:
        case 14:
        case 15:
        case 16:
            return &late_phase_runtime_state.token_evaluation_state[
                locus_index - 13];
        default:
            break;
        }

        if (locus_index >= 17 && locus_index <= 32) {
            const std::uint32_t neuron_index = locus_index - 17;
            return neuron_index < neuron_count
                       ? &neurons[neuron_index].activation
                       : nullptr;
        }
        return nullptr;
    }

    if (locus_index <= 2) {
        return &late_phase_runtime_state.token_evaluation_state[locus_index];
    }
    if (locus_index >= 3 && locus_index <= 18) {
        const std::uint32_t neuron_index = locus_index - 3;
        return neuron_index < neuron_count
                   ? &neurons[neuron_index].firing_strength
                   : nullptr;
    }
    return nullptr;
}

void Lobe::add_neuron_activation(std::uint32_t index,
                                 std::uint8_t activation_delta) {
    if (index >= neuron_count) {
        return;
    }

    LobeNeuron& target = neurons[index];
    const std::uint32_t sum =
        static_cast<std::uint32_t>(target.activation) + activation_delta;
    target.activation = static_cast<std::uint8_t>(sum > 0xffu ? 0xffu : sum);
}

void Lobe::set_neuron_activation(std::uint32_t index,
                                 std::uint8_t activation) {
    if (index >= neuron_count) {
        return;
    }
    neurons[index].activation = activation;
    // The native trig stores the same byte in the adjacent firing-strength
    // lane of the 16-byte neuron record. Keep that image-level pairing behind
    // the Lobe owner instead of exposing a byte offset to Macro.
    neurons[index].firing_strength = activation;
}

void Lobe::reset_runtime_activity() {
    for (std::uint32_t index = 0; index < neuron_count; ++index) {
        LobeNeuron& target = neurons[index];
        target.firing_strength = 0;
        target.activation = 0;

        for (std::size_t rule_index = 0; rule_index < 2; ++rule_index) {
            LobeConnection* connection_array = rule_index == 0
                                              ? target.rule0_connections_begin
                                              : target.rule1_connections_begin;
            const std::uint8_t count = rule_index == 0
                                            ? target.rule0_connection_count
                                            : target.rule1_connection_count;
            for (std::uint8_t connection_index = 0;
                 connection_index < count; ++connection_index) {
                LobeConnection& connection = connection_array[connection_index];
                connection.current_weight = 0;
                connection.target_weight = connection.baseline_weight;
            }
        }
    }
}

} // namespace creatures1::brain
