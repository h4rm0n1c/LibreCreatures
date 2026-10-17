// Function-level equivalence: the creature fan-out of CE 10.3 (MapData
// functions that pick creatures by place) against creatures/events.cpp.
//
//   QueueEventForCreaturesInSpeechRange @00422f90 - queue_events_in_speech_range
//   QueueEventForCreaturesThatCanPerceiveObject @00423030
//                                         - queue_events_for_perceiving_creatures
//   QueueTactEventForOverlappingCreatures @00423090
//                                         - queue_tact_events_for_overlapping_creatures
//   QueueForPerceivingCreatures @00423150 - queue_stimulus_for_perceiving_creatures
//   QueueForOverlappingCreatures @004231f0 - queue_stimulus_for_overlapping_creatures
//   QueueSignStimulusForPerceivingCreatures @00423390
//                                         - queue_sign_stimulus_for_perceiving_creatures
//   QueueTactStimulusForOverlappingCreatures @00423470
//                                         - queue_tact_stimulus_for_overlapping_creatures
//
// Each case makes 0 to 8 creatures (the native creature list at 0x4704bc and
// 0x4704c0) and 4 other objects, near each other, with random classifier,
// flags (vehicle, invisible), facing, foot point, movement bounds, bounds,
// bounds reference and built-in stimulus contexts.  Then 6 calls of random
// function and arguments; the source is any object or creature.
// CanPerceiveObject, FindNearestMapRoomBoundsAtPoint and QueueObjectEvent
// run as native code, with a random room table and the four rectangle
// imports bound to plain implementations.  LC's host is the game's
// (WindowsCreatureFanoutHost): creature facts from the skeleton,
// Creature::can_perceive, the context copy, and the nearest room with the
// caller's rectangle passed in.
//
// After each call, the queued object events (immediate ring at 0x466470,
// delayed table at 0x46e1e0), the queued stimuli (ring at 0x466468) and the
// passed stimulus context must match.
//
// Not compared: the native sign and tact helpers also write source and
// target into the creature's own built-in context (LC's host copies instead;
// every reader overwrites both fields).
// Not generated: negative stimulus indices (native reads before the
// contexts).

#include "native_image.hpp"

#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "creatures/events.hpp"
#include "objects/object.hpp"
#include "world/map.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace creatures1;

constexpr std::uint32_t kNativeSpeech = 0x00422f90, kNativePerceivingEvents = 0x00423030,
                        kNativeTactEvents = 0x00423090, kNativePerceivingStimulus = 0x00423150,
                        kNativeOverlappingStimulus = 0x004231f0, kNativeSign = 0x00423390,
                        kNativeTact = 0x00423470;
constexpr std::uint32_t kNativeDebugLog = 0x00418000;
constexpr std::uint32_t kMapData = 0x00468d64;
constexpr std::uint32_t kCreatureList = 0x004704bc, kCreatureCount = 0x004704c0;
// Object events: immediate ring and the delayed table.
constexpr std::uint32_t kRingWrite = 0x00466470, kRingRead = 0x0046646c, kRingStart = 0x0046cf20;
constexpr std::uint32_t kDelayed = 0x0046e1e0, kDelayedEnd = 0x0046f4a0, kDelayedCount = 0x00467db8;
constexpr std::uint32_t kNow = 0x00467d30;
constexpr std::size_t kEventSize = 0x18;
// Creature stimuli: a ring.
constexpr std::uint32_t kStimulusWrite = 0x00466468, kStimulusRead = 0x00466464,
                        kStimulusStart = 0x0046f4a0;
constexpr std::size_t kStimulusSize = 0x14;

constexpr std::size_t kBlockSize = 0x2d00;
constexpr std::size_t kClassifier = 0x4, kBoundsFlags = 0x9, kMovementBounds = 0xc,
                      kBoundsReference = 0x1c, kFacing = 0x78, kFootX = 0x7c, kFootY = 0x80,
                      kContexts = 0x26f0, kContextSize = 0x14;
constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14;

struct NativeRect {
    std::int32_t left, top, right, bottom;
};
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

// The fan-out functions take ECX and EDX and leave the stack to the caller.
extern "C" void __cdecl call_regs(std::uint32_t fn, const void* ecx, std::uint32_t edx,
                                  std::uint32_t a1, std::uint32_t a2, std::uint32_t a3);
__asm__(".globl _call_regs\n"
        "_call_regs:\n"
        "    pushl %ebx\n"
        "    movl 8(%esp), %eax\n"
        "    movl 12(%esp), %ecx\n"
        "    movl 16(%esp), %edx\n"
        "    pushl 28(%esp)\n"
        "    pushl 28(%esp)\n"
        "    pushl 28(%esp)\n"
        "    call *%eax\n"
        "    addl $12, %esp\n"
        "    popl %ebx\n"
        "    ret\n");
using NativeContextFanout = void(__attribute__((thiscall)) *)(void* context, void* target);

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

// ---- native objects: bounds and sound position from one table ----
struct Facts {
    world::WorldRect bounds{};
    int x = 0, y = 0;
};
std::map<const void*, Facts> g_native_facts;
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

// ---- LC side ----
class PlainObject final : public objects::Object {
public:
    world::WorldRect bounds{};
    int x = 0, y = 0;
    bool get_bounds(world::WorldRect* out) const override {
        *out = bounds;
        return true;
    }
    int sound_source_x() const override { return x; }
    int sound_source_y() const override { return y; }
    void put_bounds_flags(std::uint8_t flags) { set_bounds_flags(flags); }
};

struct Queued {
    const void* source;
    const void* target;
    std::uint32_t event, argument;
    std::int32_t delay;  // 0: immediate
};
struct QueuedStimulus {
    const void* source;
    const void* creature;
    std::uint8_t rest[12];
};

// As WindowsCreatureFanoutHost, which is also its own perception host.
class FanoutHost final : public creatures::CreatureEventFanoutHost,
                         public creatures::CreaturePerceptionHost {
public:
    std::vector<creatures::Creature*> creatures;
    world::MapRoomTable rooms;
    // LC object -> its native twin, for the record.
    std::map<const void*, const void*> native_of;
    std::vector<Queued> events;
    std::vector<QueuedStimulus> stimuli;

    std::size_t creature_count() const override { return creatures.size(); }
    creatures::Creature* creature_at(std::size_t i) const override { return creatures[i]; }
    void report_invalid_creature_index() const override {}
    int source_sound_source_x(const objects::Object& s) const override { return s.sound_source_x(); }
    int source_sound_source_y(const objects::Object& s) const override { return s.sound_source_y(); }
    int down_foot_x(const creatures::Creature& c) const override { return c.skeleton().down_foot_x; }
    int down_foot_y(const creatures::Creature& c) const override { return c.skeleton().down_foot_y; }
    bool is_same_object(const creatures::Creature& c, const objects::Object& o) const override {
        return &c.skeleton() == &o;
    }
    bool can_perceive(const creatures::Creature& c, const objects::Object& s) const override {
        return c.can_perceive(s, *this);
    }
    bool source_is_vehicle(const objects::Object& s) const override {
        return s.has_bounds_flag(objects::Object::kIsVehicle);
    }
    bool creature_is_bound_to_source(const creatures::Creature& c,
                                     const objects::Object& s) const override {
        return c.skeleton().bounds_reference_object() == &s;
    }
    bool is_creature_classifier(const creatures::Creature& c) const override {
        return ((c.skeleton().classifier_base() >> 24) & 0xffu) == 4u;
    }
    objects::Object& object_for_creature(creatures::Creature& c) const override { return c.skeleton(); }
    void queue_object_event(objects::Object& source, objects::Object& target,
                            objects::ObjectEventId event_id, std::uint32_t argument,
                            std::int32_t delay) override {
        events.push_back({native_of.at(&source), native_of.at(&target),
                          static_cast<std::uint32_t>(event_id), argument, delay});
    }
    bool copy_built_in_stimulus(const creatures::Creature& creature, std::int32_t index,
                                objects::Object& source,
                                objects::QueuedCreatureStimulus& out) const override {
        const auto& contexts = creature.built_in_stimulus_contexts();
        if (index < 0 || static_cast<std::size_t>(index) >= contexts.size()) return false;
        const auto& context = contexts[static_cast<std::size_t>(index)];
        out.source_object = &source;
        out.target_creature = const_cast<creatures::Creature*>(&creature);
        out.descriptor = context.descriptor;
        out.chemical_ids = context.chemical_ids;
        out.chemical_amounts = context.chemical_amounts;
        return true;
    }
    void queue_creature_stimulus(const objects::QueuedCreatureStimulus& s) override {
        QueuedStimulus q{};
        q.source = s.source_object != nullptr ? native_of.at(s.source_object) : nullptr;
        q.creature = s.target_creature != nullptr ? native_of.at(&s.target_creature->skeleton()) : nullptr;
        std::memcpy(q.rest, &s.descriptor, 12);
        stimuli.push_back(q);
    }
    void report_invalid_stimulus_index(std::int32_t) const override {}

    // CreaturePerceptionHost (only what can_perceive asks).
    std::size_t non_scenery_object_count() const override { return 0; }
    objects::Object* non_scenery_object_at(std::size_t) const override { return nullptr; }
    creatures::CreatureMotionLinkFacts motion_link_facts(const objects::Object&,
                                                         const creatures::Creature&) const override {
        return {};
    }
    bool is_this_creature(const objects::Object& t, const creatures::Creature& c) const override {
        return &t == &c.skeleton();
    }
    bool has_bounds_flag(const objects::Object& t, std::uint8_t flag) const override {
        return t.has_bounds_flag(flag);
    }
    bool read_bounds(const objects::Object& t, world::WorldRect& out) const override {
        return t.get_bounds(&out);
    }
    bool is_map_room_reference(const objects::Object& r) const override {
        return creatures::is_map_room_marker(r.classifier_base());
    }
    // As C1WindowsDocument::find_nearest_room_bounds_at_point.
    void nearest_map_room_bounds(int x, int y, world::WorldRect& out) const override {
        world::MapRectangle m{out.min_x, out.min_y, out.max_x, out.max_y};
        world::find_nearest_room_bounds_at_point(rooms, x, y, m);
        out = {m.left, m.top, m.right, m.bottom};
    }
    bool is_unplaced_new_object(const objects::Object&, const world::WorldRect&) const override {
        return false;
    }
};

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}
template <typename T>
T& global(equivalence::NativeImage& image, std::uint32_t address) {
    return *image.at<T*>(address);
}

std::uint32_t random_classifier(Random& random, bool creature) {
    const std::uint32_t family = creature ? (random.chance(10) ? 2 : 4) : random.below(5);
    return (family << 24) | (random.below(40) << 16) | (random.below(256) << 8);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0xfa0a7u;
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
    const struct { const char* name; void* fn; } imports[] = {
        {"!IsRectEmpty", reinterpret_cast<void*>(&rect_is_empty)},
        {"!PtInRect", reinterpret_cast<void*>(&point_in_rect)},
        {"!SetRect", reinterpret_cast<void*>(&set_rect)},
        {"!IntersectRect", reinterpret_cast<void*>(&intersect_rect)},
    };
    for (const auto& import : imports) {
        if (!image.bind_import(import.name, import.fn)) {
            std::fprintf(stderr, "the exe does not import %s\n", import.name + 1);
            return 2;
        }
    }
    image.make_return(kNativeDebugLog);
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
    std::vector<std::uint8_t> map_data(0x1000, 0);
    global<std::uint8_t*>(image, kMapData) = map_data.data();
    const std::int32_t now = 5000;
    global<std::int32_t>(image, kNow) = now;
    long differing = 0, calls = 0, events_seen = 0, stimuli_seen = 0;
    const char* names[] = {"speech events", "perceiving events", "tact events", "perceiving stimulus",
                           "overlapping stimulus", "sign stimulus", "tact stimulus"};
    for (long index = 0; index < cases; ++index) {
        // ---- rooms ----
        std::vector<world::MapRoom> rooms;
        const std::uint32_t room_count = random.below(12);
        const int base_x = random.range(0, 7400), base_y = random.range(0, 900);
        for (std::uint32_t r = 0; r < room_count; ++r) {
            world::MapRoom room;
            const int left = base_x + random.range(-800, 800), top = base_y + random.range(-300, 300);
            room.bounds = {left, top, left + random.range(100, 1200), top + random.range(80, 400)};
            room.room_type = random.below(4);
            rooms.push_back(room);
        }
        std::memset(map_data.data(), 0, map_data.size());
        field<std::int32_t>(map_data.data(), kRoomCount) = static_cast<std::int32_t>(room_count);
        for (std::uint32_t r = 0; r < room_count; ++r) {
            std::memcpy(map_data.data() + kRooms + r * kRoomStride, &rooms[r].bounds, 16);
            field<std::uint32_t>(map_data.data() + kRooms + r * kRoomStride, 16) = rooms[r].room_type;
        }

        // ---- creatures and objects, both sides ----
        FanoutHost host;
        host.rooms = {rooms.data(), rooms.size()};
        g_native_facts.clear();
        const std::uint32_t creature_count = random.below(9);
        std::vector<std::unique_ptr<creatures::Creature>> lc_creatures;
        std::vector<std::unique_ptr<PlainObject>> lc_plain;
        std::vector<objects::Object*> lc_all;        // creatures' skeletons first
        std::vector<std::vector<std::uint8_t>> native_blocks;
        for (std::uint32_t o = 0; o < creature_count + 4; ++o) {
            native_blocks.emplace_back(kBlockSize, 0);
            field<void*>(native_blocks.back().data(), 0) = g_vtable;
        }
        auto place = [&](world::WorldRect& bounds, int& x, int& y) {
            const int left = base_x + random.range(-700, 700), top = base_y + random.range(-300, 300);
            bounds = {left, top, left + random.range(0, 150), top + random.range(0, 150)};
            if (random.chance(30)) std::swap(bounds.min_x, bounds.max_x);  // inverted
            x = bounds.min_x;
            y = random.chance(10) ? 4000 : bounds.min_y;
        };
        for (std::uint32_t c = 0; c < creature_count; ++c) {
            lc_creatures.push_back(std::make_unique<creatures::Creature>());
            creatures::Creature& lc = *lc_creatures.back();
            auto& skeleton = lc.skeleton();
            std::uint8_t* native = native_blocks[c].data();
            const std::uint32_t classifier = random_classifier(random, true);
            skeleton.set_classifier_base(classifier);
            field<std::uint32_t>(native, kClassifier) = classifier;
            Facts facts;
            place(facts.bounds, facts.x, facts.y);
            skeleton.sprite_bounds = facts.bounds;
            facts.x = facts.bounds.min_x;  // Skeleton::sound_source_x
            facts.y = facts.bounds.min_y;
            g_native_facts[native] = facts;
            const int foot_x = random.chance(20) ? -40 : facts.bounds.min_x + random.range(0, 60);
            const int foot_y = facts.bounds.max_y;
            skeleton.down_foot_x = foot_x;
            skeleton.down_foot_y = foot_y;
            field<std::int32_t>(native, kFootX) = foot_x;
            field<std::int32_t>(native, kFootY) = foot_y;
            const auto facing = static_cast<creatures::FacingDirection>(random.below(4));
            skeleton.facing_direction = facing;
            native[kFacing] = static_cast<std::uint8_t>(facing);
            const world::WorldRect movement = {foot_x - random.range(0, 800), foot_y - random.range(0, 400),
                                               foot_x + random.range(0, 800), foot_y + random.range(0, 50)};
            skeleton.set_movement_bounds(movement);
            field<world::WorldRect>(native, kMovementBounds) = movement;
            const std::uint8_t flags = random.chance(8) ? 0x10 : static_cast<std::uint8_t>(random.below(4));
            skeleton.set_bounds_flags_for_script(flags);
            native[kBoundsFlags] = flags;
            auto& contexts = lc.built_in_stimulus_contexts();
            for (std::size_t k = 0; k < contexts.size(); ++k) {
                auto& context = contexts[k];
                context.descriptor = {random.byte(), random.byte(), random.byte(), random.byte()};
                context.chemical_ids = {random.byte(), random.byte(), random.byte(), random.byte()};
                context.chemical_amounts = {random.byte(), random.byte(), random.byte(), random.byte()};
                std::uint8_t* raw = native + kContexts + k * kContextSize;
                std::memcpy(raw + 8, &context.descriptor, 4);
                std::memcpy(raw + 12, &context.chemical_ids, 4);
                std::memcpy(raw + 16, &context.chemical_amounts, 4);
            }
            host.creatures.push_back(&lc);
            lc_all.push_back(&skeleton);
            host.native_of[&skeleton] = native;
        }
        for (int o = 0; o < 4; ++o) {
            lc_plain.push_back(std::make_unique<PlainObject>());
            PlainObject& lc = *lc_plain.back();
            std::uint8_t* native = native_blocks[creature_count + o].data();
            const std::uint32_t classifier = random_classifier(random, false);
            lc.set_classifier_base(classifier);
            field<std::uint32_t>(native, kClassifier) = classifier;
            Facts facts;
            place(facts.bounds, facts.x, facts.y);
            lc.bounds = facts.bounds;
            lc.x = facts.x;
            lc.y = facts.y;
            g_native_facts[native] = facts;
            const std::uint8_t flags =
                static_cast<std::uint8_t>((random.chance(3) ? 0x08 : 0) | (random.chance(8) ? 0x10 : 0) |
                                          random.below(4));
            lc.put_bounds_flags(flags);
            native[kBoundsFlags] = flags;
            lc_all.push_back(&lc);
            host.native_of[&lc] = native;
        }
        // Bounds references: creatures riding vehicles, or on map-room markers.
        for (std::uint32_t c = 0; c < creature_count; ++c) {
            const int reference = static_cast<int>(random.below(static_cast<std::uint32_t>(lc_all.size()) + 4)) - 4;
            if (reference < 0 || lc_all[reference] == lc_all[c]) continue;
            lc_creatures[c]->skeleton().set_bounds_reference_object(lc_all[reference]);
            field<const void*>(native_blocks[c].data(), kBoundsReference) = host.native_of.at(lc_all[reference]);
        }
        std::vector<std::uint32_t> list;
        for (std::uint32_t c = 0; c < creature_count; ++c) {
            list.push_back(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native_blocks[c].data())));
        }
        global<std::uint32_t*>(image, kCreatureList) = list.data();
        global<std::int32_t>(image, kCreatureCount) = static_cast<std::int32_t>(creature_count);

        std::string first;
        char text[256];
        for (int call = 0; call < 6; ++call, ++calls) {
            // Empty queues on both sides.
            host.events.clear();
            host.stimuli.clear();
            global<std::uint32_t>(image, kRingWrite) = kRingStart;
            global<std::uint32_t>(image, kRingRead) = kRingStart;
            std::memset(image.at<void*>(kDelayed), 0, kDelayedEnd - kDelayed);
            global<std::int32_t>(image, kDelayedCount) = 0;
            global<std::uint32_t>(image, kStimulusWrite) = kStimulusStart;
            global<std::uint32_t>(image, kStimulusRead) = kStimulusStart;

            const int which = static_cast<int>(random.below(7));
            objects::Object& source = *lc_all[random.below(static_cast<std::uint32_t>(lc_all.size()))];
            void* native_source = const_cast<void*>(host.native_of.at(&source));
            const std::uint32_t event = random.below(16);
            const std::uint32_t argument = random.next();
            const std::int32_t delay = random.chance(2) ? 0 : random.range(1, 50);
            const std::int32_t stimulus = static_cast<std::int32_t>(random.below(40));
            creatures::StimulusContext lc_context{};
            lc_context.descriptor = {random.byte(), random.byte(), random.byte(), random.byte()};
            lc_context.chemical_ids = {random.byte(), random.byte(), random.byte(), random.byte()};
            lc_context.chemical_amounts = {random.byte(), random.byte(), random.byte(), random.byte()};
            std::uint8_t native_context[kContextSize] = {};
            std::memcpy(native_context + 8, &lc_context.descriptor, 12);
            const auto id = static_cast<objects::ObjectEventId>(event);
            switch (which) {
            case 0:
                creatures::queue_events_in_speech_range(source, id, argument, delay, host);
                call_regs(kNativeSpeech, native_source, event, argument, 0,
                          static_cast<std::uint32_t>(delay));
                break;
            case 1:
                creatures::queue_events_for_perceiving_creatures(source, id, host);
                call_regs(kNativePerceivingEvents, native_source, event, 0, 0, 0);
                break;
            case 2:
                creatures::queue_tact_events_for_overlapping_creatures(source, id, host);
                call_regs(kNativeTactEvents, native_source, event, 0, 0, 0);
                break;
            case 3:
                creatures::queue_stimulus_for_perceiving_creatures(lc_context, source, host);
                image.at<NativeContextFanout>(kNativePerceivingStimulus)(native_context, native_source);
                break;
            case 4:
                creatures::queue_stimulus_for_overlapping_creatures(lc_context, source, host);
                image.at<NativeContextFanout>(kNativeOverlappingStimulus)(native_context, native_source);
                break;
            case 5:
                creatures::queue_sign_stimulus_for_perceiving_creatures(source, stimulus, host);
                call_regs(kNativeSign, native_source, static_cast<std::uint32_t>(stimulus), 0, 0, 0);
                break;
            default:
                creatures::queue_tact_stimulus_for_overlapping_creatures(source, stimulus, host);
                call_regs(kNativeTact, native_source, static_cast<std::uint32_t>(stimulus), 0, 0, 0);
                break;
            }

            // ---- native results ----
            std::vector<Queued> native_events;
            for (std::uint32_t at = kRingStart; at != global<std::uint32_t>(image, kRingWrite); at += kEventSize) {
                const std::uint8_t* e = image.at<const std::uint8_t*>(at);
                native_events.push_back({*reinterpret_cast<void* const*>(e), *reinterpret_cast<void* const*>(e + 4),
                                         *reinterpret_cast<const std::uint32_t*>(e + 8),
                                         *reinterpret_cast<const std::uint32_t*>(e + 12), 0});
            }
            for (std::uint32_t at = kDelayed; at < kDelayedEnd; at += kEventSize) {
                const std::uint8_t* e = image.at<const std::uint8_t*>(at);
                if (*reinterpret_cast<void* const*>(e) == nullptr) continue;
                native_events.push_back({*reinterpret_cast<void* const*>(e), *reinterpret_cast<void* const*>(e + 4),
                                         *reinterpret_cast<const std::uint32_t*>(e + 8),
                                         *reinterpret_cast<const std::uint32_t*>(e + 12),
                                         *reinterpret_cast<const std::int32_t*>(e + 20) - now});
            }
            std::vector<QueuedStimulus> native_stimuli;
            for (std::uint32_t at = kStimulusStart; at != global<std::uint32_t>(image, kStimulusWrite);
                 at += kStimulusSize) {
                const std::uint8_t* s = image.at<const std::uint8_t*>(at);
                QueuedStimulus q{};
                q.source = *reinterpret_cast<void* const*>(s);
                q.creature = *reinterpret_cast<void* const*>(s + 4);
                std::memcpy(q.rest, s + 8, 12);
                native_stimuli.push_back(q);
            }
            events_seen += static_cast<long>(native_events.size());
            stimuli_seen += static_cast<long>(native_stimuli.size());

            // ---- compare ----
            // LC queues delayed events through the same call; order them as
            // native lists them (immediate ring, then the delayed table).
            std::vector<Queued> lc_events;
            for (const auto& e : host.events) if (e.delay == 0) lc_events.push_back(e);
            for (const auto& e : host.events) if (e.delay != 0) lc_events.push_back(e);
            bool same = lc_events.size() == native_events.size() && host.stimuli.size() == native_stimuli.size();
            for (std::size_t i = 0; same && i < lc_events.size(); ++i) {
                const Queued& a = lc_events[i];
                const Queued& b = native_events[i];
                same = a.source == b.source && a.target == b.target && a.event == b.event &&
                       a.argument == b.argument && a.delay == b.delay;
            }
            for (std::size_t i = 0; same && i < native_stimuli.size(); ++i) {
                same = std::memcmp(&host.stimuli[i], &native_stimuli[i], sizeof(QueuedStimulus)) == 0;
            }
            if (same && (which == 3 || which == 4)) {
                const void* lc_target = lc_context.target != nullptr ? host.native_of.at(lc_context.target) : nullptr;
                const void* lc_creature =
                    lc_context.source_creature != nullptr ? host.native_of.at(&lc_context.source_creature->skeleton())
                                                          : nullptr;
                same = *reinterpret_cast<void**>(native_context) == lc_target &&
                       *reinterpret_cast<void**>(native_context + 4) == lc_creature;
            }
            if (!same && first.empty()) {
                std::snprintf(text, sizeof text, "call %d %s: native %u events %u stimuli, lc %u events %u stimuli",
                              call, names[which], static_cast<unsigned>(native_events.size()),
                              static_cast<unsigned>(native_stimuli.size()), static_cast<unsigned>(lc_events.size()),
                              static_cast<unsigned>(host.stimuli.size()));
                first = text;
            }
        }
        for (auto& c : lc_creatures) c->skeleton().set_bounds_reference_object(nullptr);
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: %u creatures, %u rooms: %s\n", index, creature_count, room_count,
                        first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld calls, %ld events and %ld stimuli queued by native\n", calls, events_seen,
                stimuli_seen);
    std::printf("%ld cases: %ld differ%s\n", cases, differing,
                all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
