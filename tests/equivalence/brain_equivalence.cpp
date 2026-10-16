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

#include "native_image.hpp"

#include "brain/brain.hpp"
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

namespace brain = creatures1::brain;
namespace genome = creatures1::creatures;

namespace {

// ---- native layout (Ghidra: CBrain, CLobe, CLobeNeuron, CLobeConnection) ----
constexpr std::size_t kBrainSize = 6412;
constexpr std::size_t kBrainLobes = 8;
constexpr std::size_t kBrainLobeCount = 6408;
constexpr std::size_t kLobeSize = 200;
constexpr std::size_t kLobeRules = 36;       // CLobeConnectionRule[2], 60 each
constexpr std::size_t kLobeNeurons = 156;
constexpr std::size_t kLobeConnections = 160;
constexpr std::size_t kLobeNeuronCount = 164;
constexpr std::size_t kLobeConnectionCount = 168;
constexpr std::size_t kLobeActive = 172;
constexpr std::size_t kLobeActiveCount = 176;
constexpr std::size_t kLobePercept = 180;
constexpr std::size_t kLobeCandidate0 = 184;
constexpr std::size_t kLobeCandidate1 = 188;
constexpr std::size_t kLobeActiveFraction = 192;
constexpr std::size_t kLobeLoose = 193;      // [2]
constexpr std::size_t kLobeTokenState = 195; // [4]
constexpr std::size_t kNeuronSize = 16;
constexpr std::size_t kDendriteSize = 12;

constexpr std::uint32_t kNativeBrainUpdate = 0x00404e90;
constexpr std::uint32_t kNativeDebugLog = 0x00418000;

using NativeUpdate = void(__attribute__((thiscall)) *)(void* self, std::uint32_t tick);

// ---- rand: the MSVC formula, shared by both sides ----
std::uint32_t g_native_rand_state = 1;
// EQUIVALENCE_CASE: where each native rand call came from.
std::vector<std::uint32_t>* g_rand_callers = nullptr;
extern "C" int __cdecl native_rand() {
    if (g_rand_callers != nullptr) {
        g_rand_callers->push_back(static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
    }
    g_native_rand_state = g_native_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_native_rand_state >> 16) & 0x7fff);
}

// LC's rand, redirected here at link time (-Wl,--wrap=rand): the same
// formula and its own state, so both sides draw alike and calls can be
// traced.
std::uint32_t g_lc_rand_state = 1;
std::vector<std::uint32_t>* g_lc_rand_callers = nullptr;
extern "C" int __wrap_rand() {
    if (g_lc_rand_callers != nullptr) {
        g_lc_rand_callers->push_back(static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
    }
    g_lc_rand_state = g_lc_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_lc_rand_state >> 16) & 0x7fff);
}
extern "C" void __wrap_srand(unsigned seed) { g_lc_rand_state = seed; }

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

// ---- the world format of a brain (Brain::serialize), parsed ----

struct Rule {
    std::uint32_t source_lobe = 0;
    std::uint8_t bytes[14] = {};  // min .. lose_rate, archive order
    std::uint8_t expressions[4][10] = {};  // gain, lose, suscept, reinforce
};

struct LobeImage {
    std::uint32_t grid_x = 0, grid_y = 0, width = 0, height = 0, percept = 0;
    std::uint8_t active_fraction = 0;
    std::uint8_t token_state[4] = {};
    std::uint8_t threshold = 0, relax = 0, baseline = 0, gain = 0;
    std::uint8_t state_rule[10] = {};
    std::uint8_t wta = 0;
    Rule rules[2];
    std::uint32_t neuron_count = 0, connection_count = 0;
};

struct DendriteImage {
    std::int32_t target = 0;
    std::uint8_t grid_x = 0, grid_y = 0, suscept = 0, stw = 0, ltw = 0, strength = 0;
    bool operator==(const DendriteImage& o) const {
        return target == o.target && grid_x == o.grid_x && grid_y == o.grid_y &&
               suscept == o.suscept && stw == o.stw && ltw == o.ltw &&
               strength == o.strength;
    }
};

struct NeuronImage {
    std::uint8_t grid_x = 0, grid_y = 0, firing = 0, activation = 0, excluded = 0, source = 0;
    std::uint8_t counts[2] = {};
    std::uint32_t begins[2] = {};
    std::vector<DendriteImage> dendrites[2];
};

struct BrainImage {
    std::vector<LobeImage> lobes;
    std::vector<std::vector<NeuronImage>> neurons;
};

class Bytes {
public:
    explicit Bytes(const std::vector<std::uint8_t>& data) : data_(data) {}
    std::uint32_t u32() {
        std::uint32_t v = 0;
        std::memcpy(&v, take(4), 4);
        return v;
    }
    std::uint8_t u8() { return *take(1); }
    void read(std::uint8_t* to, std::size_t n) { std::memcpy(to, take(n), n); }
    bool done() const { return position_ == data_.size(); }

private:
    const std::uint8_t* take(std::size_t n) {
        if (position_ + n > data_.size()) {
            std::fprintf(stderr, "brain archive ends early\n");
            std::exit(4);
        }
        position_ += n;
        return data_.data() + position_ - n;
    }
    const std::vector<std::uint8_t>& data_;
    std::size_t position_ = 0;
};

BrainImage parse(const std::vector<std::uint8_t>& data) {
    Bytes in(data);
    BrainImage image;
    image.lobes.resize(in.u32());
    for (LobeImage& l : image.lobes) {
        l.grid_x = in.u32();
        l.grid_y = in.u32();
        l.width = in.u32();
        l.height = in.u32();
        l.percept = in.u32();
        l.active_fraction = in.u8();
        in.read(l.token_state, 4);
        l.threshold = in.u8();
        l.relax = in.u8();
        l.baseline = in.u8();
        l.gain = in.u8();
        in.read(l.state_rule, 10);
        l.wta = in.u8();
        for (Rule& r : l.rules) {
            r.source_lobe = in.u32();
            in.read(r.bytes, 14);
            for (auto& e : r.expressions) in.read(e, 10);
        }
        l.neuron_count = in.u32();
        l.connection_count = in.u32();
    }
    for (const LobeImage& l : image.lobes) {
        std::vector<NeuronImage> neurons(l.neuron_count);
        for (NeuronImage& n : neurons) {
            n.grid_x = in.u8();
            n.grid_y = in.u8();
            n.firing = in.u8();
            n.activation = in.u8();
            n.excluded = in.u8();
            n.source = in.u8();
            for (int rule = 0; rule < 2; ++rule) {
                n.counts[rule] = in.u8();
                n.begins[rule] = in.u32();
                for (int d = 0; d < n.counts[rule]; ++d) {
                    DendriteImage den;
                    den.target = static_cast<std::int32_t>(in.u32());
                    den.grid_x = in.u8();
                    den.grid_y = in.u8();
                    den.suscept = in.u8();
                    den.stw = in.u8();
                    den.ltw = in.u8();
                    den.strength = in.u8();
                    n.dendrites[rule].push_back(den);
                }
            }
        }
        image.neurons.push_back(std::move(neurons));
    }
    if (!in.done()) {
        std::fprintf(stderr, "brain archive has bytes left over\n");
        std::exit(4);
    }
    return image;
}

std::vector<std::uint8_t> emit(const BrainImage& image) {
    std::vector<std::uint8_t> out;
    const auto u32 = [&out](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    const auto bytes = [&out](const std::uint8_t* p, std::size_t n) {
        out.insert(out.end(), p, p + n);
    };
    u32(static_cast<std::uint32_t>(image.lobes.size()));
    for (const LobeImage& l : image.lobes) {
        u32(l.grid_x); u32(l.grid_y); u32(l.width); u32(l.height); u32(l.percept);
        out.push_back(l.active_fraction);
        bytes(l.token_state, 4);
        out.insert(out.end(), {l.threshold, l.relax, l.baseline, l.gain});
        bytes(l.state_rule, 10);
        out.push_back(l.wta);
        for (const Rule& r : l.rules) {
            u32(r.source_lobe);
            bytes(r.bytes, 14);
            for (const auto& e : r.expressions) bytes(e, 10);
        }
        u32(l.neuron_count);
        u32(l.connection_count);
    }
    for (const auto& neurons : image.neurons) {
        for (const NeuronImage& n : neurons) {
            out.insert(out.end(), {n.grid_x, n.grid_y, n.firing, n.activation, n.excluded,
                                   n.source});
            for (int rule = 0; rule < 2; ++rule) {
                out.push_back(n.counts[rule]);
                u32(n.begins[rule]);
                for (const DendriteImage& d : n.dendrites[rule]) {
                    u32(static_cast<std::uint32_t>(d.target));
                    out.insert(out.end(), {d.grid_x, d.grid_y, d.suscept, d.stw, d.ltw,
                                           d.strength});
                }
            }
        }
    }
    return out;
}

// ---- LibreCreatures ----

class ByteArchive final : public brain::BrainArchive {
public:
    ByteArchive() = default;
    explicit ByteArchive(std::vector<std::uint8_t> in) : in_(std::move(in)), loading_(true) {}
    bool is_loading() const override { return loading_; }
    std::uint32_t read_u32() override {
        std::uint32_t v = 0;
        std::memcpy(&v, take(4), 4);
        return v;
    }
    std::uint8_t read_byte() override { return *take(1); }
    void write_u32(std::uint32_t v) override {
        for (int i = 0; i < 4; ++i) out_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void write_byte(std::uint8_t v) override { out_.push_back(v); }
    const std::vector<std::uint8_t>& written() const { return out_; }

private:
    const std::uint8_t* take(std::size_t n) {
        if (position_ + n > in_.size()) {
            std::fprintf(stderr, "LC read past the brain archive\n");
            std::exit(4);
        }
        position_ += n;
        return in_.data() + position_ - n;
    }
    std::vector<std::uint8_t> in_;
    std::vector<std::uint8_t> out_;
    std::size_t position_ = 0;
    bool loading_ = false;
};

class OneGenomeFile final : public genome::GenomeFileStore {
public:
    explicit OneGenomeFile(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    std::string secondary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    std::string primary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    bool regular_file_exists(const std::string&) const override { return true; }
    std::vector<std::uint8_t> read_file(const std::string&) override { return bytes_; }
    void write_secondary_genetics_file(genome::GenomeFilenameId,
                                       const std::vector<std::uint8_t>&) override {}

private:
    std::vector<std::uint8_t> bytes_;
};

std::vector<std::uint8_t> save(brain::Brain& b) {
    ByteArchive out;
    b.serialize(out);
    return out.written();
}

// ---- native ----

// Native's migration duplicate test packs the sum and product of neuron
// pointers into 16 bits each (original-bugs.md row 5).  The sum's base
// cancels, but with fewer than four dendrites the product's low bits depend
// on where the lobe's neurons sit, so each native neuron array is placed at
// the same address modulo 0x10000 as LC's.
constexpr std::size_t kAddressAlignment = 0x10000;

struct NativeBrain {
    std::vector<std::uint8_t> self = std::vector<std::uint8_t>(kBrainSize, 0);
    std::vector<std::vector<std::uint8_t>> neuron_storage, connections;
    std::vector<std::uint8_t*> neuron_arrays;
    std::vector<std::vector<std::uint32_t>> active;
};

std::uint8_t* lobe_at(NativeBrain& b, std::size_t i) {
    return b.self.data() + kBrainLobes + i * kLobeSize;
}
void put32(std::uint8_t* p, std::uint32_t v) { std::memcpy(p, &v, 4); }
std::uint32_t get32(const std::uint8_t* p) {
    std::uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}
std::uint32_t address(const void* p) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

void build_native(const BrainImage& image, brain::Brain& lc, NativeBrain& b) {
    const std::size_t count = image.lobes.size();
    b.neuron_storage.resize(count);
    b.neuron_arrays.resize(count);
    b.connections.resize(count);
    b.active.resize(count);
    put32(b.self.data() + kBrainLobeCount, static_cast<std::uint32_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const LobeImage& l = image.lobes[i];
        b.neuron_storage[i].assign(l.neuron_count * kNeuronSize + kAddressAlignment, 0);
        const std::uint32_t lc_address =
            l.neuron_count == 0 ? 0 : address(&lc.lobe(static_cast<std::uint32_t>(i)).neuron(0));
        const std::uint32_t storage = address(b.neuron_storage[i].data());
        const std::uint32_t offset =
            (lc_address - storage) % static_cast<std::uint32_t>(kAddressAlignment);
        b.neuron_arrays[i] = b.neuron_storage[i].data() + offset;
        b.connections[i].assign(l.connection_count * kDendriteSize + kDendriteSize, 0);
        b.active[i].assign(l.neuron_count + 1, 0);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const LobeImage& l = image.lobes[i];
        std::uint8_t* p = lobe_at(b, i);
        put32(p + 4, l.grid_x);
        put32(p + 8, l.grid_y);
        put32(p + 12, l.width);
        put32(p + 16, l.height);
        p[20] = l.threshold;
        p[21] = l.relax;
        p[22] = l.baseline;
        p[23] = l.gain;
        std::memcpy(p + 24, l.state_rule, 10);
        p[34] = l.wta;
        for (int rule = 0; rule < 2; ++rule) {
            const Rule& r = l.rules[rule];
            std::uint8_t* q = p + kLobeRules + 60 * rule;
            put32(q, r.source_lobe);
            // Archive order: min, max, spread, fanout, min/max LTW,
            // min/max strength, migration, relax suscept, relax STW,
            // LTW gain rate (offsets 4..15), gain rate (16), lose rate (27).
            std::memcpy(q + 4, r.bytes, 12);
            q[16] = r.bytes[12];
            q[27] = r.bytes[13];
            std::memcpy(q + 17, r.expressions[0], 10);  // gain
            std::memcpy(q + 28, r.expressions[1], 10);  // lose
            std::memcpy(q + 38, r.expressions[2], 10);  // suscept
            std::memcpy(q + 48, r.expressions[3], 10);  // reinforce
        }
        put32(p + kLobeNeurons, address(b.neuron_arrays[i]));
        put32(p + kLobeConnections, address(b.connections[i].data()));
        put32(p + kLobeNeuronCount, l.neuron_count);
        put32(p + kLobeConnectionCount, l.connection_count);
        put32(p + kLobeActive, address(b.active[i].data()));
        put32(p + kLobeActiveCount, 0);
        put32(p + kLobePercept, l.percept);
        // A loaded lobe's runtime state, as LC's load leaves it.
        put32(p + kLobeCandidate0, 99999);
        put32(p + kLobeCandidate1, 99999);
        p[kLobeActiveFraction] = l.active_fraction;
        p[kLobeLoose] = 0;
        p[kLobeLoose + 1] = 0;
        std::memcpy(p + kLobeTokenState, l.token_state, 4);
        for (std::size_t n = 0; n < l.neuron_count; ++n) {
            const NeuronImage& ni = image.neurons[i][n];
            std::uint8_t* np = b.neuron_arrays[i] + n * kNeuronSize;
            np[0] = ni.grid_x;
            np[1] = ni.grid_y;
            np[2] = ni.firing;
            np[3] = ni.activation;
            for (int rule = 0; rule < 2; ++rule) {
                std::uint8_t* first = b.connections[i].data() + ni.begins[rule] * kDendriteSize;
                put32(np + 4 + 4 * rule, ni.counts[rule] == 0 && ni.begins[rule] == 0
                                             ? address(first)
                                             : address(first));
                np[12 + rule] = ni.counts[rule];
                const std::uint32_t target_lobe = l.rules[rule].source_lobe;
                for (std::size_t d = 0; d < ni.counts[rule]; ++d) {
                    const DendriteImage& di = ni.dendrites[rule][d];
                    std::uint8_t* dp = first + d * kDendriteSize;
                    std::uint32_t target = 0;
                    if (target_lobe < count && di.target >= 0 &&
                        static_cast<std::uint32_t>(di.target) < image.lobes[target_lobe].neuron_count) {
                        target = address(b.neuron_arrays[target_lobe] + di.target * kNeuronSize);
                    }
                    put32(dp, target);
                    dp[4] = di.grid_x;
                    dp[5] = di.grid_y;
                    dp[6] = di.suscept;
                    dp[7] = di.stw;
                    dp[8] = di.ltw;
                    dp[9] = di.strength;
                }
            }
            np[14] = ni.excluded;
            np[15] = ni.source;
        }
    }
}

// The native brain's state in the world format's terms.
BrainImage read_native(NativeBrain& b, const BrainImage& shape) {
    BrainImage image = shape;
    const std::size_t count = shape.lobes.size();
    for (std::size_t i = 0; i < count; ++i) {
        LobeImage& l = image.lobes[i];
        std::uint8_t* p = lobe_at(b, i);
        l.active_fraction = p[kLobeActiveFraction];
        std::memcpy(l.token_state, p + kLobeTokenState, 4);
        for (std::size_t n = 0; n < l.neuron_count; ++n) {
            NeuronImage& ni = image.neurons[i][n];
            const std::uint8_t* np = b.neuron_arrays[i] + n * kNeuronSize;
            ni.grid_x = np[0];
            ni.grid_y = np[1];
            ni.firing = np[2];
            ni.activation = np[3];
            ni.excluded = np[14];
            ni.source = np[15];
            for (int rule = 0; rule < 2; ++rule) {
                const std::uint8_t* first =
                    reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(get32(np + 4 + 4 * rule)));
                const std::uint32_t target_lobe = l.rules[rule].source_lobe;
                for (std::size_t d = 0; d < ni.counts[rule]; ++d) {
                    DendriteImage& di = ni.dendrites[rule][d];
                    const std::uint8_t* dp = first + d * kDendriteSize;
                    const std::uint32_t target = get32(dp);
                    di.target = 0;
                    if (target_lobe < count && target != 0) {
                        const std::uint32_t base = address(b.neuron_arrays[target_lobe]);
                        if (target >= base && target < base + shape.lobes[target_lobe].neuron_count * kNeuronSize) {
                            di.target = static_cast<std::int32_t>((target - base) / kNeuronSize);
                        }
                    }
                    di.grid_x = dp[4];
                    di.grid_y = dp[5];
                    di.suscept = dp[6];
                    di.stw = dp[7];
                    di.ltw = dp[8];
                    di.strength = dp[9];
                }
            }
        }
    }
    return image;
}

// ---- perturbation ----

void perturb(BrainImage& image, Random& random) {
    for (std::size_t i = 0; i < image.lobes.size(); ++i) {
        LobeImage& l = image.lobes[i];
        for (std::uint8_t& c : l.token_state) {
            c = random.chance(3) ? 0 : random.byte();
        }
        for (NeuronImage& n : image.neurons[i]) {
            if (random.chance(2)) n.activation = random.byte();
            if (random.chance(3)) n.firing = random.chance(2) ? 0 : random.byte();
            if ((l.wta & 1) != 0 && random.chance(8)) n.excluded = random.below(2);
            for (int rule = 0; rule < 2; ++rule) {
                const std::uint32_t target_lobe = l.rules[rule].source_lobe;
                const std::uint32_t targets = target_lobe < image.lobes.size()
                                                  ? image.lobes[target_lobe].neuron_count
                                                  : 0;
                for (DendriteImage& d : n.dendrites[rule]) {
                    if (random.chance(4) && targets != 0) d.target = static_cast<std::int32_t>(random.below(targets));
                    if (random.chance(2)) d.suscept = random.chance(2) ? 0 : random.byte();
                    if (random.chance(2)) d.stw = random.byte();
                    if (random.chance(2)) d.ltw = random.byte();
                    if (random.chance(3)) d.strength = random.chance(4) ? 0 : random.byte();
                }
            }
        }
    }
}

std::string describe_difference(const BrainImage& a, const BrainImage& b) {
    for (std::size_t i = 0; i < a.lobes.size(); ++i) {
        if (a.lobes[i].active_fraction != b.lobes[i].active_fraction) {
            return "lobe " + std::to_string(i) + " active fraction";
        }
        for (std::size_t n = 0; n < a.neurons[i].size(); ++n) {
            const NeuronImage& x = a.neurons[i][n];
            const NeuronImage& y = b.neurons[i][n];
            const std::string where = "lobe " + std::to_string(i) + " neuron " + std::to_string(n);
            if (x.activation != y.activation) {
                return where + " activation: native " + std::to_string(x.activation) + ", LC " +
                       std::to_string(y.activation);
            }
            if (x.firing != y.firing) {
                return where + " firing: native " + std::to_string(x.firing) + ", LC " +
                       std::to_string(y.firing);
            }
            for (int rule = 0; rule < 2; ++rule) {
                for (std::size_t d = 0; d < x.dendrites[rule].size(); ++d) {
                    const DendriteImage& p = x.dendrites[rule][d];
                    const DendriteImage& q = y.dendrites[rule][d];
                    if (!(p == q)) {
                        char text[200];
                        std::snprintf(text, sizeof text,
                                      " type %d dendrite %zu: native src %d susc %u stw %u ltw %u str %u,"
                                      " LC src %d susc %u stw %u ltw %u str %u",
                                      rule, d, p.target, p.suscept, p.stw, p.ltw, p.strength,
                                      q.target, q.suscept, q.stw, q.ltw, q.strength);
                        return where + text;
                    }
                }
            }
        }
    }
    return {};
}

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
