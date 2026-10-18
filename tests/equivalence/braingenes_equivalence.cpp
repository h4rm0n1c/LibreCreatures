// Function-level equivalence: the brain genes of a genome,
// CBrain::LoadGenome @00404560 (CE 10.3: the lobe genes of both load
// passes, CLobe::LoadGenome, AllocateRuntimeState, InitializeConnections and
// the perception wiring) against brain::Brain::load_genome.  The genome
// re-expression (0040d930) calls it at stage 0 only, and both test for
// stage 0 again, so the cases are at stage 0.
//
//   braingenes_equivalence.exe <Creatures.exe> <genome.gen>... [-cases N] [-seed S]
//
// The native genome reader is a CGenome in memory, as in the creature gene
// test, with a zero pad and a gend after the genome (row 81).  The native
// CBrain is a zeroed block; its lobes allocate their neurons and dendrites
// through the MFC operator new[], bound to an allocator that zeros
// the memory (EQUIVALENCE_FILL sets another byte).  rand is the MSVC formula
// on both sides, seeded alike.
//
// Each case: one of the stock genomes, either sex; in half the cases up to
// 40 bytes of the genome change (never in a gene or end marker).  After the
// load the brains must be the same in the world format: every lobe's
// parameters and rules, and every neuron and dendrite.

#include "brain_native.hpp"

#include <fstream>
#include <iterator>

using namespace brain_case;

namespace {

constexpr std::uint32_t kLoadGenome = 0x00404560, kGenomeVtable = 0x00456094;

// EQUIVALENCE_FILL=<byte>: new memory is filled with that byte instead of
// zeros, to show any field native leaves as the heap had it.
std::vector<void*> g_allocations;
int g_fill = 0;
extern "C" void* __cdecl native_new(std::size_t n) {
    void* p = std::malloc(n == 0 ? 1 : n);
    std::memset(p, g_fill, n);
    g_allocations.push_back(p);
    return p;
}

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

const std::uint8_t* pointer_at(const std::uint8_t* p) {
    return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(get32(p)));
}

// The native brain in the world format's terms, read from its own arrays.
BrainImage read_native_brain(const std::uint8_t* self) {
    BrainImage image;
    const std::uint32_t count = get32(self + kBrainLobeCount);
    image.lobes.resize(count);
    const auto lobe = [self](std::size_t i) { return self + kBrainLobes + i * kLobeSize; };
    for (std::size_t i = 0; i < count; ++i) {
        LobeImage& l = image.lobes[i];
        const std::uint8_t* p = lobe(i);
        l.grid_x = get32(p + 4);
        l.grid_y = get32(p + 8);
        l.width = get32(p + 12);
        l.height = get32(p + 16);
        l.threshold = p[20];
        l.relax = p[21];
        l.baseline = p[22];
        l.gain = p[23];
        std::memcpy(l.state_rule, p + 24, 10);
        l.wta = p[34];
        for (int rule = 0; rule < 2; ++rule) {
            Rule& r = l.rules[rule];
            const std::uint8_t* q = p + kLobeRules + 60 * rule;
            r.source_lobe = get32(q);
            std::memcpy(r.bytes, q + 4, 12);
            r.bytes[12] = q[16];
            r.bytes[13] = q[27];
            std::memcpy(r.expressions[0], q + 17, 10);
            std::memcpy(r.expressions[1], q + 28, 10);
            std::memcpy(r.expressions[2], q + 38, 10);
            std::memcpy(r.expressions[3], q + 48, 10);
        }
        l.neuron_count = get32(p + kLobeNeuronCount);
        l.connection_count = get32(p + kLobeConnectionCount);
        l.percept = get32(p + kLobePercept);
        l.active_fraction = p[kLobeActiveFraction];
        std::memcpy(l.token_state, p + kLobeTokenState, 4);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const LobeImage& l = image.lobes[i];
        const std::uint8_t* p = lobe(i);
        const std::uint8_t* neurons = pointer_at(p + kLobeNeurons);
        const std::uint8_t* connections = pointer_at(p + kLobeConnections);
        std::vector<NeuronImage> out(l.neuron_count);
        for (std::size_t n = 0; n < l.neuron_count; ++n) {
            NeuronImage& ni = out[n];
            const std::uint8_t* np = neurons + n * kNeuronSize;
            ni.grid_x = np[0];
            ni.grid_y = np[1];
            ni.firing = np[2];
            ni.activation = np[3];
            ni.excluded = np[14];
            ni.source = np[15];
            for (int rule = 0; rule < 2; ++rule) {
                ni.counts[rule] = np[12 + rule];
                const std::uint8_t* first = pointer_at(np + 4 + 4 * rule);
                ni.begins[rule] = static_cast<std::uint32_t>((first - connections) / kDendriteSize);
                const std::uint32_t source = l.rules[rule].source_lobe;
                for (std::size_t d = 0; d < ni.counts[rule]; ++d) {
                    const std::uint8_t* dp = first + d * kDendriteSize;
                    DendriteImage di;
                    const std::uint32_t target = get32(dp);
                    if (source < count && target != 0) {
                        const std::uint32_t base = address(pointer_at(lobe(source) + kLobeNeurons));
                        const std::uint32_t size = get32(lobe(source) + kLobeNeuronCount) * kNeuronSize;
                        di.target = target >= base && target < base + size
                                        ? static_cast<std::int32_t>((target - base) / kNeuronSize)
                                        : -2;
                    }
                    di.grid_x = dp[4];
                    di.grid_y = dp[5];
                    di.suscept = dp[6];
                    di.stw = dp[7];
                    di.ltw = dp[8];
                    di.strength = dp[9];
                    ni.dendrites[rule].push_back(di);
                }
            }
        }
        image.neurons.push_back(std::move(out));
    }
    return image;
}

// Where two brain images first differ.
std::string first_difference(const BrainImage& a, const BrainImage& b) {
    char text[200];
    if (a.lobes.size() != b.lobes.size()) {
        std::snprintf(text, sizeof text, "lobe count: native %u lc %u", static_cast<unsigned>(a.lobes.size()),
                      static_cast<unsigned>(b.lobes.size()));
        return text;
    }
    for (std::size_t i = 0; i < a.lobes.size(); ++i) {
        BrainImage la, lb;
        la.lobes = {a.lobes[i]};
        lb.lobes = {b.lobes[i]};
        const std::vector<std::uint8_t> ea = emit(la), eb = emit(lb);
        if (ea != eb) {
            std::size_t at = 0;
            while (ea[at] == eb[at]) ++at;
            // Offsets in the world format, after the lobe count (4 bytes).
            std::snprintf(text, sizeof text, "lobe %u parameters: byte %u: native %u lc %u",
                          static_cast<unsigned>(i), static_cast<unsigned>(at - 4), ea[at], eb[at]);
            return text;
        }
    }
    for (std::size_t i = 0; i < a.lobes.size(); ++i) {
        for (std::size_t n = 0; n < a.neurons[i].size(); ++n) {
            BrainImage na, nb;
            na.neurons = {{a.neurons[i][n]}};
            nb.neurons = {{b.neurons[i][n]}};
            const std::vector<std::uint8_t> ea = emit(na), eb = emit(nb);
            if (ea != eb) {
                std::size_t at = 0;
                while (at < ea.size() && at < eb.size() && ea[at] == eb[at]) ++at;
                // Offsets in the neuron's world-format record.
                std::snprintf(text, sizeof text, "lobe %u neuron %u: byte %u: native %u lc %u",
                              static_cast<unsigned>(i), static_cast<unsigned>(n),
                              static_cast<unsigned>(at - 4), at < ea.size() ? ea[at] : 0u,
                              at < eb.size() ? eb[at] : 0u);
                return text;
            }
        }
    }
    return "";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0xb4a19e5u;
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
    if (!image.bind_import("!rand", reinterpret_cast<void*>(&native_rand)) ||
        !image.bind_import("!#1507", reinterpret_cast<void*>(&native_new)) ||
        !image.bind_import("!#265", reinterpret_cast<void*>(&native_new))) {
        std::fprintf(stderr, "the exe imports no rand, operator new (#1507) or new[] (#265)\n");
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
    if (const char* fill = std::getenv("EQUIVALENCE_FILL")) g_fill = std::atoi(fill);
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, neurons = 0, dendrites = 0;
    for (long index = 0; index < cases; ++index) {
        const std::size_t which = random.below(static_cast<std::uint32_t>(genomes.size()));
        const auto sex = random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female;
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
        const std::uint32_t rand_seed = random.next();

        OneGenomeFile file(bytes);
        genome::Genome g(1, sex, genome::GenomeLifeStage::stage_zero, &file);
        auto lc = std::make_unique<brain::Brain>();
        g_lc_rand_state = rand_seed;
        lc->load_genome(g);

        std::vector<std::uint8_t> reader_bytes = bytes;
        reader_bytes.resize(bytes.size() + 1024, 0);
        reader_bytes.insert(reader_bytes.end(), {'g', 'e', 'n', 'd'});
        std::vector<std::uint8_t> reader(0x20, 0);
        put32(reader.data(), kGenomeVtable);
        put32(reader.data() + 4, address(reader_bytes.data()));
        put32(reader.data() + 8, address(reader_bytes.data()));
        put32(reader.data() + 0xc, static_cast<std::uint32_t>(bytes.size()));
        put32(reader.data() + 0x10, 1);
        put32(reader.data() + 0x14, static_cast<std::uint32_t>(sex));
        std::vector<std::uint8_t> native(kBrainSize, 0);
        g_native_rand_state = rand_seed;
        native_load(native.data(), reader.data());

        const BrainImage lc_image = parse(save(*lc));
        BrainImage native_image = read_native_brain(native.data());
        if (std::getenv("EQUIVALENCE_MASK_STATE") != nullptr) {
            // Native leaves each new neuron's firing and activation, and each
            // new dendrite's susceptibility, as the heap had them (row 82);
            // LC starts them at 0.
            for (auto& lobe : native_image.neurons) {
                for (NeuronImage& n : lobe) {
                    n.firing = n.activation = 0;
                    for (auto& rule : n.dendrites) {
                        for (DendriteImage& d : rule) d.suscept = 0;
                    }
                }
            }
        }
        std::string first = emit(lc_image) == emit(native_image) ? "" : first_difference(native_image, lc_image);
        if (first.empty() && g_lc_rand_state != g_native_rand_state) first = "rand state";
        for (const auto& lobe : lc_image.neurons) {
            neurons += static_cast<long>(lobe.size());
            for (const NeuronImage& n : lobe) dendrites += n.counts[0] + n.counts[1];
        }
        if (index == verbose_case) {
            for (std::size_t i = 0; i < lc_image.lobes.size() && i < native_image.lobes.size(); ++i) {
                long sums[2][2] = {};
                for (const NeuronImage& n : lc_image.neurons[i]) { sums[1][0] += n.counts[0]; sums[1][1] += n.counts[1]; }
                for (const NeuronImage& n : native_image.neurons[i]) { sums[0][0] += n.counts[0]; sums[0][1] += n.counts[1]; }
                std::printf("lobe %u: native %ld+%ld total %u, lc %ld+%ld total %u\n", static_cast<unsigned>(i),
                            sums[0][0], sums[0][1], native_image.lobes[i].connection_count, sums[1][0], sums[1][1],
                            lc_image.lobes[i].connection_count);
            }
        }
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: genome %u sex %u: %s\n", index, static_cast<unsigned>(which),
                        static_cast<unsigned>(sex), first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
        for (void* p : g_allocations) std::free(p);
        g_allocations.clear();
    }
    std::printf("covered: %ld neurons, %ld dendrites\n", neurons, dendrites);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
