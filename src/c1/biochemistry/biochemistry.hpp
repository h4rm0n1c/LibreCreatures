#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "../common/logging.hpp"
#include "../creatures/genome.hpp"

namespace creatures1::creatures {
class Creature;
}

namespace creatures1::biochemistry {

class BiochemistryArchive {
public:
    virtual ~BiochemistryArchive() = default;
    virtual bool is_loading() const = 0;
    virtual creatures1::creatures::Creature* read_creature_reference() = 0;
    virtual void write_creature_reference(
        creatures1::creatures::Creature* creature) = 0;
    virtual std::uint32_t read_uint32() = 0;
    virtual void write_uint32(std::uint32_t value) = 0;
    virtual std::uint8_t read_byte() = 0;
    virtual void write_byte(std::uint8_t value) = 0;
};

enum class LocusDomain : std::uint8_t { brain = 0, creature = 1 };

// Resolving a genome locus reaches either the embedded Brain resolver or the
// Creature resolver. Those object layouts and virtual dispatch surfaces are
// host-owned; biochemistry owns only the domain-selection policy.
class BiochemistryLocusHost {
public:
    virtual ~BiochemistryLocusHost() = default;
    virtual std::uint8_t* resolve_brain_locus(
        creatures1::creatures::GenomeLocusKind kind,
        std::uint8_t tissue_index,
        std::uint8_t locus_index) = 0;
    virtual std::uint8_t* resolve_creature_locus(
        creatures1::creatures::GenomeLocusKind kind,
        std::uint8_t tissue_index,
        std::uint8_t locus_index) = 0;
};

// C1 chemical state storage.  These are deliberately byte-sized: the
// original executable stores concentration and the half-life selector as a
// packed two-byte record, with 256 entries.
struct ChemicalState {
    std::uint8_t concentration = 0;
    std::uint8_t half_life_selector = 0;
};

struct ReceptorRecord {
    std::uint8_t target_domain = 0;
    std::uint8_t tissue_index = 0;
    std::uint8_t locus_index = 0;
    std::uint8_t chemical_index = 0;
    // Field order is the on-disk genome/archive order: the chemical excess is
    // measured above `threshold`, scaled by `gain`, then offset from `nominal`.
    std::uint8_t threshold = 0;
    std::uint8_t nominal = 0;
    std::uint8_t gain = 0;
    std::uint8_t flags = 0;
    std::uint8_t* target_locus = nullptr;
};

struct EmitterRecord {
    std::uint8_t target_domain = 0;
    std::uint8_t tissue_index = 0;
    std::uint8_t locus_index = 0;
    std::uint8_t chemical_index = 0;
    std::uint8_t threshold = 0;
    std::uint8_t emission_period = 0;
    std::uint8_t emission_amount = 0;
    std::uint8_t flags = 0;
    std::uint8_t* source_locus = nullptr;
};

struct ReactionRecord {
    std::uint8_t reactant_1_amount = 0;
    std::uint8_t reactant_1_chemical = 0;
    std::uint8_t reactant_2_amount = 0;
    std::uint8_t reactant_2_chemical = 0;
    std::uint8_t product_1_amount = 0;
    std::uint8_t product_1_chemical = 0;
    std::uint8_t product_2_amount = 0;
    std::uint8_t product_2_chemical = 0;
    std::uint8_t tick_rate_selector = 0;
};

// Owns the C1 chemical state and the genome-derived receptor/emitter/reaction
// tables.  MFC runtime metadata and backend/debug UI are supplied by the
// surrounding application; this class contains the game policy.
class Biochemistry {
public:
    Biochemistry();

    void add_chemical_moles(
        int chemical_index,
        std::uint32_t moles,
        creatures1::common::DebugLogHost* log_host = nullptr);

    void update(
        std::uint32_t tick,
        creatures1::common::DebugLogHost* log_host = nullptr);

    void serialize(BiochemistryArchive& archive,
                   BiochemistryLocusHost& locus_host,
                   creatures1::common::DebugLogHost* log_host = nullptr);
    void load_genome(
        creatures1::creatures::Genome& genome,
        BiochemistryLocusHost& locus_host,
        creatures1::common::DebugLogHost* log_host = nullptr);

    std::array<ChemicalState, 256>& chemical_states() {
        return chemical_states_;
    }
    const std::array<ChemicalState, 256>& chemical_states() const {
        return chemical_states_;
    }

    std::array<ReceptorRecord, 128>& receptor_records() {
        return receptor_records_;
    }
    std::array<EmitterRecord, 128>& emitter_records() {
        return emitter_records_;
    }
    std::array<ReactionRecord, 128>& reaction_records() {
        return reaction_records_;
    }

    creatures1::creatures::Creature* owner() const { return owner_; }
    void set_owner(creatures1::creatures::Creature* owner) { owner_ = owner; }

    std::uint32_t receptor_gene_count() const { return receptor_gene_count_; }
    std::uint32_t emitter_gene_count() const { return emitter_gene_count_; }
    std::uint32_t reaction_gene_count() const { return reaction_gene_count_; }

private:
    creatures1::creatures::Creature* owner_ = nullptr;
    std::array<ChemicalState, 256> chemical_states_{};
    std::array<ReceptorRecord, 128> receptor_records_{};
    std::array<EmitterRecord, 128> emitter_records_{};
    // The native constructor does not touch this table; LoadGenome owns its
    // population.  Keeping it separate makes that lifetime boundary explicit.
    std::array<ReactionRecord, 128> reaction_records_;
    std::uint32_t receptor_gene_count_ = 0;
    std::uint32_t emitter_gene_count_ = 0;
    std::uint32_t reaction_gene_count_ = 0;
};

// The original CreateObject entry is an MFC runtime-class factory.  Clean
// callers use ordinary ownership; the MFC factory itself is represented by
// framework metadata rather than exported as a decompiler-shaped function.
std::unique_ptr<Biochemistry> create_biochemistry();

} // namespace creatures1::biochemistry
