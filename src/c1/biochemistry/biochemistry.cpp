#include "biochemistry.hpp"

#include "../../../include/C1BiochemistrySelectorData.hpp"

#include <algorithm>

namespace creatures1::biochemistry {

namespace {

constexpr std::uint32_t kChemicalDebugCategory = 0x10u;
constexpr std::uint8_t kInvertSourceLocus = 0x04u;
constexpr std::uint8_t kFixedEmissionAmount = 0x02u;
constexpr std::uint8_t kClearSourceLocus = 0x01u;
constexpr std::uint8_t kReduceReceptorSignal = 0x01u;
constexpr std::uint8_t kDigitalReceptorSignal = 0x02u;

std::uint8_t saturating_add(std::uint8_t current, std::uint32_t amount) {
    return static_cast<std::uint8_t>(
        std::min<std::uint32_t>(0xffu,
                                static_cast<std::uint32_t>(current) + amount));
}

std::uint8_t saturating_subtract(std::uint8_t current,
                                 std::uint32_t amount) {
    return amount >= current
               ? 0
               : static_cast<std::uint8_t>(current - amount);
}

void log_chemical_change(creatures1::common::DebugLogHost* log_host,
                         const char* format,
                         std::uint32_t amount,
                         std::uint32_t chemical_index,
                         std::uint8_t concentration) {
    if (log_host == nullptr ||
        !log_host->debug_category_enabled(kChemicalDebugCategory)) {
        return;
    }
    creatures1::common::debug_log(
        *log_host, kChemicalDebugCategory, format, amount, chemical_index,
        static_cast<unsigned int>(concentration));
}

} // namespace

Biochemistry::Biochemistry() {
    chemical_states_.fill({});
    receptor_records_.fill({});
    emitter_records_.fill({});
    owner_ = nullptr;
    receptor_gene_count_ = 0;
    emitter_gene_count_ = 0;
    reaction_gene_count_ = 0;
}

void Biochemistry::add_chemical_moles(
    int chemical_index,
    std::uint32_t moles,
    creatures1::common::DebugLogHost* log_host) {
    if (chemical_index != 0) {
        const std::uint32_t sum =
            (moles & 0xffu) + chemical_states_[chemical_index].concentration;
        chemical_states_[chemical_index].concentration =
            static_cast<std::uint8_t>(std::min(sum, 0xffu));
    }

    if (log_host != nullptr && log_host->debug_category_enabled(0x10u)) {
        creatures1::common::debug_log(
            *log_host, 0x10u,
            "Add %d moles of Chemical %d (conc->%d)\n",
            static_cast<unsigned int>(moles & 0xffu), chemical_index,
            static_cast<unsigned int>(
                chemical_states_[chemical_index].concentration));
    }
}

void Biochemistry::update(
    std::uint32_t tick,
    creatures1::common::DebugLogHost* log_host) {
    // Emitters run first.  Their source pointers are resolved while loading
    // the genome; a null source means that the locus is not available.
    for (std::uint32_t index = 0; index < emitter_gene_count_; ++index) {
        EmitterRecord& emitter = emitter_records_[index];
        if (emitter.source_locus == nullptr) {
            continue;
        }

        std::uint32_t source_value = *emitter.source_locus;
        if ((emitter.flags & kInvertSourceLocus) != 0) {
            source_value = 0xffu - source_value;
        }
        if (source_value == 0 || tick % emitter.emission_period != 0 ||
            source_value <= emitter.threshold) {
            continue;
        }

        const std::uint32_t chemical_index = emitter.chemical_index;
        if (chemical_index != 0) {
            std::uint32_t amount = emitter.emission_amount;
            if ((emitter.flags & kFixedEmissionAmount) == 0) {
                amount = (amount * (source_value - emitter.threshold) >> 8) &
                         0xffu;
            }
            chemical_states_[chemical_index].concentration = saturating_add(
                chemical_states_[chemical_index].concentration, amount);
        }
        log_chemical_change(log_host,
                            "Add %d moles of Chemical %d (conc->%d)\n",
                            emitter.emission_amount, chemical_index,
                            chemical_states_[chemical_index].concentration);

        if ((emitter.flags & kClearSourceLocus) != 0) {
            *emitter.source_locus = 0;
        }
    }

    // Receptors turn chemical excess above threshold into a locus value.
    for (std::uint32_t index = 0; index < receptor_gene_count_; ++index) {
        ReceptorRecord& receptor = receptor_records_[index];
        if (receptor.chemical_index == 0 || receptor.target_locus == nullptr) {
            continue;
        }

        const int excess = static_cast<int>(
            chemical_states_[receptor.chemical_index].concentration) -
            receptor.threshold;
        const std::uint32_t positive_excess = excess < 0 ? 0 : excess;
        std::uint32_t signal = 0;
        if (positive_excess != 0) {
            signal = receptor.gain;
            if ((receptor.flags & kDigitalReceptorSignal) == 0) {
                signal = (positive_excess * signal >> 8) & 0xffu;
            }
        }

        if ((receptor.flags & kReduceReceptorSignal) == 0) {
            *receptor.target_locus = saturating_add(receptor.nominal, signal);
        } else {
            *receptor.target_locus =
                saturating_subtract(receptor.nominal, signal);
        }
    }

    // A reaction consumes the limiting reactant, subject to its tick-rate
    // selector, then produces both products.  Chemical index zero denotes
    // the absent reactant/product and is never modified.
    for (std::uint32_t index = 0; index < reaction_gene_count_; ++index) {
        ReactionRecord& reaction = reaction_records_[index];
        std::uint32_t reactant_1_available = 0xffu;
        if (reaction.reactant_1_chemical != 0) {
            reactant_1_available =
                chemical_states_[reaction.reactant_1_chemical].concentration /
                reaction.reactant_1_amount;
        }
        std::uint32_t reactant_2_available = 0xffu;
        if (reaction.reactant_2_chemical != 0) {
            reactant_2_available =
                chemical_states_[reaction.reactant_2_chemical].concentration /
                reaction.reactant_2_amount;
        }

        const std::uint32_t reaction_extent =
            std::min(reactant_1_available, reactant_2_available);
        if (reaction_extent == 0) {
            continue;
        }

        const std::uint8_t selector = reaction.tick_rate_selector >> 3;
        std::uint32_t gated_extent = reaction_extent;
        if ((g_biochemistry_tick_selector_masks[selector] & tick) ==
            g_biochemistry_tick_selector_masks[selector]) {
            gated_extent =
                reaction_extent *
                g_biochemistry_tick_selector_q16_multipliers[selector] >> 16;
        }
        const std::uint8_t processed_extent =
            static_cast<std::uint8_t>(reaction_extent - gated_extent);

        if (reaction.reactant_1_chemical != 0) {
            const std::uint32_t amount =
                (static_cast<std::uint32_t>(reaction.reactant_1_amount) *
                 processed_extent) & 0xffu;
            chemical_states_[reaction.reactant_1_chemical].concentration =
                saturating_subtract(
                    chemical_states_[reaction.reactant_1_chemical].concentration,
                    amount);
        }
        log_chemical_change(log_host,
                            "Remove %d moles of Chemical %d (conc->%d)\n",
                            (static_cast<std::uint32_t>(
                                 reaction.reactant_1_amount) * processed_extent) &
                                0xffu,
                            reaction.reactant_1_chemical,
                            chemical_states_[reaction.reactant_1_chemical]
                                .concentration);

        if (reaction.reactant_2_chemical != 0) {
            const std::uint32_t amount =
                (static_cast<std::uint32_t>(reaction.reactant_2_amount) *
                 processed_extent) & 0xffu;
            chemical_states_[reaction.reactant_2_chemical].concentration =
                saturating_subtract(
                    chemical_states_[reaction.reactant_2_chemical].concentration,
                    amount);
        }
        log_chemical_change(log_host,
                            "Remove %d moles of Chemical %d (conc->%d)\n",
                            (static_cast<std::uint32_t>(
                                 reaction.reactant_2_amount) * processed_extent) &
                                0xffu,
                            reaction.reactant_2_chemical,
                            chemical_states_[reaction.reactant_2_chemical]
                                .concentration);

        if (reaction.product_1_chemical != 0) {
            const std::uint32_t amount =
                (static_cast<std::uint32_t>(reaction.product_1_amount) *
                 processed_extent) & 0xffu;
            chemical_states_[reaction.product_1_chemical].concentration =
                saturating_add(
                    chemical_states_[reaction.product_1_chemical].concentration,
                    amount);
        }
        log_chemical_change(log_host,
                            "Add %d moles of Chemical %d (conc->%d)\n",
                            (static_cast<std::uint32_t>(
                                 reaction.product_1_amount) * processed_extent) &
                                0xffu,
                            reaction.product_1_chemical,
                            chemical_states_[reaction.product_1_chemical]
                                .concentration);

        if (reaction.product_2_chemical != 0) {
            const std::uint32_t amount =
                (static_cast<std::uint32_t>(reaction.product_2_amount) *
                 processed_extent) & 0xffu;
            chemical_states_[reaction.product_2_chemical].concentration =
                saturating_add(
                    chemical_states_[reaction.product_2_chemical].concentration,
                    amount);
        }
        log_chemical_change(log_host,
                            "Add %d moles of Chemical %d (conc->%d)\n",
                            (static_cast<std::uint32_t>(
                                 reaction.product_2_amount) * processed_extent) &
                                0xffu,
                            reaction.product_2_chemical,
                            chemical_states_[reaction.product_2_chemical]
                                .concentration);
    }

    // Decay is the final phase and examines every chemical, including ones
    // not referenced by a current receptor/emitter/reaction table.
    for (ChemicalState& chemical : chemical_states_) {
        if (chemical.concentration == 0) {
            continue;
        }
        const std::uint8_t selector = chemical.half_life_selector >> 3;
        if ((g_biochemistry_tick_selector_masks[selector] & tick) ==
            g_biochemistry_tick_selector_masks[selector]) {
            chemical.concentration = static_cast<std::uint8_t>(
                static_cast<std::uint32_t>(chemical.concentration) *
                g_biochemistry_tick_selector_q16_multipliers[selector] >>
                16);
        }
    }
}

namespace {

std::uint8_t normalized_locus_domain(std::uint8_t value) {
    return value < 2 ? value : static_cast<std::uint8_t>(value & 1u);
}

std::uint8_t* resolve_locus(BiochemistryLocusHost& host,
                            std::uint8_t domain,
                            creatures1::creatures::GenomeLocusKind kind,
                            std::uint8_t tissue_index,
                            std::uint8_t locus_index) {
    if (domain == static_cast<std::uint8_t>(LocusDomain::brain)) {
        return host.resolve_brain_locus(kind, tissue_index, locus_index);
    }
    if (domain == static_cast<std::uint8_t>(LocusDomain::creature)) {
        return host.resolve_creature_locus(kind, tissue_index, locus_index);
    }
    return nullptr;
}

std::uint8_t normalize_reaction_amount(std::uint8_t value) {
    // The original byte policy maps zero and values above sixteen through
    // the low nibble, then adds one.  This is intentionally not a clamp.
    return static_cast<std::uint8_t>(value - 1u) > 0x0fu
               ? static_cast<std::uint8_t>((value & 0x0fu) + 1u)
               : value;
}

void log_gene_limit(creatures1::common::DebugLogHost* log_host,
                    const char* message) {
    if (log_host != nullptr && log_host->debug_category_enabled(0x200u)) {
        creatures1::common::debug_log(*log_host, 0x200u, "%s", message);
    }
}

} // namespace

void Biochemistry::serialize(
    BiochemistryArchive& archive,
    BiochemistryLocusHost& locus_host,
    creatures1::common::DebugLogHost* log_host) {
    if (log_host != nullptr && log_host->debug_category_enabled(4u)) {
        creatures1::common::debug_log(*log_host, 4u,
                                      "Serialising Biochemistry\n");
    }

    if (archive.is_loading()) {
        owner_ = archive.read_creature_reference();
        emitter_gene_count_ = archive.read_uint32();
        receptor_gene_count_ = archive.read_uint32();
        reaction_gene_count_ = archive.read_uint32();

        for (ChemicalState& chemical : chemical_states_) {
            chemical.concentration = archive.read_byte();
            chemical.half_life_selector = archive.read_byte();
        }

        for (std::uint32_t index = 0; index < emitter_gene_count_; ++index) {
            EmitterRecord& emitter = emitter_records_[index];
            emitter.target_domain = archive.read_byte();
            emitter.tissue_index = archive.read_byte();
            emitter.locus_index = archive.read_byte();
            emitter.chemical_index = archive.read_byte();
            emitter.threshold = archive.read_byte();
            emitter.emission_period = archive.read_byte();
            emitter.emission_amount = archive.read_byte();
            emitter.flags = archive.read_byte();
            emitter.source_locus = resolve_locus(
                locus_host, emitter.target_domain,
                creatures1::creatures::GenomeLocusKind::emitter,
                emitter.tissue_index, emitter.locus_index);
        }

        for (std::uint32_t index = 0; index < receptor_gene_count_; ++index) {
            ReceptorRecord& receptor = receptor_records_[index];
            receptor.target_domain = archive.read_byte();
            receptor.tissue_index = archive.read_byte();
            receptor.locus_index = archive.read_byte();
            receptor.chemical_index = archive.read_byte();
            receptor.threshold = archive.read_byte();
            receptor.nominal = archive.read_byte();
            receptor.gain = archive.read_byte();
            receptor.flags = archive.read_byte();
            receptor.target_locus = resolve_locus(
                locus_host, receptor.target_domain,
                creatures1::creatures::GenomeLocusKind::receptor,
                receptor.tissue_index, receptor.locus_index);
            if (receptor.target_locus != nullptr) {
                *receptor.target_locus = receptor.nominal;
            }
        }

        for (std::uint32_t index = 0; index < reaction_gene_count_; ++index) {
            ReactionRecord& reaction = reaction_records_[index];
            // The archived record is the runtime struct dumped byte for
            // byte, and CBiochemistryReactionRecord puts the tick-rate
            // selector at offset 4 -- in the MIDDLE, not at the end:
            //   0 r1_amount  1 r1_chemical  2 r2_amount  3 r2_chemical
            //   4 tick_rate_selector
            //   5 p1_amount  6 p1_chemical  7 p2_amount  8 p2_chemical
            // Reading the selector last shifted every byte from 4 onward by
            // one, so a reaction that should consume chem58 to make chem59
            // and chem35 instead made bulk chem1.  That is why loaded
            // creatures lost their life force: nothing replenished chemical
            // 59 and its ordinary decay took it to zero within seconds.
            //
            // The genome gene has its own byte order and is read separately
            // below; only this raw-record path was wrong.
            reaction.reactant_1_amount = archive.read_byte();
            reaction.reactant_1_chemical = archive.read_byte();
            reaction.reactant_2_amount = archive.read_byte();
            reaction.reactant_2_chemical = archive.read_byte();
            reaction.tick_rate_selector = archive.read_byte();
            reaction.product_1_amount = archive.read_byte();
            reaction.product_1_chemical = archive.read_byte();
            reaction.product_2_amount = archive.read_byte();
            reaction.product_2_chemical = archive.read_byte();
        }
        return;
    }

    archive.write_creature_reference(owner_);
    archive.write_uint32(emitter_gene_count_);
    archive.write_uint32(receptor_gene_count_);
    archive.write_uint32(reaction_gene_count_);
    for (const ChemicalState& chemical : chemical_states_) {
        archive.write_byte(chemical.concentration);
        archive.write_byte(chemical.half_life_selector);
    }
    for (std::uint32_t index = 0; index < emitter_gene_count_; ++index) {
        const EmitterRecord& emitter = emitter_records_[index];
        archive.write_byte(emitter.target_domain);
        archive.write_byte(emitter.tissue_index);
        archive.write_byte(emitter.locus_index);
        archive.write_byte(emitter.chemical_index);
        archive.write_byte(emitter.threshold);
        archive.write_byte(emitter.emission_period);
        archive.write_byte(emitter.emission_amount);
        archive.write_byte(emitter.flags);
    }
    for (std::uint32_t index = 0; index < receptor_gene_count_; ++index) {
        const ReceptorRecord& receptor = receptor_records_[index];
        archive.write_byte(receptor.target_domain);
        archive.write_byte(receptor.tissue_index);
        archive.write_byte(receptor.locus_index);
        archive.write_byte(receptor.chemical_index);
        archive.write_byte(receptor.threshold);
        archive.write_byte(receptor.nominal);
        archive.write_byte(receptor.gain);
        archive.write_byte(receptor.flags);
    }
    for (std::uint32_t index = 0; index < reaction_gene_count_; ++index) {
        const ReactionRecord& reaction = reaction_records_[index];
        // Offsets as above; the selector sits between the reactants and the
        // products.  Writing it last also handed the real game a shifted
        // record, so a world saved by this port had its reactions rewired.
        archive.write_byte(reaction.reactant_1_amount);
        archive.write_byte(reaction.reactant_1_chemical);
        archive.write_byte(reaction.reactant_2_amount);
        archive.write_byte(reaction.reactant_2_chemical);
        archive.write_byte(reaction.tick_rate_selector);
        archive.write_byte(reaction.product_1_amount);
        archive.write_byte(reaction.product_1_chemical);
        archive.write_byte(reaction.product_2_amount);
        archive.write_byte(reaction.product_2_chemical);
    }
}

void Biochemistry::load_genome(
    creatures1::creatures::Genome& genome,
    BiochemistryLocusHost& locus_host,
    creatures1::common::DebugLogHost* log_host) {
    constexpr std::uint8_t kBiochemistryFamily = static_cast<std::uint8_t>(
        creatures1::creatures::GenomeGeneFamily::biochemistry);

    // CBiochemistry::LoadGenome @ 0x0042e940 matches the genome load stage on
    // all five biochemistry gene types; C1 chemistry is staged, and loading
    // every stage at once stacked later stages over the current one.
    genome.set_cursor(0);
    while (genome.find_next_matching_gene(
        kBiochemistryFamily, 0, 5,
        creatures1::creatures::GenomeStageFilter::match_genome_load_stage)) {
        if (receptor_gene_count_ == receptor_records_.size()) {
            log_gene_limit(log_host, "ERROR: TOO MANY RECEPTOR GENES!");
            break;
        }

        const std::uint8_t domain = normalized_locus_domain(
            genome.read_payload_byte());
        const std::uint8_t tissue_index = genome.read_payload_byte();
        const std::uint8_t locus_index = genome.read_payload_byte();
        std::uint8_t* target_locus = resolve_locus(
            locus_host, domain,
            creatures1::creatures::GenomeLocusKind::receptor, tissue_index,
            locus_index);

        std::size_t record_index = 0;
        while (record_index < receptor_gene_count_ &&
               receptor_records_[record_index].target_locus != target_locus) {
            ++record_index;
        }
        if (record_index == receptor_gene_count_) {
            record_index = receptor_gene_count_++;
        }

        ReceptorRecord& receptor = receptor_records_[record_index];
        receptor.target_domain = domain;
        receptor.tissue_index = tissue_index;
        receptor.locus_index = locus_index;
        receptor.target_locus = target_locus;
        if (target_locus != nullptr) {
            receptor.chemical_index = genome.read_payload_byte();
            receptor.threshold = genome.read_payload_byte();
            receptor.nominal = genome.read_payload_byte();
            receptor.gain = genome.read_payload_byte();
            receptor.flags = genome.read_payload_byte();
            *target_locus = receptor.nominal;
        }
    }

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(
        kBiochemistryFamily, 1, 5,
        creatures1::creatures::GenomeStageFilter::match_genome_load_stage)) {
        if (emitter_gene_count_ == emitter_records_.size()) {
            log_gene_limit(log_host, "ERROR: TOO MANY EMITTER GENES!");
            break;
        }

        const std::uint8_t domain = normalized_locus_domain(
            genome.read_payload_byte());
        const std::uint8_t tissue_index = genome.read_payload_byte();
        const std::uint8_t locus_index = genome.read_payload_byte();
        const std::uint8_t chemical_index = genome.read_payload_byte();
        std::uint8_t* source_locus = resolve_locus(
            locus_host, domain,
            creatures1::creatures::GenomeLocusKind::emitter, tissue_index,
            locus_index);

        std::size_t record_index = 0;
        while (record_index < emitter_gene_count_ &&
               (emitter_records_[record_index].source_locus != source_locus ||
                emitter_records_[record_index].chemical_index != chemical_index)) {
            ++record_index;
        }
        if (record_index == emitter_gene_count_) {
            record_index = emitter_gene_count_++;
        }

        EmitterRecord& emitter = emitter_records_[record_index];
        emitter.target_domain = domain;
        emitter.tissue_index = tissue_index;
        emitter.locus_index = locus_index;
        emitter.source_locus = source_locus;
        if (source_locus != nullptr) {
            emitter.chemical_index = chemical_index;
            emitter.threshold = genome.read_payload_byte();
            emitter.emission_period = genome.read_payload_byte();
            if (emitter.emission_period == 0) {
                emitter.emission_period = 1;
            }
            emitter.emission_amount = genome.read_payload_byte();
            emitter.flags = genome.read_payload_byte();
        }
    }

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(
        kBiochemistryFamily, 2, 5,
        creatures1::creatures::GenomeStageFilter::match_genome_load_stage)) {
        if (reaction_gene_count_ == reaction_records_.size()) {
            log_gene_limit(log_host, "ERROR: TOO MANY REACTION GENES!");
            break;
        }
        ReactionRecord& reaction = reaction_records_[reaction_gene_count_++];
        reaction.reactant_1_amount = normalize_reaction_amount(
            genome.read_payload_byte());
        reaction.reactant_1_chemical = genome.read_payload_byte();
        reaction.reactant_2_amount = normalize_reaction_amount(
            genome.read_payload_byte());
        reaction.reactant_2_chemical = genome.read_payload_byte();
        reaction.product_1_amount = normalize_reaction_amount(
            genome.read_payload_byte());
        reaction.product_1_chemical = genome.read_payload_byte();
        reaction.product_2_amount = normalize_reaction_amount(
            genome.read_payload_byte());
        reaction.product_2_chemical = genome.read_payload_byte();
        reaction.tick_rate_selector = genome.read_payload_byte();
    }

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(
        kBiochemistryFamily, 3, 5,
        creatures1::creatures::GenomeStageFilter::match_genome_load_stage)) {
        for (ChemicalState& chemical : chemical_states_) {
            chemical.half_life_selector = genome.read_payload_byte();
        }
    }

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(
        kBiochemistryFamily, 4, 5,
        creatures1::creatures::GenomeStageFilter::match_genome_load_stage)) {
        const std::uint8_t chemical_index = genome.read_payload_byte();
        const std::uint8_t concentration = genome.read_payload_byte();
        chemical_states_[chemical_index].concentration = concentration;
    }
}

} // namespace creatures1::biochemistry
