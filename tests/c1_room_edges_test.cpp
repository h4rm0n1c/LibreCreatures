#include "world/room_edges.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

namespace c1 = creatures1;
using c1::world::MapRoom;
using c1::world::MapRoomTable;
using c1::world::rooms_with_edge_near;

int main() {
    const MapRoom rooms[] = {
        {{100, 100, 300, 200}, 0},
        {{300, 150, 500, 200}, 1},  // shares the first room's right side
        {{8200, 50, 8351, 90}, 0},  // at the world's right end
    };
    const MapRoomTable table{rooms, 3};
    using Found = std::vector<std::size_t>;

    // Inside a room, away from its edges: nothing.
    assert(rooms_with_edge_near(table, 200, 150, 2).empty());
    // On a side, top and floor, and just inside the tolerance.
    assert(rooms_with_edge_near(table, 100, 150, 2) == Found{0});
    assert(rooms_with_edge_near(table, 98, 150, 2) == Found{0});
    assert(rooms_with_edge_near(table, 97, 150, 2).empty());
    assert(rooms_with_edge_near(table, 200, 101, 2) == Found{0});
    assert(rooms_with_edge_near(table, 150, 202, 2) == Found{0});
    // In line with a side but past the room's end: nothing.
    assert(rooms_with_edge_near(table, 100, 250, 2).empty());
    // A shared edge lists both rooms, in table order.
    assert(rooms_with_edge_near(table, 300, 180, 2) == (Found{0, 1}));
    // x is taken round the world's width.
    assert(rooms_with_edge_near(table, 8300 - c1::world::kWorldWidth, 50, 2) == Found{2});
    assert(rooms_with_edge_near(table, 8300 + c1::world::kWorldWidth, 90, 2) == Found{2});
    std::puts("c1_room_edges_test: all passed");
    return 0;
}
