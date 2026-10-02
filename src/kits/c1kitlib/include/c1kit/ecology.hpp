#pragma once

// The Ecology Kit's view of the world: rooms, creatures and the things in
// each room, from the LibreCreatures `dde: ecol` reply and plain CAOS.
// Header-only and portable.
//
// `dde: ecol` (LibreCreatures; `dde: dcap` bit 2) answers "%d|" fields:
//   room count, then per room: left, top, right, bottom, type (1 takes the
//     outdoor temperature), and the temperature at its centre;
//   creature count, then per creature: handle (for `targ`), down-foot x and
//     y, family, genus, species, sex (1 male, 2 female), dead, infected,
//     selected (each 0 or 1).
// C1's temperature is coarse: rooms other than type 1 are always 0, and
// type 1 rooms all share the outdoor value (-1, 0 or 1).

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace c1kit {

constexpr int kDdeCapabilityEcology = 2;  // `dde: dcap` bit for `dde: ecol`
constexpr int kWorldWidth = 8352;
constexpr int kWorldHeight = 1200;

struct EcologyRoom {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int type = 0;
    int temperature = 0;
};

struct EcologyCreature {
    std::uint32_t handle = 0;
    int x = 0;
    int y = 0;
    int family = 0;
    int genus = 0;
    int species = 0;
    int sex = 0;
    bool dead = false;
    bool infected = false;
    bool selected = false;
};

struct EcologySnapshot {
    std::vector<EcologyRoom> rooms;
    std::vector<EcologyCreature> creatures;
};

// False when the reply is short or its counts do not fit.
inline bool parse_ecology(const std::string& reply, EcologySnapshot& out) {
    out = EcologySnapshot();
    std::vector<long> fields;
    const char* cursor = reply.c_str();
    while (*cursor != '\0') {
        char* end = nullptr;
        const long value = std::strtol(cursor, &end, 10);
        if (end == cursor) {
            break;
        }
        fields.push_back(value);
        cursor = *end == '|' ? end + 1 : end;
    }
    std::size_t at = 0;
    const auto next = [&fields, &at](long& value) {
        if (at >= fields.size()) {
            return false;
        }
        value = fields[at++];
        return true;
    };
    long count = 0;
    if (!next(count) || count < 0 || count > 1000) {
        return false;
    }
    for (long i = 0; i < count; ++i) {
        long f[6];
        for (long& value : f) {
            if (!next(value)) {
                return false;
            }
        }
        out.rooms.push_back({static_cast<int>(f[0]), static_cast<int>(f[1]),
                             static_cast<int>(f[2]), static_cast<int>(f[3]),
                             static_cast<int>(f[4]), static_cast<int>(f[5])});
    }
    if (!next(count) || count < 0 || count > 1000) {
        return false;
    }
    for (long i = 0; i < count; ++i) {
        long f[10];
        for (long& value : f) {
            if (!next(value)) {
                return false;
            }
        }
        EcologyCreature creature;
        creature.handle = static_cast<std::uint32_t>(f[0]);
        creature.x = static_cast<int>(f[1]);
        creature.y = static_cast<int>(f[2]);
        creature.family = static_cast<int>(f[3]);
        creature.genus = static_cast<int>(f[4]);
        creature.species = static_cast<int>(f[5]);
        creature.sex = static_cast<int>(f[6]);
        creature.dead = f[7] != 0;
        creature.infected = f[8] != 0;
        creature.selected = f[9] != 0;
        out.creatures.push_back(creature);
    }
    return true;
}

// The first room that holds the point (as the game's own room lookup), or
// -1.  x is taken round the world's width first.
inline int room_at(const std::vector<EcologyRoom>& rooms, int x, int y) {
    x %= kWorldWidth;
    if (x < 0) {
        x += kWorldWidth;
    }
    for (std::size_t i = 0; i < rooms.size(); ++i) {
        const EcologyRoom& room = rooms[i];
        if (x >= room.left && x <= room.right && y >= room.top && y <= room.bottom) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// What the map can colour its rooms by.
enum class EcologyLayer : int { none, temperature, crowding, food, toys, disease, count };

inline const char* layer_name(EcologyLayer layer) {
    switch (layer) {
    case EcologyLayer::none: return "Rooms only";
    case EcologyLayer::temperature: return "Temperature";
    case EcologyLayer::crowding: return "Crowding";
    case EcologyLayer::food: return "Food and drink";
    case EcologyLayer::toys: return "Toys and instruments";
    case EcologyLayer::disease: return "Disease";
    default: return "";
    }
}

// The things the kit counts per room, each found with one CAOS loop:
// `enum 2 <genus> 0,dde: putv posl,dde: putv posr,dde: putv posb,next`.
struct EcologyObjectKind {
    int genus;
    EcologyLayer layer;
};
constexpr EcologyObjectKind kEcologyObjectKinds[] = {
    {6, EcologyLayer::food},   // food
    {7, EcologyLayer::food},   // drinks
    {13, EcologyLayer::toys},  // small toys
    {14, EcologyLayer::toys},  // big toys
    {9, EcologyLayer::toys},   // instruments
};

inline std::string object_places_script(int genus) {
    return "enum 2 " + std::to_string(genus) +
           " 0,dde: putv posl,dde: putv posr,dde: putv posb,next,endm";
}

// The reply's objects as points (their bottom centre).
inline std::vector<std::pair<int, int>> parse_object_places(const std::string& reply) {
    std::vector<long> fields;
    const char* cursor = reply.c_str();
    while (*cursor != '\0') {
        char* end = nullptr;
        const long value = std::strtol(cursor, &end, 10);
        if (end == cursor) {
            break;
        }
        fields.push_back(value);
        cursor = *end == '|' ? end + 1 : end;
    }
    std::vector<std::pair<int, int>> places;
    for (std::size_t i = 0; i + 2 < fields.size(); i += 3) {
        places.emplace_back(static_cast<int>((fields[i] + fields[i + 1]) / 2),
                            static_cast<int>(fields[i + 2] - 1));
    }
    return places;
}

// One value per room for a layer: temperature (-1 to 1), or a count.
inline std::vector<int> layer_values(const EcologySnapshot& world, EcologyLayer layer,
                                     const std::vector<int>& food_per_room,
                                     const std::vector<int>& toys_per_room) {
    std::vector<int> values(world.rooms.size(), 0);
    switch (layer) {
    case EcologyLayer::temperature:
        for (std::size_t i = 0; i < world.rooms.size(); ++i) {
            values[i] = world.rooms[i].temperature;
        }
        break;
    case EcologyLayer::crowding:
    case EcologyLayer::disease:
        for (const EcologyCreature& creature : world.creatures) {
            if (creature.dead || (layer == EcologyLayer::disease && !creature.infected)) {
                continue;
            }
            const int room = room_at(world.rooms, creature.x, creature.y - 1);
            if (room >= 0) {
                ++values[static_cast<std::size_t>(room)];
            }
        }
        break;
    case EcologyLayer::food:
    case EcologyLayer::toys: {
        const std::vector<int>& counts =
            layer == EcologyLayer::food ? food_per_room : toys_per_room;
        for (std::size_t i = 0; i < values.size() && i < counts.size(); ++i) {
            values[i] = counts[i];
        }
        break;
    }
    default:
        break;
    }
    return values;
}

} // namespace c1kit
