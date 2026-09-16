#include "map.hpp"

#include <cstdlib>

#include "../platform/mfc_adapters.hpp"

namespace creatures1::world {

namespace {
constexpr std::size_t kGroundHeightCount = 0x105;
constexpr std::size_t kBacteriumCount = 100;
}

void serialize_map_data(MapDataSerializationHost& host,
                        MapDataArchive& archive) {
    if (archive.is_loading()) {
        host.save_format_version() = archive.read_int32();
        host.ambient_environment_index() = archive.read_uint32();
        host.set_background_gallery(archive.read_gallery());
        host.room_count() = archive.read_int32();

        if (host.room_count() > 0) {
            for (std::int32_t index = 0; index < host.room_count(); ++index) {
                MapRoom& room = host.room_at(static_cast<std::size_t>(index));
                room.bounds.left = archive.read_int32();
                room.bounds.top = archive.read_int32();
                room.bounds.right = archive.read_int32();
                room.bounds.bottom = archive.read_int32();
                room.room_type = archive.read_uint32();
            }
        }

        for (std::size_t index = 0; index < kGroundHeightCount; ++index) {
            host.ground_height_at(index) = archive.read_int32();
        }
        for (std::size_t index = 0; index < kBacteriumCount; ++index) {
            host.bacterium_at(index).serialize(archive);
        }
        return;
    }

    archive.write_int32(host.save_format_version());
    archive.write_uint32(host.ambient_environment_index());
    archive.write_gallery(host.background_gallery());
    archive.write_int32(host.room_count());

    for (std::int32_t index = 0; index < host.room_count(); ++index) {
        const MapRoom& room = host.room_at(static_cast<std::size_t>(index));
        archive.write_int32(room.bounds.left);
        archive.write_int32(room.bounds.top);
        archive.write_int32(room.bounds.right);
        archive.write_int32(room.bounds.bottom);
        archive.write_uint32(room.room_type);
    }

    for (std::size_t index = 0; index < kGroundHeightCount; ++index) {
        archive.write_int32(host.ground_height_at(index));
    }
    for (std::size_t index = 0; index < kBacteriumCount; ++index) {
        host.bacterium_at(index).serialize(archive);
    }
}

void MapData::reset_for_document_delete() {
    save_format_version_ = 0;
    ambient_environment_index_ = 0;
    background_gallery_ = nullptr;
    room_count_ = 0;
    rooms_.fill({});
    ground_height_by_x_block_.fill(0);
    bacteria_.fill(creatures1::creatures::Bacterium{});
}

void find_nearest_room_bounds_at_point(const MapRoomTable& room_table,
                                       std::int32_t world_x,
                                       std::int32_t world_y,
                                       MapRectangle& output_bounds) {
    output_bounds.bottom = kNoRoomBottom;

    for (std::size_t room_index = 0; room_index < room_table.room_count;
         ++room_index) {
        const MapRectangle& room_bounds = room_table.rooms[room_index].bounds;
        const bool spans_x = room_bounds.left <= world_x &&
                             world_x <= room_bounds.right;
        if (!spans_x) {
            continue;
        }

        const auto room_bottom_distance =
            std::abs(world_y - room_bounds.bottom);
        const auto best_bottom_distance =
            std::abs(world_y - output_bounds.bottom);
        if (room_bottom_distance < best_bottom_distance) {
            output_bounds = room_bounds;
        }
    }
}

const MapRoom* find_room_containing_point(
    const MapRoomTable& room_table,
    std::int32_t world_x,
    std::int32_t world_y,
    const platform::RectangleHitTestApi& hit_test) {
    for (std::size_t room_index = 0; room_index < room_table.room_count;
         ++room_index) {
        const MapRoom& room = room_table.rooms[room_index];
        const platform::Rectangle rectangle{
            room.bounds.left,
            room.bounds.top,
            room.bounds.right,
            room.bounds.bottom,
        };
        if (platform::point_in_rect_xy(hit_test, rectangle, world_x, world_y)) {
            return &room;
        }
    }
    return nullptr;
}

int get_ambient_temperature_at_point(
    const MapRoomTable& room_table,
    std::uint32_t ambient_environment_index,
    const AmbientEnvironmentTable& environment_table,
    std::int32_t world_x,
    std::int32_t world_y,
    const platform::RectangleHitTestApi& hit_test) {
    const MapRoom* containing_room = find_room_containing_point(
        room_table, world_x, world_y, hit_test);
    if (containing_room != nullptr &&
        containing_room->room_type != kUsesMapAmbientTemperature) {
        return 0;
    }

    constexpr std::size_t kTemperatureProfileRowOffset = 5;
    return static_cast<int>(environment_table
                                .records[ambient_environment_index +
                                         kTemperatureProfileRowOffset]
                                .temperature_delta_raw);
}

} // namespace creatures1::world
