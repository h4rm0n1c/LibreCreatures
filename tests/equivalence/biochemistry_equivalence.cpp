// Function-level equivalence: CBiochemistry::Update @0042ee10 in the
// original Creatures.exe against LibreCreatures' Biochemistry::update.
//
//   biochemistry_equivalence.exe <Creatures.exe> [cases] [seed]
//
// Each case builds one random biochemistry (chemicals, emitters, receptors,
// reactions, the loci they read and write) twice: as the native struct, laid
// out as CBiochemistry, and as an LC Biochemistry loaded through its world
// archive path.  Both run the same ticks; every concentration and locus byte
// must then be the same.
//
// Built native (LC_NATIVE_MODE) every case must match.  Built with LC's
// fixes, run_biochemistry.sh checks that every differing case also differs
// with only row 50 (a product yield past a byte) or only row 51 (the same
// reactant in both slots) fixed, so no difference comes from anything else.  Inputs native would crash on
// (a zero emission period or reaction amount: a division by zero) are not
// generated.

#include "biochemistry_native.hpp"

using namespace biochemistry_case;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> [cases] [seed]\n", argv[0]);
        return 2;
    }
    const long cases = argc > 2 ? std::atol(argv[2]) : 100000;
    const std::uint64_t seed = argc > 3 ? std::strtoull(argv[3], nullptr, 0) : 0x1996c1u;

    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    const auto native_update = image.at<NativeUpdate>(kNativeUpdate);

    if (const char* only = std::getenv("EQUIVALENCE_ONLY")) {
        g_only = only;
    }
    // Built with every switch native, any difference is a failure.  Built
    // with some fixes on, the differing cases are listed (EQUIVALENCE_DIFFS
    // names the file) for run_biochemistry.sh to attribute to their rows.
#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    std::FILE* differences = nullptr;
    if (const char* path = std::getenv("EQUIVALENCE_DIFFS")) {
        differences = std::fopen(path, "w");
    }
    Random random{seed};
    long differing = 0, unexplained = 0, ticks = 0;  // unexplained: native build
    for (long index = 0; index < cases; ++index) {
        const Case c = make_case(random);
        NativeState n;
        build_native(c, n);
        LcState l;
        build_lc(c, l);
        bool differs = false;
        for (std::uint32_t tick : c.ticks) {
            native_update(n.self.data(), tick);
            l.biochemistry.update(tick);
            ++ticks;
        }
        std::string first;
        for (std::size_t i = 0; i < 256; ++i) {
            const std::uint8_t a = n.self[kChemicals + 2 * i];
            const std::uint8_t b = l.biochemistry.chemical_states()[i].concentration;
            if (a != b) {
                differs = true;
                if (first.empty()) {
                    first = "chemical " + std::to_string(i) + ": native " + std::to_string(a) +
                            ", LC " + std::to_string(b);
                }
            }
        }
        for (std::size_t i = 0; i < kLocusPool; ++i) {
            if (n.loci[i] != l.loci[i]) {
                differs = true;
                if (first.empty()) {
                    first = "locus " + std::to_string(i) + ": native " +
                            std::to_string(n.loci[i]) + ", LC " + std::to_string(l.loci[i]);
                }
            }
        }
        if (!differs) {
            continue;
        }
        ++differing;
        if (differences != nullptr) {
            std::fprintf(differences, "%ld\n", index);
        }
        if (all_native) {
            if (++unexplained <= 10) {
                std::printf("case %ld differs: %s (%zu emitters, %zu receptors, %zu reactions, "
                            "%zu ticks)\n", index, first.c_str(), c.emitters.size(),
                            c.receptors.size(), c.reactions.size(), c.ticks.size());
            }
        }
    }
    std::printf("%ld cases, %ld updates, LC native rows: %s\n", cases, ticks,
                creatures1::common::compat_summary().c_str());
    if (differences != nullptr) {
        std::fclose(differences);
    }
    if (!all_native) {
        std::printf("differing cases: %ld (fixes on: see the attribution)\n", differing);
        return 0;
    }
    std::printf("differing cases: %ld -> %s\n", differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
