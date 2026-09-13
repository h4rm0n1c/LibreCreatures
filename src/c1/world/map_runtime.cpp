#include "map.hpp"

#include <algorithm>

namespace creatures1::world {

void MapData::initialize_new_world(const MapRoom* initial_rooms,
                                    std::size_t initial_room_count,
                                    display::Gallery* background_gallery,
                                    std::int32_t default_ground_height) {
    ambient_environment_index_ = 0;
    room_count_ = static_cast<std::int32_t>(std::min(
        initial_room_count, kRoomCapacity));
    background_gallery_ = background_gallery;
    rooms_.fill({});
    ground_height_by_x_block_.fill(default_ground_height);
    if (initial_rooms != nullptr && room_count_ != 0) {
        std::copy_n(initial_rooms, static_cast<std::size_t>(room_count_),
                    rooms_.begin());
    }
}

} // namespace creatures1::world
