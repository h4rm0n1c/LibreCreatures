// Function-level equivalence: Creature::ProcessDreaming @00409360 with
// CInstinct::RunDreamStep @00407370 (CE 10.3), against
// creatures::Creature::process_dreaming.
//
// Each case gives a creature a brain (a stock genome's, saved and perturbed
// as in the brain test), a random biochemistry (as in the biochemistry
// test) and a queue of 1 to 4 random instincts, then runs up to 90 dream
// steps: the reset step, replay steps (CBiochemistry::Update, the first
// eight lobes cleared, the instinct's neurons and decision neuron driven,
// CBrain::Update), the dream chemical restored on steps 0x23-0x27, and the
// finishing step that raises every dendrite's long-term weight to its
// short-term weight, resets the brain, drops the instinct and counts the
// dream down.  The native side is real: a CBrain and a CBiochemistry built
// by the brain and biochemistry tests' builders, native CInstinct blocks,
// and a Creature-shaped block at the offsets ProcessDreaming reads.  rand is
// one formula with equal seeds on both sides.
//
// After each step the whole brain (every neuron and dendrite), every
// concentration and locus, the instinct queue (count, countdown, each
// instinct's fields), the biochemistry tick and the instincts deleted must
// match.
//
// Not generated: negative instinct neuron indices (genome bytes are 0-255).

#include "brain_native.hpp"
#include "biochemistry_native.hpp"

#include "creatures/creature.hpp"

#include <fstream>
#include <iterator>
#include <memory>

namespace creatures1::creatures {
// Declared a friend in creature.hpp.
class CreatureTestAccess {
public:
    static std::uint8_t& life_stage(Creature& c) { return c.genome_life_stage_; }
    static std::uint32_t& biochemistry_tick(Creature& c) { return c.biochemistry_tick_; }
};
}  // namespace creatures1::creatures

namespace {

using creatures1::creatures::CreatureTestAccess;
using brain_case::Random;

constexpr std::uint32_t kNativeProcessDreaming = 0x00409360;

// Native Creature layout used by ProcessDreaming.
constexpr std::size_t kCreatureSize = 0x2d00;
constexpr std::size_t kBrain = 0x2c18, kBiochemistry = 0x2c1c, kTick = 0x2c20;
constexpr std::size_t kInstincts = 0x2c68;  // CInstinct* [20]
constexpr std::size_t kInstinctCount = 0x2cb8, kDreamCountdown = 0x2cbc, kLifeStage = 0x2cc0;
// CInstinct: vptr, lobes[3], neurons[3], decision neuron, chemical,
// concentration, step.
constexpr std::size_t kInstinctSize = 44;

using NativeProcess = void(__attribute__((thiscall)) *)(void* creature);

// The finished instinct is deleted through its vtable; record it instead.
int g_native_deleted = 0;
void __attribute__((thiscall)) native_instinct_delete(void*, int) { ++g_native_deleted; }
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected CInstinct virtual was called\n");
    std::exit(5);
}
void* g_instinct_vtable[8];

std::uint32_t& u32(std::uint8_t* p) { return *reinterpret_cast<std::uint32_t*>(p); }

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

}  // namespace

int main(int argc, char** argv) {
    using namespace brain_case;
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n",
                     argv[0]);
        return 2;
    }
    long cases = 500;
    std::uint64_t seed = 0xd7ea11u;
    std::vector<std::vector<std::uint8_t>> genomes;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-cases" && i + 1 < argc) {
            cases = std::atol(argv[++i]);
        } else if (arg == "-seed" && i + 1 < argc) {
            seed = std::strtoull(argv[++i], nullptr, 0);
        } else {
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
        std::fprintf(stderr, "the exe imports no rand\n");
        return 2;
    }
    image.make_return(kNativeDebugLog);
    const auto native_process = image.at<NativeProcess>(kNativeProcessDreaming);
    for (auto& slot : g_instinct_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_instinct_vtable[1] = reinterpret_cast<void*>(&native_instinct_delete);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, steps = 0, finished = 0;
    for (long index = 0; index < cases; ++index) {
        // ---- brain, as the brain test makes it ----
        OneGenomeFile file(genomes[random.below(static_cast<std::uint32_t>(genomes.size()))]);
        genome::Genome g(1, random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female,
                         genome::GenomeLifeStage::stage_zero, &file);
        auto grown = std::make_unique<brain::Brain>();
        std::srand(random.next());
        grown->load_genome(g);
        BrainImage start = parse(save(*grown));
        perturb(start, random);
        auto* lc_brain = new brain::Brain();
        ByteArchive load(emit(start));
        lc_brain->serialize(load);
        NativeBrain native_brain;
        build_native(start, *lc_brain, native_brain);

        // ---- biochemistry, as the biochemistry test makes it ----
        biochemistry_case::Random bio_random{random.next() | 1};
        const biochemistry_case::Case chemistry = biochemistry_case::make_case(bio_random);
        biochemistry_case::NativeState native_bio;
        biochemistry_case::build_native(chemistry, native_bio);
        auto* lc_bio = new bio::Biochemistry();
        std::array<std::uint8_t, biochemistry_case::kLocusPool> lc_loci{};
        biochemistry_case::build_lc_into(chemistry, *lc_bio, lc_loci);

        // ---- creature ----
        auto creature = std::make_unique<creatures1::creatures::Creature>();
        creature->set_brain(lc_brain);
        creature->set_biochemistry(lc_bio);
        std::vector<std::uint8_t> native(kCreatureSize, 0);
        u32(native.data() + kBrain) = address(native_brain.self.data());
        u32(native.data() + kBiochemistry) = address(native_bio.self.data());
        const std::uint32_t tick = random.next();
        CreatureTestAccess::biochemistry_tick(*creature) = tick;
        u32(native.data() + kTick) = tick;
        const std::uint8_t stage = static_cast<std::uint8_t>(random.below(3));
        CreatureTestAccess::life_stage(*creature) = stage;
        native[kLifeStage] = stage;

        auto& state = creature->instinct_runtime_state();
        const std::uint32_t count = 1 + random.below(4);
        const std::uint32_t countdown = 1 + random.below(count + 1);
        state.instinct_count = count;
        state.dream_countdown = countdown;
        u32(native.data() + kInstinctCount) = count;
        u32(native.data() + kDreamCountdown) = countdown;
        std::vector<std::vector<std::uint8_t>> native_instincts;
        const std::uint32_t lobe_count = static_cast<std::uint32_t>(start.lobes.size());
        for (std::uint32_t i = 0; i < count; ++i) {
            auto& instinct = state.instincts[i];
            for (int r = 0; r < 3; ++r) {
                instinct.replay_lobe_indices[r] =
                    random.chance(3) ? 0 : random.below(lobe_count + 2);
                instinct.replay_neuron_indices[r] = random.below(80);
            }
            instinct.decision_lobe_neuron_index = random.below(20);
            instinct.dream_chemical_index = random.byte();
            instinct.dream_chemical_concentration = random.byte();
            instinct.dream_step_index = random.chance(4) ? random.below(0x29) : 0;
            native_instincts.emplace_back(kInstinctSize, 0);
            std::uint8_t* p = native_instincts.back().data();
            u32(p) = address(g_instinct_vtable);
            for (int r = 0; r < 3; ++r) {
                u32(p + 4 + 4 * r) = instinct.replay_lobe_indices[r];
                u32(p + 16 + 4 * r) = instinct.replay_neuron_indices[r];
            }
            u32(p + 28) = instinct.decision_lobe_neuron_index;
            u32(p + 32) = instinct.dream_chemical_index;
            u32(p + 36) = instinct.dream_chemical_concentration;
            u32(p + 40) = instinct.dream_step_index;
            u32(native.data() + kInstincts + 4 * i) = address(p);
        }

        // ---- dream ----
        const std::uint32_t rand_seed = random.next();
        g_lc_rand_state = rand_seed;
        g_native_rand_state = rand_seed;
        g_native_deleted = 0;
        int lc_deleted = 0;
        const std::uint32_t step_count = 1 + random.below(90);
        bool case_differs = false;
        for (std::uint32_t step = 0; step < step_count && !case_differs; ++step) {
            if (state.dream_countdown == 0 && u32(native.data() + kDreamCountdown) == 0) break;
            const std::uint32_t lc_count_before = state.instinct_count;
            creature->process_dreaming();
            if (state.instinct_count < lc_count_before) ++lc_deleted;
            native_process(native.data());
            ++steps;

            std::string first;
            char text[240];
            auto note = [&](const std::string& what) { if (first.empty()) first = what; };
            const std::string brain_difference =
                describe_difference(read_native(native_brain, start), parse(save(*lc_brain)));
            if (!brain_difference.empty()) note("brain: " + brain_difference);
            for (int c = 0; c < 256; ++c) {
                if (native_bio.self[biochemistry_case::kChemicals + 2 * c] !=
                    lc_bio->chemical_states()[c].concentration) {
                    std::snprintf(text, sizeof text, "chemical %d: native %u lc %u", c,
                                  native_bio.self[biochemistry_case::kChemicals + 2 * c],
                                  lc_bio->chemical_states()[c].concentration);
                    note(text);
                }
            }
            if (native_bio.loci != lc_loci) note("loci");
            if (u32(native.data() + kTick) != CreatureTestAccess::biochemistry_tick(*creature)) {
                note("biochemistry tick");
            }
            const std::uint32_t native_count = u32(native.data() + kInstinctCount);
            if (native_count != state.instinct_count) {
                std::snprintf(text, sizeof text, "instinct count: native %u lc %u", native_count,
                              state.instinct_count);
                note(text);
            }
            if (u32(native.data() + kDreamCountdown) != state.dream_countdown) {
                std::snprintf(text, sizeof text, "dream countdown: native %u lc %u",
                              u32(native.data() + kDreamCountdown), state.dream_countdown);
                note(text);
            }
            if (g_native_deleted != lc_deleted) note("instincts deleted");
            for (std::uint32_t i = 0; i < native_count && i < state.instinct_count; ++i) {
                std::uint8_t* p = reinterpret_cast<std::uint8_t*>(
                    static_cast<std::uintptr_t>(u32(native.data() + kInstincts + 4 * i)));
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
                    std::snprintf(text, sizeof text, "instinct %u: native step %u lc step %u", i,
                                  u32(p + 40), instinct.dream_step_index);
                    note(text);
                }
            }
            if (g_native_rand_state != g_lc_rand_state) note("rand draws");
            if (!first.empty() || index == verbose_case) {
                std::printf("case %ld step %u: instincts %u countdown %u stage %u: %s\n", index, step,
                            count, countdown, stage, first.empty() ? "same" : first.c_str());
            }
            if (!first.empty()) case_differs = true;
        }
        finished += g_native_deleted;
        if (case_differs) ++differing;
    }
    std::printf("%ld cases, %ld dream steps, %ld instincts finished: %ld differ%s\n", cases, steps,
                finished, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
