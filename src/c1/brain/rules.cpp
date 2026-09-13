#include "lobe.hpp"
#include "rules.hpp"
#include <cstddef>

namespace creatures1::brain {

namespace {
constexpr std::uint8_t kRuleTokenEnd = 30;

std::uint8_t normalize_range_max(std::uint8_t minimum,
                                 std::uint8_t encoded_maximum) {
    if (encoded_maximum >= minimum) {
        return encoded_maximum;
    }

    // The original byte-domain wrap is modulo (256 - minimum), then shifted
    // back into the inclusive range beginning at minimum.
    const auto range = static_cast<unsigned int>(0x100u - minimum);
    return static_cast<std::uint8_t>(minimum + (encoded_maximum % range));
}

std::uint8_t normalize_rule_token(std::uint8_t raw_token) {
    return raw_token > (kRuleTokenEnd - 1)
               ? static_cast<std::uint8_t>(raw_token % kRuleTokenEnd)
               : raw_token;
}
} // namespace

void LobeRuleExpression::load_from_genome(Genome& genome) {
    for (std::size_t index = 0; index < 8; ++index) {
        tokens[index] = normalize_rule_token(genome.read_next_gene_byte());
    }
    tokens[8] = kRuleTokenEnd;
    tokens[9] = kRuleTokenEnd;
}

void LobeConnectionRule::load_from_genome(Genome& genome) {
    target_lobe_index = genome.read_next_gene_byte();

    connection_count_min = genome.read_next_gene_byte();
    connection_count_max = normalize_range_max(
        connection_count_min, genome.read_next_gene_byte());

    const auto encoded_distribution = genome.read_next_gene_byte();
    count_distribution = static_cast<ConnectionCountDistribution>(
        encoded_distribution < 4u ? encoded_distribution
                                   : encoded_distribution & 3u);

    auto spread_radius = genome.read_next_gene_byte();
    if (spread_radius > 8u) {
        spread_radius = static_cast<std::uint8_t>(spread_radius % 9u);
    }
    target_cell_spread_radius = spread_radius;

    baseline_weight_min = genome.read_next_gene_byte();
    baseline_weight_max = normalize_range_max(
        baseline_weight_min, genome.read_next_gene_byte());

    dendrite_state_min = genome.read_next_gene_byte();
    dendrite_state_max = normalize_range_max(
        dendrite_state_min, genome.read_next_gene_byte());

    auto encoded_mode = genome.read_next_gene_byte();
    if (encoded_mode > 2u) {
        encoded_mode = static_cast<std::uint8_t>(encoded_mode % 3u);
    }
    connection_mode = static_cast<ConnectionMode>(encoded_mode);

    current_weight_decay_selector = genome.read_next_gene_byte();
    target_weight_convergence_selector = genome.read_next_gene_byte();
    baseline_weight_step_interval = genome.read_next_gene_byte();
    dendrite_growth_interval = genome.read_next_gene_byte();

    dendrite_growth_expression.load_from_genome(genome);
    dendrite_decay_interval = genome.read_next_gene_byte();
    dendrite_decay_expression.load_from_genome(genome);
    current_weight_expression.load_from_genome(genome);
    target_weight_expression.load_from_genome(genome);
}

} // namespace creatures1::brain
