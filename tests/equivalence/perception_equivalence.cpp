// Function-level equivalence: Creature::UpdatePerception @0040bf10 (CE 10.3),
// with the CanPerceiveObject @00409750, TriggerBuiltInStimulus @0040b820,
// GetFirstSeenStimulusMagnitude @0040c5c0 and ApplyStimulus @0040b8a0 it
// calls, against creatures::Creature::update_perception.
//
// Each case gives an LC creature (brain grown from a stock genome) and a
// native Creature-shaped block the same state at the offsets these functions
// read: the 40 attention records (targets, positions, visible and seen
// flags), the built-in stimulus contexts, the goal matrix, the attention (7),
// sensory (5) and stimulus-source (2) lobes, all chemicals, sleep, the
// motion link, facing, foot point, limb end, movement bounds, bounds
// reference, monikers and sex.  Eight objects of random kind, some of them
// creatures with monikers and a sex, stand around it; the native object
// registry and LC's host list them alike.  Object bounds, positions and
// hidden flags come from one table on both sides.  The four rectangle
// functions CanPerceiveObject imports are bound to plain implementations,
// and FindNearestMapRoomBoundsAtPoint (world geometry, not under test) is
// replaced on both sides by one function.  LC's kin rules
// (motion_link_facts_for) and map-room test (is_map_room_marker) are the
// ones the game's host calls.
//
// One update per case.  After it, every attention record (target, position,
// sense neuron and activation, visible, seen), the three lobes, all
// chemicals and the goal matrix must match.
//
// Not generated: genera with no attention record (row 10).

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

namespace creatures1::creatures {
// Declared a friend in creature.hpp.
class CreatureTestAccess {
public:
    static GenomeSex& sex(Creature& c) { return c.genome_sex_; }
};
}  // namespace creatures1::creatures

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;
using creatures::CreatureTestAccess;

constexpr std::uint32_t kNativeUpdatePerception = 0x0040bf10;
constexpr std::uint32_t kNativeNearestRoom = 0x00422d70;
constexpr std::uint32_t kNativeDebugLog = 0x00418000;
constexpr std::uint32_t kRegistryData = 0x004704a8, kRegistryCount = 0x004704ac;

// Native Creature layout used by these functions.
constexpr std::size_t kCreatureSize = 0x2d00;
constexpr std::size_t kClassifier = 0x4, kBoundsFlags = 0x9, kMovementBounds = 0xc;
constexpr std::size_t kBoundsReference = 0x1c;
constexpr std::size_t kGenome = 0x50, kMother = 0x54, kFather = 0x58;
constexpr std::size_t kFacing = 0x78, kFootX = 0x7c, kFootY = 0x80, kLimbEndX = 0xcc;
constexpr std::size_t kSleepIndicator = 0x11f, kMotionLink = 0x7f0;
constexpr std::size_t kAttentionRecords = 0x844, kAttentionStride = 20;
constexpr std::size_t kGoalMatrix = 0x1cf0;
constexpr std::size_t kContexts = 0x26f0, kContextStride = 20;
constexpr std::size_t kBrain = 0x2c18, kBiochemistry = 0x2c1c;
constexpr std::size_t kDeathState = 0x2ccc, kSex = 0x2cc1;
constexpr std::size_t kBrainSize = 0x800;
constexpr std::size_t lobe_neurons(int lobe) { return 8 + 200 * lobe + 156; }
constexpr std::size_t lobe_count(int lobe) { return 8 + 200 * lobe + 164; }
constexpr std::size_t kNeuronSize = 16, kFiring = 2, kActivation = 3;
constexpr int kLobes[] = {2, 5, 7};
constexpr std::size_t kBiochemistrySize = 8 + 2 * 256 + 64;

using NativeUpdate = void(__attribute__((thiscall)) *)(void* creature);

// ---- rand: the MSVC formula, one state per side ----
std::uint32_t g_native_rand_state = 1;
extern "C" int __cdecl native_rand() {
    g_native_rand_state = g_native_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_native_rand_state >> 16) & 0x7fff);
}
std::uint32_t g_lc_rand_state = 1;
extern "C" int __wrap_rand() {
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

// ---- one table of object facts for both sides ----
struct Facts {
    world::WorldRect bounds{};
    int x = 0, y = 0;
};
std::map<const void*, Facts> g_native_facts;            // native object -> facts
std::map<const objects::Object*, Facts> g_lc_facts;     // LC object -> facts

void __attribute__((thiscall)) native_get_bounds(void* self, world::WorldRect* out) {
    *out = g_native_facts.at(self).bounds;
}
int __attribute__((thiscall)) native_sound_x(void* self) { return g_native_facts.at(self).x; }
int __attribute__((thiscall)) native_sound_y(void* self) { return g_native_facts.at(self).y; }
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called on a native object\n");
    std::exit(5);
}
void* g_vtable[64];

// ---- the map-room lookup, the same on both sides ----
void nearest_room(int x, int y, world::WorldRect& rect) {
    if ((x / 7 + y / 11) % 4 == 0) return;  // no room: the rectangle is unchanged
    const int left = x - x % 600, top = y - y % 300;
    rect = {left, top, left + 600, top + 300};
    if ((x + y) % 5 == 0) rect = {left + 700, top, left + 900, top + 200};  // not around the point
}
void __attribute__((stdcall)) native_nearest_room(int x, int y, world::WorldRect* rect) {
    nearest_room(x, y, *rect);
}

// ---- the rectangle imports CanPerceiveObject calls ----
struct NativeRect { std::int32_t left, top, right, bottom; };
int __attribute__((stdcall)) rect_is_empty(const NativeRect* r) {
    return r->right <= r->left || r->bottom <= r->top;
}
int __attribute__((stdcall)) point_in_rect(const NativeRect* r, std::int32_t x, std::int32_t y) {
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}
int __attribute__((stdcall)) set_rect(NativeRect* r, int left, int top, int right, int bottom) {
    *r = {left, top, right, bottom};
    return 1;
}
int __attribute__((stdcall)) intersect_rect(NativeRect* out, const NativeRect* a, const NativeRect* b) {
    NativeRect r{a->left > b->left ? a->left : b->left, a->top > b->top ? a->top : b->top,
                 a->right < b->right ? a->right : b->right,
                 a->bottom < b->bottom ? a->bottom : b->bottom};
    if (r.right <= r.left || r.bottom <= r.top) {
        *out = {0, 0, 0, 0};
        return 0;
    }
    *out = r;
    return 1;
}

// ---- LC hosts ----
class WorldHost final : public creatures::CreaturePerceptionHost {
public:
    std::vector<objects::Object*> registry;
    std::map<const objects::Object*, std::uint8_t> flags;
    std::map<const objects::Object*, const creatures::Creature*> creature_of;
    const creatures::Creature* self = nullptr;

    std::size_t non_scenery_object_count() const override { return registry.size(); }
    objects::Object* non_scenery_object_at(std::size_t i) const override { return registry[i]; }
    creatures::CreatureMotionLinkFacts motion_link_facts(const objects::Object& object,
                                                         const creatures::Creature& c) const override {
        const auto it = creature_of.find(&object);
        return creatures::motion_link_facts_for(c, it == creature_of.end() ? nullptr : it->second,
                                                object.classifier_base());
    }
    bool is_this_creature(const objects::Object& target, const creatures::Creature& c) const override {
        return &target == &c.skeleton();
    }
    bool has_bounds_flag(const objects::Object& target, std::uint8_t flag) const override {
        return (flags.at(&target) & flag) == flag;
    }
    bool read_bounds(const objects::Object& target, world::WorldRect& out) const override {
        out = g_lc_facts.at(&target).bounds;
        return true;
    }
    bool is_map_room_reference(const objects::Object& reference) const override {
        return creatures::is_map_room_marker(reference.classifier_base());
    }
    void nearest_map_room_bounds(int x, int y, world::WorldRect& out) const override {
        nearest_room(x, y, out);
    }
    bool is_unplaced_new_object(const objects::Object&, const world::WorldRect&) const override {
        return false;
    }
};

class SourceHost final : public creatures::Creature::StimulusSourceHost {
public:
    const objects::Object* self = nullptr;
    creatures::AttentionClassifier classify(const objects::Object& o) const override {
        const std::uint32_t c = o.classifier_base();
        return {static_cast<creatures::AttentionObjectFamily>(c >> 24), static_cast<std::uint8_t>(c >> 16)};
    }
    int sound_source_x(const objects::Object& o) const override { return g_lc_facts.at(&o).x; }
    int sound_source_y(const objects::Object& o) const override { return g_lc_facts.at(&o).y; }
    bool is_this_creature(const objects::Object& o, const creatures::Creature&) const override {
        return &o == self;
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
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

std::uint32_t pack(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    return a | (b << 8) | (c << 16) | (static_cast<std::uint32_t>(d) << 24);
}

std::uint32_t attention_slot(std::uint32_t classifier) {
    const std::uint32_t family = classifier >> 24, genus = (classifier >> 16) & 0xff;
    return family == 2 ? genus : family == 3 ? genus + 25 : family == 4 ? genus + 35 : 0;
}

world::WorldRect random_rect(Random& random, int near_x, int near_y) {
    const int w = 10 + static_cast<int>(random.below(200)), h = 10 + static_cast<int>(random.below(150));
    const int x = near_x + static_cast<int>(random.below(1400)) - 700;
    const int y = near_y + static_cast<int>(random.below(600)) - 300;
    if (random.chance(20)) return {x, y, x, y + h};  // empty
    return {x, y, x + w, y + h};
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n",
                     argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x9e7ce9u;
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
    const struct { const char* name; void* fn; } imports[] = {
        {"!IsRectEmpty", reinterpret_cast<void*>(&rect_is_empty)},
        {"!PtInRect", reinterpret_cast<void*>(&point_in_rect)},
        {"!SetRect", reinterpret_cast<void*>(&set_rect)},
        {"!IntersectRect", reinterpret_cast<void*>(&intersect_rect)},
        {"!rand", reinterpret_cast<void*>(&native_rand)},
    };
    for (const auto& import : imports) {
        if (!image.bind_import(import.name, import.fn)) {
            std::fprintf(stderr, "the exe does not import %s\n", import.name + 1);
            return 2;
        }
    }
    image.make_return(kNativeDebugLog);
    image.redirect(kNativeNearestRoom, reinterpret_cast<const void*>(&native_nearest_room));
    const auto native_update = image.at<NativeUpdate>(kNativeUpdatePerception);
    for (auto& slot : g_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_vtable[26] = reinterpret_cast<void*>(&native_get_bounds);
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
    long differing = 0, seen = 0, kin = 0;
    std::uint32_t monikers[4] = {0, 0x41414141, 0x42424242, 0x43434343};
    for (long index = 0; index < cases; ++index) {
        // ---- LC creature and its native twin ----
        OneGenomeFile file(genomes[random.below(static_cast<std::uint32_t>(genomes.size()))]);
        genome::Genome g(1, random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female,
                         genome::GenomeLifeStage::stage_zero, &file);
        auto creature = std::make_unique<creatures::Creature>();
        auto* brain = new brain::Brain();
        brain->load_genome(g);
        creature->set_brain(brain);
        creature->set_biochemistry(new biochemistry::Biochemistry());
        std::vector<std::uint8_t> native_storage(kCreatureSize, 0);
        std::uint8_t* native = native_storage.data();
        std::vector<std::uint8_t> native_brain(kBrainSize, 0);
        std::vector<std::uint8_t> native_bio(kBiochemistrySize, 0);
        std::map<int, std::vector<std::uint8_t>> neurons;
        field<void*>(native, 0) = g_vtable;
        field<void*>(native, kBrain) = native_brain.data();
        field<void*>(native, kBiochemistry) = native_bio.data();
        for (int lobe : kLobes) {
            const std::uint32_t count = brain->lobe(lobe).neuron_count_value();
            neurons[lobe].assign(kNeuronSize * (count + 1), 0);
            field<void*>(native_brain.data(), lobe_neurons(lobe)) = neurons[lobe].data();
            field<std::int32_t>(native_brain.data(), lobe_count(lobe)) = static_cast<std::int32_t>(count);
            for (std::uint32_t n = 0; n < count; ++n) {
                auto& neuron = brain->lobe(lobe).neuron(n);
                neuron.firing_strength = random.chance(3) ? 0 : random.byte();
                neuron.activation = random.byte();
                neurons[lobe][n * kNeuronSize + kFiring] = neuron.firing_strength;
                neurons[lobe][n * kNeuronSize + kActivation] = neuron.activation;
            }
        }
        auto& chemicals = creature->biochemistry()->chemical_states();
        for (int c = 0; c < 256; ++c) {
            chemicals[c].concentration = random.byte();
            native_bio[8 + 2 * c] = chemicals[c].concentration;
        }
        auto& matrix = creature->goal_direction_weight_matrix();
        for (std::size_t r = 0; r < 40; ++r) {
            for (std::size_t c = 0; c < 16; ++c) {
                matrix[r][c] = random.byte();
                field<std::int32_t>(native, kGoalMatrix + (r * 16 + c) * 4) = matrix[r][c];
            }
        }
        auto& contexts = creature->built_in_stimulus_contexts();
        for (std::size_t k = 0; k < contexts.size() && k < 16; ++k) {
            auto& context = contexts[k];
            context.descriptor = {random.byte(), random.byte(), random.byte(),
                                  static_cast<std::uint8_t>(random.below(8))};
            context.chemical_ids = {random.chance(2) ? std::uint8_t(0) : random.byte(), random.byte(),
                                    static_cast<std::uint8_t>(0x21 + random.below(16)), 0};
            context.chemical_amounts = {random.byte(), random.byte(), random.byte(), random.byte()};
            std::uint8_t* raw = native + kContexts + k * kContextStride;
            field<std::uint32_t>(raw, 8) = pack(context.descriptor.attention_activation,
                                                context.descriptor.target_neuron_index,
                                                context.descriptor.target_lobe_activation,
                                                context.descriptor.flags);
            field<std::uint32_t>(raw, 12) = pack(context.chemical_ids.first, context.chemical_ids.second,
                                                 context.chemical_ids.third, context.chemical_ids.fourth);
            field<std::uint32_t>(raw, 16) = pack(context.chemical_amounts.first, context.chemical_amounts.second,
                                                 context.chemical_amounts.third, context.chemical_amounts.fourth);
        }

        auto& skeleton = creature->skeleton();
        const std::uint32_t own_classifier = (4u << 24) | (random.below(5) << 16) | (random.below(256) << 8);
        skeleton.set_classifier_base(own_classifier);
        field<std::uint32_t>(native, kClassifier) = own_classifier;
        const int foot_x = random.chance(20) ? -50 : static_cast<int>(random.below(8352));
        const int foot_y = static_cast<int>(random.below(1200));
        skeleton.down_foot_x = foot_x;
        skeleton.down_foot_y = foot_y;
        field<std::int32_t>(native, kFootX) = foot_x;
        field<std::int32_t>(native, kFootY) = foot_y;
        const auto facing = static_cast<creatures::FacingDirection>(random.below(4));
        skeleton.facing_direction = facing;
        native[kFacing] = static_cast<std::uint8_t>(facing);
        const world::WorldRect movement = {foot_x - static_cast<int>(random.below(800)), 0,
                                           foot_x + static_cast<int>(random.below(800)), 1200};
        skeleton.set_movement_bounds(movement);
        field<world::WorldRect>(native, kMovementBounds) = movement;
        const int limb = random.chance(2) ? movement.min_x + static_cast<int>(random.below(120)) - 20
                                          : movement.max_x - static_cast<int>(random.below(120)) + 20;
        skeleton.limb_chain_end_x[0] = limb;
        field<std::int32_t>(native, kLimbEndX) = limb;
        const std::uint32_t own_genome = monikers[1 + random.below(3)];
        skeleton.genome_source_filename = own_genome;
        skeleton.mother_moniker = monikers[random.below(4)];
        skeleton.father_moniker = monikers[random.below(4)];
        field<std::uint32_t>(native, kGenome) = own_genome;
        field<std::uint32_t>(native, kMother) = skeleton.mother_moniker;
        field<std::uint32_t>(native, kFather) = skeleton.father_moniker;
        const auto own_sex = random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female;
        CreatureTestAccess::sex(*creature) = own_sex;
        native[kSex] = static_cast<std::uint8_t>(own_sex);
        const bool asleep = random.chance(5);
        skeleton.sleep_indicator_active = asleep;
        native[kSleepIndicator] = asleep;

        // ---- objects: 0-7 around the creature, 2 of them creatures ----
        WorldHost world_host;
        SourceHost source_host;
        world_host.self = creature.get();
        source_host.self = &skeleton;
        g_native_facts.clear();
        g_lc_facts.clear();
        std::vector<std::unique_ptr<objects::Object>> plain;
        std::vector<std::unique_ptr<creatures::Creature>> others;
        std::vector<std::vector<std::uint8_t>> native_objects;
        std::vector<objects::Object*> lc_of;
        std::vector<std::uint8_t*> native_of;
        for (int o = 0; o < 8; ++o) {
            const bool is_creature = o < 2;
            std::uint32_t family = is_creature ? 4 : 2 + random.below(2);
            if (!is_creature && random.chance(10)) family = random.below(2);  // unknown or scenery
            const std::uint32_t genus = family == 2 ? random.below(40) : family == 3 ? random.below(15)
                                        : family == 4 ? random.below(5) : random.below(256);
            std::uint32_t classifier = (family << 24) | (genus << 16) | (random.below(256) << 8);
            if (family == 3 && random.chance(6)) classifier = 0x03020000u | (random.below(256) << 8);
            native_objects.emplace_back(kCreatureSize, 0);
            std::uint8_t* n = native_objects.back().data();
            objects::Object* lc = nullptr;
            if (is_creature) {
                others.push_back(std::make_unique<creatures::Creature>());
                creatures::Creature& other = *others.back();
                other.skeleton().genome_source_filename = monikers[random.below(4)];
                other.skeleton().mother_moniker = monikers[random.below(4)];
                other.skeleton().father_moniker = monikers[random.below(4)];
                CreatureTestAccess::sex(other) = random.chance(2) ? genome::GenomeSex::male
                                                                  : genome::GenomeSex::female;
                field<std::uint32_t>(n, kGenome) = other.skeleton().genome_source_filename;
                field<std::uint32_t>(n, kMother) = other.skeleton().mother_moniker;
                field<std::uint32_t>(n, kFather) = other.skeleton().father_moniker;
                n[kSex] = static_cast<std::uint8_t>(CreatureTestAccess::sex(other));
                lc = &other.skeleton();
                world_host.creature_of[lc] = &other;
            } else {
                plain.push_back(std::make_unique<objects::Object>());
                lc = plain.back().get();
            }
            lc->set_classifier_base(classifier);
            field<void*>(n, 0) = g_vtable;
            field<std::uint32_t>(n, kClassifier) = classifier;
            const std::uint8_t flags = random.chance(6) ? 0x10 : static_cast<std::uint8_t>(random.below(4));
            n[kBoundsFlags] = flags;
            world_host.flags[lc] = flags;
            Facts facts;
            facts.bounds = random_rect(random, foot_x, foot_y);
            facts.x = facts.bounds.min_x + static_cast<int>(random.below(100));
            facts.y = random.chance(10) ? 4000 : facts.bounds.min_y;
            g_native_facts[n] = facts;
            g_lc_facts[lc] = facts;
            lc_of.push_back(lc);
            native_of.push_back(n);
        }
        // The creature itself, as a source and in the registry.
        {
            Facts facts;
            // Native reads its own position through vtable slots 30/31, LC
            // through Skeleton::sound_source_x/y (the sprite bounds' corner).
            facts.bounds = {foot_x - 30, foot_y - 60, foot_x + 30, foot_y};
            facts.x = facts.bounds.min_x;
            facts.y = facts.bounds.min_y;
            skeleton.sprite_bounds = facts.bounds;
            g_native_facts[native] = facts;
            g_lc_facts[&skeleton] = facts;
            world_host.flags[&skeleton] = 0;
            lc_of.push_back(&skeleton);
            native_of.push_back(native);
        }
        auto native_for = [&](const objects::Object* object) -> void* {
            for (std::size_t o = 0; o < lc_of.size(); ++o) {
                if (lc_of[o] == object) return native_of[o];
            }
            return nullptr;
        };
        std::vector<std::uint32_t> registry;
        for (std::size_t o = 0; o < lc_of.size(); ++o) {
            if (o == 8 && random.chance(2)) continue;  // the creature itself, sometimes
            if (random.chance(8)) continue;
            world_host.registry.push_back(lc_of[o]);
            registry.push_back(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native_of[o])));
        }
        *image.at<std::uint32_t*>(kRegistryData) =
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(registry.data()));
        *image.at<std::int32_t*>(kRegistryCount) = static_cast<std::int32_t>(registry.size());

        // ---- links: bounds reference, motion link, attention records ----
        const int reference = static_cast<int>(random.below(14)) - 6;
        skeleton.set_bounds_reference_object(reference >= 0 ? lc_of[reference] : nullptr);
        field<void*>(native, kBoundsReference) = reference >= 0 ? native_of[reference] : nullptr;
        const int motion = static_cast<int>(random.below(12)) - 4;
        skeleton.motion_link = motion >= 0 ? lc_of[motion] : nullptr;
        field<void*>(native, kMotionLink) = motion >= 0 ? native_of[motion] : nullptr;
        auto& records = creature->attention_records();
        for (std::size_t r = 0; r < records.size(); ++r) {
            auto& record = records[r];
            record.target = nullptr;
            if (r > 0 && random.chance(3)) {
                // Mostly an object of the record's own kind; sometimes one
                // whose classifier has changed since (setv clas).
                std::vector<int> own_kind;
                for (int o = 0; o < 8; ++o) {
                    if (attention_slot(lc_of[o]->classifier_base()) == r) own_kind.push_back(o);
                }
                if (!own_kind.empty() && !random.chance(5)) {
                    record.target = lc_of[own_kind[random.below(static_cast<std::uint32_t>(own_kind.size()))]];
                } else if (random.chance(3)) {
                    record.target = lc_of[random.below(8)];
                }
            }
            record.world_x = random.chance(4) ? 0 : static_cast<int>(random.below(8352));
            record.world_y = static_cast<int>(random.below(1200));
            record.target_neuron_index = random.byte();
            record.lobe_activation = random.byte();
            record.visible = random.chance(2);
            record.stimulus_seen = random.chance(2);
            std::uint8_t* raw = native + kAttentionRecords + r * kAttentionStride;
            field<void*>(raw, 0) = native_for(record.target);
            raw[4] = record.target_neuron_index;
            raw[5] = record.lobe_activation;
            field<std::int32_t>(raw, 8) = record.world_x;
            field<std::int32_t>(raw, 12) = record.world_y;
            raw[16] = record.visible;
            raw[17] = record.stimulus_seen;
        }

        // ---- one update ----
        std::vector<std::uint8_t> seen_before(40);
        for (std::size_t r = 0; r < 40; ++r) seen_before[r] = records[r].stimulus_seen;
        g_native_rand_state = g_lc_rand_state = random.next();
        creature->update_perception(world_host, source_host);
        native_update(native);
        for (std::size_t r = 0; r < 40; ++r) seen += records[r].stimulus_seen != seen_before[r];
        if (motion >= 0 && motion < 2) ++kin;

        // ---- compare ----
        std::string first;
        auto note = [&](const std::string& what) { if (first.empty()) first = what; };
        char text[256];
        for (std::size_t r = 0; r < records.size(); ++r) {
            const auto& record = records[r];
            std::uint8_t* raw = native + kAttentionRecords + r * kAttentionStride;
            if (field<void*>(raw, 0) != native_for(record.target) || raw[4] != record.target_neuron_index ||
                raw[5] != record.lobe_activation || field<std::int32_t>(raw, 8) != record.world_x ||
                field<std::int32_t>(raw, 12) != record.world_y || (raw[16] != 0) != record.visible ||
                (raw[17] != 0) != record.stimulus_seen) {
                std::snprintf(text, sizeof text,
                              "record %u: native n%u a%u (%d,%d) v%u s%u, lc n%u a%u (%d,%d) v%u s%u",
                              static_cast<unsigned>(r), raw[4], raw[5], field<std::int32_t>(raw, 8),
                              field<std::int32_t>(raw, 12), raw[16], raw[17], record.target_neuron_index,
                              record.lobe_activation, record.world_x, record.world_y,
                              record.visible ? 1 : 0, record.stimulus_seen ? 1 : 0);
                note(text);
            }
        }
        for (int lobe : kLobes) {
            const std::uint32_t count = brain->lobe(lobe).neuron_count_value();
            for (std::uint32_t n = 0; n < count; ++n) {
                const auto& neuron = brain->lobe(lobe).neuron(n);
                const std::uint8_t* raw = neurons[lobe].data() + n * kNeuronSize;
                if (raw[kFiring] != neuron.firing_strength || raw[kActivation] != neuron.activation) {
                    std::snprintf(text, sizeof text, "lobe %d neuron %u: native %u/%u lc %u/%u", lobe, n,
                                  raw[kFiring], raw[kActivation], neuron.firing_strength,
                                  neuron.activation);
                    note(text);
                }
            }
        }
        for (int c = 0; c < 256; ++c) {
            if (native_bio[8 + 2 * c] != chemicals[c].concentration) {
                std::snprintf(text, sizeof text, "chemical %d: native %u lc %u", c, native_bio[8 + 2 * c],
                              chemicals[c].concentration);
                note(text);
            }
        }
        for (std::size_t r = 0; r < 40; ++r) {
            for (std::size_t c = 0; c < 16; ++c) {
                if (field<std::int32_t>(native, kGoalMatrix + (r * 16 + c) * 4) != matrix[r][c]) {
                    std::snprintf(text, sizeof text, "goal matrix [%u][%u]", static_cast<unsigned>(r),
                                  static_cast<unsigned>(c));
                    note(text);
                }
            }
        }
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: asleep %d facing %u foot (%d,%d) reference %d motion %d: %s\n", index,
                        asleep ? 1 : 0, static_cast<unsigned>(facing), foot_x, foot_y, reference, motion,
                        first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
        skeleton.motion_link = nullptr;
        skeleton.set_bounds_reference_object(nullptr);
        for (auto& record : records) record.target = nullptr;
        for (auto& context : contexts) context.target = nullptr;
        creature.reset();
    }
    std::printf("covered: %ld records first seen, %ld cases with a creature as motion link\n", seen, kin);
    std::printf("%ld cases: %ld differ%s\n", cases, differing,
                all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
