// Function-level equivalence: CBrain::Update @00404e90 in the original
// Creatures.exe (the early phase of lobes 0-5, the perception merge and the
// late phase of lobes 6+, with its SVRules, learning, re-attachment and
// migration) against LibreCreatures' Brain::update.
//
//   brain_equivalence.exe <Creatures.exe> <genome.gen>... [-cases N] [-seed S]
//
// Each case builds a brain from one of the genomes with LC's genome loader,
// saves it in the world format, and perturbs the saved state at random:
// neuron activations and firing, dendrite sources, weights and strengths
// (loose ones included), each lobe's four chemical inputs and the
// winner-take-all exclusions.  The perturbed state goes into an LC Brain
// through its world load, and into a native CBrain laid out as Ghidra has it.
// Both run the same ticks with rand() seeded alike, and every neuron and
// dendrite field, and each lobe's active count, active fraction and loose
// dendrite counts, must then be the same.
//
// Built native (LC_NATIVE_MODE) every case must match.  Built with fixes,
// differing cases are listed (EQUIVALENCE_DIFFS) for attribution.

#include "brain_native.hpp"

#include <fstream>
#include <iterator>

using namespace brain_case;

namespace {

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n",
                     argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0xc1b4a1u;
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
    const auto native_update = image.at<NativeUpdate>(kNativeBrainUpdate);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    std::FILE* differences = nullptr;
    if (const char* path = std::getenv("EQUIVALENCE_DIFFS")) {
        differences = std::fopen(path, "w");
    }

    // EQUIVALENCE_CASE=<n> prints that case tick by tick.
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, updates = 0;
    for (long index = 0; index < cases; ++index) {
        // A brain from a real genome, saved, then perturbed.
        OneGenomeFile file(genomes[random.below(static_cast<std::uint32_t>(genomes.size()))]);
        genome::Genome g(1, random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female,
                         genome::GenomeLifeStage::stage_zero, &file);
        auto grown = std::make_unique<brain::Brain>();
        std::srand(random.next());
        grown->load_genome(g);
        BrainImage start = parse(save(*grown));
        perturb(start, random);

        auto lc = std::make_unique<brain::Brain>();
        ByteArchive load(emit(start));
        lc->serialize(load);
        NativeBrain native;
        build_native(start, *lc, native);

        const std::uint32_t rand_seed = random.next();
        std::uint32_t tick = random.next();
        const std::uint32_t ticks = 1 + random.below(6);
        // Tick by tick, so a difference can be placed; each side keeps its
        // own rand stream, seeded alike.
        g_lc_rand_state = rand_seed;
        g_native_rand_state = rand_seed;
        std::uint32_t first_tick_differing = ticks;
        std::uint32_t rand_calls_native = 0, rand_calls_lc = 0;
        for (std::uint32_t t = 0; t < ticks; ++t) {
            const std::uint32_t native_before = g_native_rand_state;
            std::vector<std::uint32_t> callers;
            g_rand_callers = verbose_case == index ? &callers : nullptr;
            native_update(native.self.data(), tick + t);
            g_rand_callers = nullptr;
            if (verbose_case == index) {
                std::printf("tick %u native rand callers:", t);
                std::uint32_t last = 0, run = 0;
                for (std::size_t c = 0; c <= callers.size(); ++c) {
                    if (c < callers.size() && callers[c] == last) {
                        ++run;
                        continue;
                    }
                    if (run != 0) std::printf(" %08x x%u", last, run);
                    if (c < callers.size()) { last = callers[c]; run = 1; }
                }
                std::printf("\n");
            }
            for (std::uint32_t s = native_before; s != g_native_rand_state;
                 s = s * 214013u + 2531011u) {
                ++rand_calls_native;
                if (rand_calls_native > 100000) break;
            }
            const std::uint32_t lc_before = g_lc_rand_state;
            std::vector<std::uint32_t> lc_callers;
            g_lc_rand_callers = verbose_case == index ? &lc_callers : nullptr;
            lc->update(tick + t);
            g_lc_rand_callers = nullptr;
            for (std::uint32_t s = lc_before; s != g_lc_rand_state; s = s * 214013u + 2531011u) {
                ++rand_calls_lc;
                if (rand_calls_lc > 100000) break;
            }
            if (verbose_case == index) {
                std::printf("tick %u LC rand callers:", t);
                std::uint32_t last = 0, run = 0;
                for (std::size_t c = 0; c <= lc_callers.size(); ++c) {
                    if (c < lc_callers.size() && lc_callers[c] == last) {
                        ++run;
                        continue;
                    }
                    if (run != 0) std::printf(" %08x x%u", last, run);
                    if (c < lc_callers.size()) { last = lc_callers[c]; run = 1; }
                }
                std::printf("\n");
            }
            if (first_tick_differing == ticks && verbose_case == index) {
                std::printf("tick %u: rand calls native %u, LC %u; %s\n", t,
                            rand_calls_native, rand_calls_lc,
                            describe_difference(read_native(native, start),
                                                parse(save(*lc))).c_str());
            }
        }
        updates += ticks;

        const BrainImage after_native = read_native(native, start);
        BrainImage after_lc = parse(save(*lc));
        std::string difference = describe_difference(after_native, after_lc);
        for (std::size_t i = 0; difference.empty() && i < start.lobes.size(); ++i) {
            std::uint8_t* p = lobe_at(native, i);
            brain::Lobe& lobe = lc->lobe(static_cast<std::uint32_t>(i));
            if (get32(p + kLobeActiveCount) != lobe.active_neuron_count()) {
                difference = "lobe " + std::to_string(i) + " active count: native " +
                             std::to_string(get32(p + kLobeActiveCount)) + ", LC " +
                             std::to_string(lobe.active_neuron_count());
            } else if (p[kLobeLoose] != lobe.late_phase_state().per_type_loose_dendrite_count[0] ||
                       p[kLobeLoose + 1] != lobe.late_phase_state().per_type_loose_dendrite_count[1]) {
                difference = "lobe " + std::to_string(i) + " loose dendrite counts";
            }
        }
        if (difference.empty()) {
            continue;
        }
        ++differing;
        if (differences != nullptr) {
            std::fprintf(differences, "%ld\n", index);
        }
        if (all_native && differing <= 10) {
            std::printf("case %ld differs after %u ticks (rand calls native %u, LC %u): %s\n",
                        index, ticks, rand_calls_native, rand_calls_lc, difference.c_str());
        }
    }
    if (differences != nullptr) {
        std::fclose(differences);
    }
    std::printf("%ld cases, %ld brain updates, LC native rows: %s\n", cases, updates,
                creatures1::common::compat_summary().c_str());
    if (!all_native) {
        std::printf("differing cases: %ld (fixes on: see the attribution)\n", differing);
        return 0;
    }
    std::printf("differing cases: %ld -> %s\n", differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
