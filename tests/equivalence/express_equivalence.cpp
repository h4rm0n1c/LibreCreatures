// Function-level equivalence: the creature genes of a genome at each life
// stage, Creature::LoadGenome @00408960 (CE 10.3, called by the genome
// re-expression @0040d930 at every stage change) against
// creatures::Creature::load_genome.
//
// The native genome reader is a CGenome built in memory: vtable 00456094,
// the genome bytes at +4, the cursor at +8, the size at +0xc, the moniker,
// the sex and the life stage.  (Only its constructor reads a file.)  The
// native creature is a Creature-shaped block with its brain's lobe sizes
// (for the instinct genes), the built-in stimulus contexts (+0x26f0), the
// pose table (+0x120), the gait table (+0x760) and the instinct list
// (+0x2c68, count +0x2cb8).  The MFC operator new is bound to malloc.
//
// Each case: one of the stock genomes, either sex, a life stage 0 to 6, and
// random earlier contents in every table (a gene overwrites only its own
// entry); half the cases change bytes of the genome.  After the load, the 36
// stimulus contexts, the 100 poses, the 8 gaits (all 18 bytes) and the
// instinct list must match.

#include "native_image.hpp"

#include "biochemistry/biochemistry.hpp"
#include "brain/brain.hpp"
#include "common/compat.hpp"
#include "creatures/creature.hpp"
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

namespace creatures1::creatures {
// Declared a friend in creature.hpp.
class CreatureTestAccess {
public:
    static Creature::InstinctRuntimeState& instincts(Creature& c) { return c.instinct_runtime_state_; }
};
}  // namespace creatures1::creatures

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;
using creatures::CreatureTestAccess;

constexpr std::uint32_t kLoadGenome = 0x00408960, kGenomeVtable = 0x00456094, kNativeDebugLog = 0x00418000;
constexpr std::size_t kBlockSize = 0x2d00, kBrain = 0x2c18, kContexts = 0x26f0, kContextStride = 20,
                      kPoseTable = 0x120, kGaitTable = 0x760, kGaitStride = 18, kInstincts = 0x2c68,
                      kInstinctCount = 0x2cb8, kInstinctSize = 44;
constexpr std::size_t kBrainSize = 0x800;

extern "C" void* __cdecl native_new(std::size_t n) { return std::calloc(1, n); }
extern "C" const char* __cdecl native_strchr(const char* s, int c) { return std::strchr(s, c); }

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
std::uint32_t u32(const std::uint8_t* p) { return *reinterpret_cast<const std::uint32_t*>(p); }

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0xe7e55u;
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
    if (!image.bind_import("!#1507", reinterpret_cast<void*>(&native_new)) ||
        !image.bind_import("!strchr", reinterpret_cast<void*>(&native_strchr))) {
        std::fprintf(stderr, "the exe does not import operator new (#1507) and strchr\n");
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
    long differing = 0, instincts_loaded = 0;
    for (long index = 0; index < cases; ++index) {
        const std::size_t which = random.below(static_cast<std::uint32_t>(genomes.size()));
        const auto sex = random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female;
        const auto stage = static_cast<genome::GenomeLifeStage>(random.below(7));
        // The brain grows from stage 0 of the unchanged genome, as the game's
        // creature does.
        OneGenomeFile brain_file(genomes[which]);
        genome::Genome brain_genome(1, sex, genome::GenomeLifeStage::stage_zero, &brain_file);
        // Half the cases change up to 40 bytes of the genome to the loaded
        // one, never in or to a gene or end marker, so that the loader also
        // sees values the stock genomes do not hold.
        std::vector<std::uint8_t> bytes = genomes[which];
        if (random.chance(2)) {
            for (std::uint32_t n = 1 + random.below(40); n != 0; --n) {
                // One in four is 0, 100 or 200: the values that end a gait.
                std::uint8_t value = random.chance(4) ? static_cast<std::uint8_t>(100 * random.below(3)) : random.byte();
                if (value != 0 && std::strchr("gends", value) != nullptr) value = 100;
                const std::size_t at = 4 + random.below(static_cast<std::uint32_t>(bytes.size() - 8));
                bool in_marker = false;
                for (std::size_t start = at - 3; start <= at; ++start) {
                    in_marker = in_marker || std::memcmp(&bytes[start], "gen", 3) == 0;
                }
                if (!in_marker) bytes[at] = value;
            }
        }
        OneGenomeFile file(bytes);
        auto creature = std::make_unique<creatures::Creature>();
        auto* brain = new brain::Brain();
        brain->load_genome(brain_genome);
        creature->set_brain(brain);
        creature->set_biochemistry(new biochemistry::Biochemistry());

        std::vector<std::uint8_t> native_storage(kBlockSize, 0), native_brain(kBrainSize, 0);
        std::uint8_t* native = native_storage.data();
        field<void*>(native, kBrain) = native_brain.data();
        for (int lobe = 0; lobe < 8; ++lobe) {
            field<std::int32_t>(native_brain.data(), 8 + 200 * lobe + 164) =
                static_cast<std::int32_t>(brain->lobe(static_cast<std::uint32_t>(lobe)).neuron_count_value());
        }
        // Earlier contents: a gene overwrites only its own entry.
        auto& contexts = creature->built_in_stimulus_contexts();
        for (std::size_t k = 0; k < contexts.size(); ++k) {
            std::uint8_t* raw = native + kContexts + k * kContextStride;
            for (int b = 0; b < 12; ++b) raw[8 + b] = random.byte();
            std::memcpy(&contexts[k].descriptor, raw + 8, 12);
        }
        auto& skeleton = creature->skeleton();
        for (std::size_t p = 0; p < creatures::kPoseTableEntryCount; ++p) {
            for (std::size_t c = 0; c < 15; ++c) {
                const char ch = "X0123456789?!"[random.below(13)];
                skeleton.pose_string_table[p].characters[c] = ch;
                native[kPoseTable + 16 * p + c] = static_cast<std::uint8_t>(ch);
            }
        }
        for (std::size_t g = 0; g < creatures::kGaitTableEntryCount; ++g) {
            for (std::size_t c = 0; c < kGaitStride; ++c) {
                const char ch = static_cast<char>(c < 17 && random.chance(2) ? '0' + random.below(10) : 0);
                skeleton.gait_animation_table[g][c] = ch;
                native[kGaitTable + kGaitStride * g + c] = static_cast<std::uint8_t>(ch);
            }
        }
        // The native reader, in memory.
        std::vector<std::uint8_t> reader(0x20, 0);
        field<std::uint32_t>(reader.data(), 0) = kGenomeVtable;
        field<std::uint8_t*>(reader.data(), 4) = bytes.data();
        field<std::uint8_t*>(reader.data(), 8) = bytes.data();
        field<std::uint32_t>(reader.data(), 0xc) = static_cast<std::uint32_t>(bytes.size());
        field<std::uint32_t>(reader.data(), 0x10) = 1;
        field<std::int32_t>(reader.data(), 0x14) = static_cast<std::int32_t>(sex);
        reader[0x18] = static_cast<std::uint8_t>(stage);

        genome::Genome g(1, sex, stage, &file);
        creature->load_genome(g);
        native_load(native, reader.data());

        // ---- compare ----
        std::string first;
        char text[256];
        auto note = [&](const std::string& what) { if (first.empty()) first = what; };
        for (std::size_t k = 0; k < contexts.size(); ++k) {
            if (std::memcmp(native + kContexts + k * kContextStride + 8, &contexts[k].descriptor, 12) != 0) {
                std::snprintf(text, sizeof text, "stimulus context %u", static_cast<unsigned>(k));
                note(text);
            }
        }
        for (std::size_t p = 0; p < creatures::kPoseTableEntryCount; ++p) {
            if (std::memcmp(native + kPoseTable + 16 * p, skeleton.pose_string_table[p].characters.data(), 15) != 0) {
                std::snprintf(text, sizeof text, "pose %u: native %.15s lc %.15s", static_cast<unsigned>(p),
                              reinterpret_cast<const char*>(native + kPoseTable + 16 * p),
                              skeleton.pose_string_table[p].characters.data());
                note(text);
            }
        }
        for (std::size_t gt = 0; gt < creatures::kGaitTableEntryCount; ++gt) {
            if (std::memcmp(native + kGaitTable + kGaitStride * gt, skeleton.gait_animation_table[gt].data(), kGaitStride) != 0) {
                const char* native_gait = reinterpret_cast<const char*>(native + kGaitTable + kGaitStride * gt);
                const char* lc_gait = skeleton.gait_animation_table[gt].data();
                std::snprintf(text, sizeof text, "gait %u: native %.18s lc %.18s%s", static_cast<unsigned>(gt),
                              native_gait, lc_gait,
                              std::strncmp(native_gait, lc_gait, kGaitStride) == 0 ? " (after the terminator)" : "");
                note(text);
            }
        }
        const auto& state = CreatureTestAccess::instincts(*creature);
        const std::uint32_t native_count = u32(native + kInstinctCount);
        instincts_loaded += native_count;
        if (native_count != state.instinct_count) {
            std::snprintf(text, sizeof text, "instinct count: native %u lc %u", native_count, state.instinct_count);
            note(text);
        } else {
            for (std::uint32_t i = 0; i < native_count; ++i) {
                const auto* p = reinterpret_cast<const std::uint8_t*>(
                    static_cast<std::uintptr_t>(u32(native + kInstincts + 4 * i)));
                const auto& instinct = state.instincts[i];
                bool same = u32(p + 28) == instinct.decision_lobe_neuron_index &&
                            u32(p + 32) == instinct.dream_chemical_index &&
                            u32(p + 36) == instinct.dream_chemical_concentration &&
                            u32(p + 40) == instinct.dream_step_index;
                for (int r = 0; r < 3; ++r) {
                    same = same && u32(p + 4 + 4 * r) == instinct.replay_lobe_indices[r] &&
                           u32(p + 16 + 4 * r) == instinct.replay_neuron_indices[r];
                }
                if (!same) {
                    std::snprintf(text, sizeof text, "instinct %u", i);
                    note(text);
                }
            }
        }
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: genome %u sex %u stage %u: %s\n", index, static_cast<unsigned>(which),
                        static_cast<unsigned>(sex), static_cast<unsigned>(stage), first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
        for (std::uint32_t i = 0; i < native_count; ++i) {
            std::free(reinterpret_cast<void*>(static_cast<std::uintptr_t>(u32(native + kInstincts + 4 * i))));
        }
    }
    std::printf("covered: %ld instincts loaded\n", instincts_loaded);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
