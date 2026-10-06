#pragma once

#include "geometry.hpp"
#include "map.hpp"

#include <cstddef>
#include <cstdlib>
#include <vector>

namespace creatures1::world {

// Not native (neorender): View > Developer view > Show rooms.  The rooms
// (their numbers are their places in the map's room table) whose outline
// passes within `tolerance` world pixels of (world_x, world_y), in table
// order.  world_x is taken round the world's width first.
inline std::vector<std::size_t> rooms_with_edge_near(const MapRoomTable& table,
                                                     int world_x, int world_y,
                                                     int tolerance) {
    world_x %= kWorldWidth;
    if (world_x < 0) {
        world_x += kWorldWidth;
    }
    std::vector<std::size_t> found;
    for (std::size_t index = 0; index < table.room_count; ++index) {
        const MapRectangle& room = table.rooms[index].bounds;
        const bool in_x = world_x >= room.left - tolerance &&
                          world_x <= room.right + tolerance;
        const bool in_y = world_y >= room.top - tolerance &&
                          world_y <= room.bottom + tolerance;
        const bool on_side = in_y && (std::abs(world_x - room.left) <= tolerance ||
                                      std::abs(world_x - room.right) <= tolerance);
        const bool on_top_or_floor =
            in_x && (std::abs(world_y - room.top) <= tolerance ||
                     std::abs(world_y - room.bottom) <= tolerance);
        if (on_side || on_top_or_floor) {
            found.push_back(index);
        }
    }
    return found;
}

} // namespace creatures1::world
