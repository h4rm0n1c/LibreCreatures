// A creature's genome loci: the original's Creature::ResolveGenomeLocus
// @00408d30 (vtable slot 0 of the sub-object at Creature +0x804) against
// creatures::Creature::resolve_genome_locus.
//
//   locus_equivalence.exe <Creatures.exe>
//
// Every (kind, tissue, locus) a gene can name -- receptor and emitter,
// tissues 0 to 7, loci 0 to 255 -- is resolved on both sides.  The
// original gives an address in its Creature block (or its "no locus"
// byte outside it); LC gives a field of its Creature (or its sentinel).
// Genes reach a creature's state only through this function, so the pairs
// say where each LC field is in the original.  They must be one to one
// (one LC field, one offset), "no locus" on one side only where it is on
// the other, and every field of LC's ControlState must be at the offset of
// its place in the struct from +0x2c24, which is what the load test's
// table (src/c1/creatures/load_dump_fields.inc) uses for the control
// state, the floating and gait loci and the drives.  The other pairs (the
// pose transitions, the death state) are listed and checked against the
// table's offsets too.
#include "native_image.hpp"

#include "creatures/creature.hpp"
#include "creatures/genome.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace c1 = creatures1;
using c1::creatures::Creature;
using c1::creatures::GenomeLocusKind;

namespace {

constexpr std::uint32_t kResolveGenomeLocus = 0x00408d30;
constexpr std::size_t kBlockSize = 0x2d00, kResolver = 0x804;
// The load test's offsets this test checks.
constexpr std::size_t kControlState = 0x2c24, kPoseTransitions = 0x800, kDeathState = 0x2ccc;

using NativeResolve = std::uint8_t*(__attribute__((thiscall)) *)(void*, std::uint32_t, std::uint32_t, std::uint32_t);

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: locus_equivalence <Creatures.exe>\n");
        return 2;
    }
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    std::vector<std::uint8_t> native(kBlockSize, 0);
    const auto resolve = image.at<NativeResolve>(kResolveGenomeLocus);
    auto lc = std::make_unique<Creature>();
    const auto* lc_base = reinterpret_cast<const std::uint8_t*>(lc.get());
    const auto* controls = reinterpret_cast<const std::uint8_t*>(&lc->control_state());
    const std::size_t controls_size = sizeof(lc->control_state());
    const auto* pose_transitions = &lc->skeleton().pose_transition_component_count;

    long resolved = 0, unresolved = 0, failures = 0;
    std::map<const std::uint8_t*, long> lc_to_native;
    std::map<long, const std::uint8_t*> native_to_lc;
    auto fail = [&](const std::string& what) {
        if (++failures <= 20) std::printf("%s\n", what.c_str());
    };
    for (int kind = 0; kind < 2; ++kind) {
        for (int tissue = 0; tissue < 8; ++tissue) {
            for (int locus = 0; locus < 256; ++locus) {
                const std::uint8_t* n = resolve(native.data() + kResolver, kind, tissue, locus);
                const long offset = n - native.data();
                const bool native_none = offset < 0 || offset >= static_cast<long>(kBlockSize);
                const std::uint8_t* p = lc->resolve_genome_locus(static_cast<GenomeLocusKind>(kind),
                                                                 static_cast<std::uint8_t>(tissue),
                                                                 static_cast<std::uint8_t>(locus));
                const long lc_offset = p - lc_base;
                const bool lc_none = lc_offset < 0 || lc_offset >= static_cast<long>(sizeof(Creature));
                char text[160];
                std::snprintf(text, sizeof text, "kind %d tissue %d locus %d: native %s0x%lx, lc %s+0x%lx", kind,
                              tissue, locus, native_none ? "none " : "", native_none ? 0L : offset,
                              lc_none ? "none " : "", lc_none ? 0L : lc_offset);
                if (native_none != lc_none) {
                    fail(std::string("one side has no locus: ") + text);
                    continue;
                }
                if (native_none) {
                    ++unresolved;
                    continue;
                }
                ++resolved;
                const auto a = lc_to_native.emplace(p, offset);
                const auto b = native_to_lc.emplace(offset, p);
                if (a.first->second != offset || b.first->second != p) {
                    fail(std::string("not one to one: ") + text);
                }
                if (p >= controls && p < controls + controls_size) {
                    if (offset != static_cast<long>(kControlState) + (p - controls)) {
                        fail(std::string("control state out of place: ") + text);
                    }
                } else if (p == pose_transitions) {
                    if (offset != static_cast<long>(kPoseTransitions)) fail(std::string("pose transitions: ") + text);
                } else if (kind == 1 && tissue == 3 && locus == 0) {
                    // Emitter tissue 3 locus 0: LC's death state.
                    if (offset != static_cast<long>(kDeathState)) fail(std::string("death state: ") + text);
                } else {
                    fail(std::string("a field the test does not know: ") + text);
                }
            }
        }
    }
    std::printf("%ld loci resolved to %zu fields, %ld with no locus on both sides; control state 0x%zx bytes\n",
                resolved, lc_to_native.size(), unresolved, controls_size);
    for (const auto& [offset, p] : native_to_lc) {
        std::printf("  0x%04lx <- lc +0x%04lx\n", offset, static_cast<long>(p - lc_base));
    }
    std::printf("%s (%ld failures)\n", failures == 0 ? "OK" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
