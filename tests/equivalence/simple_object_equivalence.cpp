// Function-level equivalence: simple-object pick-up, carrying and putting
// down of CE 10.3 against objects::SimpleObject.
//
//   SimpleObject::HandleQueuedEvent4 @00427cc0 (pick up)  handle_queued_event_4
//   SimpleObject::HandleQueuedEvent5 (put down) with
//     EndInteractionWithSource @00428bb0                  handle_queued_event_5
//   SimpleObject::Tick @00427a70, carried (mode 4):
//     UpdateEntityForExplicitRectBoundsAndRedraw @00428d30 tick
//   SimpleObject::FinalizeObjectEdit @00426ee0            finalize_object_edit
//   with UpdateMovementBounds, FindNearestMapRoomBoundsAtPoint and
//   FindTopmostOverlappingObject @00425c30 as native code.
//
// The native objects are real native objects with the native vtable of
// their class (SimpleObject 004579b8, Vehicle 004576c4, Lift 0045760c),
// except slots 22 and 23 (MoveBy/MoveToAndRedraw), which are the native
// MoveBy and MoveTo without the redraw.  SetBoundsMode (004257f0) only
// stores the mode: its renderable-set upkeep is not compared.  The carried
// tick returns once the object is placed (the jump at 00428ea7 skips the
// redraw).  Script dispatch (00419e00) is a recorder on both sides.
//
// Each case: a room table; the object under test with random carry flags
// (hand 0x02, creature 0x01, vehicle-proof 0x20, room-bound 0x40), plane
// and position; two other simple objects, one of them sometimes carried by
// creature A; two creatures (A, B) with random body plane, sprite bounds,
// right-hand chain end and facing; two vehicles (lift-like genus 3 2, or a
// plain vehicle) with random parts, planes and local bounds; and the hand.
// Then 40 operations: pick up by the hand, A, B or an object; put down by
// the hand, A or B; a tick while carried; the edit drop at a random
// privilege; and moves of the creatures and vehicles.  After each, every
// simple object's mode, reference, flags, position, plane and movement
// bounds, the scripts run and the queued events and stimuli must match.
//
// Not generated: a machine as carrier (native reads a pointer byte for the
// plane), the hand's own tick (row 29, mouse input), image sequences.

#include "native_image.hpp"

#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "creatures/events.hpp"
#include "objects/compound_object.hpp"
#include "objects/simple_object.hpp"
#include "objects/vehicle.hpp"
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
using objects::Object;

constexpr std::uint32_t kNativeDebugLog = 0x00418000, kExecuteScript = 0x00419e00,
                        kDirtyQueue = 0x00412610, kUpdateSound = 0x00426190,
                        kSetBoundsMode = 0x004257f0, kAdvanceImage = 0x00415580;
constexpr std::uint32_t kEvent4 = 0x00427cc0, kEvent5Slot = 9, kTickSlot = 41,
                        kFinalizeEdit = 0x00426ee0;
constexpr std::uint32_t kSimpleVtable = 0x004579b8, kVehicleVtable = 0x004576c4, kLiftVtable = 0x0045760c;
constexpr std::uint32_t kMapData = 0x00468d64, kEditObject = 0x00468d68, kDocument = 0x00468d84,
                        kPrivilegeDocument = 0x00468d80, kPointerTool = 0x00467dbc;
constexpr std::uint32_t kRegistryData = 0x004704a8, kRegistryCount = 0x004704ac;
constexpr std::uint32_t kRingWrite = 0x00466470, kRingRead = 0x0046646c, kRingStart = 0x0046cf20;
constexpr std::uint32_t kStimulusWrite = 0x00466468, kStimulusRead = 0x00466464, kStimulusStart = 0x0046f4a0;
constexpr std::size_t kEventSize = 0x18, kStimulusSize = 0x14;

constexpr std::size_t kClassifier = 0x4, kBoundsMode = 0x8, kBoundsFlags = 0x9, kMovementBounds = 0xc,
                      kBoundsReference = 0x1c, kSimpleEntity = 0x50, kSavedPlane = 0x54;
constexpr std::size_t kParts = 0x54, kPartStride = 0xc, kPartCount = 0x50, kLocalBounds = 0x154;
constexpr std::size_t kBody = 0x5c, kFacing = 0x78, kChainEndX = 0xcc, kChainEndY = 0xe4,
                      kSpriteBounds = 0xfc, kContexts = 0x26f0, kBlockSize = 0x2d00;
constexpr std::size_t kEntitySize = 0x40, kImageStride = 0x28;
constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14;

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

std::map<const void*, int> g_names;
std::vector<std::string> g_native_scripts, g_lc_scripts;
std::string script_text(const void* owner, const void* from, std::uint32_t classifier) {
    char text[64];
    std::snprintf(text, sizeof text, "%d<-%d %08x", g_names.count(owner) ? g_names[owner] : -1,
                  g_names.count(from) ? g_names[from] : -1, classifier);
    return text;
}
extern "C" int __cdecl native_execute_script(void* owner, void* from, std::uint32_t classifier, int) {
    g_native_scripts.push_back(script_text(owner, from, classifier));
    return 1;
}
__asm__(".globl _native_execute_script_thunk\n"
        "_native_execute_script_thunk:\n"
        "    pushl 8(%esp)\n"
        "    pushl 8(%esp)\n"
        "    pushl %edx\n"
        "    pushl %ecx\n"
        "    call _native_execute_script\n"
        "    addl $16, %esp\n"
        "    ret\n");
extern "C" void native_execute_script_thunk();
void __attribute__((thiscall)) native_dirty(void*, int, int, int, int) {}
void __attribute__((thiscall)) native_set_bounds_mode(std::uint8_t* self, std::uint32_t mode) {
    self[kBoundsMode] = static_cast<std::uint8_t>(mode);
}
using NativeMove = void(__attribute__((thiscall)) *)(void*, int, int);
NativeMove g_simple_move_by = nullptr, g_simple_move_to = nullptr, g_compound_move_by = nullptr;
void __attribute__((thiscall)) simple_move_by(void* self, int dx, int dy) { g_simple_move_by(self, dx, dy); }
void __attribute__((thiscall)) simple_move_to(void* self, int x, int y) { g_simple_move_to(self, x, y); }
void __attribute__((thiscall)) compound_move_by(void* self, int dx, int dy) { g_compound_move_by(self, dx, dy); }
void* g_simple_vtable[64];
void* g_vehicle_vtable[64];
void* g_lift_vtable[64];
void* g_creature_vtable[64];
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called on a native creature\n");
    std::exit(5);
}

struct NativeRect {
    std::int32_t left, top, right, bottom;
};
int __attribute__((stdcall)) point_in_rect(const NativeRect* r, std::int32_t x, std::int32_t y) {
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}
int __attribute__((stdcall)) set_rect_empty(NativeRect* r) {
    *r = {0, 0, 0, 0};
    return 1;
}

// ---- LC hosts, as the game's ----
class Hosts final : public objects::SimpleObjectTickHost,
                    public objects::SimpleObjectInteractionHost,
                    public objects::SimpleObjectEditHost,
                    public creatures::CreatureEventFanoutHost {
public:
    std::vector<world::MapRoom>* rooms = nullptr;
    std::vector<Object*> registry;
    std::vector<creatures::Creature*> creatures;
    objects::SimpleObject* hand = nullptr;
    int pointer_x = 0, pointer_y = 0, privilege = 0;
    Object* edit = nullptr;
    std::vector<std::string> events;
    std::vector<std::string> stimuli;

    // Sound: muted.
    world::ViewportBounds sound_viewport() const override { return {}; }
    bool sounds_muted() const override { return true; }
    sound::SoundManager& sound_manager() override { std::exit(5); }
    bool debug_console_available() const override { return false; }
    void log_sound_event(LogEvent, sound::SoundId, int) override {}
    // Renderable set: not compared.
    bool contains(const Object&) const override { return false; }
    void insert(Object&) override {}
    void erase(Object&) override {}
    // Scripts.
    int execute_script_for_classifier(Object& owner, Object* from, std::uint32_t classifier, bool) override {
        g_lc_scripts.push_back(script_text(&owner, from, classifier));
        return 1;
    }
    void redraw_image_sequence_change(objects::Entity&, const world::WorldRect&, const world::WorldRect&) override {}
    void redraw_after_simple_object_move(objects::SimpleObject&, const world::WorldRect&,
                                         const world::WorldRect&) override {}
    void present_or_queue_dirty_world_rect(const world::WorldRect&) override {}
    // The hand.
    bool pointer_input_pending() const override { return false; }
    void process_pending_pointer_input(objects::SimpleObject&) override {}
    objects::SimpleObject* pointer_tool() const override { return hand; }
    int pointer_mouse_world_x() const override { return pointer_x; }
    int pointer_mouse_world_y() const override { return pointer_y; }
    int pointer_world_x() const override { return pointer_x; }
    int pointer_world_y() const override { return pointer_y; }
    // Carrying (the game's WindowsSimpleObjectInteractionHost; creatures only).
    creatures::Creature* creature_for(const Object& o) const {
        for (auto* c : creatures) if (&c->skeleton() == &o) return c;
        return nullptr;
    }
    bool carried_object_anchor(const Object& reference, int& x, int& y) const override {
        const creatures::Creature* c = creature_for(reference);
        if (c == nullptr) return false;
        x = c->skeleton().limb_chain_end_x[4];
        y = c->skeleton().limb_chain_end_y[4];
        return true;
    }
    int carried_object_render_plane_offset(const Object& reference) const override {
        static constexpr int kOffsets[4] = {-1, 1, 1, -1};
        const creatures::Creature* c = creature_for(reference);
        return c != nullptr ? kOffsets[static_cast<std::size_t>(c->skeleton().facing_direction) & 3u] : 1;
    }
    // Movement bounds.
    void clear_edit_object() override { edit = nullptr; }
    void find_nearest_room_bounds_at_point(int x, int y, world::WorldRect& out) const override {
        world::MapRectangle m{out.min_x, out.min_y, out.max_x, out.max_y};
        world::find_nearest_room_bounds_at_point({rooms->data(), rooms->size()}, x, y, m);
        out = {m.left, m.top, m.right, m.bottom};
    }
    world::WorldRect vehicle_local_bounds(const Object& v) const override {
        return static_cast<const objects::Vehicle&>(v).creature_event_bounds_local;
    }
    int vehicle_primary_entity_x(const Object& v) const override {
        return static_cast<const objects::Vehicle&>(v).part(0).entity->world_x();
    }
    int vehicle_primary_entity_y(const Object& v) const override {
        return static_cast<const objects::Vehicle&>(v).part(0).entity->world_y();
    }
    Object* edit_object() const override { return edit; }
    // Overlap search.
    std::size_t object_count() const override { return registry.size(); }
    Object* object_at(std::size_t i) const override { return registry[i]; }
    void report_invalid_index() const override {}
    bool is_pointer_tool(const Object& o) const override { return &o == hand; }
    world::WorldRect pointer_tool_bounds(const Object&) const override { return {}; }
    world::WorldRect vehicle_interaction_bounds(const Object& o) const override {
        const auto& v = static_cast<const objects::Vehicle&>(o);
        world::WorldRect b = v.creature_event_bounds_local;
        const int x = v.part(0).entity->world_x(), y = v.part(0).entity->world_y();
        return {b.min_x + x, b.min_y + y, b.max_x + x, b.max_y + y};
    }
    std::size_t non_scenery_object_count() const override { return registry.size(); }
    Object* non_scenery_object_at(std::size_t i) const override { return registry[i]; }
    void report_invalid_non_scenery_index() const override {}
    void queue_immediate_event(Object& source, Object& target, objects::ObjectEventId id,
                               std::uint32_t argument) override {
        char t[64];
        std::snprintf(t, sizeof t, "%d->%d e%u a%u", g_names.count(&source) ? g_names[&source] : -1,
                      g_names.count(&target) ? g_names[&target] : -1, static_cast<unsigned>(id), argument);
        events.push_back(t);
    }
    int privilege_level() const override { return privilege; }
    // Creature fan-out (for the pick-up's creature tests and stimulus).
    std::size_t creature_count() const override { return creatures.size(); }
    creatures::Creature* creature_at(std::size_t i) const override { return creatures[i]; }
    void report_invalid_creature_index() const override {}
    int source_sound_source_x(const Object& s) const override { return s.sound_source_x(); }
    int source_sound_source_y(const Object& s) const override { return s.sound_source_y(); }
    int down_foot_x(const creatures::Creature& c) const override { return c.skeleton().down_foot_x; }
    int down_foot_y(const creatures::Creature& c) const override { return c.skeleton().down_foot_y; }
    bool is_same_object(const creatures::Creature& c, const Object& o) const override { return &c.skeleton() == &o; }
    bool can_perceive(const creatures::Creature&, const Object&) const override { return false; }
    bool source_is_vehicle(const Object& s) const override { return s.has_bounds_flag(Object::kIsVehicle); }
    bool creature_is_bound_to_source(const creatures::Creature& c, const Object& s) const override {
        return c.skeleton().bounds_reference_object() == &s;
    }
    bool is_creature_classifier(const creatures::Creature& c) const override {
        return ((c.skeleton().classifier_base() >> 24) & 0xffu) == 4u;
    }
    Object& object_for_creature(creatures::Creature& c) const override { return c.skeleton(); }
    void queue_object_event(Object& source, Object& target, objects::ObjectEventId id, std::uint32_t argument,
                            std::int32_t) override {
        queue_immediate_event(source, target, id, argument);
    }
    bool copy_built_in_stimulus(const creatures::Creature& creature, std::int32_t index, Object& source,
                                objects::QueuedCreatureStimulus& out) const override {
        const auto& context = creature.built_in_stimulus_contexts()[static_cast<std::size_t>(index)];
        out.source_object = &source;
        out.target_creature = const_cast<creatures::Creature*>(&creature);
        out.descriptor = context.descriptor;
        out.chemical_ids = context.chemical_ids;
        out.chemical_amounts = context.chemical_amounts;
        return true;
    }
    void queue_creature_stimulus(const objects::QueuedCreatureStimulus& s) override {
        char t[96];
        std::uint32_t words[3];
        std::memcpy(words, &s.descriptor, 12);
        std::snprintf(t, sizeof t, "%d %d %08x %08x %08x",
                      g_names.count(s.source_object) ? g_names[s.source_object] : -1,
                      s.target_creature != nullptr ? g_names[&s.target_creature->skeleton()] : -1, words[0], words[1],
                      words[2]);
        stimuli.push_back(t);
    }
    void report_invalid_stimulus_index(std::int32_t) const override {}
};

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}
template <typename T>
T& global(equivalence::NativeImage& image, std::uint32_t address) {
    return *image.at<T*>(address);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x5180bu;
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
    if (!image.bind_import("!PtInRect", reinterpret_cast<void*>(&point_in_rect)) ||
        !image.bind_import("!SetRectEmpty", reinterpret_cast<void*>(&set_rect_empty))) {
        std::fprintf(stderr, "the exe does not import PtInRect and SetRectEmpty\n");
        return 2;
    }
    image.make_return(kNativeDebugLog);
    image.make_return(kUpdateSound);
    image.redirect(kExecuteScript, reinterpret_cast<const void*>(&native_execute_script_thunk));
    image.redirect(kDirtyQueue, reinterpret_cast<const void*>(&native_dirty));
    image.redirect(kSetBoundsMode, reinterpret_cast<const void*>(&native_set_bounds_mode));
    // The carried tick: placed and planed by 00428ea7; skip its redraw.
    image.redirect(0x00428ea7, image.at<const void*>(0x00429402));
    g_simple_move_by = image.at<NativeMove>(0x00426800);
    g_simple_move_to = image.at<NativeMove>(0x004267c0);
    g_compound_move_by = image.at<NativeMove>(0x0042aa10);
    std::memcpy(g_simple_vtable, image.at<void*>(kSimpleVtable), sizeof g_simple_vtable);
    std::memcpy(g_vehicle_vtable, image.at<void*>(kVehicleVtable), sizeof g_vehicle_vtable);
    std::memcpy(g_lift_vtable, image.at<void*>(kLiftVtable), sizeof g_lift_vtable);
    g_simple_vtable[22] = reinterpret_cast<void*>(&simple_move_by);
    g_simple_vtable[23] = reinterpret_cast<void*>(&simple_move_to);
    g_vehicle_vtable[22] = g_lift_vtable[22] = reinterpret_cast<void*>(&compound_move_by);
    for (auto& s : g_creature_vtable) s = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_creature_vtable[26] = image.at<void*>(0x00406d40);  // CopySpriteBounds
    g_creature_vtable[27] = image.at<void*>(0x00406d60);  // body plane
    g_creature_vtable[30] = image.at<void*>(0x00406d70);
    g_creature_vtable[31] = image.at<void*>(0x00406d80);
    std::vector<std::uint8_t> document(0x200, 0), view(0x80, 0), privilege_document(0x200, 0);
    field<void*>(document.data(), 0x14c) = view.data();
    global<void*>(image, kDocument) = document.data();
    global<void*>(image, kPrivilegeDocument) = privilege_document.data();
    std::vector<std::uint8_t> map_data(0x1000, 0);
    global<void*>(image, kMapData) = map_data.data();

    using Call0 = void(__attribute__((thiscall)) *)(void*);
    using Call1 = void(__attribute__((thiscall)) *)(void*, const void*);
    auto slot = [](std::uint8_t* object, int n) { return field<void**>(object, 0)[n]; };

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, operations = 0, pickups = 0, into_vehicle = 0, carried_ticks = 0;
    const char* names[] = {"hand picks up", "A picks up", "B picks up", "object picks up", "hand puts down",
                           "A puts down", "B puts down", "carried tick", "carried tick", "edit drop",
                           "creatures and vehicles move"};

    for (long index = 0; index < cases; ++index) {
        // ---- gallery ----
        display::Gallery lc_gallery;
        lc_gallery.image_count = 256;
        lc_gallery.images = std::make_unique<display::Image[]>(256);
        std::vector<std::uint8_t> native_images(256 * kImageStride, 0);
        std::vector<std::uint8_t> native_gallery(16, 0);
        field<void*>(native_gallery.data(), 4) = native_images.data();
        // Image count read by the carried tick (gallery + 8).
        field<std::int32_t>(native_gallery.data(), 8) = 256;
        for (int i = 0; i < 256; ++i) {
            const int w = random.range(10, 90), h = random.range(10, 90);
            lc_gallery.images[i].configure(&lc_gallery, w, h, 0);
            field<std::int32_t>(native_images.data() + i * kImageStride, 0x10) = w;
            field<std::int32_t>(native_images.data() + i * kImageStride, 0x14) = h;
        }
        // ---- rooms: a few storeys ----
        std::vector<world::MapRoom> rooms;
        const int base_x = random.range(600, 7600), base_y = random.range(200, 700);
        for (int r = 0, n = random.range(1, 6); r < n; ++r) {
            world::MapRoom room;
            const int left = base_x + random.range(-700, 100), top = base_y + random.range(-300, 300);
            room.bounds = {left, top, left + random.range(200, 1200), top + random.range(100, 300)};
            rooms.push_back(room);
        }
        std::memset(map_data.data(), 0, map_data.size());
        field<std::int32_t>(map_data.data(), kRoomCount) = static_cast<std::int32_t>(rooms.size());
        for (std::size_t r = 0; r < rooms.size(); ++r) {
            std::memcpy(map_data.data() + kRooms + r * kRoomStride, &rooms[r].bounds, 16);
        }
        global<void*>(image, kEditObject) = nullptr;

        Hosts hosts;
        hosts.rooms = &rooms;
        g_names.clear();
        std::vector<std::vector<std::uint8_t>> blocks;
        auto new_block = [&](std::size_t size) {
            blocks.emplace_back(size, 0);
            return blocks.back().data();
        };
        auto make_entity = [&](objects::Entity& lc, std::uint8_t* raw, int x, int y, int plane) {
            const std::uint8_t image_index = random.byte();
            lc.set_gallery(&lc_gallery);
            lc.set_image_index(image_index);
            lc.set_world_x(x);
            lc.set_world_y(y);
            lc.set_render_plane(plane);
            field<void*>(raw, 4) = native_gallery.data();
            raw[8] = image_index;
            field<std::int32_t>(raw, 0xc) = plane;
            field<std::int32_t>(raw, 0x10) = x;
            field<std::int32_t>(raw, 0x14) = y;
        };
        // ---- simple objects: 0 under test, 1 and 2 others, 3 the hand ----
        std::vector<std::unique_ptr<objects::SimpleObject>> lc_simple;
        std::vector<std::uint8_t*> native_simple, native_simple_entity;
        for (int s = 0; s < 4; ++s) {
            lc_simple.push_back(std::make_unique<objects::SimpleObject>());
            objects::SimpleObject& lc = *lc_simple.back();
            std::uint8_t* raw = new_block(0x200);
            field<void*>(raw, 0) = g_simple_vtable;
            // Object::Object @00425750 starts with the world rectangle, as LC does.
            field<world::WorldRect>(raw, kMovementBounds) = {0, 0, 0x20a0, 0x4b0};
            auto entity = std::make_unique<objects::Entity>();
            std::uint8_t* raw_entity = new_block(kEntitySize);
            const int plane = random.range(0, 5000);
            make_entity(*entity, raw_entity, base_x + random.range(-300, 300), base_y + random.range(-200, 200), plane);
            lc.set_entity(std::move(entity));
            field<void*>(raw, kSimpleEntity) = raw_entity;
            lc.saved_entity_render_plane = plane;
            field<std::int32_t>(raw, kSavedPlane) = plane;
            const std::uint32_t classifier = s == 3 ? 0x02010000u : 0x02000000u | (random.below(40) << 16);
            lc.set_classifier_base(classifier);
            field<std::uint32_t>(raw, kClassifier) = classifier;
            std::uint8_t flags = s == 3 ? 0 : static_cast<std::uint8_t>(random.below(4) | (random.chance(5) ? 0x20 : 0) |
                                                                         (random.chance(2) ? 0x40 : 0));
            lc.set_bounds_flags_for_script(flags);
            raw[kBoundsFlags] = flags;
            g_names[&lc] = s;
            g_names[raw] = s;
            native_simple.push_back(raw);
            native_simple_entity.push_back(raw_entity);
        }
        hosts.hand = lc_simple[3].get();
        global<void*>(image, kPointerTool) = native_simple[3];
        hosts.pointer_x = base_x + random.range(-200, 200);
        hosts.pointer_y = base_y + random.range(-200, 200);
        field<std::int32_t>(document.data(), 0x88) = hosts.pointer_x;
        field<std::int32_t>(document.data(), 0x8c) = hosts.pointer_y;
        // ---- creatures A and B ----
        std::vector<std::unique_ptr<creatures::Creature>> lc_creatures;
        std::vector<std::uint8_t*> native_creatures, native_bodies;
        for (int c = 0; c < 2; ++c) {
            lc_creatures.push_back(std::make_unique<creatures::Creature>());
            creatures::Creature& lc = *lc_creatures.back();
            auto& skeleton = lc.skeleton();
            std::uint8_t* raw = new_block(kBlockSize);
            field<void*>(raw, 0) = g_creature_vtable;
            skeleton.set_classifier_base(0x04010000u);
            field<std::uint32_t>(raw, kClassifier) = 0x04010000u;
            skeleton.body = creatures::create_body();
            std::uint8_t* raw_body = new_block(0xa0);
            const int plane = random.range(0, 5000);
            skeleton.body->set_render_plane(plane);
            field<std::int32_t>(raw_body, 0xc) = plane;
            field<void*>(raw, kBody) = raw_body;
            auto& context = lc.built_in_stimulus_contexts()[0];
            context.descriptor = {random.byte(), random.byte(), random.byte(), random.byte()};
            context.chemical_ids = {random.byte(), random.byte(), random.byte(), random.byte()};
            context.chemical_amounts = {random.byte(), random.byte(), random.byte(), random.byte()};
            std::memcpy(raw + kContexts + 8, &context.descriptor, 12);
            g_names[&skeleton] = 10 + c;
            g_names[raw] = 10 + c;
            native_creatures.push_back(raw);
            native_bodies.push_back(raw_body);
            hosts.creatures.push_back(&lc);
        }
        auto place_creature = [&](int c) {
            auto& skeleton = lc_creatures[c]->skeleton();
            std::uint8_t* raw = native_creatures[c];
            const int x = base_x + random.range(-300, 300), y = base_y + random.range(-200, 200);
            skeleton.sprite_bounds = {x, y, x + random.range(20, 80), y + random.range(30, 100)};
            field<world::WorldRect>(raw, kSpriteBounds) = skeleton.sprite_bounds;
            const int hx = x + random.range(-20, 80), hy = y + random.range(0, 90);
            skeleton.limb_chain_end_x[4] = hx;
            skeleton.limb_chain_end_y[4] = hy;
            field<std::int32_t>(raw, kChainEndX + 16) = hx;
            field<std::int32_t>(raw, kChainEndY + 16) = hy;
            const auto facing = static_cast<creatures::FacingDirection>(random.below(4));
            skeleton.facing_direction = facing;
            raw[kFacing] = static_cast<std::uint8_t>(facing);
            const world::WorldRect movement = {x - random.range(0, 500), y - random.range(0, 300),
                                               x + random.range(50, 500), y + random.range(50, 300)};
            skeleton.set_movement_bounds(movement);
            field<world::WorldRect>(raw, kMovementBounds) = movement;
        };
        place_creature(0);
        place_creature(1);
        // ---- vehicles ----
        std::vector<std::unique_ptr<objects::Vehicle>> lc_vehicles;
        std::vector<std::uint8_t*> native_vehicles;
        std::vector<std::vector<std::uint8_t*>> native_vehicle_parts;
        for (int v = 0; v < 2; ++v) {
            const bool lift_like = random.chance(2);
            lc_vehicles.push_back(std::make_unique<objects::Vehicle>());
            objects::Vehicle& lc = *lc_vehicles.back();
            std::uint8_t* raw = new_block(0x200);
            field<void*>(raw, 0) = lift_like ? g_lift_vtable : g_vehicle_vtable;
            const std::uint32_t classifier = lift_like ? 0x03020000u : 0x03000000u | (random.below(30) << 16);
            lc.set_classifier_base(classifier);
            field<std::uint32_t>(raw, kClassifier) = classifier;
            lc.set_bounds_flags_for_script(0x08);
            raw[kBoundsFlags] = 0x08;
            const int parts = 2;
            field<std::int32_t>(raw, kPartCount) = parts;
            native_vehicle_parts.emplace_back();
            const int vx = base_x + random.range(-300, 300), vy = base_y + random.range(-200, 200);
            for (int p = 0; p < parts; ++p) {
                auto entity = std::make_unique<objects::Entity>();
                std::uint8_t* raw_entity = new_block(kEntitySize);
                make_entity(*entity, raw_entity, vx + 10 * p, vy, random.range(0, 5000));
                lc.install_part(static_cast<std::size_t>(p), std::move(entity), 10 * p, 0);
                field<void*>(raw, kParts + p * kPartStride) = raw_entity;
                field<std::int32_t>(raw, kParts + p * kPartStride + 4) = 10 * p;
                native_vehicle_parts.back().push_back(raw_entity);
            }
            const world::WorldRect local = {random.range(-20, 20), random.range(-20, 20), random.range(30, 200),
                                            random.range(30, 160)};
            lc.creature_event_bounds_local = local;
            field<world::WorldRect>(raw, kLocalBounds) = local;
            g_names[&lc] = 20 + v;
            g_names[raw] = 20 + v;
            native_vehicles.push_back(raw);
        }
        // Object 2 is sometimes already carried by creature A.
        if (random.chance(2)) {
            lc_simple[2]->set_bounds_mode(4, hosts);
            lc_simple[2]->set_bounds_reference_object(&lc_creatures[0]->skeleton());
            native_simple[2][kBoundsMode] = 4;
            field<void*>(native_simple[2], kBoundsReference) = native_creatures[0];
        }
        // The registry: simple objects, vehicles and creatures.
        std::vector<std::uint32_t> registry;
        auto add = [&](Object* lc, std::uint8_t* raw) {
            hosts.registry.push_back(lc);
            registry.push_back(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(raw)));
        };
        for (int s = 0; s < 4; ++s) add(lc_simple[s].get(), native_simple[s]);
        for (int v = 0; v < 2; ++v) add(lc_vehicles[v].get(), native_vehicles[v]);
        for (int c = 0; c < 2; ++c) add(&lc_creatures[c]->skeleton(), native_creatures[c]);
        global<std::uint32_t*>(image, kRegistryData) = registry.data();
        global<std::int32_t>(image, kRegistryCount) = static_cast<std::int32_t>(registry.size());

        // Initial movement bounds, both sides.
        for (int s = 0; s < 3; ++s) {
            lc_simple[s]->update_movement_bounds(hosts);
            reinterpret_cast<Call0>(image.at<void*>(0x00426470))(native_simple[s]);
        }
        g_native_scripts.clear();
        g_lc_scripts.clear();

        std::string first;
        char text[256];
        objects::SimpleObject& lc = *lc_simple[0];
        std::uint8_t* native = native_simple[0];
        for (int op = 0; op < 40 && first.empty(); ++op, ++operations) {
            hosts.events.clear();
            hosts.stimuli.clear();
            global<std::uint32_t>(image, kRingWrite) = kRingStart;
            global<std::uint32_t>(image, kRingRead) = kRingStart;
            global<std::uint32_t>(image, kStimulusWrite) = kStimulusStart;
            global<std::uint32_t>(image, kStimulusRead) = kStimulusStart;
            int which = static_cast<int>(random.below(11));
            if ((which == 7 || which == 8) && lc.bounds_mode() == Object::BoundsMode::unbounded_1) which = 10;
            objects::QueuedObjectEvent lc_event;
            const void* native_source = nullptr;
            auto source = [&](int who) {
                if (who == 0) { lc_event.source = hosts.hand; native_source = native_simple[3]; }
                else if (who == 3) { lc_event.source = lc_simple[1].get(); native_source = native_simple[1]; }
                else { lc_event.source = &lc_creatures[who - 1]->skeleton(); native_source = native_creatures[who - 1]; }
            };
            std::uint32_t native_event[6] = {};
            switch (which) {
            case 0: case 1: case 2: case 3:
                source(which);
                native_event[0] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native_source));
                lc.handle_queued_event_4(lc_event, hosts, hosts);
                reinterpret_cast<Call1>(image.at<void*>(kEvent4))(native, native_event);
                break;
            case 4: case 5: case 6:
                source(which - 4);
                native_event[0] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native_source));
                lc.handle_queued_event_5(lc_event, hosts);
                reinterpret_cast<Call1>(slot(native, kEvent5Slot))(native, native_event);
                break;
            case 7: case 8:
                lc.tick(hosts);
                reinterpret_cast<Call0>(slot(native, kTickSlot))(native);
                break;
            case 9:
                hosts.privilege = random.range(0, 3);
                field<std::int32_t>(privilege_document.data(), 0xd0) = hosts.privilege;
                lc.finalize_object_edit(hosts);
                reinterpret_cast<Call0>(image.at<void*>(kFinalizeEdit))(native);
                break;
            default:
                place_creature(static_cast<int>(random.below(2)));
                if (random.chance(2)) {
                    const int v = static_cast<int>(random.below(2));
                    const int dx = random.range(-60, 60), dy = random.range(-60, 60);
                    lc_vehicles[v]->move_by(dx, dy);
                    g_compound_move_by(native_vehicles[v], dx, dy);
                }
                break;
            }
            if (lc.bounds_mode() == Object::BoundsMode::explicit_rectangle) ++pickups;
            if (lc.bounds_mode() == Object::BoundsMode::vehicle_local) ++into_vehicle;
            if ((which == 7 || which == 8) && lc.bounds_mode() == Object::BoundsMode::explicit_rectangle) ++carried_ticks;

            auto note = [&](const std::string& what) {
                if (first.empty()) {
                    std::snprintf(text, sizeof text, "op %d %s: ", op, names[which]);
                    first = text + what;
                }
            };
            for (int s = 0; s < 3; ++s) {
                const objects::SimpleObject& o = *lc_simple[s];
                std::uint8_t* raw = native_simple[s];
                std::uint8_t* raw_entity = native_simple_entity[s];
                const auto& nm = field<world::WorldRect>(raw, kMovementBounds);
                const auto& m = o.movement_bounds();
                const void* ref = o.bounds_reference_object();
                const void* native_ref = field<void*>(raw, kBoundsReference);
                const bool same_ref = (ref == nullptr && native_ref == nullptr) ||
                                      (ref != nullptr && native_ref != nullptr && g_names.count(ref) &&
                                       g_names.count(native_ref) && g_names[ref] == g_names[native_ref]);
                if (raw[kBoundsMode] != static_cast<std::uint8_t>(o.bounds_mode()) || !same_ref ||
                    field<std::int32_t>(raw_entity, 0x10) != o.entity()->world_x() ||
                    field<std::int32_t>(raw_entity, 0x14) != o.entity()->world_y() ||
                    field<std::int32_t>(raw_entity, 0xc) != o.entity()->render_plane() || nm.min_x != m.min_x ||
                    nm.min_y != m.min_y || nm.max_x != m.max_x || nm.max_y != m.max_y) {
                    std::snprintf(text, sizeof text,
                                  "object %d: native mode %u ref %d (%d,%d) plane %d mb {%d,%d,%d,%d}, lc mode %u ref "
                                  "%d (%d,%d) plane %d mb {%d,%d,%d,%d}",
                                  s, raw[kBoundsMode], g_names.count(native_ref) ? g_names[native_ref] : -1,
                                  field<std::int32_t>(raw_entity, 0x10), field<std::int32_t>(raw_entity, 0x14),
                                  field<std::int32_t>(raw_entity, 0xc), nm.min_x, nm.min_y, nm.max_x, nm.max_y,
                                  static_cast<unsigned>(o.bounds_mode()), g_names.count(ref) ? g_names[ref] : -1,
                                  o.entity()->world_x(), o.entity()->world_y(), o.entity()->render_plane(), m.min_x,
                                  m.min_y, m.max_x, m.max_y);
                    note(text);
                }
            }
            if (g_native_scripts != g_lc_scripts) {
                std::snprintf(text, sizeof text, "scripts: native %u [%s] lc %u [%s]",
                              static_cast<unsigned>(g_native_scripts.size()),
                              g_native_scripts.empty() ? "" : g_native_scripts.back().c_str(),
                              static_cast<unsigned>(g_lc_scripts.size()),
                              g_lc_scripts.empty() ? "" : g_lc_scripts.back().c_str());
                note(text);
            }
            std::vector<std::string> native_events;
            for (std::uint32_t at = kRingStart; at != global<std::uint32_t>(image, kRingWrite); at += kEventSize) {
                const std::uint8_t* e = image.at<const std::uint8_t*>(at);
                char t[64];
                const void* s = *reinterpret_cast<void* const*>(e);
                const void* d = *reinterpret_cast<void* const*>(e + 4);
                std::snprintf(t, sizeof t, "%d->%d e%u a%u", g_names.count(s) ? g_names[s] : -1,
                              g_names.count(d) ? g_names[d] : -1, *reinterpret_cast<const std::uint32_t*>(e + 8),
                              *reinterpret_cast<const std::uint32_t*>(e + 12));
                native_events.push_back(t);
            }
            if (native_events != hosts.events) {
                std::snprintf(text, sizeof text, "events: native %u [%s] lc %u [%s]",
                              static_cast<unsigned>(native_events.size()),
                              native_events.empty() ? "" : native_events[0].c_str(),
                              static_cast<unsigned>(hosts.events.size()),
                              hosts.events.empty() ? "" : hosts.events[0].c_str());
                note(text);
            }
            std::vector<std::string> native_stimuli;
            for (std::uint32_t at = kStimulusStart; at != global<std::uint32_t>(image, kStimulusWrite);
                 at += kStimulusSize) {
                const std::uint8_t* s = image.at<const std::uint8_t*>(at);
                char t[96];
                const void* a = *reinterpret_cast<void* const*>(s);
                const void* b = *reinterpret_cast<void* const*>(s + 4);
                std::snprintf(t, sizeof t, "%d %d %08x %08x %08x", g_names.count(a) ? g_names[a] : -1,
                              g_names.count(b) ? g_names[b] : -1, *reinterpret_cast<const std::uint32_t*>(s + 8),
                              *reinterpret_cast<const std::uint32_t*>(s + 12),
                              *reinterpret_cast<const std::uint32_t*>(s + 16));
                native_stimuli.push_back(t);
            }
            if (native_stimuli != hosts.stimuli) {
                std::snprintf(text, sizeof text, "stimuli: native %u lc %u", static_cast<unsigned>(native_stimuli.size()),
                              static_cast<unsigned>(hosts.stimuli.size()));
                note(text);
            }
        }
        for (auto& s : lc_simple) s->set_bounds_reference_object(nullptr);
        for (auto& c : lc_creatures) c->skeleton().set_bounds_reference_object(nullptr);

        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: flags %02x: %s\n", index, native[kBoundsFlags], first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld operations, %ld with the object carried by a creature, %ld in a vehicle, %ld carried "
                "ticks\n",
                operations, pickups, into_vehicle, carried_ticks);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
