// Function-level equivalence: Creature::ApplyStimulus @0040b8a0 (CE 10.3)
// against creatures::Creature::apply_stimulus.
//
// Each case builds one creature twice: an LC Creature whose brain grows from
// a stock genome, and a native Creature-shaped block holding the same state
// at the offsets ApplyStimulus reads (taken from its disassembly).  Random
// sources (simple, compound, creature, scenery and unknown objects, and the
// creature itself) then send 1 to 4 random stimuli.  After each, every
// attention record, the attention (2) and general sensory (5) lobes'
// activations, all 256 chemical concentrations and the goal-direction matrix
// must match.
//
// Not generated: genera with no attention record (row 10's guard: native
// writes past the 40 records) and dead creatures (both return at once).

#include "native_image.hpp"

#include "biochemistry/biochemistry.hpp"
#include "brain/brain.hpp"
#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "creatures/genome.hpp"
#include "objects/object.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;

constexpr std::uint32_t kNativeApplyStimulus = 0x0040b8a0;
constexpr std::uint32_t kNativeDebugLog = 0x00418000;

// Native Creature layout used by ApplyStimulus (offsets from its code).
constexpr std::size_t kCreatureSize = 0x2d00;
constexpr std::size_t kSleepIndicator = 0x11f;
constexpr std::size_t kMotionLink = 0x7f0;
constexpr std::size_t kAttentionRecords = 0x844;  // 40 x 20 bytes
constexpr std::size_t kAttentionStride = 20;
constexpr std::size_t kGoalMatrix = 0x1cf0;       // 40 x 16 int32
constexpr std::size_t kBrain = 0x2c18;
constexpr std::size_t kBiochemistry = 0x2c1c;
constexpr std::size_t kDeathState = 0x2ccc;
// Native CBrain: lobe 2 and lobe 5 neuron counts and arrays.
constexpr std::size_t kBrainSize = 0x800;
constexpr std::size_t kLobe2Neurons = 0x234, kLobe2Count = 0x23c;
constexpr std::size_t kLobe5Neurons = 0x48c, kLobe5Count = 0x494;
constexpr std::size_t kNeuronSize = 16, kNeuronActivation = 3;
// Native CBiochemistry: concentration of chemical c at 8 + 2c.
constexpr std::size_t kBiochemistrySize = 8 + 2 * 256 + 64;

using NativeApply = void(__attribute__((thiscall)) *)(
    void* creature, void* source_object, void* source_creature, std::uint32_t descriptor,
    std::uint32_t chemical_ids, std::uint32_t chemical_amounts, std::uint32_t magnitude);

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

// ---- native source objects: a vtable whose sound-position getters read a
// table, so the creature block itself can be a source too ----

struct SourcePosition {
    int x = 0, y = 0;
};
std::map<const void*, SourcePosition> g_native_positions;

int __attribute__((thiscall)) native_sound_x(void* self) { return g_native_positions[self].x; }
int __attribute__((thiscall)) native_sound_y(void* self) { return g_native_positions[self].y; }
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "ApplyStimulus called an unexpected virtual on a source\n");
    std::exit(5);
}

void* g_vtable[64];

struct NativeSource {
    void** vptr = g_vtable;
    std::uint32_t classifier = 0;
    std::uint8_t rest[0x60] = {};
};

// ---- LC side ----

struct LcSource {
    creatures::AttentionClassifier classifier;
    int x = 0, y = 0;
};

class SourceHost final : public creatures::Creature::StimulusSourceHost {
public:
    std::map<const objects::Object*, LcSource> sources;
    const objects::Object* self = nullptr;

    creatures::AttentionClassifier classify(const objects::Object& object) const override {
        return sources.at(&object).classifier;
    }
    int sound_source_x(const objects::Object& object) const override {
        return sources.at(&object).x;
    }
    int sound_source_y(const objects::Object& object) const override {
        return sources.at(&object).y;
    }
    bool is_this_creature(const objects::Object& object,
                          const creatures::Creature&) const override {
        return &object == self;
    }
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

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}

template <typename T>
T& field(std::vector<std::uint8_t>& block, std::size_t offset) {
    return *reinterpret_cast<T*>(block.data() + offset);
}

std::uint32_t pack(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    return a | (b << 8) | (c << 16) | (static_cast<std::uint32_t>(d) << 24);
}

// A random genus that has an attention record for its family.
std::uint8_t genus_for(creatures::AttentionObjectFamily family, Random& random) {
    switch (family) {
    case creatures::AttentionObjectFamily::simple_object: return static_cast<std::uint8_t>(random.below(40));
    case creatures::AttentionObjectFamily::compound_object: return static_cast<std::uint8_t>(random.below(15));
    case creatures::AttentionObjectFamily::creature: return static_cast<std::uint8_t>(random.below(5));
    default: return random.byte();
    }
}

std::uint8_t chemical_id(Random& random) {
    switch (random.below(4)) {
    case 0: return 0;
    case 1: return static_cast<std::uint8_t>(0x21 + random.below(16));  // drive reducers
    default: return random.byte();
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n",
                     argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x51a1c1u;
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
    image.make_return(kNativeDebugLog);
    const auto native_apply = image.at<NativeApply>(kNativeApplyStimulus);
    for (auto& slot : g_vtable) {
        slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    }
    g_vtable[30] = reinterpret_cast<void*>(&native_sound_x);
    g_vtable[31] = reinterpret_cast<void*>(&native_sound_y);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, stimuli = 0;
    for (long index = 0; index < cases; ++index) {
        // ---- LC creature ----
        OneGenomeFile file(genomes[random.below(static_cast<std::uint32_t>(genomes.size()))]);
        genome::Genome g(1, random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female,
                         genome::GenomeLifeStage::stage_zero, &file);
        auto creature = std::make_unique<creatures::Creature>();
        auto* brain = new brain::Brain();
        brain->load_genome(g);
        creature->set_brain(brain);
        creature->set_biochemistry(new biochemistry::Biochemistry());

        // ---- native creature ----
        std::vector<std::uint8_t> native(kCreatureSize, 0);
        std::vector<std::uint8_t> native_brain(kBrainSize, 0);
        std::vector<std::uint8_t> native_bio(kBiochemistrySize, 0);
        const std::uint32_t lobe2 = brain->lobe(2).neuron_count_value();
        const std::uint32_t lobe5 = brain->lobe(5).neuron_count_value();
        std::vector<std::uint8_t> neurons2(kNeuronSize * (lobe2 + 1), 0);
        std::vector<std::uint8_t> neurons5(kNeuronSize * (lobe5 + 1), 0);
        field<void*>(native, kBrain) = native_brain.data();
        field<void*>(native, kBiochemistry) = native_bio.data();
        field<std::uint8_t>(native, kDeathState) = 0;
        field<void*>(native_brain, kLobe2Neurons) = neurons2.data();
        field<std::int32_t>(native_brain, kLobe2Count) = static_cast<std::int32_t>(lobe2);
        field<void*>(native_brain, kLobe5Neurons) = neurons5.data();
        field<std::int32_t>(native_brain, kLobe5Count) = static_cast<std::int32_t>(lobe5);

        // ---- sources: six objects and the creature itself ----
        SourceHost host;
        host.self = &creature->skeleton();
        std::vector<std::unique_ptr<objects::Object>> lc_objects;
        std::vector<std::unique_ptr<NativeSource>> native_objects;
        std::vector<const objects::Object*> lc_source(7);
        std::vector<void*> native_source(7);
        g_native_positions.clear();
        for (int s = 0; s < 7; ++s) {
            const auto family = static_cast<creatures::AttentionObjectFamily>(
                s == 6 ? 4 : random.below(5));
            const std::uint8_t genus = genus_for(family, random);
            LcSource info;
            info.classifier = {family, genus};
            info.x = static_cast<int>(random.below(8352));
            info.y = random.chance(8) ? 4000 : static_cast<int>(random.below(1200));
            const std::uint32_t native_classifier =
                (static_cast<std::uint32_t>(family) << 24) | (genus << 16);
            if (s < 6) {
                lc_objects.push_back(std::make_unique<objects::Object>());
                native_objects.push_back(std::make_unique<NativeSource>());
                native_objects.back()->classifier = native_classifier;
                lc_source[s] = lc_objects.back().get();
                native_source[s] = native_objects.back().get();
            } else {
                lc_source[s] = host.self;
                native_source[s] = native.data();
                field<void*>(native, 0) = g_vtable;
                field<std::uint32_t>(native, 4) = native_classifier;
            }
            host.sources[lc_source[s]] = info;
            g_native_positions[native_source[s]] = {info.x, info.y};
        }
        auto native_of = [&](const objects::Object* object) -> void* {
            for (int s = 0; s < 7; ++s) {
                if (lc_source[s] == object) return native_source[s];
            }
            return nullptr;
        };

        // ---- shared state ----
        const bool asleep = random.chance(4);
        creature->skeleton().sleep_indicator_active = asleep;
        field<std::uint8_t>(native, kSleepIndicator) = asleep ? 1 : 0;
        const int motion = static_cast<int>(random.below(9)) - 2;  // -2..-1: none
        creature->skeleton().motion_link =
            motion >= 0 && motion < 7 ? const_cast<objects::Object*>(lc_source[motion]) : nullptr;
        field<void*>(native, kMotionLink) = native_of(creature->skeleton().motion_link);
        auto& records = creature->attention_records();
        for (std::size_t r = 0; r < records.size(); ++r) {
            auto& record = records[r];
            const int target = static_cast<int>(random.below(10)) - 3;
            record.target = target >= 0 && target < 7
                                ? const_cast<objects::Object*>(lc_source[target]) : nullptr;
            record.target_neuron_index = random.byte();
            record.lobe_activation = random.byte();
            record.world_x = static_cast<int>(random.below(8352));
            record.world_y = static_cast<int>(random.below(1200));
            record.visible = random.chance(2);
            record.stimulus_seen = random.chance(2);
            std::uint8_t* raw = native.data() + kAttentionRecords + r * kAttentionStride;
            *reinterpret_cast<void**>(raw) = native_of(record.target);
            raw[4] = record.target_neuron_index;
            raw[5] = record.lobe_activation;
            *reinterpret_cast<int*>(raw + 8) = record.world_x;
            *reinterpret_cast<int*>(raw + 12) = record.world_y;
            raw[16] = record.visible ? 1 : 0;
            raw[17] = record.stimulus_seen ? 1 : 0;
        }
        auto& matrix = creature->goal_direction_weight_matrix();
        for (std::size_t r = 0; r < 40; ++r) {
            for (std::size_t c = 0; c < 16; ++c) {
                matrix[r][c] = static_cast<std::int32_t>(random.byte());
                field<std::int32_t>(native, kGoalMatrix + (r * 16 + c) * 4) = matrix[r][c];
            }
        }
        for (std::uint32_t n = 0; n < lobe2; ++n) {
            const std::uint8_t a = random.byte();
            brain->lobe(2).neuron(n).activation = a;
            neurons2[n * kNeuronSize + kNeuronActivation] = a;
        }
        for (std::uint32_t n = 0; n < lobe5; ++n) {
            const std::uint8_t a = random.byte();
            brain->lobe(5).neuron(n).activation = a;
            neurons5[n * kNeuronSize + kNeuronActivation] = a;
        }
        auto& chemicals = creature->biochemistry()->chemical_states();
        for (int c = 0; c < 256; ++c) {
            const std::uint8_t level = random.byte();
            chemicals[c].concentration = level;
            native_bio[8 + 2 * c] = level;
        }

        // ---- stimuli ----
        const int count = 1 + static_cast<int>(random.below(4));
        bool case_differs = false;
        for (int k = 0; k < count && !case_differs; ++k) {
            const int s = random.chance(5) ? 6 : static_cast<int>(random.below(6));
            creatures::StimulusDescriptor d{random.byte(), random.byte(), random.byte(),
                                            static_cast<std::uint8_t>(random.below(8))};
            creatures::StimulusChemicalIds ids{chemical_id(random), chemical_id(random),
                                               chemical_id(random), chemical_id(random)};
            creatures::StimulusChemicalAmounts amounts{random.byte(), random.byte(),
                                                       random.byte(), random.byte()};
            std::uint32_t magnitude;
            switch (random.below(4)) {
            case 0: magnitude = static_cast<std::uint32_t>(-static_cast<int>(random.below(300))); break;
            case 1: magnitude = 256 + random.below(1000); break;
            default: magnitude = random.below(256);
            }
            creature->apply_stimulus(const_cast<objects::Object*>(lc_source[s]), creature.get(),
                                     d, ids, amounts, magnitude, host);
            native_apply(native.data(), native_source[s], native.data(),
                         pack(d.attention_activation, d.target_neuron_index,
                              d.target_lobe_activation, d.flags),
                         pack(ids.first, ids.second, ids.third, ids.fourth),
                         pack(amounts.first, amounts.second, amounts.third, amounts.fourth),
                         magnitude);
            ++stimuli;

            // ---- compare ----
            std::string first;
            auto note = [&](const std::string& what) {
                if (first.empty()) first = what;
            };
            char text[200];
            for (std::size_t r = 0; r < records.size(); ++r) {
                const auto& record = records[r];
                const std::uint8_t* raw = native.data() + kAttentionRecords + r * kAttentionStride;
                if (*reinterpret_cast<void* const*>(raw) != native_of(record.target) ||
                    raw[4] != record.target_neuron_index || raw[5] != record.lobe_activation ||
                    *reinterpret_cast<const int*>(raw + 8) != record.world_x ||
                    *reinterpret_cast<const int*>(raw + 12) != record.world_y ||
                    (raw[16] != 0) != record.visible || (raw[17] != 0) != record.stimulus_seen) {
                    std::snprintf(text, sizeof text,
                                  "attention record %u: native n%u a%u (%d,%d) v%u s%u, lc n%u a%u (%d,%d) v%u s%u",
                                  static_cast<unsigned>(r), raw[4], raw[5],
                                  *reinterpret_cast<const int*>(raw + 8),
                                  *reinterpret_cast<const int*>(raw + 12), raw[16], raw[17],
                                  record.target_neuron_index, record.lobe_activation,
                                  record.world_x, record.world_y, record.visible ? 1 : 0,
                                  record.stimulus_seen ? 1 : 0);
                    note(text);
                }
            }
            for (std::uint32_t n = 0; n < lobe2; ++n) {
                if (neurons2[n * kNeuronSize + kNeuronActivation] != brain->lobe(2).neuron(n).activation) {
                    std::snprintf(text, sizeof text, "lobe 2 neuron %u", n);
                    note(text);
                }
            }
            for (std::uint32_t n = 0; n < lobe5; ++n) {
                if (neurons5[n * kNeuronSize + kNeuronActivation] != brain->lobe(5).neuron(n).activation) {
                    std::snprintf(text, sizeof text, "lobe 5 neuron %u", n);
                    note(text);
                }
            }
            for (int c = 0; c < 256; ++c) {
                if (native_bio[8 + 2 * c] != chemicals[c].concentration) {
                    std::snprintf(text, sizeof text, "chemical %d: native %u lc %u", c,
                                  native_bio[8 + 2 * c], chemicals[c].concentration);
                    note(text);
                }
            }
            for (std::size_t r = 0; r < 40; ++r) {
                for (std::size_t c = 0; c < 16; ++c) {
                    const std::int32_t n = field<std::int32_t>(native, kGoalMatrix + (r * 16 + c) * 4);
                    if (n != matrix[r][c]) {
                        std::snprintf(text, sizeof text, "goal matrix [%u][%u]: native %d lc %d",
                                      static_cast<unsigned>(r), static_cast<unsigned>(c), n, matrix[r][c]);
                        note(text);
                    }
                }
            }
            if (!first.empty() || index == verbose_case) {
                const auto& info = host.sources.at(lc_source[s]);
                std::printf("case %ld stimulus %d: source %d family %u genus %u at (%d,%d) "
                            "desc %02x %02x %02x flags %u ids %u %u %u %u amounts %u %u %u %u "
                            "magnitude %d asleep %d: %s\n",
                            index, k, s, static_cast<unsigned>(info.classifier.family),
                            info.classifier.genus, info.x, info.y, d.attention_activation,
                            d.target_neuron_index, d.target_lobe_activation, d.flags, ids.first,
                            ids.second, ids.third, ids.fourth, amounts.first, amounts.second,
                            amounts.third, amounts.fourth, static_cast<int>(magnitude),
                            asleep ? 1 : 0, first.empty() ? "same" : first.c_str());
            }
            if (!first.empty()) {
                case_differs = true;
            }
        }
        if (case_differs) {
            ++differing;
        }
        // The LC objects die before the creature that may point at them.
        creature->skeleton().motion_link = nullptr;
        for (auto& record : records) record.target = nullptr;
        creature.reset();
    }
    std::printf("%ld cases, %ld stimuli: %ld differ%s\n", cases, stimuli, differing,
                all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
