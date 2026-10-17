// Function-level equivalence: Creature::UpdateGoalDirection @0040ccc0 (CE
// 10.3), with the ApplyGoalDirection @0040abc0 it calls, against
// creatures::Creature::update_goal_direction.
//
// Each case gives an LC creature (brain grown from a stock genome) and a
// native Creature-shaped block the same state at the offsets the functions
// read: the 16 goal drive levels, the 40 attention records, the goal matrix,
// the goal-direction state, the stimulus-source (2) and verb (3) lobes,
// death, sleep, the dream countdown, the bounds reference, the motion link,
// facing, foot point, movement bounds and sound position.  Six objects of
// random kind can be record targets or the motion link; their interaction
// event byte (+0x24) is random.  The room table (0x468d64) holds 0 to 6
// random rooms near the creature, of random type; LC's host answers from
// the same table, as the game's host does (first room that contains the
// point).  PtInRect is bound to a plain implementation.
//
// One update per case.  After it, every attention record, the goal-direction
// state and both lobes (firing and activation) must match.
//
// Not generated: genera with no attention record (row 10) and goal weights
// below 0 (the game keeps them 0-255).

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
    static Creature::GoalDirectionState& goal(Creature& c) { return c.goal_direction_state_; }
    static std::uint32_t& dream_countdown(Creature& c) {
        return c.instinct_runtime_state_.dream_countdown;
    }
    static std::uint8_t& death_state(Creature& c) { return c.death_state_; }
};
}  // namespace creatures1::creatures

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;
using creatures::CreatureTestAccess;

constexpr std::uint32_t kNativeUpdateGoalDirection = 0x0040ccc0;
constexpr std::uint32_t kNativeDebugLog = 0x00418000;
constexpr std::uint32_t kRoomTable = 0x00468d64;

// Native Creature layout used by the two functions.
constexpr std::size_t kCreatureSize = 0x2d00;
constexpr std::size_t kMovementBounds = 0xc, kBoundsReference = 0x1c;
constexpr std::size_t kFacing = 0x78, kFootX = 0x7c, kFootY = 0x80;
constexpr std::size_t kSleepIndicator = 0x11f, kMotionLink = 0x7f0;
constexpr std::size_t kAttentionRecords = 0x844, kAttentionStride = 20;
constexpr std::size_t kGoalRecord = 0x1ce4, kGoalVerb = 0x1ce8, kGoalCommitment = 0x1cec,
                      kGoalCountdown = 0x1ced;
constexpr std::size_t kGoalMatrix = 0x1cf0;
constexpr std::size_t kBrain = 0x2c18;
constexpr std::size_t kDrives = 0x2c56;
constexpr std::size_t kDreamCountdown = 0x2cbc;
constexpr std::size_t kDeathState = 0x2ccc;
// A native object: classifier at +4, interaction event byte at +0x24.
constexpr std::size_t kObjectSize = 0x200, kObjectClassifier = 0x4, kObjectEvent = 0x24;
// Native CBrain: lobe i at 8 + 200 i, neurons at +156, count at +164.
constexpr std::size_t kBrainSize = 0x800;
constexpr std::size_t lobe_neurons(int lobe) { return 8 + 200 * lobe + 156; }
constexpr std::size_t lobe_count(int lobe) { return 8 + 200 * lobe + 164; }
constexpr std::size_t kNeuronSize = 16, kFiring = 2, kActivation = 3;
constexpr int kLobes[] = {2, 3};
// Native MapData: room count at +8, rooms from +0xc, 0x14 bytes each
// (left, top, right, bottom, type).
constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14;

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

struct Room {
    std::int32_t left, top, right, bottom, type;
};
std::vector<Room> g_rooms;

int __attribute__((stdcall)) point_in_rect(const Room* r, std::int32_t x, std::int32_t y) {
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

// The creature's own sound position (vtable slots 30/31).
int g_self_x = 0, g_self_y = 0;
int __attribute__((thiscall)) native_sound_x(void*) { return g_self_x; }
int __attribute__((thiscall)) native_sound_y(void*) { return g_self_y; }
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called on a native object\n");
    std::exit(5);
}
void* g_vtable[64];

class GoalHost final : public creatures::CreatureGoalDirectionHost {
public:
    std::int32_t room_index_at(int x, int y) const override {
        for (std::size_t i = 0; i < g_rooms.size(); ++i) {
            if (point_in_rect(&g_rooms[i], x, y)) return static_cast<std::int32_t>(i);
        }
        return -1;
    }
    std::int32_t room_class_at(int x, int y) const override {
        const std::int32_t index = room_index_at(x, y);
        return index < 0 || g_rooms[static_cast<std::size_t>(index)].type == 1 ? 1 : 0;
    }
    std::uint32_t current_interaction_event_id(const objects::Object& o) const override {
        return o.current_interaction_event_id();
    }
    std::int32_t attention_record_index(const objects::Object& o) const override {
        const std::uint32_t c = o.classifier_base();
        return static_cast<std::int32_t>(creatures::get_attention_record_index(
            {static_cast<creatures::AttentionObjectFamily>(c >> 24),
             static_cast<std::uint8_t>(c >> 16)}));
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

std::uint32_t random_classifier(Random& random) {
    const std::uint32_t family = 2 + random.below(3);
    const std::uint32_t genus = family == 2 ? random.below(40) : family == 3 ? random.below(15)
                                                                           : random.below(5);
    return (family << 24) | (genus << 16) | (random.below(256) << 8);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n",
                     argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x90a1d1u;
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
    if (!image.bind_import("!rand", reinterpret_cast<void*>(&native_rand)) ||
        !image.bind_import("!PtInRect", reinterpret_cast<void*>(&point_in_rect))) {
        std::fprintf(stderr, "the exe does not import rand and PtInRect\n");
        return 2;
    }
    image.make_return(kNativeDebugLog);
    const auto native_update = image.at<NativeUpdate>(kNativeUpdateGoalDirection);
    for (auto& slot : g_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
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
    long differing = 0, cleared = 0, boosted = 0, verbs = 0;
    std::vector<std::uint8_t> map_data(kRooms + 8 * kRoomStride, 0);
    *image.at<std::uint8_t**>(kRoomTable) = map_data.data();
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
        std::map<int, std::vector<std::uint8_t>> neurons;
        field<void*>(native, 0) = g_vtable;
        field<void*>(native, kBrain) = native_brain.data();
        for (int lobe : kLobes) {
            const std::uint32_t count = brain->lobe(lobe).neuron_count_value();
            neurons[lobe].assign(kNeuronSize * (count + 1), 0);
            field<void*>(native_brain.data(), lobe_neurons(lobe)) = neurons[lobe].data();
            field<std::int32_t>(native_brain.data(), lobe_count(lobe)) = static_cast<std::int32_t>(count);
            for (std::uint32_t n = 0; n < count; ++n) {
                auto& neuron = brain->lobe(lobe).neuron(n);
                neuron.firing_strength = random.byte();
                neuron.activation = random.chance(3) ? 0 : random.byte();
                neurons[lobe][n * kNeuronSize + kFiring] = neuron.firing_strength;
                neurons[lobe][n * kNeuronSize + kActivation] = neuron.activation;
            }
        }

        // Guards: each one stops the update.
        const bool dead = random.chance(40);
        CreatureTestAccess::death_state(*creature) = dead ? 1 : 0;
        native[kDeathState] = dead ? 1 : 0;
        const bool asleep = random.chance(20);
        auto& skeleton = creature->skeleton();
        skeleton.sleep_indicator_active = asleep;
        native[kSleepIndicator] = asleep;
        const std::uint32_t dreaming = random.chance(20) ? 1 + random.below(30) : 0;
        CreatureTestAccess::dream_countdown(*creature) = dreaming;
        field<std::uint32_t>(native, kDreamCountdown) = dreaming;

        // Drives: often low, so that no goal is also generated.
        auto& drives = creature->control_state().goal_direction_drive_levels;
        const bool calm = random.chance(5);
        for (std::size_t d = 0; d < 16; ++d) {
            const std::uint8_t level = calm ? static_cast<std::uint8_t>(random.below(20))
                                            : random.chance(3) ? 0 : random.byte();
            drives[d] = level;
            native[kDrives + d] = level;
        }
        auto& matrix = creature->goal_direction_weight_matrix();
        for (std::size_t r = 0; r < 40; ++r) {
            const bool empty = random.chance(3);
            for (std::size_t c = 0; c < 16; ++c) {
                matrix[r][c] = empty ? 0 : random.chance(2) ? 0 : random.byte();
                field<std::int32_t>(native, kGoalMatrix + (r * 16 + c) * 4) = matrix[r][c];
            }
        }

        // Body: foot, facing, movement bounds, sound position.
        const int foot_x = static_cast<int>(random.below(8352));
        const int foot_y = static_cast<int>(random.below(1200));
        skeleton.down_foot_x = foot_x;
        skeleton.down_foot_y = foot_y;
        field<std::int32_t>(native, kFootX) = foot_x;
        field<std::int32_t>(native, kFootY) = foot_y;
        const auto facing = static_cast<creatures::FacingDirection>(random.below(4));
        skeleton.facing_direction = facing;
        native[kFacing] = static_cast<std::uint8_t>(facing);
        const int top = foot_y - static_cast<int>(random.below(400));
        const world::WorldRect movement = {foot_x - static_cast<int>(random.below(800)), top,
                                           foot_x + static_cast<int>(random.below(800)),
                                           foot_y + static_cast<int>(random.below(100))};
        skeleton.set_movement_bounds(movement);
        field<world::WorldRect>(native, kMovementBounds) = movement;
        skeleton.sprite_bounds = {foot_x - static_cast<int>(random.below(60)),
                                  foot_y - static_cast<int>(random.below(80)), foot_x + 30, foot_y};
        g_self_x = skeleton.sprite_bounds.min_x;
        g_self_y = skeleton.sprite_bounds.min_y;

        // Rooms near the creature, some around it, some overlapping.
        g_rooms.clear();
        const std::uint32_t room_count = random.below(7);
        for (std::uint32_t r = 0; r < room_count; ++r) {
            const int left = foot_x - static_cast<int>(random.below(1200));
            const int top_y = foot_y - static_cast<int>(random.below(500));
            g_rooms.push_back({left, top_y, left + 100 + static_cast<int>(random.below(1400)),
                               top_y + 50 + static_cast<int>(random.below(600)),
                               static_cast<std::int32_t>(random.below(4))});
        }
        field<std::int32_t>(map_data.data(), kRoomCount) = static_cast<std::int32_t>(room_count);
        for (std::uint32_t r = 0; r < room_count; ++r) {
            std::memcpy(map_data.data() + kRooms + r * kRoomStride, &g_rooms[r], sizeof(Room));
        }

        // Objects: record targets, the motion link, the bounds reference.
        GoalHost host;
        std::vector<std::unique_ptr<objects::Object>> lc_objects;
        std::vector<std::vector<std::uint8_t>> native_objects;
        for (int o = 0; o < 6; ++o) {
            const std::uint32_t classifier = random_classifier(random);
            const std::uint8_t event = random.chance(2) ? 0 : static_cast<std::uint8_t>(1 + random.below(8));
            lc_objects.push_back(std::make_unique<objects::Object>());
            lc_objects.back()->set_classifier_base(classifier);
            lc_objects.back()->set_current_interaction_event_id_for_script(event);
            native_objects.emplace_back(kObjectSize, 0);
            field<std::uint32_t>(native_objects.back().data(), kObjectClassifier) = classifier;
            native_objects.back()[kObjectEvent] = event;
        }
        auto native_for = [&](const objects::Object* object) -> void* {
            for (std::size_t o = 0; o < lc_objects.size(); ++o) {
                if (lc_objects[o].get() == object) return native_objects[o].data();
            }
            return nullptr;
        };
        if (random.chance(15)) {
            skeleton.set_bounds_reference_object(lc_objects[random.below(6)].get());
        }
        field<void*>(native, kBoundsReference) = native_for(skeleton.bounds_reference_object());
        auto& records = creature->attention_records();
        for (std::size_t r = 0; r < records.size(); ++r) {
            auto& record = records[r];
            record.target = random.chance(2) ? lc_objects[random.below(6)].get() : nullptr;
            record.target_neuron_index = random.byte();
            record.lobe_activation = random.byte();
            if (random.chance(4)) {
                record.world_x = -1;
                record.world_y = -1;
            } else {
                record.world_x = foot_x + static_cast<int>(random.below(1600)) - 800;
                record.world_y = foot_y + static_cast<int>(random.below(800)) - 400;
            }
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
        const int motion = static_cast<int>(random.below(10)) - 4;
        skeleton.motion_link = motion >= 0 ? lc_objects[motion].get() : nullptr;
        if (random.chance(4)) {
            // Often the target of some record, so the motion-link tests trip.
            const auto& r = records[1 + random.below(39)];
            if (r.target != nullptr) skeleton.motion_link = r.target;
        }
        field<void*>(native, kMotionLink) = native_for(skeleton.motion_link);
        auto& goal = CreatureTestAccess::goal(*creature);
        goal.attention_record_index = random.chance(2) ? -1 : static_cast<std::int32_t>(random.below(40));
        goal.action_lobe_neuron_index = static_cast<std::int32_t>(random.below(16));
        goal.commitment = random.byte();
        goal.delivery_countdown = static_cast<std::uint8_t>(random.below(6));
        field<std::int32_t>(native, kGoalRecord) = goal.attention_record_index;
        field<std::int32_t>(native, kGoalVerb) = goal.action_lobe_neuron_index;
        native[kGoalCommitment] = goal.commitment;
        native[kGoalCountdown] = goal.delivery_countdown;

        // ---- one update ----
        g_native_rand_state = g_lc_rand_state = random.next();
        std::vector<std::uint8_t> native_before(native, native + kCreatureSize);
        auto neurons_before = neurons;
        creature->update_goal_direction(host);
        native_update(native);

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
                std::snprintf(text, sizeof text, "record %u: native (%d,%d) lc (%d,%d)",
                              static_cast<unsigned>(r), field<std::int32_t>(raw, 8),
                              field<std::int32_t>(raw, 12), record.world_x, record.world_y);
                note(text);
            }
            if (field<std::int32_t>(raw, 8) == -1 &&
                field<std::int32_t>(native_before.data(), kAttentionRecords + r * kAttentionStride + 8) != -1) {
                ++cleared;
            }
        }
        if (field<std::int32_t>(native, kGoalRecord) != goal.attention_record_index ||
            field<std::int32_t>(native, kGoalVerb) != goal.action_lobe_neuron_index ||
            native[kGoalCommitment] != goal.commitment ||
            native[kGoalCountdown] != goal.delivery_countdown) {
            std::snprintf(text, sizeof text,
                          "goal state: native %d/%d/%u/%u lc %d/%d/%u/%u",
                          field<std::int32_t>(native, kGoalRecord), field<std::int32_t>(native, kGoalVerb),
                          native[kGoalCommitment], native[kGoalCountdown], goal.attention_record_index,
                          goal.action_lobe_neuron_index, goal.commitment, goal.delivery_countdown);
            note(text);
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
        if (neurons[2] != neurons_before[2]) ++boosted;
        if (neurons[3] != neurons_before[3] ||
            std::memcmp(native + kGoalRecord, native_before.data() + kGoalRecord, 10) != 0) {
            ++verbs;
        }
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: dead %d asleep %d dream %u facing %u foot (%d,%d) rooms %u motion %d: %s\n",
                        index, dead ? 1 : 0, asleep ? 1 : 0, dreaming, static_cast<unsigned>(facing),
                        foot_x, foot_y, room_count, motion, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
        skeleton.motion_link = nullptr;
        skeleton.set_bounds_reference_object(nullptr);
        for (auto& record : records) record.target = nullptr;
        creature.reset();
    }
    std::printf("covered: %ld memories cleared, %ld fed the stimulus-source lobe, %ld gave a verb\n",
                cleared, boosted, verbs);
    std::printf("%ld cases: %ld differ%s\n", cases, differing,
                all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
