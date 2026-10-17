// Function-level equivalence: the map geometry of CE 10.3 against LC.
//
//   FindNearestMapRoomBoundsAtPoint @00422d70 - world::find_nearest_room_bounds_at_point
//   MapData::FindRoomContainingPoint @00422df0 - world::find_room_containing_point
//   GetAmbientTemperatureAtPoint @00422e40     - world::get_ambient_temperature_at_point
//   Object::UpdateMovementBounds @00426470     - objects::Object::update_movement_bounds
//
// Each case makes a room table (0 to 40 rooms) in the native MapData layout
// (count +8, rooms from +0xc, 0x14 bytes each: left, top, right, bottom,
// type) at the MapData pointer 0x468d64, and the same table for LC.  Rooms
// are mostly a grid of shared edges, as the game's rooms are, with gaps,
// overlaps and empty rooms.  Then 32 points, many on room edges, go to the
// three room functions, and 8 objects of random bounds mode, bounds flags,
// bounds and reference go to UpdateMovementBounds.  FindNearest starts from
// a random rectangle each time, because it changes the caller's rectangle in
// place.  The vehicle of mode 3 has its local bounds at +0x154 and its
// primary entity's position at +0x54 -> +0x10/+0x14 on the native side; LC's
// host gives the same values.  LC's host for find_nearest is the game's
// (C1WindowsDocument): it passes the caller's rectangle in.
//
// The rooms, the result rectangle, the room found, the temperature and the
// movement bounds must match.
//
// Not generated: mode 3 without a vehicle (native reads through null).

#include "native_image.hpp"

#include "common/compat.hpp"
#include "objects/object.hpp"
#include "platform/mfc_adapters.hpp"
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

constexpr std::uint32_t kNativeFindNearest = 0x00422d70;
constexpr std::uint32_t kNativeFindRoom = 0x00422df0;
constexpr std::uint32_t kNativeTemperature = 0x00422e40;
constexpr std::uint32_t kNativeUpdateMovementBounds = 0x00426470;
constexpr std::uint32_t kNativeDebugLog = 0x00418000;
constexpr std::uint32_t kMapData = 0x00468d64, kEditObject = 0x00468d68;

constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14;
constexpr std::size_t kAmbientIndex = 0xbf0, kMapDataSize = 0x1000;
constexpr std::size_t kMaxRooms = 40;
// A native object: bounds mode +8, bounds flags +9, movement bounds +0xc,
// bounds reference +0x1c; a vehicle's local bounds +0x154 and primary
// entity +0x54 (position +0x10, +0x14).
constexpr std::size_t kObjectSize = 0x200;
constexpr std::size_t kBoundsMode = 0x8, kBoundsFlags = 0x9, kMovementBounds = 0xc,
                      kBoundsReference = 0x1c, kVehicleLocal = 0x154, kVehicleEntity = 0x54;
constexpr std::size_t kEntityX = 0x10, kEntityY = 0x14;

struct Rect {
    std::int32_t left, top, right, bottom;
};

using NativeFindNearest = void(__attribute__((stdcall)) *)(int x, int y, Rect* out);
using NativeFindRoom = const void*(__attribute__((thiscall)) *)(const void* map, int x, int y);
using NativeTemperature = int(__attribute__((stdcall)) *)(int x, int y);
using NativeUpdateMovementBounds = void(__attribute__((thiscall)) *)(void* object);

int __attribute__((stdcall)) point_in_rect(const Rect* r, std::int32_t x, std::int32_t y) {
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint32_t below(std::uint32_t n) { return n == 0 ? 0 : next() % n; }
    bool chance(std::uint32_t one_in) { return below(one_in) == 0; }
    int range(int low, int high) { return low + static_cast<int>(below(static_cast<std::uint32_t>(high - low + 1))); }
};

// ---- native objects: GetBounds (slot 26) reads a table ----
std::map<const void*, world::WorldRect> g_native_bounds;
void __attribute__((thiscall)) native_get_bounds(void* self, world::WorldRect* out) {
    *out = g_native_bounds.at(self);
}
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called on a native object\n");
    std::exit(5);
}
void* g_vtable[64];

// ---- LC side ----
class HitTest final : public platform::RectangleHitTestApi {
public:
    bool contains(const platform::Rectangle& r, std::int32_t x, std::int32_t y) const override {
        // The game's C1WindowsDocument::contains: PtInRect.
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }
};

class BoundedObject final : public objects::Object {
public:
    world::WorldRect bounds{};
    bool get_bounds(world::WorldRect* out) const override {
        *out = bounds;
        return true;
    }
    void put_bounds_flags(std::uint8_t flags) { set_bounds_flags(flags); }
};

class NoRenderables final : public objects::ObjectRenderableSetHost {
public:
    bool contains(const objects::Object&) const override { return false; }
    void insert(objects::Object&) override {}
    void erase(objects::Object&) override {}
};

class MovementHost final : public objects::ObjectMovementBoundsHost {
public:
    world::MapRoomTable rooms;
    std::map<const objects::Object*, world::WorldRect> vehicle_local;
    std::map<const objects::Object*, std::pair<int, int>> vehicle_entity;
    objects::Object* edit = nullptr;

    void clear_edit_object() override { edit = nullptr; }
    // As C1WindowsDocument::find_nearest_room_bounds_at_point.
    void find_nearest_room_bounds_at_point(int x, int y, world::WorldRect& out) const override {
        world::MapRectangle map_bounds{out.min_x, out.min_y, out.max_x, out.max_y};
        world::find_nearest_room_bounds_at_point(rooms, x, y, map_bounds);
        out = {map_bounds.left, map_bounds.top, map_bounds.right, map_bounds.bottom};
    }
    world::WorldRect vehicle_local_bounds(const objects::Object& v) const override {
        return vehicle_local.at(&v);
    }
    int vehicle_primary_entity_x(const objects::Object& v) const override {
        return vehicle_entity.at(&v).first;
    }
    int vehicle_primary_entity_y(const objects::Object& v) const override {
        return vehicle_entity.at(&v).second;
    }
    objects::Object* edit_object() const override { return edit; }
};

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

bool same(const Rect& a, const world::MapRectangle& b) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}
bool same(const world::WorldRect& a, const world::WorldRect& b) {
    return a.min_x == b.min_x && a.min_y == b.min_y && a.max_x == b.max_x && a.max_y == b.max_y;
}

// Rooms: a grid of shared edges, as the game's rooms are, with gaps,
// overlaps and some empty or inverted rooms.
std::vector<world::MapRoom> make_rooms(Random& random) {
    std::vector<world::MapRoom> rooms;
    const std::uint32_t count = random.chance(20) ? 0 : 1 + random.below(kMaxRooms);
    const int column = 200 + static_cast<int>(random.below(800));
    const int row = 150 + static_cast<int>(random.below(300));
    for (std::uint32_t r = 0; r < count; ++r) {
        world::MapRoom room;
        const int cx = static_cast<int>(random.below(12)), cy = static_cast<int>(random.below(6));
        room.bounds = {cx * column, cy * row, (cx + 1 + static_cast<int>(random.below(2))) * column,
                       (cy + 1) * row};
        switch (random.below(6)) {
        case 0:  // shrunk: a gap
            room.bounds.left += random.range(1, 40);
            room.bounds.right -= random.range(1, 40);
            break;
        case 1:  // stretched: an overlap
            room.bounds.top -= random.range(1, 60);
            room.bounds.right += random.range(1, 60);
            break;
        case 2:
            if (random.chance(4)) room.bounds.right = room.bounds.left;  // empty
            else if (random.chance(4)) std::swap(room.bounds.top, room.bounds.bottom);  // inverted
            break;
        default:
            break;
        }
        room.room_type = random.below(4);
        rooms.push_back(room);
    }
    return rooms;
}

std::pair<int, int> make_point(Random& random, const std::vector<world::MapRoom>& rooms) {
    if (rooms.empty() || random.chance(4)) {
        return {random.range(-50, 8402), random.range(-50, 1250)};
    }
    const auto& b = rooms[random.below(static_cast<std::uint32_t>(rooms.size()))].bounds;
    const int xs[] = {b.left, b.right, b.left - 1, b.right - 1, b.right + 1,
                      b.left + (b.right - b.left) / 2};
    const int ys[] = {b.top, b.bottom, b.top - 1, b.bottom - 1, b.bottom + 1,
                      b.top + (b.bottom - b.top) / 2, b.top - random.range(0, 400)};
    return {xs[random.below(6)] + (random.chance(3) ? random.range(-30, 30) : 0),
            ys[random.below(7)] + (random.chance(3) ? random.range(-30, 30) : 0)};
}

world::WorldRect random_rect(Random& random) {
    const int x = random.range(-100, 8400), y = random.range(-100, 1200);
    return {x, y, x + random.range(0, 300), y + random.range(0, 300)};
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x6e0e7au;
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
    const auto native_find_nearest = image.at<NativeFindNearest>(kNativeFindNearest);
    const auto native_find_room = image.at<NativeFindRoom>(kNativeFindRoom);
    const auto native_temperature = image.at<NativeTemperature>(kNativeTemperature);
    const auto native_update_bounds = image.at<NativeUpdateMovementBounds>(kNativeUpdateMovementBounds);
    for (auto& slot : g_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_vtable[26] = reinterpret_cast<void*>(&native_get_bounds);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    std::vector<std::uint8_t> map_data(kMapDataSize, 0);
    *image.at<std::uint8_t**>(kMapData) = map_data.data();
    const world::AmbientEnvironmentTable environment{world::kAmbientEnvironmentRecords.data(),
                                                     world::kAmbientEnvironmentRecords.size()};
    const HitTest hit_test;
    NoRenderables renderables;
    long differing = 0, points = 0, no_room_spans = 0, objects_tested = 0;
    for (long index = 0; index < cases; ++index) {
        // ---- the room table, both sides ----
        const std::vector<world::MapRoom> rooms = make_rooms(random);
        std::memset(map_data.data(), 0, map_data.size());
        field<std::int32_t>(map_data.data(), kRoomCount) = static_cast<std::int32_t>(rooms.size());
        for (std::size_t r = 0; r < rooms.size(); ++r) {
            std::uint8_t* raw = map_data.data() + kRooms + r * kRoomStride;
            std::memcpy(raw, &rooms[r].bounds, 16);
            field<std::uint32_t>(raw, 16) = rooms[r].room_type;
        }
        const std::uint32_t ambient = random.below(5);
        field<std::uint32_t>(map_data.data(), kAmbientIndex) = ambient;
        const world::MapRoomTable table{rooms.data(), rooms.size()};

        std::string first;
        auto note = [&](const std::string& what) { if (first.empty()) first = what; };
        char text[256];

        // ---- the three room functions ----
        for (int p = 0; p < 32; ++p, ++points) {
            const auto [x, y] = make_point(random, rooms);
            const world::WorldRect start = random_rect(random);
            Rect native_rect{start.min_x, start.min_y, start.max_x, start.max_y};
            world::MapRectangle lc_rect{start.min_x, start.min_y, start.max_x, start.max_y};
            native_find_nearest(x, y, &native_rect);
            world::find_nearest_room_bounds_at_point(table, x, y, lc_rect);
            if (native_rect.bottom == 9999) ++no_room_spans;
            if (!same(native_rect, lc_rect)) {
                std::snprintf(text, sizeof text,
                              "nearest at (%d,%d): native {%d,%d,%d,%d} lc {%d,%d,%d,%d}", x, y,
                              native_rect.left, native_rect.top, native_rect.right, native_rect.bottom,
                              lc_rect.left, lc_rect.top, lc_rect.right, lc_rect.bottom);
                note(text);
            }

            const void* native_room = native_find_room(map_data.data(), x, y);
            const world::MapRoom* lc_room = world::find_room_containing_point(table, x, y, hit_test);
            const long native_index =
                native_room == nullptr
                    ? -1
                    : (static_cast<const std::uint8_t*>(native_room) - (map_data.data() + kRooms)) /
                          static_cast<long>(kRoomStride);
            const long lc_index = lc_room == nullptr ? -1 : lc_room - rooms.data();
            if (native_index != lc_index) {
                std::snprintf(text, sizeof text, "room at (%d,%d): native %ld lc %ld", x, y,
                              native_index, lc_index);
                note(text);
            }

            const int native_temperature_value = native_temperature(x, y);
            const int lc_temperature =
                world::get_ambient_temperature_at_point(table, ambient, environment, x, y, hit_test);
            if (native_temperature_value != lc_temperature) {
                std::snprintf(text, sizeof text, "temperature at (%d,%d) ambient %u: native %d lc %d", x,
                              y, ambient, native_temperature_value, lc_temperature);
                note(text);
            }
        }

        // ---- UpdateMovementBounds ----
        MovementHost host;
        host.rooms = table;
        g_native_bounds.clear();
        std::vector<std::unique_ptr<BoundedObject>> lc_objects;
        std::vector<std::vector<std::uint8_t>> native_objects;
        std::vector<std::vector<std::uint8_t>> entities;
        for (int o = 0; o < 8; ++o) {
            lc_objects.push_back(std::make_unique<BoundedObject>());
            native_objects.emplace_back(kObjectSize, 0);
            entities.emplace_back(0x40, 0);
        }
        for (int o = 0; o < 8; ++o) {
            BoundedObject& lc = *lc_objects[o];
            std::uint8_t* native = native_objects[o].data();
            field<void*>(native, 0) = g_vtable;
            // Bounds: often standing on or near a room floor.
            world::WorldRect bounds = random_rect(random);
            if (!rooms.empty() && random.chance(2)) {
                const auto [x, y] = make_point(random, rooms);
                bounds = {x, y - random.range(10, 120), x + random.range(10, 120), y};
            }
            lc.bounds = bounds;
            g_native_bounds[native] = bounds;
            // Vehicle data, used when another object is in mode 3 on this one.
            const world::WorldRect local = random_rect(random);
            const int ex = random.range(-200, 8000), ey = random.range(-200, 1200);
            host.vehicle_local[&lc] = local;
            host.vehicle_entity[&lc] = {ex, ey};
            field<world::WorldRect>(native, kVehicleLocal) = local;
            field<void*>(native, kVehicleEntity) = entities[o].data();
            field<std::int32_t>(entities[o].data(), kEntityX) = ex;
            field<std::int32_t>(entities[o].data(), kEntityY) = ey;
            const world::WorldRect movement = random_rect(random);
            lc.set_movement_bounds(movement);
            field<world::WorldRect>(native, kMovementBounds) = movement;
        }
        const int edit = static_cast<int>(random.below(12)) - 4;
        host.edit = edit >= 0 ? lc_objects[edit].get() : nullptr;
        *image.at<void**>(kEditObject) = edit >= 0 ? native_objects[edit].data() : nullptr;
        for (int o = 0; o < 8; ++o, ++objects_tested) {
            BoundedObject& lc = *lc_objects[o];
            std::uint8_t* native = native_objects[o].data();
            const std::uint32_t mode = random.below(5);
            int reference = static_cast<int>(random.below(10)) - 2;
            if (reference == o) reference = -1;
            if (mode == 3 && reference < 0) reference = (o + 1) % 8;
            const std::uint8_t flags = static_cast<std::uint8_t>(random.below(256));
            lc.set_bounds_mode(mode, renderables);
            lc.put_bounds_flags(flags);
            lc.set_bounds_reference_object(reference >= 0 ? lc_objects[reference].get() : nullptr);
            native[kBoundsMode] = static_cast<std::uint8_t>(mode);
            native[kBoundsFlags] = flags;
            field<void*>(native, kBoundsReference) =
                reference >= 0 ? native_objects[reference].data() : nullptr;
            lc.update_movement_bounds(host);
            native_update_bounds(native);
            const auto& native_movement = field<world::WorldRect>(native, kMovementBounds);
            if (!same(native_movement, lc.movement_bounds())) {
                const auto& m = lc.movement_bounds();
                std::snprintf(text, sizeof text,
                              "object %d mode %u flags %02x ref %d: native {%d,%d,%d,%d} lc {%d,%d,%d,%d}",
                              o, mode, flags, reference, native_movement.min_x, native_movement.min_y,
                              native_movement.max_x, native_movement.max_y, m.min_x, m.min_y, m.max_x,
                              m.max_y);
                note(text);
            }
        }
        for (auto& lc : lc_objects) lc->set_bounds_reference_object(nullptr);

        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: %u rooms, ambient %u: %s\n", index, static_cast<unsigned>(rooms.size()),
                        ambient, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld points (%ld with no room spanning x), %ld objects\n", points,
                no_room_spans, objects_tested);
    std::printf("%ld cases: %ld differ%s\n", cases, differing,
                all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
