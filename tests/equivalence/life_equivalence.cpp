// Function-level equivalence: a creature's environment loci and life stage,
// Creature::UpdateEnvironmentAndLifeStage @0040c8d0 (CE 10.3) against
// creatures::Creature::update_environment_and_life_stage.
//
// The native creature is a Creature-shaped block with the state the
// function reads and writes: the control-state loci from +0x2c24 (heat,
// cold, light, nearness, fertile, pregnant, ovulation, the seven ageing
// loci, death), the life stage (+0x2cc0), the gamete and child genome
// monikers (+0x2cc4, +0x2cc8), the genome moniker (+0x50), the classifier,
// the death state and the attention records.  Its sound position and that
// of the attention target come from one table on both sides.  The room
// table (with room types) and the season's ambient index are at the MapData
// pointer; LC's host samples them as the game's does.  The genome
// re-expression (0040d930) and Die (0040dc30) are recorders on both sides.
//
// Each case: random loci, stage 0 to 7, gamete and child monikers, a room
// table, a season, the creature and possibly a creature of its own kind
// near it; then 6 updates with random changes to the ovulation, ageing and
// death loci between them.  After each, the control-state loci, the life
// stage, the gamete moniker and the calls to re-express and to die must
// match.

#include "native_image.hpp"

#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "objects/object.hpp"
#include "world/map.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace creatures1::creatures {
// Declared a friend in creature.hpp.
class CreatureTestAccess {
public:
    static std::uint8_t& stage(Creature& c) { return c.genome_life_stage_; }
    static std::uint8_t& death_state(Creature& c) { return c.death_state_; }
};
}  // namespace creatures1::creatures

namespace {

using namespace creatures1;
using creatures::CreatureTestAccess;

constexpr std::uint32_t kUpdate = 0x0040c8d0, kExpress = 0x0040d930, kDie = 0x0040dc30;
constexpr std::uint32_t kNativeDebugLog = 0x00418000, kMapData = 0x00468d64;
constexpr std::size_t kBlockSize = 0x2d00, kClassifier = 0x4, kGenome = 0x50, kAttentionRecords = 0x844,
                      kAttentionStride = 20, kControlState = 0x2c24, kControlStateSize = 34, kStage = 0x2cc0,
                      kGamete = 0x2cc4, kChild = 0x2cc8, kDeathState = 0x2ccc;
constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14, kAmbientIndex = 0xbf0;

static_assert(offsetof(creatures::Creature::ControlState, death_signal) == 33,
              "ControlState must match the native loci at +0x2c24");

std::vector<std::string> g_native_calls, g_lc_calls;
void __attribute__((thiscall)) native_express(void*) { g_native_calls.push_back("express"); }
void __attribute__((thiscall)) native_die(void*) { g_native_calls.push_back("die"); }

std::map<const void*, int> g_sound_x, g_sound_y;
int __attribute__((thiscall)) native_sound_x(void* self) { return g_sound_x.at(self); }
int __attribute__((thiscall)) native_sound_y(void* self) { return g_sound_y.at(self); }
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called\n");
    std::exit(5);
}
void* g_vtable[64];
int __attribute__((stdcall)) point_in_rect(const world::WorldRect* r, std::int32_t x, std::int32_t y) {
    return x >= r->min_x && x < r->max_x && y >= r->min_y && y < r->max_y;
}

class EnvironmentHost final : public creatures::CreatureEnvironmentHost {
public:
    std::vector<world::MapRoom>* rooms = nullptr;
    std::uint32_t ambient = 0;
    // As the game's: the room at the point decides whether the map's
    // temperature applies; the season gives temperature and light.
    creatures::CreatureEnvironmentSample sample_at(int x, int y) const override {
        creatures::CreatureEnvironmentSample sample;
        const world::MapRoom* room = nullptr;
        for (const auto& r : *rooms) {
            if (x >= r.bounds.left && x < r.bounds.right && y >= r.bounds.top && y < r.bounds.bottom) {
                room = &r;
                break;
            }
        }
        sample.uses_ambient_temperature = room == nullptr || room->room_type == 1;
        sample.temperature_delta_raw = world::kAmbientEnvironmentRecords[ambient + 5].temperature_delta_raw;
        sample.ambient_light_level = world::kAmbientLightProfiles[ambient].light_level;
        return sample;
    }
    int sound_source_x(const objects::Object& o) const override { return g_sound_x.at(&o); }
    bool is_selected_creature(const creatures::Creature&) const override { return false; }
    void initialize_from_genome(creatures::Creature&) override { g_lc_calls.push_back("express"); }
    void die(creatures::Creature&) override { g_lc_calls.push_back("die"); }
};

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
    int range(int low, int high) { return low + static_cast<int>(below(static_cast<std::uint32_t>(high - low + 1))); }
};
template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x11fe5u;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-cases" && i + 1 < argc) cases = std::atol(argv[++i]);
        else if (arg == "-seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 0);
    }
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    if (!image.bind_import("!PtInRect", reinterpret_cast<void*>(&point_in_rect))) {
        std::fprintf(stderr, "the exe does not import PtInRect\n");
        return 2;
    }
    image.make_return(kNativeDebugLog);
    image.redirect(kExpress, reinterpret_cast<const void*>(&native_express));
    image.redirect(kDie, reinterpret_cast<const void*>(&native_die));
    for (auto& s : g_vtable) s = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_vtable[30] = reinterpret_cast<void*>(&native_sound_x);
    g_vtable[31] = reinterpret_cast<void*>(&native_sound_y);
    std::vector<std::uint8_t> map_data(0x1000, 0);
    *image.at<void**>(kMapData) = map_data.data();
    using Call0 = void(__attribute__((thiscall)) *)(void*);
    const auto native_update = image.at<Call0>(kUpdate);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, updates = 0, stage_changes = 0, deaths = 0;
    for (long index = 0; index < cases; ++index) {
        // ---- rooms and season ----
        std::vector<world::MapRoom> rooms;
        const int base_x = random.range(500, 7500), base_y = random.range(200, 900);
        for (int r = 0, n = random.range(0, 4); r < n; ++r) {
            world::MapRoom room;
            const int left = base_x + random.range(-400, 100), top = base_y + random.range(-300, 100);
            room.bounds = {left, top, left + random.range(100, 700), top + random.range(100, 400)};
            room.room_type = random.below(3);
            rooms.push_back(room);
        }
        std::memset(map_data.data(), 0, map_data.size());
        field<std::int32_t>(map_data.data(), kRoomCount) = static_cast<std::int32_t>(rooms.size());
        for (std::size_t r = 0; r < rooms.size(); ++r) {
            std::memcpy(map_data.data() + kRooms + r * kRoomStride, &rooms[r].bounds, 16);
            field<std::uint32_t>(map_data.data() + kRooms + r * kRoomStride, 16) = rooms[r].room_type;
        }
        EnvironmentHost host;
        host.rooms = &rooms;
        host.ambient = random.below(4);
        field<std::uint32_t>(map_data.data(), kAmbientIndex) = host.ambient;

        // ---- the creature ----
        auto creature = std::make_unique<creatures::Creature>();
        std::vector<std::uint8_t> native_storage(kBlockSize, 0);
        std::uint8_t* native = native_storage.data();
        field<void*>(native, 0) = g_vtable;
        auto& skeleton = creature->skeleton();
        const std::uint32_t classifier = 0x04000000u | (random.below(5) << 16) | (random.below(256) << 8);
        skeleton.set_classifier_base(classifier);
        field<std::uint32_t>(native, kClassifier) = classifier;
        const std::uint32_t moniker = random.next() | 1u;
        skeleton.genome_source_filename = moniker;
        field<std::uint32_t>(native, kGenome) = moniker;
        const int x = base_x + random.range(-300, 300), y = base_y + random.range(-200, 200);
        skeleton.sprite_bounds = {x, y, x + 40, y + 60};
        g_sound_x.clear();
        g_sound_y.clear();
        g_sound_x[native] = x;
        g_sound_y[native] = y;
        auto& control = creature->control_state();
        auto* control_bytes = reinterpret_cast<std::uint8_t*>(&control);
        for (std::size_t b = 0; b < kControlStateSize; ++b) {
            control_bytes[b] = random.chance(3) ? random.byte() : 0;
        }
        std::memcpy(native + kControlState, control_bytes, kControlStateSize);
        const std::uint8_t stage = static_cast<std::uint8_t>(random.below(8));
        CreatureTestAccess::stage(*creature) = stage;
        native[kStage] = stage;
        const std::uint32_t gamete = random.chance(2) ? 0 : random.next();
        const std::uint32_t child = random.chance(2) ? 0 : random.next();
        creature->set_gamete_genome_source_filename(gamete);
        creature->set_child_genome_source_filename(child);
        field<std::uint32_t>(native, kGamete) = gamete;
        field<std::uint32_t>(native, kChild) = child;
        const bool dead = random.chance(20);
        CreatureTestAccess::death_state(*creature) = dead ? 1 : 0;
        native[kDeathState] = dead ? 1 : 0;
        // A creature of its own kind in the attention record for that kind.
        objects::Object other;
        std::vector<std::uint8_t> native_other(0x200, 0);
        field<void*>(native_other.data(), 0) = g_vtable;
        const std::uint32_t slot = creatures::get_attention_record_index(
            {static_cast<creatures::AttentionObjectFamily>(4), static_cast<std::uint8_t>(classifier >> 16)});
        if (random.chance(2)) {
            const int ox = x + random.range(-400, 400);
            g_sound_x[&other] = ox;
            g_sound_x[native_other.data()] = ox;
            g_sound_y[native_other.data()] = y;
            creature->attention_records()[slot].target = &other;
            field<void*>(native + kAttentionRecords + slot * kAttentionStride, 0) = native_other.data();
        }

        std::string first;
        char text[256];
        for (int u = 0; u < 6 && first.empty(); ++u, ++updates) {
            g_native_calls.clear();
            g_lc_calls.clear();
            if (u > 0) {
                // Between updates: the ovulation, ageing and death loci change.
                control.ovulation_signal = random.chance(2) ? random.byte() : control.ovulation_signal;
                const std::size_t a = random.below(7);
                control.life_stage_advance_signals[a] = random.chance(3) ? random.byte() : 0;
                control.death_signal = random.chance(10) ? random.byte() : 0;
                std::memcpy(native + kControlState, control_bytes, kControlStateSize);
            }
            const std::uint8_t stage_before = CreatureTestAccess::stage(*creature);
            creature->update_environment_and_life_stage(host);
            native_update(native);
            if (CreatureTestAccess::stage(*creature) != stage_before) ++stage_changes;
            for (const auto& c : g_lc_calls) if (c == "die") ++deaths;

            auto note = [&](const std::string& what) {
                if (first.empty()) {
                    std::snprintf(text, sizeof text, "update %d (stage %u): ", u, stage_before);
                    first = text + what;
                }
            };
            for (std::size_t b = 0; b < kControlStateSize; ++b) {
                if (native[kControlState + b] != control_bytes[b]) {
                    std::snprintf(text, sizeof text, "locus +0x%x: native %u lc %u",
                                  static_cast<unsigned>(kControlState + b), native[kControlState + b], control_bytes[b]);
                    note(text);
                }
            }
            if (native[kStage] != CreatureTestAccess::stage(*creature)) {
                std::snprintf(text, sizeof text, "stage: native %u lc %u", native[kStage],
                              CreatureTestAccess::stage(*creature));
                note(text);
            }
            if (field<std::uint32_t>(native, kGamete) != creature->gamete_genome_source_filename()) note("gamete");
            if (g_native_calls != g_lc_calls) {
                std::snprintf(text, sizeof text, "calls: native %u lc %u", static_cast<unsigned>(g_native_calls.size()),
                              static_cast<unsigned>(g_lc_calls.size()));
                note(text);
            }
        }
        creature->attention_records()[slot].target = nullptr;
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: %s\n", index, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld updates, %ld stage changes, %ld deaths\n", updates, stage_changes, deaths);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
