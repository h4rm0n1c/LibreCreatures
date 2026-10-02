// g++ -std=c++17 -I../include ecology_test.cpp
#include "c1kit/ecology.hpp"

#include <cassert>
#include <cstdio>

using namespace c1kit;

int main() {
    // Two rooms (one outdoor at +1), two creatures: a live infected norn in
    // room 0 and a dead grendel in room 1.
    const std::string reply =
        "2|100|100|300|200|0|0|400|100|600|200|1|1|"
        "2|22489800|150|200|4|1|1|2|0|1|1|22737440|500|200|4|2|1|1|1|0|0|";
    EcologySnapshot world;
    assert(parse_ecology(reply, world));
    assert(world.rooms.size() == 2 && world.rooms[1].type == 1 && world.rooms[1].temperature == 1);
    assert(world.creatures.size() == 2);
    assert(world.creatures[0].handle == 22489800u && world.creatures[0].sex == 2 &&
           world.creatures[0].infected && world.creatures[0].selected);
    assert(world.creatures[1].dead && world.creatures[1].genus == 2);

    EcologySnapshot broken;
    assert(!parse_ecology("2|100|100|300|", broken));
    assert(!parse_ecology("", broken));

    // Rooms by point, round the world's width.
    EcologySnapshot rooms_only;
    assert(parse_ecology("2|100|100|300|200|0|0|400|100|600|200|1|1|0|", rooms_only));
    assert(room_at(rooms_only.rooms, 150, 150) == 0);
    assert(room_at(rooms_only.rooms, 150 + kWorldWidth, 150) == 0);
    assert(room_at(rooms_only.rooms, 350, 150) == -1);

    // Layers.
    const std::vector<int> food = {3, 0};
    const std::vector<int> toys = {0, 2};
    assert((layer_values(world, EcologyLayer::temperature, food, toys) == std::vector<int>{0, 1}));
    // The dead grendel does not crowd; the infected norn counts for disease.
    assert((layer_values(world, EcologyLayer::crowding, food, toys) == std::vector<int>{1, 0}));
    assert((layer_values(world, EcologyLayer::disease, food, toys) == std::vector<int>{1, 0}));
    assert((layer_values(world, EcologyLayer::food, food, toys) == std::vector<int>{3, 0}));
    assert((layer_values(world, EcologyLayer::toys, food, toys) == std::vector<int>{0, 2}));

    // Object places: the bottom centre.
    const auto places = parse_object_places("100|140|200|300|310|420|");
    assert(places.size() == 2 && places[0].first == 120 && places[0].second == 199 &&
           places[1].first == 305);
    assert(object_places_script(6) ==
           "enum 2 6 0,dde: putv posl,dde: putv posr,dde: putv posb,next,endm");
    std::puts("ecology_test: all passed");
    return 0;
}
