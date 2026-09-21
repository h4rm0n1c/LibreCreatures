#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace creatures1::creatures {

using GenomeFilenameId = std::uint32_t;
using GenomePayloadByteCount = std::uint32_t;
using GenomeGeneCount = std::uint32_t;

// Native C1CreatureSex: MALE = 1, FEMALE = 2 (CGenome::FindNextMatchingGene
// @ 00419040 compares genome_gender with 1).  The value is stored raw in the
// classifier species byte (Skeleton::LoadGenome @ 0043c800), in CGenome's
// archive record (CGenome::Serialize @ 004185c0) and in Creature's gender
// byte, so the enum carries the native numbers.
enum class GenomeSex : std::uint32_t { male = 1, female = 2 };
enum class GenomeLifeStage : std::uint8_t {
    stage_zero = 0,
    stage_one = 1,
    stage_two = 2,
    stage_three = 3,
    stage_four = 4,
    stage_five = 5,
    stage_six = 6,
    terminal_stage_seven = 7,
};

// FindNextMatchingGene's stage selector is an on-disk/runtime contract.  The
// values are verified from the C1 scanner at 00419040; they are not a local
// convenience ordering.
enum class GenomeStageFilter : std::uint8_t {
    match_genome_load_stage = 0,
    ignore = 1,
    match_stage_zero = 2,
};

enum class GenomeLocusKind : std::uint8_t { receptor = 0, emitter = 1 };

enum class GenomeGeneFamily : std::uint8_t {
    brain = 0,
    biochemistry = 1,
    creature = 2,
};

struct GenomeGeneCountReport {
    GenomeGeneCount brain_lobe_gene_count = 0;
    GenomeGeneCount biochemistry_receptor_gene_count = 0;
    GenomeGeneCount biochemistry_emitter_gene_count = 0;
    GenomeGeneCount biochemistry_reaction_gene_count = 0;
    GenomeGeneCount biochemistry_half_life_gene_count = 0;
    GenomeGeneCount biochemistry_initial_chemical_gene_count = 0;
    GenomeGeneCount creature_stimulus_gene_count = 0;
    GenomeGeneCount creature_genus_gene_count = 0;
    GenomeGeneCount creature_appearance_gene_count = 0;
    GenomeGeneCount creature_pose_gene_count = 0;
    GenomeGeneCount creature_gait_gene_count = 0;
    GenomeGeneCount creature_instinct_gene_count = 0;
    GenomeGeneCount creature_life_stage_gene_count = 0;
    GenomeGeneCount total_creature_life_stage_advance_loci = 0;
};

struct GenomeGeneCountSelection {
    GenomeFilenameId source_filename = 0;
    GenomeSex sex = GenomeSex::male;
    GenomeLifeStage life_stage = GenomeLifeStage::stage_zero;
};

// The first ten bytes of every gene record are stable across the C1 genome
// files.  Payload bytes follow the header and continue up to the next record.
struct GenomeGeneHeader {
    std::uint8_t tag[4];
    std::uint8_t family;
    std::uint8_t subtype;
    std::uint8_t sequence;
    std::uint8_t load_pass_selector;
    std::uint8_t switch_on_life_stage;
    std::uint8_t flags;
};

// These are the packed payloads consumed by Skeleton::LoadGenome.  The
// genome format does not insert alignment padding between their fields.
#pragma pack(push, 1)
struct CreatureGenusGenePayload {
    std::uint8_t genus_selector = 0;
    GenomeFilenameId mother_moniker = 0;
    GenomeFilenameId father_moniker = 0;
};

struct CreatureAppearanceGenePayload {
    std::uint8_t body_part_group_selector = 0;
    std::uint8_t variant_index = 0;
};

struct CreaturePigmentGenePayload {
    std::uint8_t pigment_channel = 0;
    std::uint8_t amount = 0;
};
#pragma pack(pop)

static_assert(sizeof(CreatureGenusGenePayload) == 9,
              "CreatureGenusGenePayload layout mismatch");
static_assert(sizeof(CreatureAppearanceGenePayload) == 2,
              "CreatureAppearanceGenePayload layout mismatch");
static_assert(sizeof(CreaturePigmentGenePayload) == 2,
              "CreaturePigmentGenePayload layout mismatch");

constexpr std::uint32_t kGeneTag = 0x656e6567u;       // "gene"
constexpr std::uint32_t kGenomeEndTag = 0x646e6567u; // "gend"
// "gext", the undocumented pigment extension tag added in 1.04.  Unlike
// "gene" and "gend" it sits INSIDE a gene's payload, so gene_end() walks
// straight over it and the copier would otherwise mutate it.  See
// Genome::copy_gene_with_mutation.
constexpr std::uint32_t kPigmentExtensionTag = 0x74786567u; // "gext"
// The sex-applicability bits are 3 and 4, not 0 and 1.  CGenome's own gene
// tests read them as `flags & 0x18` for "restricted at all", `& 0x08` for
// male and `& 0x10` for female (CountMatchingGenes @ 00418f70 and
// FindNextMatchingGene).  Bits 0 and 1 are the mutation and duplication
// switches declared just below, so putting the sex bits there made every
// gene-applicability test read the wrong two flags: genes with neither
// mutation nor duplication set looked unrestricted and were always counted,
// while a gene with only the mutation bit set looked female-only and was
// dropped for a male.  `dde: gene` showed it as stimulus and appearance
// counts of exactly zero against the shipped binary's 25 and 4.
constexpr std::uint8_t kMaleApplicable = 0x08;
constexpr std::uint8_t kFemaleApplicable = 0x10;
constexpr std::uint8_t kGeneMutationEnabled = 0x01;
constexpr std::uint8_t kGeneDuplicationEnabled = 0x02;
constexpr std::uint8_t kGeneOmissionEnabled = 0x04;

class GenomeFileStore {
public:
    virtual ~GenomeFileStore() = default;

    virtual std::string secondary_genetics_path(GenomeFilenameId id) const = 0;
    virtual std::string primary_genetics_path(GenomeFilenameId id) const = 0;
    virtual bool regular_file_exists(const std::string& path) const = 0;
    virtual std::vector<std::uint8_t> read_file(const std::string& path) = 0;
    virtual void write_secondary_genetics_file(
        GenomeFilenameId id, const std::vector<std::uint8_t>& payload) = 0;
};

class GenomeRandomSource {
public:
    virtual ~GenomeRandomSource() = default;

    virtual std::uint32_t next() = 0;
    virtual void log_mutation(std::uint8_t old_value,
                              std::uint8_t new_value,
                              std::uint8_t bit_distance) = 0;
    virtual bool debug_logging_enabled() const = 0;
    virtual void log_conception(std::uint32_t crossovers,
                                std::uint32_t duplications,
                                std::uint32_t omissions,
                                std::uint32_t mutations) = 0;
};

class GenomeFilenameRegistry {
public:
    virtual ~GenomeFilenameRegistry() = default;

    virtual bool creature_uses_filename(GenomeFilenameId id) const = 0;
    virtual bool family_four_object_uses_filename(
        GenomeFilenameId id) const = 0;
};

class GenomeArchive {
public:
    virtual ~GenomeArchive() = default;
    virtual bool is_loading() const = 0;
    virtual GenomePayloadByteCount read_u32() = 0;
    virtual std::uint8_t read_u8() = 0;
    virtual void write_u32(std::uint32_t value) = 0;
    virtual void write_u8(std::uint8_t value) = 0;
    virtual std::vector<std::uint8_t> read_bytes(
        GenomePayloadByteCount count) = 0;
    virtual void write_bytes(const std::vector<std::uint8_t>& bytes) = 0;
};

class GenomeLocusStorage {
public:
    virtual ~GenomeLocusStorage() = default;

    virtual std::uint8_t* receptor_locus(std::uint8_t tissue,
                                          std::uint8_t locus) = 0;
    virtual std::uint8_t* emitter_locus(std::uint8_t tissue,
                                        std::uint8_t locus) = 0;
    virtual bool selected_for_diagnostics() const = 0;
    virtual void log_unrecognised_locus(std::uint8_t tissue,
                                        std::uint8_t locus) = 0;
    virtual std::uint8_t* unresolved_locus_sentinel() = 0;
};

class Genome {
public:
    Genome() = default;
    Genome(GenomeFilenameId source_filename,
           GenomeSex sex,
           GenomeLifeStage life_stage,
           GenomeFileStore* files = nullptr);
    ~Genome();

    std::string_view runtime_class_name() const noexcept;

    GenomeFilenameId source_filename() const { return source_filename_; }
    void set_source_filename(GenomeFilenameId id) { source_filename_ = id; }
    GenomeSex sex() const { return sex_; }
    GenomeLifeStage life_stage() const { return life_stage_; }
    GenomePayloadByteCount payload_size() const {
        return static_cast<GenomePayloadByteCount>(payload_.size());
    }
    const std::vector<std::uint8_t>& payload() const { return payload_; }
    std::vector<std::uint8_t>& payload() { return payload_; }

    void save_generated_genome(GenomeFileStore& files) const;
    void serialize(GenomeArchive& archive);

    void copy_byte_with_mutation(Genome& source,
                                 bool mutation_allowed,
                                 GenomeRandomSource& random);
    void copy_gene_with_mutation(Genome& source, GenomeRandomSource& random);
    GenomeGeneCount count_matching_genes(std::uint8_t family,
                                         std::uint8_t subtype,
                                         std::uint8_t subtype_modulus);
    bool find_next_matching_gene(std::uint8_t family,
                                 std::uint8_t subtype,
                                 std::uint8_t subtype_modulus,
                                 GenomeStageFilter stage_filter);

    // FindNextMatchingGene leaves the cursor at payload byte zero. The C1
    // brain loader reads the gene's load-pass selector at header offset +7,
    // three bytes before that payload cursor.
    std::uint8_t current_gene_load_pass() const;

    void recombine_genome_streams(Genome& maternal,
                                  Genome& paternal,
                                  GenomeRandomSource& random,
                                  std::uint32_t& crossovers,
                                  std::uint32_t& duplications,
                                  std::uint32_t& omissions,
                                  std::uint32_t& mutations);

    std::size_t cursor() const { return cursor_; }
    void set_cursor(std::size_t cursor) { cursor_ = cursor; }

    // FindNextMatchingGene leaves the cursor at the first payload byte of
    // the matching record, as in the original C1 scanner.  Loaders consume
    // that payload through this cursor rather than indexing a decompiler
    // buffer directly.
    std::uint8_t read_payload_byte() { return payload_[cursor_++]; }

    // Read one packed gene payload without allowing a malformed genome to
    // advance past the available bytes.  The caller chooses the exact
    // source-owned payload type; this is deliberately separate from the
    // native unchecked byte reader used by already-translated loaders.
    bool read_payload_bytes(std::uint8_t* destination, std::size_t count);

    template <typename Payload>
    bool read_payload_record(Payload& destination) {
        return read_payload_bytes(reinterpret_cast<std::uint8_t*>(&destination),
                                  sizeof(Payload));
    }

    // C1's lobe and biochemical loaders use this operation after locating a
    // gene. It is a real Genome operation, not a synthetic polymorphic
    // boundary invented by the decompiler translation.
    std::uint8_t read_next_gene_byte() { return read_payload_byte(); }

private:
    GenomeFilenameId source_filename_ = 0;
    GenomeSex sex_ = GenomeSex::male;
    GenomeLifeStage life_stage_ = GenomeLifeStage::stage_zero;
    std::vector<std::uint8_t> payload_;
    std::size_t cursor_ = 0;
    std::uint32_t mutation_count_ = 0;
};

std::uint8_t* resolve_genome_locus(GenomeLocusStorage& storage,
                                   GenomeLocusKind kind,
                                   std::uint8_t tissue,
                                   std::uint8_t locus);

GenomeFilenameId generate_offspring_genome_file(
    GenomeFilenameId maternal_source_filename,
    GenomeFilenameId paternal_source_filename,
    GenomeFileStore& files,
    GenomeRandomSource& random,
    GenomeFilenameId generated_source_filename);

void generate_unique_genome_filename(Genome& genome,
                                     GenomeFilenameRegistry& registry,
                                     GenomeRandomSource& random);

std::unique_ptr<Genome> create_genome();

// Builds the diagnostic gene-count record for the selected creature.  The
// selected-creature/global UI lookup and file-system implementation remain
// outside this source owner; the genome-count policy is C1-owned here.
GenomeGeneCountReport build_current_creature_genome_gene_counts(
    const GenomeGeneCountSelection& selection,
    GenomeFileStore& files);

} // namespace creatures1::creatures
