// Function-level equivalence: vehicles, lifts and call buttons of CE 10.3
// against objects::Vehicle, Lift and CallButton.
//
//   Vehicle::Tick @0042bd90 (Lift::Tick @0042c4f0 calls it)  tick
//   Vehicle queued events 0/1/2 @0042c0b0/0042c170/0042c230  handle_queued_event_0..2
//   Lift::InitializeState @0042c320                          initialize_state
//   Lift::Tick @0042c4f0                                     Lift::tick
//   Lift::RequestMoveUp/Down @0042c5c0/0042c680              request_move_up/down
//   Lift::SelectNearestCallButtonAndStartMove @0042c590      select_nearest_call_button_and_start_move
//   CallButton::UpdateLiftStateAndQueueRedraw @00429a70      update_lift_state_and_queue_redraw
//   CallButton::RequestLiftCall @00429c10                    request_lift_call
//
// The native objects are real native objects: each block has the native
// vtable of its class (Lift 0045760c, Vehicle 004576c4, CallButton
// 00457be0, SimpleObject 004579b8), so every virtual call the functions
// make runs native code.  Script dispatch (ExecuteScriptForClassifier
// 00419e00), the renderer's dirty queue (00412610) and the sound update
// (00426190) are replaced on the native side; LC's hosts record the same
// script calls.
//
// A lift case: a lift of 1 to 3 parts, 2 to 6 floors stacked in rooms, one
// call button on each floor registered with UpdateLiftState, passengers
// (simple objects whose bounds reference is the lift), and a creature and
// an object that ask for the lift.  A vehicle case: a vehicle with
// movement-bounds collision (flag 0x40) or terrain collision (flag 0x80,
// on random ground heights), passengers, and the same two sources.  Then
// 200 (lift) or 80 (vehicle) random operations; after each, the vehicle (parts' positions,
// velocity, fixed-point position, collision sides, interaction, floors,
// call buttons, selection, cooldown), the passengers (position, movement
// bounds), the call buttons (interaction, floor), the scripts run, the
// queued object events and the queued stimuli must match.
//
// Not generated: part animation sequences, and vehicles beyond one world
// width (row 52).  Not compared: dirty rectangles.

#include "native_image.hpp"

#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "objects/call_button.hpp"
#include "objects/lift.hpp"
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
                        kLiftInitialize = 0x0042c320;
constexpr std::uint32_t kLiftVtable = 0x0045760c, kVehicleVtable = 0x004576c4,
                        kCallButtonVtable = 0x00457be0, kSimpleVtable = 0x004579b8;
constexpr std::uint32_t kMapData = 0x00468d64, kEditObject = 0x00468d68, kActiveLift = 0x00468d60;
constexpr std::uint32_t kRegistryData = 0x004704a8, kRegistryCount = 0x004704ac;
constexpr std::uint32_t kRingWrite = 0x00466470, kRingRead = 0x0046646c, kRingStart = 0x0046cf20;
constexpr std::uint32_t kStimulusWrite = 0x00466468, kStimulusRead = 0x00466464,
                        kStimulusStart = 0x0046f4a0;
constexpr std::size_t kEventSize = 0x18, kStimulusSize = 0x14;

// Native Object, SimpleObject, CompoundObject, Vehicle and Lift layouts.
constexpr std::size_t kClassifier = 0x4, kBoundsMode = 0x8, kBoundsFlags = 0x9, kMovementBounds = 0xc,
                      kBoundsReference = 0x1c, kInteraction = 0x24, kObjectVariable0 = 0x34,
                      kTimerPeriod = 0x44, kTimerCountdown = 0x48;
constexpr std::size_t kSimpleEntity = 0x50, kSimpleFlags = 0x5b, kButtonLift = 0x5c, kButtonFloor = 0x60;
constexpr std::size_t kPartCount = 0x50, kParts = 0x54, kPartStride = 0xc;
constexpr std::size_t kEventConfig = 0x12c;
constexpr std::size_t kVelocityX = 0x144, kVelocityY = 0x148, kPositionX = 0x14c, kPositionY = 0x150,
                      kLocalBounds = 0x154, kCollision = 0x164;
constexpr std::size_t kFloorY = 0x168, kFloorCount = 0x188, kCurrentFloor = 0x18c, kButtons = 0x190,
                      kSelected = 0x1b0, kCooldown = 0x1b4;
constexpr std::size_t kBlockSize = 0x2d00, kContexts = 0x26f0;
// Native Entity: gallery +4, image +8, base +9, plane +0xc, position +0x10/+0x14.
constexpr std::size_t kEntitySize = 0x40;
constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14, kGroundHeights = 0x32c;
constexpr std::size_t kImageStride = 0x28;

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

// ---- the script calls, recorded on both sides ----
std::map<const void*, int> g_names;  // object -> number, both sides
std::vector<std::string> g_native_scripts, g_lc_scripts;
std::string script_text(int owner, int from, std::uint32_t classifier) {
    char text[64];
    std::snprintf(text, sizeof text, "%d<-%d %08x", owner, from, classifier);
    return text;
}
extern "C" int __cdecl native_execute_script(void* owner, void* from, std::uint32_t classifier, int) {
    g_native_scripts.push_back(script_text(g_names.count(owner) ? g_names[owner] : -1,
                                           g_names.count(from) ? g_names[from] : -1, classifier));
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
// Slot 22 (MoveByAndRedraw) without the redraw: the native MoveBy
// (0042aa10).  Redrawing is not under test.
using NativeMoveBy = void(__attribute__((thiscall)) *)(void*, int, int);
NativeMoveBy g_native_compound_move_by = nullptr;
void __attribute__((thiscall)) native_move_by_no_redraw(void* self, int dx, int dy) {
    g_native_compound_move_by(self, dx, dy);
}
void* g_lift_vtable[64];
void* g_vehicle_vtable[64];

// ---- LC hosts, as the game's ----
class Hosts final : public objects::LiftRuntimeHost, public objects::CallButtonRuntimeHost {
public:
    std::vector<world::MapRoom>* rooms = nullptr;
    std::vector<std::int32_t>* ground = nullptr;
    std::vector<Object*> registry;
    Object* edit = nullptr;
    Object* active_lift = nullptr;
    std::vector<objects::QueuedObjectEvent> events;
    std::vector<objects::QueuedCreatureStimulus> stimuli;
    std::map<const Object*, creatures::Creature*> creature_of;

    // Sound: muted.
    world::ViewportBounds sound_viewport() const override { return {}; }
    bool sounds_muted() const override { return true; }
    sound::SoundManager& sound_manager() override { std::exit(5); }
    bool debug_console_available() const override { return false; }
    void log_sound_event(LogEvent, sound::SoundId, int) override {}
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
    // Scripts.
    int execute_script_for_classifier(Object& owner, Object* from, std::uint32_t classifier, bool) override {
        g_lc_scripts.push_back(script_text(g_names.count(&owner) ? g_names[&owner] : -1,
                                           g_names.count(from) ? g_names[from] : -1, classifier));
        return 1;
    }
    // Registry.
    void add_object(Object*) override {}
    std::size_t object_count() const override { return registry.size(); }
    Object* object_at(std::size_t i) const override { return registry[i]; }
    void remove_object_at(std::size_t) override {}
    void report_invalid_index() const override {}
    // Rendering: not compared.
    void redraw_image_sequence_change(objects::Entity&, const world::WorldRect&, const world::WorldRect&) override {}
    void redraw_after_compound_object_move(objects::CompoundObject&, const world::WorldRect&,
                                           const world::WorldRect&) override {}
    void queue_compound_object_dirty_rect(objects::CompoundObject&, const world::WorldRect&) override {}
    bool contains(const Object&) const override { return false; }
    void insert(Object&) override {}
    void erase(Object&) override {}
    // Map.
    int ground_height_at_x_block(std::size_t block) const override { return (*ground)[block]; }
    std::size_t room_count() const override { return rooms->size(); }
    bool room_contains_point(std::size_t i, int x, int y) const override {
        const auto& b = (*rooms)[i].bounds;
        return x >= b.left && x < b.right && y >= b.top && y < b.bottom;
    }
    int room_bottom(std::size_t i) const override { return (*rooms)[i].bounds.bottom; }
    // Events and stimuli.
    bool source_is_creature(const Object& source) const override {
        return ((source.classifier_base() >> 24) & 0xffu) == 4u;
    }
    int dispatch_script_event(objects::CompoundObject& target, Object* source,
                              objects::ObjectEventId event_id) override {
        return target.dispatch_script_event(event_id, source, false, *this);
    }
    bool copy_built_in_stimulus(const Object& source, Object& target, std::uint32_t index,
                                objects::QueuedCreatureStimulus& out) const override {
        const auto it = creature_of.find(&source);
        if (it == creature_of.end()) return false;
        const auto& context = it->second->built_in_stimulus_contexts()[index];
        out.source_object = &target;
        out.target_creature = it->second;
        out.descriptor = context.descriptor;
        out.chemical_ids = context.chemical_ids;
        out.chemical_amounts = context.chemical_amounts;
        return true;
    }
    void queue_creature_stimulus(const objects::QueuedCreatureStimulus& s) override { stimuli.push_back(s); }
    void queue_immediate_event(Object& source, Object& target, objects::ObjectEventId id,
                               std::uint32_t argument) override {
        objects::QueuedObjectEvent e;
        e.source = &source;
        e.target = &target;
        e.event_id = id;
        e.argument = argument;
        events.push_back(e);
    }
    void publish_active_lift(objects::Lift& lift) override { active_lift = &lift; }
    int call_button_floor(const objects::CallButton& b) const override { return b.floor_index; }
    void deactivate_call_button(objects::CallButton& b) override { b.deactivate(*this); }
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
    std::uint64_t seed = 0x11f7u;
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
    struct NativeRect { std::int32_t left, top, right, bottom; };
    static auto point_in_rect = [](const NativeRect* r, std::int32_t x, std::int32_t y) -> int {
        return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
    };
    struct Imports {
        static int __attribute__((stdcall)) pt_in_rect(const NativeRect* r, std::int32_t x, std::int32_t y) {
            return point_in_rect(r, x, y);
        }
        static int __attribute__((stdcall)) set_rect_empty(NativeRect* r) {
            *r = {0, 0, 0, 0};
            return 1;
        }
    };
    if (!image.bind_import("!PtInRect", reinterpret_cast<void*>(&Imports::pt_in_rect)) ||
        !image.bind_import("!SetRectEmpty", reinterpret_cast<void*>(&Imports::set_rect_empty))) {
        std::fprintf(stderr, "the exe does not import PtInRect and SetRectEmpty\n");
        return 2;
    }
    image.make_return(kNativeDebugLog);
    image.make_return(kUpdateSound);
    image.redirect(kExecuteScript, reinterpret_cast<const void*>(&native_execute_script_thunk));
    image.redirect(kDirtyQueue, reinterpret_cast<const void*>(&native_dirty));
    image.make_return(0x0042b2b0);  // CompleteFloorArrival's primary-part redraw
    g_native_compound_move_by = image.at<NativeMoveBy>(0x0042aa10);
    std::memcpy(g_lift_vtable, image.at<void*>(kLiftVtable), sizeof g_lift_vtable);
    std::memcpy(g_vehicle_vtable, image.at<void*>(kVehicleVtable), sizeof g_vehicle_vtable);
    g_lift_vtable[22] = reinterpret_cast<void*>(&native_move_by_no_redraw);
    g_vehicle_vtable[22] = reinterpret_cast<void*>(&native_move_by_no_redraw);
    std::vector<std::uint8_t> document(0x200, 0);
    *image.at<void**>(0x00468d84) = document.data();

    using Call0 = void(__attribute__((thiscall)) *)(void*);
    using Call1 = void(__attribute__((thiscall)) *)(void*, const void*);
    auto slot = [](std::uint8_t* object, int n) { return field<void**>(object, 0)[n]; };
    auto call0 = [&](std::uint8_t* object, int n) { reinterpret_cast<Call0>(slot(object, n))(object); };
    auto call1 = [&](std::uint8_t* object, int n, const void* a) {
        reinterpret_cast<Call1>(slot(object, n))(object, a);
    };

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, operations = 0, arrivals = 0, collisions = 0, carried = 0;
    std::vector<std::uint8_t> map_data(0x1000, 0);
    global<void*>(image, kMapData) = map_data.data();
    const char* names[] = {"tick", "tick", "tick", "select call", "request up", "request down",
                           "button call", "set velocity", "event 0", "event 1", "event 2"};

    for (long index = 0; index < cases; ++index) {
        const bool is_lift = !random.chance(3);
        // ---- gallery: 256 images ----
        display::Gallery lc_gallery;
        lc_gallery.image_count = 256;
        lc_gallery.images = std::make_unique<display::Image[]>(256);
        std::vector<std::uint8_t> native_images(256 * kImageStride, 0);
        std::vector<std::uint8_t> native_gallery(16, 0);
        field<void*>(native_gallery.data(), 4) = native_images.data();
        for (int i = 0; i < 256; ++i) {
            const int w = random.range(10, 120), h = random.range(10, 120);
            lc_gallery.images[i].configure(&lc_gallery, w, h, 0);
            field<std::int32_t>(native_images.data() + i * kImageStride, 0x10) = w;
            field<std::int32_t>(native_images.data() + i * kImageStride, 0x14) = h;
        }

        // ---- the map: rooms stacked in a shaft, ground heights ----
        std::vector<world::MapRoom> rooms;
        std::vector<std::int32_t> ground(261);
        const int shaft_x = random.range(400, 7600);
        const int floors = is_lift ? random.range(2, 6) : 0;
        const int storey = random.range(150, 300);
        for (int f = 0; f < floors; ++f) {
            world::MapRoom room;
            const int top = 100 + f * storey;
            room.bounds = {shaft_x - 400, top, shaft_x + 400, top + storey};
            room.room_type = random.below(3);
            rooms.push_back(room);
        }
        for (auto& g : ground) g = random.range(600, 1100);
        std::memset(map_data.data(), 0, map_data.size());
        field<std::int32_t>(map_data.data(), kRoomCount) = static_cast<std::int32_t>(rooms.size());
        for (std::size_t r = 0; r < rooms.size(); ++r) {
            std::memcpy(map_data.data() + kRooms + r * kRoomStride, &rooms[r].bounds, 16);
            field<std::uint32_t>(map_data.data() + kRooms + r * kRoomStride, 16) = rooms[r].room_type;
        }
        for (std::size_t g = 0; g < ground.size(); ++g) field<std::int32_t>(map_data.data(), kGroundHeights + 4 * g) = ground[g];
        global<void*>(image, kEditObject) = nullptr;
        global<void*>(image, kActiveLift) = nullptr;

        Hosts hosts;
        hosts.rooms = &rooms;
        hosts.ground = &ground;
        g_names.clear();
        std::vector<std::vector<std::uint8_t>> native_blocks;
        auto new_block = [&](std::size_t size, std::uint32_t vtable) {
            native_blocks.emplace_back(size, 0);
            std::uint8_t* b = native_blocks.back().data();
            field<std::uint32_t>(b, 0) = vtable;
            return b;
        };
        auto make_entity = [&](objects::Entity& lc, std::uint8_t* raw, int x, int y) {
            const std::uint8_t image_index = random.byte();
            lc.set_gallery(&lc_gallery);
            lc.set_image_index_base(0);
            lc.set_image_index(image_index);
            lc.set_world_x(x);
            lc.set_world_y(y);
            field<void*>(raw, 4) = native_gallery.data();
            raw[8] = image_index;
            field<std::int32_t>(raw, 0x10) = x;
            field<std::int32_t>(raw, 0x14) = y;
        };

        // ---- the vehicle or lift ----
        std::unique_ptr<objects::Vehicle> lc_vehicle;
        objects::Lift* lc_lift = nullptr;
        if (is_lift) {
            auto lift = std::make_unique<objects::Lift>();
            lc_lift = lift.get();
            lc_vehicle = std::move(lift);
        } else {
            lc_vehicle = std::make_unique<objects::Vehicle>();
        }
        objects::Vehicle& lc = *lc_vehicle;
        std::uint8_t* native = new_block(kBlockSize, 0);
        field<void*>(native, 0) = is_lift ? g_lift_vtable : g_vehicle_vtable;
        g_names[&lc] = 0;
        g_names[native] = 0;
        const std::uint32_t classifier = is_lift ? 0x03020000u | (random.below(256) << 8)
                                                 : 0x03000000u | (random.below(30) << 16) | (random.below(256) << 8);
        lc.set_classifier_base(classifier);
        field<std::uint32_t>(native, kClassifier) = classifier;
        const int parts = random.range(1, 3);
        std::vector<std::uint8_t*> native_entities;
        field<std::int32_t>(native, kPartCount) = parts;
        const int base_x = is_lift ? shaft_x - 60 : random.range(200, 8000);
        const int base_y = is_lift ? 100 + random.below(static_cast<std::uint32_t>(floors)) * storey + storey - 130
                                   : random.range(200, 700);
        for (int p = 0; p < parts; ++p) {
            auto entity = std::make_unique<objects::Entity>();
            std::uint8_t* raw = new_block(kEntitySize, 0);
            const int dx = p == 0 ? 0 : random.range(-40, 40), dy = p == 0 ? 0 : random.range(-40, 40);
            make_entity(*entity, raw, base_x + dx, base_y + dy);
            lc.install_part(static_cast<std::size_t>(p), std::move(entity), dx, dy);
            field<void*>(native, kParts + p * kPartStride) = raw;
            field<std::int32_t>(native, kParts + p * kPartStride + 4) = dx;
            field<std::int32_t>(native, kParts + p * kPartStride + 8) = dy;
            native_entities.push_back(raw);
        }
        for (std::size_t c = 0; c < 3; ++c) {
            const int value = random.chance(2) ? -1 : random.range(0, 5);
            lc.creature_event_config.event_config_value[c] = value;
            field<std::int32_t>(native, kEventConfig + 4 * c) = value;
        }
        const world::WorldRect local = {0, 0, random.range(40, 120), random.range(60, 130)};
        lc.creature_event_bounds_local = local;
        field<world::WorldRect>(native, kLocalBounds) = local;
        if (!is_lift) {
            const std::uint8_t flags = static_cast<std::uint8_t>(0x08 | (random.chance(2) ? 0x40 : 0x80));
            lc.set_bounds_flags_for_script(flags);
            native[kBoundsFlags] = flags;
            const world::WorldRect movement = {base_x - random.range(0, 600), base_y - random.range(0, 300),
                                               base_x + random.range(60, 700), base_y + random.range(60, 400)};
            lc.set_movement_bounds(movement);
            field<world::WorldRect>(native, kMovementBounds) = movement;
        } else {
            lc.set_bounds_flags_for_script(0x08);
            native[kBoundsFlags] = 0x08;
        }
        if (random.chance(3)) {
            const int period = random.range(1, 5);
            lc.set_timer_for_script(period);
            field<std::int32_t>(native, kTimerPeriod) = period;
            field<std::int32_t>(native, kTimerCountdown) = period;
        }
        // Fixed-point position as the game keeps it.
        lc.position_x_8_8 = base_x << 8;
        lc.position_y_8_8 = base_y << 8;
        field<std::int32_t>(native, kPositionX) = base_x << 8;
        field<std::int32_t>(native, kPositionY) = base_y << 8;

        // ---- call buttons, one per floor ----
        std::vector<std::unique_ptr<objects::CallButton>> lc_buttons;
        std::vector<std::uint8_t*> native_buttons;
        if (is_lift) {
            hosts.edit = nullptr;
            lc_lift->initialize_state(hosts);
            reinterpret_cast<Call0>(image.at<void*>(kLiftInitialize))(native);
            for (int f = 0; f < floors; ++f) {
                lc_buttons.push_back(std::make_unique<objects::CallButton>());
                objects::CallButton& b = *lc_buttons.back();
                std::uint8_t* raw = new_block(0x200, kCallButtonVtable);
                auto entity = std::make_unique<objects::Entity>();
                std::uint8_t* raw_entity = new_block(kEntitySize, 0);
                make_entity(*entity, raw_entity, shaft_x + 100, 100 + f * storey + storey - 60);
                b.set_entity(std::move(entity));
                field<void*>(raw, kSimpleEntity) = raw_entity;
                b.set_classifier_base(0x02040000u);
                field<std::uint32_t>(raw, kClassifier) = 0x02040000u;
                const std::uint8_t flags = random.chance(2) ? 1 : 0;
                b.interaction_event_flags = flags;
                raw[kSimpleFlags] = flags;
                b.set_lift(lc_lift);
                field<void*>(raw, kButtonLift) = native;
                raw[kButtonFloor] = 0xff;
                g_names[&b] = 10 + f;
                g_names[raw] = 10 + f;
                native_buttons.push_back(raw);
            }
        }
        // ---- passengers ----
        std::vector<std::unique_ptr<objects::SimpleObject>> lc_passengers;
        std::vector<std::uint8_t*> native_passengers;
        std::vector<std::uint32_t> registry;
        for (int p = 0, n = random.range(0, 3); p < n; ++p) {
            lc_passengers.push_back(std::make_unique<objects::SimpleObject>());
            objects::SimpleObject& s = *lc_passengers.back();
            std::uint8_t* raw = new_block(0x200, kSimpleVtable);
            auto entity = std::make_unique<objects::Entity>();
            std::uint8_t* raw_entity = new_block(kEntitySize, 0);
            make_entity(*entity, raw_entity, base_x + random.range(0, 60), base_y + random.range(0, 60));
            s.set_entity(std::move(entity));
            field<void*>(raw, kSimpleEntity) = raw_entity;
            s.set_classifier_base(0x02000000u | (random.below(40) << 16));
            field<std::uint32_t>(raw, kClassifier) = s.classifier_base();
            const bool rides = !random.chance(4);
            s.set_bounds_reference_object(rides ? &lc : nullptr);
            field<void*>(raw, kBoundsReference) = rides ? native : nullptr;
            const std::uint8_t mode = rides ? 3 : 0;
            s.set_bounds_mode(mode, hosts);
            raw[kBoundsMode] = mode;
            g_names[&s] = 20 + p;
            g_names[raw] = 20 + p;
            lc_passengers.back()->update_movement_bounds(hosts);
            reinterpret_cast<Call0>(image.at<void*>(0x00426470))(raw);
            hosts.registry.push_back(&s);
            registry.push_back(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(raw)));
            native_passengers.push_back(raw);
        }
        hosts.registry.push_back(&lc);
        registry.push_back(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native)));
        global<std::uint32_t*>(image, kRegistryData) = registry.data();
        global<std::int32_t>(image, kRegistryCount) = static_cast<std::int32_t>(registry.size());

        // ---- sources: a creature and an object ----
        auto creature = std::make_unique<creatures::Creature>();
        std::uint8_t* native_creature = new_block(kBlockSize, 0);
        creature->skeleton().set_classifier_base(0x04010000u);
        field<std::uint32_t>(native_creature, kClassifier) = 0x04010000u;
        {
            auto& c0 = creature->built_in_stimulus_contexts()[0];
            c0.descriptor = {random.byte(), random.byte(), random.byte(), random.byte()};
            c0.chemical_ids = {random.byte(), random.byte(), random.byte(), random.byte()};
            c0.chemical_amounts = {random.byte(), random.byte(), random.byte(), random.byte()};
            std::memcpy(native_creature + kContexts + 8, &c0.descriptor, 12);
        }
        hosts.creature_of[&creature->skeleton()] = creature.get();
        g_names[&creature->skeleton()] = 30;
        g_names[native_creature] = 30;
        objects::SimpleObject lc_other;
        lc_other.set_classifier_base(0x02050000u);
        std::uint8_t* native_other = new_block(0x200, kSimpleVtable);
        field<std::uint32_t>(native_other, kClassifier) = 0x02050000u;
        g_names[&lc_other] = 31;
        g_names[native_other] = 31;

        // Register the call buttons, in floor order.
        g_native_scripts.clear();
        g_lc_scripts.clear();
        auto reset_queues = [&]() {
            hosts.events.clear();
            hosts.stimuli.clear();
            global<std::uint32_t>(image, kRingWrite) = kRingStart;
            global<std::uint32_t>(image, kRingRead) = kRingStart;
            global<std::uint32_t>(image, kStimulusWrite) = kStimulusStart;
            global<std::uint32_t>(image, kStimulusRead) = kStimulusStart;
        };
        reset_queues();
        if (std::getenv("EQUIVALENCE_TRACE")) {
            std::fprintf(stderr, "case %ld: register %u buttons\n", index, static_cast<unsigned>(lc_buttons.size()));
            std::fflush(stderr);
        }
        for (std::size_t b = 0; b < lc_buttons.size(); ++b) {
            lc_buttons[b]->update_lift_state_and_queue_redraw(hosts);
            call0(native_buttons[b], 13);
        }

        std::string first;
        char text[256];
        auto native_of = [&](const Object* o) -> const void* {
            if (o == nullptr) return nullptr;
            if (o == &lc) return native;
            if (o == &creature->skeleton()) return native_creature;
            if (o == &lc_other) return native_other;
            for (std::size_t i = 0; i < lc_buttons.size(); ++i) if (o == lc_buttons[i].get()) return native_buttons[i];
            for (std::size_t i = 0; i < lc_passengers.size(); ++i) if (o == lc_passengers[i].get()) return native_passengers[i];
            return reinterpret_cast<const void*>(1);
        };
        for (int op = -1; op < (is_lift ? 200 : 80) && first.empty(); ++op, ++operations) {
            int which = -1;
            const std::uint32_t state_before = lc.current_interaction_event_id();
            if (op >= 0) {
                reset_queues();
                which = static_cast<int>(random.below(11));
                if (is_lift && random.chance(2)) which = 0;  // lifts need many ticks to arrive
                if (!is_lift && which >= 3 && which <= 6) which = 7;
                const bool from_creature = random.chance(2);
                objects::QueuedObjectEvent lc_event;
                lc_event.source = from_creature ? static_cast<Object*>(&creature->skeleton()) : &lc_other;
                std::uint32_t native_event[6] = {static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                                                     from_creature ? native_creature : native_other)),
                                                 0, 0, 0, 0, 0};
                if (std::getenv("EQUIVALENCE_TRACE")) {
                    std::fprintf(stderr, "case %ld op %d %s\n", index, op, names[which]);
                    std::fflush(stderr);
                }
                switch (which) {
                case 0: case 1: case 2:
                    if (is_lift) { lc_lift->tick(hosts); } else { lc.tick(hosts); }
                    call0(native, 41);
                    break;
                case 3: lc_lift->select_nearest_call_button_and_start_move(hosts); call0(native, 42); break;
                case 4: lc_lift->request_move_up(lc_event, hosts); call1(native, 5, native_event); break;
                case 5: lc_lift->request_move_down(lc_event, hosts); call1(native, 6, native_event); break;
                case 6: {
                    const std::size_t b = random.below(static_cast<std::uint32_t>(lc_buttons.size()));
                    lc_buttons[b]->request_lift_call(lc_event, hosts);
                    call1(native_buttons[b], 5, native_event);
                    break;
                }
                case 7: {
                    const int vx = is_lift ? 0 : random.range(-900, 900);
                    const int vy = random.chance(4) ? 0 : (is_lift ? (random.chance(2) ? 1 : -1) * random.range(128, 900)
                                                                   : random.range(-900, 900));
                    lc.velocity_x_8_8 = vx;
                    lc.velocity_y_8_8 = vy;
                    field<std::int32_t>(native, kVelocityX) = vx;
                    field<std::int32_t>(native, kVelocityY) = vy;
                    if (random.chance(6)) {  // obv0, which holds a lift still
                        const std::uint32_t v = random.below(2);
                        lc.set_object_variable(0, v);
                        field<std::uint32_t>(native, kObjectVariable0) = v;
                    }
                    break;
                }
                case 8:
                    if (is_lift) { lc_lift->request_move_up(lc_event, hosts); call1(native, 5, native_event); }
                    else { lc.handle_queued_event_0(lc_event, hosts); call1(native, 5, native_event); }
                    break;
                case 9:
                    if (is_lift) { lc_lift->request_move_down(lc_event, hosts); call1(native, 6, native_event); }
                    else { lc.handle_queued_event_1(lc_event, hosts); call1(native, 6, native_event); }
                    break;
                default:
                    if (is_lift) { lc_lift->select_nearest_call_button_and_start_move(hosts); call0(native, 42); }
                    else { lc.handle_queued_event_2(lc_event, hosts, hosts); call1(native, 7, native_event); }
                    break;
                }
            }
            // A started lift: its script sets the speed toward the floor, as
            // the game's lift scripts do (both sides, before the next tick).
            if (is_lift && state_before == 0 && lc.current_interaction_event_id() != 0) {
                const int cabin_y = lc.sound_source_y() + lc.creature_event_bounds_local.max_y;
                const int floor_y = lc_lift->floor_y_by_index[static_cast<std::size_t>(lc_lift->current_floor_index)];
                const int speed = random.chance(4) ? random.range(128, 768) : random.range(1, 3) * 256;
                const int vy = floor_y > cabin_y ? speed : -speed;
                lc.velocity_y_8_8 = vy;
                field<std::int32_t>(native, kVelocityY) = vy;
            }
            auto note = [&](const std::string& what) {
                if (first.empty()) {
                    std::snprintf(text, sizeof text, "op %d %s: ", op, op < 0 ? "register buttons" : names[which]);
                    first = text + what;
                }
            };
            // ---- the vehicle ----
            for (int p = 0; p < parts; ++p) {
                const objects::Entity& e = *lc.part(static_cast<std::size_t>(p)).entity;
                if (field<std::int32_t>(native_entities[p], 0x10) != e.world_x() ||
                    field<std::int32_t>(native_entities[p], 0x14) != e.world_y()) {
                    std::snprintf(text, sizeof text, "part %d: native (%d,%d) lc (%d,%d)", p,
                                  field<std::int32_t>(native_entities[p], 0x10),
                                  field<std::int32_t>(native_entities[p], 0x14), e.world_x(), e.world_y());
                    note(text);
                }
            }
            if (field<std::int32_t>(native, kVelocityX) != lc.velocity_x_8_8 ||
                field<std::int32_t>(native, kVelocityY) != lc.velocity_y_8_8 ||
                field<std::int32_t>(native, kPositionX) != lc.position_x_8_8 ||
                field<std::int32_t>(native, kPositionY) != lc.position_y_8_8 ||
                field<std::uint32_t>(native, kCollision) != static_cast<std::uint32_t>(lc.collision_side_flags) ||
                native[kInteraction] != lc.current_interaction_event_id()) {
                std::snprintf(text, sizeof text,
                              "vehicle: native v(%d,%d) p(%d,%d) side %u act %u, lc v(%d,%d) p(%d,%d) side %u act %u",
                              field<std::int32_t>(native, kVelocityX), field<std::int32_t>(native, kVelocityY),
                              field<std::int32_t>(native, kPositionX), field<std::int32_t>(native, kPositionY),
                              field<std::uint32_t>(native, kCollision), native[kInteraction],
                              lc.velocity_x_8_8, lc.velocity_y_8_8,
                              lc.position_x_8_8, lc.position_y_8_8,
                              static_cast<unsigned>(lc.collision_side_flags), lc.current_interaction_event_id());
                note(text);
            }
            if (is_lift) {
                bool same = field<std::int32_t>(native, kFloorCount) == lc_lift->floor_count &&
                            field<std::int32_t>(native, kCurrentFloor) == lc_lift->current_floor_index &&
                            field<std::int32_t>(native, kSelected) == lc_lift->selected_call_button_index &&
                            native[kCooldown] == lc_lift->call_selection_cooldown;
                for (std::size_t s = 0; s < 8; ++s) {
                    same = same && field<std::int32_t>(native, kFloorY + 4 * s) == lc_lift->floor_y_by_index[s] &&
                           field<const void*>(native, kButtons + 4 * s) == native_of(lc_lift->call_button_refs[s]);
                }
                if (!same) {
                    std::snprintf(text, sizeof text, "lift: native floor %d/%d sel %d cool %u, lc floor %d/%d sel %d cool %u",
                                  field<std::int32_t>(native, kCurrentFloor), field<std::int32_t>(native, kFloorCount),
                                  field<std::int32_t>(native, kSelected), native[kCooldown],
                                  lc_lift->current_floor_index, lc_lift->floor_count,
                                  lc_lift->selected_call_button_index, lc_lift->call_selection_cooldown);
                    note(text);
                }
                if ((global<void*>(image, kActiveLift) == native) != (hosts.active_lift == lc_lift)) note("active lift");
                for (std::size_t b = 0; b < lc_buttons.size(); ++b) {
                    if (native_buttons[b][kInteraction] != lc_buttons[b]->current_interaction_event_id() ||
                        native_buttons[b][kButtonFloor] != lc_buttons[b]->floor_index) {
                        std::snprintf(text, sizeof text, "button %u: native act %u floor %u, lc act %u floor %u",
                                      static_cast<unsigned>(b), native_buttons[b][kInteraction],
                                      native_buttons[b][kButtonFloor], lc_buttons[b]->current_interaction_event_id(),
                                      lc_buttons[b]->floor_index);
                        note(text);
                    }
                }
            }
            for (std::size_t p = 0; p < lc_passengers.size(); ++p) {
                const objects::Entity& e = *lc_passengers[p]->entity();
                std::uint8_t* raw_entity = field<std::uint8_t*>(native_passengers[p], kSimpleEntity);
                const auto& nm = field<world::WorldRect>(native_passengers[p], kMovementBounds);
                const auto& m = lc_passengers[p]->movement_bounds();
                if (field<std::int32_t>(raw_entity, 0x10) != e.world_x() ||
                    field<std::int32_t>(raw_entity, 0x14) != e.world_y() || nm.min_x != m.min_x ||
                    nm.min_y != m.min_y || nm.max_x != m.max_x || nm.max_y != m.max_y) {
                    std::snprintf(text, sizeof text, "passenger %u: native (%d,%d) lc (%d,%d)", static_cast<unsigned>(p),
                                  field<std::int32_t>(raw_entity, 0x10), field<std::int32_t>(raw_entity, 0x14),
                                  e.world_x(), e.world_y());
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
            // Queued events and stimuli.
            std::vector<std::string> native_events, lc_events;
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
            for (const auto& e : hosts.events) {
                char t[64];
                std::snprintf(t, sizeof t, "%d->%d e%u a%u", g_names.count(e.source) ? g_names[e.source] : -1,
                              g_names.count(e.target) ? g_names[e.target] : -1, static_cast<unsigned>(e.event_id),
                              e.argument);
                lc_events.push_back(t);
            }
            if (native_events != lc_events) {
                std::snprintf(text, sizeof text, "events: native %u lc %u", static_cast<unsigned>(native_events.size()),
                              static_cast<unsigned>(lc_events.size()));
                note(text);
            }
            std::vector<std::string> native_stimuli, lc_stimuli;
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
            for (const auto& s : hosts.stimuli) {
                char t[96];
                std::uint32_t words[3];
                std::memcpy(words, &s.descriptor, 12);
                std::snprintf(t, sizeof t, "%d %d %08x %08x %08x",
                              g_names.count(s.source_object) ? g_names[s.source_object] : -1,
                              s.target_creature != nullptr ? g_names[&s.target_creature->skeleton()] : -1, words[0],
                              words[1], words[2]);
                lc_stimuli.push_back(t);
            }
            if (native_stimuli != lc_stimuli) {
                std::snprintf(text, sizeof text, "stimuli: native %u lc %u", static_cast<unsigned>(native_stimuli.size()),
                              static_cast<unsigned>(lc_stimuli.size()));
                note(text);
            }
            if (lc.collision_side_flags != objects::VehicleCollisionSide::none) ++collisions;
            if (is_lift && lc_lift->call_selection_cooldown != 0) ++arrivals;
            if (!lc_passengers.empty() && (which >= 0 && which <= 2)) ++carried;
        }
        for (auto& p : lc_passengers) p->set_bounds_reference_object(nullptr);
        if (is_lift) lc_lift->call_button_refs.fill(nullptr);
        for (auto& b : lc_buttons) b->set_lift(nullptr);

        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: %s, %d floors, %d parts: %s\n", index, is_lift ? "lift" : "vehicle", floors, parts,
                        first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld operations, %ld after an arrival, %ld with a collision side, %ld ticks with passengers\n",
                operations, arrivals, collisions, carried);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
