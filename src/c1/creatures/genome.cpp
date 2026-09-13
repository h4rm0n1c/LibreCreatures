#include "genome.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace creatures1::creatures {
namespace {

constexpr std::size_t kGeneHeaderSize = 10;

std::uint32_t read_tag(const std::vector<std::uint8_t>& bytes,
                       std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        return 0;
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool is_gene(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return read_tag(bytes, offset) == kGeneTag;
}

bool is_end(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return read_tag(bytes, offset) == kGenomeEndTag;
}

std::size_t next_record(const std::vector<std::uint8_t>& bytes,
                        std::size_t offset) {
    while (offset + 4 <= bytes.size()) {
        if (is_gene(bytes, offset) || is_end(bytes, offset)) {
            return offset;
        }
        ++offset;
    }
    return bytes.size();
}

std::size_t next_gene(const std::vector<std::uint8_t>& bytes,
                      std::size_t offset) {
    offset = next_record(bytes, offset);
    return is_gene(bytes, offset) ? offset : bytes.size();
}

std::size_t gene_end(const std::vector<std::uint8_t>& bytes,
                     std::size_t offset) {
    if (offset + kGeneHeaderSize > bytes.size()) {
        return bytes.size();
    }
    std::size_t cursor = offset + kGeneHeaderSize;
    while (cursor + 4 <= bytes.size() &&
           !is_gene(bytes, cursor) && !is_end(bytes, cursor)) {
        ++cursor;
    }
    return cursor;
}

std::uint8_t normalized_selector(std::uint8_t value,
                                 std::uint8_t modulus) {
    return modulus == 0 ? value : static_cast<std::uint8_t>(value % modulus);
}

bool gene_applies_to(const Genome& genome, std::uint8_t flags) {
    const bool unrestricted =
        (flags & (kFemaleApplicable | kMaleApplicable)) == 0;
    const bool male_match =
        (flags & kMaleApplicable) != 0 && genome.sex() == GenomeSex::male;
    const bool female_match =
        (flags & kFemaleApplicable) != 0 && genome.sex() == GenomeSex::female;
    return unrestricted || male_match || female_match;
}

std::uint8_t random_mutation_mask(GenomeRandomSource& random,
                                  std::uint8_t& bit_distance) {
    std::uint8_t mask = 1;
    bit_distance = 0;
    do {
        if ((random.next() & 1u) != 0) {
            break;
        }
        ++bit_distance;
        mask = static_cast<std::uint8_t>(mask << 1);
    } while (bit_distance < 8);
    return mask;
}

void append_end_tag(std::vector<std::uint8_t>& bytes) {
    bytes.push_back('g');
    bytes.push_back('e');
    bytes.push_back('n');
    bytes.push_back('d');
}

} // namespace

std::uint8_t* resolve_genome_locus(GenomeLocusStorage& storage,
                                   GenomeLocusKind kind,
                                   std::uint8_t tissue,
                                   std::uint8_t locus) {
    std::uint8_t* result = nullptr;
    if (tissue < 6) {
        if (kind == GenomeLocusKind::receptor) {
            const bool valid =
                (tissue == 0 && locus <= 6) ||
                (tissue == 1 && locus <= 7) ||
                (tissue == 2 && locus <= 1) ||
                (tissue == 3 && locus == 0) ||
                (tissue == 4 && locus <= 15) ||
                (tissue == 5 && locus <= 15 && locus != 10);
            if (valid) {
                result = storage.receptor_locus(tissue, locus);
            }
        } else {
            const bool valid =
                (tissue == 0 && locus == 0) ||
                (tissue == 1 && locus <= 7) ||
                (tissue == 2 && locus <= 1) ||
                (tissue == 3 && locus == 0) ||
                (tissue == 4 && locus <= 5) ||
                (tissue == 5 && locus <= 15 && locus != 10);
            if (valid) {
                result = storage.emitter_locus(tissue, locus);
            }
        }
    }
    if (result != nullptr) {
        return result;
    }
    if (storage.selected_for_diagnostics()) {
        storage.log_unrecognised_locus(tissue, locus);
    }
    return storage.unresolved_locus_sentinel();
}

Genome::Genome(GenomeFilenameId source_filename,
               GenomeSex sex,
               GenomeLifeStage life_stage,
               GenomeFileStore* files)
    : source_filename_(source_filename), sex_(sex), life_stage_(life_stage) {
    if (source_filename_ == 0 || files == nullptr) {
        return;
    }

    std::string path = files->secondary_genetics_path(source_filename_);
    if (!files->regular_file_exists(path)) {
        path = files->primary_genetics_path(source_filename_);
    }
    payload_ = files->read_file(path);
    cursor_ = 0;
}

Genome::~Genome() = default;

std::string_view Genome::runtime_class_name() const noexcept {
    return "Genome";
}

bool Genome::read_payload_bytes(std::uint8_t* destination,
                                std::size_t count) {
    if (destination == nullptr || cursor_ > payload_.size() ||
        count > payload_.size() - cursor_) {
        return false;
    }
    std::memcpy(destination, payload_.data() + cursor_, count);
    cursor_ += count;
    return true;
}

void Genome::serialize(GenomeArchive& archive) {
    if (archive.is_loading()) {
        const GenomePayloadByteCount size = archive.read_u32();
        source_filename_ = archive.read_u32();
        sex_ = static_cast<GenomeSex>(archive.read_u32());
        life_stage_ = static_cast<GenomeLifeStage>(archive.read_u8());
        payload_ = archive.read_bytes(size);
        cursor_ = 0;
        mutation_count_ = 0;
        return;
    }

    archive.write_u32(payload_size());
    archive.write_u32(source_filename_);
    archive.write_u32(static_cast<std::uint32_t>(sex_));
    archive.write_u8(static_cast<std::uint8_t>(life_stage_));
    archive.write_bytes(payload_);
}

void generate_unique_genome_filename(Genome& genome,
                                     GenomeFilenameRegistry& registry,
                                     GenomeRandomSource& random) {
    while (true) {
        GenomeFilenameId candidate = genome.source_filename();
        if (candidate == 0) {
            const auto letter = [&random]() -> std::uint32_t {
                return (random.next() % 26u) + static_cast<std::uint32_t>('A');
            };
            candidate = (letter() << 24) |
                        (letter() << 16) |
                        (letter() << 8) |
                        ((random.next() % 10u) + static_cast<std::uint32_t>('0'));
            genome.set_source_filename(candidate);
        }

        if (!registry.creature_uses_filename(candidate) &&
            !registry.family_four_object_uses_filename(candidate)) {
            return;
        }
    genome.set_source_filename(0);
    }
}

std::unique_ptr<Genome> create_genome() {
    return std::make_unique<Genome>();
}

GenomeGeneCountReport build_current_creature_genome_gene_counts(
    const GenomeGeneCountSelection& selection,
    GenomeFileStore& files) {
    Genome genome(selection.source_filename, selection.sex,
                  selection.life_stage, &files);
    GenomeGeneCountReport report;

    report.brain_lobe_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::brain), 0, 1);
    report.biochemistry_receptor_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::biochemistry), 0, 5);
    report.biochemistry_emitter_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::biochemistry), 1, 5);
    report.biochemistry_reaction_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::biochemistry), 2, 5);
    report.biochemistry_half_life_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::biochemistry), 3, 5);
    report.biochemistry_initial_chemical_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::biochemistry), 4, 5);
    report.creature_stimulus_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::creature), 0, 7);
    report.creature_genus_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::creature), 1, 7);
    report.creature_appearance_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::creature), 2, 7);
    report.creature_pose_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::creature), 3, 7);
    report.creature_gait_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::creature), 4, 7);
    report.creature_instinct_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::creature), 5, 7);
    report.creature_life_stage_gene_count = genome.count_matching_genes(
        static_cast<std::uint8_t>(GenomeGeneFamily::creature), 6, 7);

    // BuildCurrentCreatureGenomeGeneCounts @ 0040e350 scans this tail with
    // IGNORE_STAGE, not with the genome's own load stage.  Filtering by stage
    // here left the total at zero, because the life-stage-advance genes it is
    // summing are precisely the ones tagged for stages the creature is not in.
    genome.set_cursor(0);
    std::uint32_t total_life_stage_advance_loci = 0;
    while (genome.find_next_matching_gene(
        static_cast<std::uint8_t>(GenomeGeneFamily::creature), 6, 7,
        GenomeStageFilter::ignore)) {
        genome.set_cursor(genome.cursor() + 1);
        total_life_stage_advance_loci += genome.read_payload_byte();
    }
    report.total_creature_life_stage_advance_loci =
        total_life_stage_advance_loci;
    return report;
}

void Genome::save_generated_genome(GenomeFileStore& files) const {
    files.write_secondary_genetics_file(source_filename_, payload_);
}

void Genome::copy_byte_with_mutation(Genome& source,
                                     bool mutation_allowed,
                                     GenomeRandomSource& random) {
    if (source.cursor_ >= source.payload_.size()) {
        return;
    }
    std::uint8_t value = source.payload_[source.cursor_++];
    if (mutation_allowed && random.next() % 0x641u == 0) {
        const std::uint8_t old_value = value;
        std::uint8_t bit_distance = 0;
        value = static_cast<std::uint8_t>(
            value ^ random_mutation_mask(random, bit_distance));
        if (random.debug_logging_enabled()) {
            random.log_mutation(old_value, value, bit_distance);
        }
        ++mutation_count_;
    }
    payload_.push_back(value);
}

void Genome::copy_gene_with_mutation(Genome& source,
                                     GenomeRandomSource& random) {
    const std::size_t start = source.cursor_;
    if (!is_gene(source.payload_, start) ||
        start + kGeneHeaderSize > source.payload_.size()) {
        return;
    }
    const std::size_t end = gene_end(source.payload_, start);
    payload_.insert(payload_.end(), source.payload_.begin() + start,
                    source.payload_.begin() + end);
    source.cursor_ = end;

    const std::uint8_t flags = source.payload_[start + 9];
    if ((flags & kGeneMutationEnabled) != 0 &&
        random.next() % 0x641u == 0 && end > start + kGeneHeaderSize) {
        const std::size_t payload_offset = payload_.size() - (end - start) +
                                           kGeneHeaderSize;
        const std::uint8_t old_value = payload_[payload_offset];
        std::uint8_t bit_distance = 0;
        payload_[payload_offset] = static_cast<std::uint8_t>(
            old_value ^ random_mutation_mask(random, bit_distance));
        if (random.debug_logging_enabled()) {
            random.log_mutation(old_value, payload_[payload_offset],
                                bit_distance);
        }
        ++mutation_count_;
    }
}

GenomeGeneCount Genome::count_matching_genes(std::uint8_t family,
                                             std::uint8_t subtype,
                                             std::uint8_t subtype_modulus) {
    GenomeGeneCount count = 0;
    cursor_ = 0;
    while (true) {
        const std::size_t record = next_record(payload_, cursor_);
        cursor_ = record;
        if (is_end(payload_, record) || record == payload_.size()) {
            break;
        }
        if (record + kGeneHeaderSize <= payload_.size() &&
            normalized_selector(payload_[record + 4], 3) == family &&
            normalized_selector(payload_[record + 5], subtype_modulus) ==
                subtype &&
            gene_applies_to(*this, payload_[record + 9])) {
            ++count;
        }
        cursor_ = gene_end(payload_, record);
    }
    return count;
}

bool Genome::find_next_matching_gene(std::uint8_t family,
                                     std::uint8_t subtype,
                                     std::uint8_t subtype_modulus,
                                     GenomeStageFilter stage_filter) {
    while (true) {
        const std::size_t record = next_record(payload_, cursor_);
        cursor_ = record;
        if (is_end(payload_, record) || record == payload_.size()) {
            return false;
        }
        if (record + kGeneHeaderSize > payload_.size()) {
            return false;
        }
        const auto& bytes = payload_;
        const bool family_match = normalized_selector(bytes[record + 4], 3) == family;
        const bool subtype_match =
            normalized_selector(bytes[record + 5], subtype_modulus) == subtype;
        const bool stage_match =
            (stage_filter == GenomeStageFilter::match_genome_load_stage &&
             bytes[record + 8] == static_cast<std::uint8_t>(life_stage_)) ||
            stage_filter == GenomeStageFilter::ignore ||
            (stage_filter == GenomeStageFilter::match_stage_zero &&
             bytes[record + 8] == 0);
        cursor_ = gene_end(bytes, record);
        if (family_match && subtype_match && stage_match &&
            gene_applies_to(*this, bytes[record + 9])) {
            cursor_ = record + kGeneHeaderSize;
            return true;
        }
    }
}

std::uint8_t Genome::current_gene_load_pass() const {
    if (cursor_ < 3 || cursor_ > payload_.size()) {
        return 0;
    }
    return payload_[cursor_ - 3];
}

void Genome::recombine_genome_streams(Genome& maternal,
                                      Genome& paternal,
                                      GenomeRandomSource& random,
                                      std::uint32_t& crossovers,
                                      std::uint32_t& duplications,
                                      std::uint32_t& omissions,
                                      std::uint32_t& mutations) {
    maternal.cursor_ = 0;
    paternal.cursor_ = 0;
    cursor_ = 0;
    mutation_count_ = 0;

    Genome* active = (random.next() & 1u) == 0 ? &paternal : &maternal;
    Genome* other = active == &maternal ? &paternal : &maternal;
    while (true) {
        const std::size_t run_length = random.next() % 0x5b + 10;
        std::size_t copied = 0;
        while (copied < run_length) {
            active->cursor_ = next_gene(active->payload_, active->cursor_);
            if (active->cursor_ == active->payload_.size()) {
                mutations = mutation_count_;
                append_end_tag(payload_);
                return;
            }
            copy_gene_with_mutation(*active, random);
            ++copied;
        }

        const std::size_t active_gene = next_gene(active->payload_, active->cursor_);
        if (active_gene == active->payload_.size()) {
            mutations = mutation_count_;
            append_end_tag(payload_);
            return;
        }
        const std::array<std::uint8_t, 3> identity = {
            active->payload_[active_gene + 4], active->payload_[active_gene + 5],
            active->payload_[active_gene + 6]};
        other->cursor_ = next_gene(other->payload_, 0);
        while (other->cursor_ != other->payload_.size()) {
            if (other->cursor_ + 7 <= other->payload_.size() &&
                std::array<std::uint8_t, 3>{other->payload_[other->cursor_ + 4],
                                            other->payload_[other->cursor_ + 5],
                                            other->payload_[other->cursor_ + 6]} ==
                    identity) {
                break;
            }
            other->cursor_ = next_gene(other->payload_,
                                       gene_end(other->payload_, other->cursor_));
        }
        if (other->cursor_ == other->payload_.size()) {
            continue;
        }

        ++crossovers;
        if (random.next() % 0x51u != 0) {
            std::swap(active, other);
            continue;
        }

        const std::uint32_t decision = random.next() & 1u;
        if (decision == 0) {
            if ((active->payload_[active->cursor_ + 9] &
                 kGeneDuplicationEnabled) != 0) {
                ++duplications;
                copy_gene_with_mutation(*active, random);
            }
        } else if ((other->payload_[other->cursor_ + 9] &
                    kGeneOmissionEnabled) != 0) {
            ++omissions;
            other->cursor_ = next_gene(
                other->payload_,
                gene_end(other->payload_, other->cursor_));
        }
        std::swap(active, other);
    }
}

GenomeFilenameId generate_offspring_genome_file(
    GenomeFilenameId maternal_source_filename,
    GenomeFilenameId paternal_source_filename,
    GenomeFileStore& files,
    GenomeRandomSource& random,
    GenomeFilenameId generated_source_filename) {
    Genome maternal(maternal_source_filename, GenomeSex::male,
                    GenomeLifeStage::stage_zero, &files);
    Genome paternal(paternal_source_filename, GenomeSex::male,
                    GenomeLifeStage::stage_zero, &files);
    Genome offspring(generated_source_filename, GenomeSex::male,
                     GenomeLifeStage::stage_zero);
    const std::size_t reserve_size =
        std::max(maternal.payload().size(), paternal.payload().size()) + 2000000;
    offspring.payload().reserve(reserve_size);

    std::uint32_t crossovers = 0;
    std::uint32_t duplications = 0;
    std::uint32_t omissions = 0;
    std::uint32_t mutations = 0;
    if (paternal_source_filename == 0) {
        offspring.payload() = maternal.payload();
    } else {
        offspring.recombine_genome_streams(maternal, paternal, random,
                                           crossovers, duplications, omissions,
                                           mutations);
    }
    if (offspring.payload().size() >= 0x13) {
        std::memcpy(offspring.payload().data() + 0x0b,
                    &maternal_source_filename, sizeof(maternal_source_filename));
        std::memcpy(offspring.payload().data() + 0x0f,
                    &paternal_source_filename, sizeof(paternal_source_filename));
    }
    if (random.debug_logging_enabled()) {
        random.log_conception(crossovers, duplications, omissions, mutations);
    }
    offspring.save_generated_genome(files);
    return offspring.source_filename();
}

} // namespace creatures1::creatures
