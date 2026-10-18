// Function-level equivalence: the biochemistry genes of a genome at each life
// stage, CBiochemistry::LoadGenome @0042e940 (CE 10.3, called by the genome
// re-expression @0040d930 at every stage change) against
// biochemistry::Biochemistry::load_genome.
//
// The native genome reader is a CGenome in memory, as in the creature gene
// test.  The native CBiochemistry (0x1294 bytes) has its owner creature at
// +4.  A receptor or emitter locus resolves through the owner: the brain's
// resolver at [creature+0x2c18]+4, the creature's at creature+0x804, each
// called through vtable slot 0 as thiscall(kind, tissue, locus).  Both sides
// resolve through one function: a hash of (domain, kind, tissue, locus)
// gives a byte in a small pool, or no locus.  Different genes share loci, so
// the de-duplication of receptors and emitters runs.
//
// Each case: one of the stock genomes, either sex, a last life stage 0 to 6;
// in half the cases up to 40 bytes of the genome change (never in a gene or
// end marker).  Both sides load stage 0, then each later stage up to the
// last, on the same biochemistry, as the game does at each stage change.
// After each load, all 256 chemicals (concentration and half-life), the
// receptor, emitter and reaction records with their counts, the locus of
// each record and the pool bytes must match.  A zero pad and a gend follow
// the native genome (row 81).

#include "native_image.hpp"

#include "biochemistry/biochemistry.hpp"
#include "common/compat.hpp"
#include "creatures/genome.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;
namespace bio = creatures1::biochemistry;

constexpr std::uint32_t kLoadGenome = 0x0042e940, kGenomeVtable = 0x00456094, kNativeDebugLog = 0x00418000;
constexpr std::size_t kNativeSize = 0x1294, kOwner = 4, kChemicals = 8, kReceptors = 0x208, kEmitters = 0x808,
                      kReactions = 0xe08, kReceptorCount = 0x1288, kEmitterCount = 0x128c,
                      kReactionCount = 0x1290;
constexpr std::size_t kCreatureSize = 0x2d00, kCreatureResolver = 0x804, kBrainPointer = 0x2c18,
                      kBrainResolver = 4, kBrainSize = 0x100;
constexpr std::size_t kPool = 48;

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next()); }
    std::uint32_t below(std::uint32_t n) { return n == 0 ? 0 : next() % n; }
    bool chance(std::uint32_t one_in) { return below(one_in) == 0; }
};

// The shared resolver: a pool slot, or none for one locus in six.
int pool_slot(int domain, int kind, std::uint8_t tissue, std::uint8_t locus) {
    const std::uint32_t h = static_cast<std::uint32_t>(domain * 7 + kind * 13 + tissue * 31 + locus * 17);
    if ((h ^ (h >> 3)) % 6 == 0) return -1;
    return static_cast<int>(h % kPool);
}

std::uint8_t g_native_pool[kPool];

extern "C" std::uint8_t* __attribute__((thiscall))
native_resolve_brain(void*, std::uint32_t kind, std::uint32_t tissue, std::uint32_t locus) {
    // Only the low byte of each pushed register is set.
    const int slot = pool_slot(0, static_cast<int>(kind & 0xff), static_cast<std::uint8_t>(tissue),
                               static_cast<std::uint8_t>(locus));
    return slot < 0 ? nullptr : &g_native_pool[slot];
}
extern "C" std::uint8_t* __attribute__((thiscall))
native_resolve_creature(void*, std::uint32_t kind, std::uint32_t tissue, std::uint32_t locus) {
    const int slot = pool_slot(1, static_cast<int>(kind & 0xff), static_cast<std::uint8_t>(tissue),
                               static_cast<std::uint8_t>(locus));
    return slot < 0 ? nullptr : &g_native_pool[slot];
}
void* g_brain_vtable[1] = {reinterpret_cast<void*>(&native_resolve_brain)};
void* g_creature_vtable[1] = {reinterpret_cast<void*>(&native_resolve_creature)};

class PoolLoci final : public bio::BiochemistryLocusHost {
public:
    std::uint8_t pool[kPool] = {};
    std::uint8_t* resolve_brain_locus(genome::GenomeLocusKind kind, std::uint8_t tissue,
                                      std::uint8_t locus) override {
        return at(pool_slot(0, static_cast<int>(kind), tissue, locus));
    }
    std::uint8_t* resolve_creature_locus(genome::GenomeLocusKind kind, std::uint8_t tissue,
                                         std::uint8_t locus) override {
        return at(pool_slot(1, static_cast<int>(kind), tissue, locus));
    }

private:
    std::uint8_t* at(int slot) { return slot < 0 ? nullptr : &pool[slot]; }
};

class OneGenomeFile final : public genome::GenomeFileStore {
public:
    explicit OneGenomeFile(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    std::string secondary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    std::string primary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    bool regular_file_exists(const std::string&) const override { return true; }
    std::vector<std::uint8_t> read_file(const std::string&) override { return bytes_; }
    void write_secondary_genetics_file(genome::GenomeFilenameId, const std::vector<std::uint8_t>&) override {}

private:
    std::vector<std::uint8_t> bytes_;
};
std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}
std::uint32_t u32(const std::uint8_t* p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
void put32(std::uint8_t* p, std::uint32_t v) { std::memcpy(p, &v, 4); }
// A record's locus as a pool slot (-1: none, -2: outside the pool).
int slot_of(const std::uint8_t* locus, const std::uint8_t* pool) {
    if (locus == nullptr) return -1;
    return locus >= pool && locus < pool + kPool ? static_cast<int>(locus - pool) : -2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0xb10c4e5u;
    std::vector<std::vector<std::uint8_t>> genomes;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-cases" && i + 1 < argc) cases = std::atol(argv[++i]);
        else if (arg == "-seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 0);
        else {
            genomes.push_back(read_file(argv[i]));
            if (genomes.back().empty()) {
                std::fprintf(stderr, "cannot read %s\n", argv[i]);
                return 2;
            }
        }
    }
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    image.make_return(kNativeDebugLog);
    using Load = void(__attribute__((thiscall)) *)(void*, void*);
    const auto native_load = image.at<Load>(kLoadGenome);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, loads = 0, receptors = 0, emitters = 0, reactions = 0;
    std::vector<std::uint8_t> native_creature(kCreatureSize, 0), native_brain(kBrainSize, 0);
    put32(native_creature.data() + kCreatureResolver,
          static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_creature_vtable)));
    put32(native_creature.data() + kBrainPointer,
          static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native_brain.data())));
    put32(native_brain.data() + kBrainResolver,
          static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_brain_vtable)));
    for (long index = 0; index < cases; ++index) {
        const std::size_t which = random.below(static_cast<std::uint32_t>(genomes.size()));
        const auto sex = random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female;
        const std::uint32_t last_stage = random.below(7);
        std::vector<std::uint8_t> bytes = genomes[which];
        if (random.chance(2)) {
            for (std::uint32_t n = 1 + random.below(40); n != 0; --n) {
                std::uint8_t value = random.byte();
                if (value != 0 && std::strchr("gends", value) != nullptr) value = 0;
                const std::size_t at = 4 + random.below(static_cast<std::uint32_t>(bytes.size() - 8));
                bool in_marker = false;
                for (std::size_t start = at - 3; start <= at; ++start) {
                    in_marker = in_marker || std::memcmp(&bytes[start], "gen", 3) == 0;
                }
                if (!in_marker) bytes[at] = value;
            }
        }
        OneGenomeFile file(bytes);

        auto lc = std::make_unique<bio::Biochemistry>();
        PoolLoci lc_loci;
        std::vector<std::uint8_t> native(kNativeSize, 0);
        put32(native.data() + kOwner,
              static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native_creature.data())));
        std::memset(g_native_pool, 0, kPool);

        std::string first;
        char text[256];
        auto note = [&](const std::string& what) { if (first.empty()) first = what; };
        for (std::uint32_t stage = 0; stage <= last_stage && first.empty(); ++stage) {
            genome::Genome g(1, sex, static_cast<genome::GenomeLifeStage>(stage), &file);
            lc->load_genome(g, lc_loci);
            // Row 81: native reads on past the end of the genome when a
            // payload runs past the last gend.  A zero pad and a gend after
            // the genome make that read the same each run; LC reads 0 past
            // the end.
            std::vector<std::uint8_t> reader_bytes = bytes;
            reader_bytes.resize(bytes.size() + 1024, 0);
            reader_bytes.insert(reader_bytes.end(), {'g', 'e', 'n', 'd'});
            std::vector<std::uint8_t> reader(0x20, 0);
            put32(reader.data(), kGenomeVtable);
            put32(reader.data() + 4, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(reader_bytes.data())));
            put32(reader.data() + 8, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(reader_bytes.data())));
            put32(reader.data() + 0xc, static_cast<std::uint32_t>(bytes.size()));
            put32(reader.data() + 0x10, 1);
            put32(reader.data() + 0x14, static_cast<std::uint32_t>(sex));
            reader[0x18] = static_cast<std::uint8_t>(stage);
            native_load(native.data(), reader.data());
            ++loads;

            // ---- compare ----
            const std::uint8_t* n = native.data();
            const auto& chemicals = lc->chemical_states();
            for (std::size_t c = 0; c < 256; ++c) {
                if (n[kChemicals + 2 * c] != chemicals[c].concentration ||
                    n[kChemicals + 2 * c + 1] != chemicals[c].half_life_selector) {
                    std::snprintf(text, sizeof text, "stage %u: chemical %u: native %u/%u lc %u/%u", stage,
                                  static_cast<unsigned>(c), n[kChemicals + 2 * c], n[kChemicals + 2 * c + 1],
                                  chemicals[c].concentration, chemicals[c].half_life_selector);
                    note(text);
                }
            }
            const std::uint32_t counts[3] = {u32(n + kReceptorCount), u32(n + kEmitterCount),
                                             u32(n + kReactionCount)};
            const std::uint32_t lc_counts[3] = {lc->receptor_gene_count(), lc->emitter_gene_count(),
                                                lc->reaction_gene_count()};
            if (std::memcmp(counts, lc_counts, sizeof counts) != 0) {
                std::snprintf(text, sizeof text, "stage %u: counts: native %u/%u/%u lc %u/%u/%u", stage,
                              counts[0], counts[1], counts[2], lc_counts[0], lc_counts[1], lc_counts[2]);
                note(text);
                break;
            }
            for (std::uint32_t r = 0; r < counts[0]; ++r) {
                const std::uint8_t* p = n + kReceptors + 12 * r;
                const auto& e = lc->receptor_records()[r];
                const std::uint8_t lc_bytes[8] = {e.target_domain, e.tissue_index, e.locus_index, e.chemical_index,
                                                  e.threshold, e.nominal, e.gain, e.flags};
                const auto* native_locus = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(u32(p + 8)));
                if (std::memcmp(p, lc_bytes, 8) != 0 ||
                    slot_of(native_locus, g_native_pool) != slot_of(e.target_locus, lc_loci.pool)) {
                    std::snprintf(text, sizeof text, "stage %u: receptor %u", stage, r);
                    note(text);
                }
            }
            for (std::uint32_t r = 0; r < counts[1]; ++r) {
                const std::uint8_t* p = n + kEmitters + 12 * r;
                const auto& e = lc->emitter_records()[r];
                const std::uint8_t lc_bytes[8] = {e.target_domain, e.tissue_index, e.locus_index, e.chemical_index,
                                                  e.threshold, e.emission_period, e.emission_amount, e.flags};
                const auto* native_locus = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(u32(p + 8)));
                if (std::memcmp(p, lc_bytes, 8) != 0 ||
                    slot_of(native_locus, g_native_pool) != slot_of(e.source_locus, lc_loci.pool)) {
                    std::snprintf(text, sizeof text, "stage %u: emitter %u", stage, r);
                    note(text);
                }
            }
            for (std::uint32_t r = 0; r < counts[2]; ++r) {
                const auto& e = lc->reaction_records()[r];
                const std::uint8_t lc_bytes[9] = {e.reactant_1_amount, e.reactant_1_chemical, e.reactant_2_amount,
                                                  e.reactant_2_chemical, e.tick_rate_selector, e.product_1_amount,
                                                  e.product_1_chemical, e.product_2_amount, e.product_2_chemical};
                if (std::memcmp(n + kReactions + 9 * r, lc_bytes, 9) != 0) {
                    std::snprintf(text, sizeof text, "stage %u: reaction %u", stage, r);
                    note(text);
                }
            }
            if (std::memcmp(g_native_pool, lc_loci.pool, kPool) != 0) {
                std::snprintf(text, sizeof text, "stage %u: locus pool", stage);
                note(text);
            }
            receptors += counts[0];
            emitters += counts[1];
            reactions += counts[2];
        }
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: genome %u sex %u last stage %u: %s\n", index, static_cast<unsigned>(which),
                        static_cast<unsigned>(sex), last_stage, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld loads, %ld receptor, %ld emitter and %ld reaction records compared\n", loads,
                receptors, emitters, reactions);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
