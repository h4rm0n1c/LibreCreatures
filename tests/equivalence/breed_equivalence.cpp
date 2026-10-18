// Function-level equivalence: making a child's genome from its parents,
// CGenome::RecombineGenomeStreams @00418a20 (CE 10.3) with
// CopyGenomeGeneWithMutation @00418c70 and CopyGenomeByteWithMutation
// @00418e00, against creatures::Genome::recombine_genome_streams.
//
//   breed_equivalence.exe <Creatures.exe> <genome.gen>... [-cases N] [-seed S]
//
// The native genomes are CGenome blocks in memory (vtable 00456094, the bytes
// at +4, the cursor at +8, the size at +0xc), as in the gene expression test.
// rand() is the MSVC formula on both sides, from one state.  The conception
// counters are the globals 00467da8 (crossovers), 00467da4 (duplications),
// 00467da0 (omissions) and 00467d4c (mutations).
//
// Each case: two parents, each a stock genome; half the cases change up to
// 40 bytes of each parent (never in or to a marker), set the duplication,
// omission and mutation flags of random genes, or give a gene the identity
// of the gene before it.  After the recombination the child's bytes, both
// parents' bytes (the original writes into the parent it duplicates from)
// the four counters and the number of rand() draws must match.

#include "native_image.hpp"

#include "common/compat.hpp"
#include "creatures/genome.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;

constexpr std::uint32_t kRecombine = 0x00418a20, kGenomeVtable = 0x00456094, kNativeDebugLog = 0x00418000;
constexpr std::uint32_t kCrossovers = 0x00467da8, kDuplications = 0x00467da4, kOmissions = 0x00467da0,
                        kMutations = 0x00467d4c;

// One rand() stream for both sides: the MSVC formula.
std::uint32_t g_rand_state = 1;
extern "C" int __cdecl native_rand() {
    g_rand_state = g_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_rand_state >> 16) & 0x7fff);
}

class SharedRandom final : public genome::GenomeRandomSource {
public:
    std::uint32_t next() override { return static_cast<std::uint32_t>(native_rand()); }
    void log_mutation(std::uint8_t, std::uint8_t, std::uint8_t) override {}
    bool debug_logging_enabled() const override { return false; }
    void log_conception(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) override {}
};

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

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

std::vector<std::size_t> gene_offsets(const std::vector<std::uint8_t>& bytes) {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i + 10 <= bytes.size(); ++i) {
        if (std::memcmp(&bytes[i], "gene", 4) == 0) out.push_back(i);
    }
    return out;
}

// Changes a parent: bytes, gene flags, repeated identities.
void vary(std::vector<std::uint8_t>& bytes, Random& random) {
    const auto genes = gene_offsets(bytes);
    if (genes.empty()) return;
    if (random.chance(2)) {
        for (std::uint32_t n = 1 + random.below(40); n != 0; --n) {
            std::uint8_t value = random.byte();
            if (value != 0 && std::strchr("gends", value) != nullptr) value = 100;
            const std::size_t at = 4 + random.below(static_cast<std::uint32_t>(bytes.size() - 8));
            bool in_marker = false;
            for (std::size_t start = at - 3; start <= at; ++start) {
                in_marker = in_marker || std::memcmp(&bytes[start], "gen", 3) == 0;
            }
            if (!in_marker) bytes[at] = value;
        }
    }
    if (random.chance(2)) {
        // Flags byte 9: 1 mutation, 2 duplication, 4 omission.
        for (std::uint32_t n = 1 + random.below(30); n != 0; --n) {
            const std::size_t gene = genes[random.below(static_cast<std::uint32_t>(genes.size()))];
            bytes[gene + 9] = static_cast<std::uint8_t>(bytes[gene + 9] ^ (1u << random.below(3)));
        }
    }
    if (random.chance(3)) {
        for (std::uint32_t n = 1 + random.below(6); n != 0; --n) {
            const std::size_t k = 1 + random.below(static_cast<std::uint32_t>(genes.size() - 1));
            std::memcpy(&bytes[genes[k] + 4], &bytes[genes[k - 1] + 4], 3);
        }
    }
}

// A native CGenome over `bytes`, which keep a zero pad and a gend after the
// genome so that a read past its end (row 81) is the same each run.
struct NativeGenome {
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> block = std::vector<std::uint8_t>(0x20, 0);
    NativeGenome(const std::vector<std::uint8_t>& genome_bytes, std::size_t capacity) {
        bytes = genome_bytes;
        bytes.resize(std::max(capacity, genome_bytes.size() + 1024), 0);
        bytes.insert(bytes.end(), {'g', 'e', 'n', 'd'});
        field<std::uint32_t>(block.data(), 0) = kGenomeVtable;
        field<std::uint8_t*>(block.data(), 4) = bytes.data();
        field<std::uint8_t*>(block.data(), 8) = bytes.data();
        field<std::uint32_t>(block.data(), 0xc) = static_cast<std::uint32_t>(genome_bytes.size());
    }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0xb4eedu;
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
    if (!image.bind_import("!rand", reinterpret_cast<void*>(&native_rand))) {
        std::fprintf(stderr, "the exe does not import rand\n");
        return 2;
    }
    image.make_return(kNativeDebugLog);
    using Recombine = void(__attribute__((thiscall)) *)(void*, void*, void*);
    const auto native_recombine = image.at<Recombine>(kRecombine);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    SharedRandom shared;
    long differing = 0;
    unsigned long long crossovers_seen = 0, duplications_seen = 0, omissions_seen = 0, mutations_seen = 0;
    for (long index = 0; index < cases; ++index) {
        std::vector<std::uint8_t> mother = genomes[random.below(static_cast<std::uint32_t>(genomes.size()))];
        std::vector<std::uint8_t> father = random.chance(4)
            ? mother : genomes[random.below(static_cast<std::uint32_t>(genomes.size()))];
        vary(mother, random);
        vary(father, random);
        const std::uint32_t rand_seed = random.next();

        // ---- LC ----
        OneGenomeFile mother_file(mother), father_file(father);
        genome::Genome lc_mother(1, genome::GenomeSex::male, genome::GenomeLifeStage::stage_zero, &mother_file);
        genome::Genome lc_father(2, genome::GenomeSex::male, genome::GenomeLifeStage::stage_zero, &father_file);
        genome::Genome lc_child(3, genome::GenomeSex::male, genome::GenomeLifeStage::stage_zero);
        std::uint32_t lc_crossovers = 0, lc_duplications = 0, lc_omissions = 0, lc_mutations = 0;
        g_rand_state = rand_seed;
        lc_child.recombine_genome_streams(lc_mother, lc_father, shared, lc_crossovers, lc_duplications,
                                          lc_omissions, lc_mutations);
        const std::uint32_t lc_rand_after = g_rand_state;

        // ---- native ----
        const std::size_t capacity = 4 * std::max(mother.size(), father.size()) + 65536;
        NativeGenome native_mother(mother, 0), native_father(father, 0);
        NativeGenome native_child(std::vector<std::uint8_t>(), capacity);
        field<std::uint32_t>(native_child.block.data(), 0xc) = 0;
        *image.at<std::uint32_t*>(kCrossovers) = 0xdead;
        g_rand_state = rand_seed;
        native_recombine(native_child.block.data(), native_mother.block.data(), native_father.block.data());
        const std::uint32_t native_rand_after = g_rand_state;
        const std::uint32_t native_size = field<std::uint32_t>(native_child.block.data(), 0xc);
        const std::uint32_t native_crossovers = *image.at<std::uint32_t*>(kCrossovers);
        const std::uint32_t native_duplications = *image.at<std::uint32_t*>(kDuplications);
        const std::uint32_t native_omissions = *image.at<std::uint32_t*>(kOmissions);
        const std::uint32_t native_mutations = *image.at<std::uint32_t*>(kMutations);
        crossovers_seen += native_crossovers;
        duplications_seen += native_duplications;
        omissions_seen += native_omissions;
        mutations_seen += native_mutations;

        // ---- compare ----
        std::string first;
        char text[256];
        auto note = [&](const std::string& what) { first += (first.empty() ? "" : "; ") + what; };
        const auto& child = lc_child.payload();
        if (native_size != child.size()) {
            std::snprintf(text, sizeof text, "child size: native %u lc %u", native_size,
                          static_cast<unsigned>(child.size()));
            note(text);
        } else if (std::memcmp(native_child.bytes.data(), child.data(), child.size()) != 0) {
            std::size_t at = 0;
            while (native_child.bytes[at] == child[at]) ++at;
            std::snprintf(text, sizeof text, "child byte %u: native %02x lc %02x", static_cast<unsigned>(at),
                          native_child.bytes[at], child[at]);
            note(text);
        }
        if (std::memcmp(native_mother.bytes.data(), lc_mother.payload().data(), mother.size()) != 0) note("mother's bytes");
        if (std::memcmp(native_father.bytes.data(), lc_father.payload().data(), father.size()) != 0) note("father's bytes");
        if (native_crossovers != lc_crossovers || native_duplications != lc_duplications ||
            native_omissions != lc_omissions || native_mutations != lc_mutations) {
            std::snprintf(text, sizeof text, "counters: native %u %u %u %u lc %u %u %u %u", native_crossovers,
                          native_duplications, native_omissions, native_mutations, lc_crossovers,
                          lc_duplications, lc_omissions, lc_mutations);
            note(text);
        }
        if (native_rand_after != lc_rand_after) note("rand() draws");
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: %s\n", index, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %llu crossovers, %llu duplications, %llu omissions, %llu mutations\n",
                crossovers_seen, duplications_seen, omissions_seen, mutations_seen);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
