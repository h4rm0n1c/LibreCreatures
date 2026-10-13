#include "lobe.hpp"
#include "rules.hpp"
#include <cstddef>

namespace creatures1::brain {

namespace {
// Genome rule bytes are folded into the 0..29 token domain.
constexpr std::uint8_t kSVRuleTokenModulus = 30;

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
    return raw_token > (kSVRuleTokenModulus - 1)
               ? static_cast<std::uint8_t>(raw_token % kSVRuleTokenModulus)
               : raw_token;
}
} // namespace

void SVRule::load_from_genome(Genome& genome) {
    for (std::size_t index = 0; index < 8; ++index) {
        tokens[index] = normalize_rule_token(genome.read_next_gene_byte());
    }
    // Native CLobeRuleExpression::LoadFromGenome @00402250 pads the 10-byte
    // runtime expression with two END tokens, and END is token 0 (the
    // evaluator stops on MOV AL,[ESI]; TEST AL,AL; JNZ at 004053b4).  30 is
    // only the normalisation modulus, not a terminator.
    tokens[8] = static_cast<std::uint8_t>(SVRuleToken::end);
    tokens[9] = static_cast<std::uint8_t>(SVRuleToken::end);
}

void DendriteType::load_from_genome(Genome& genome) {
    source_lobe = genome.read_next_gene_byte();

    min_dendrites = genome.read_next_gene_byte();
    max_dendrites = normalize_range_max(
        min_dendrites, genome.read_next_gene_byte());

    const auto encoded_distribution = genome.read_next_gene_byte();
    spread = static_cast<DendriteSpread>(
        encoded_distribution < 4u ? encoded_distribution
                                   : encoded_distribution & 3u);

    auto spread_radius = genome.read_next_gene_byte();
    if (spread_radius > 8u) {
        spread_radius = static_cast<std::uint8_t>(spread_radius % 9u);
    }
    fanout = spread_radius;

    min_ltw = genome.read_next_gene_byte();
    max_ltw = normalize_range_max(
        min_ltw, genome.read_next_gene_byte());

    min_strength = genome.read_next_gene_byte();
    max_strength = normalize_range_max(
        min_strength, genome.read_next_gene_byte());

    auto encoded_mode = genome.read_next_gene_byte();
    if (encoded_mode > 2u) {
        encoded_mode = static_cast<std::uint8_t>(encoded_mode % 3u);
    }
    migration = static_cast<MigrationMode>(encoded_mode);

    relax_suscept = genome.read_next_gene_byte();
    relax_stw = genome.read_next_gene_byte();
    ltw_gain_rate = genome.read_next_gene_byte();
    gain_rate = genome.read_next_gene_byte();

    gain_svrule.load_from_genome(genome);
    lose_rate = genome.read_next_gene_byte();
    lose_svrule.load_from_genome(genome);
    suscept_svrule.load_from_genome(genome);
    reinforce_svrule.load_from_genome(genome);
}

} // namespace creatures1::brain
