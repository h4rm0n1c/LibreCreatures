#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>

#include "../creatures/bacterium.hpp"
#include "../display/gallery.hpp"

namespace creatures1::platform {
class RectangleHitTestApi;
}

namespace creatures1::world {

struct MapRectangle {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 0;
    std::int32_t bottom = 0;
};

struct MapRoom {
    MapRectangle bounds{};
    // Serialized C1MapRoomType. Value 1 is the only room discriminator
    // proven to use the map's ambient-temperature profile.
    std::uint32_t room_type = 0;
};

// Recovered from Creatures.exe g_InitialWorldRoomRecords @ 00457060.
// OnNewDocument copies all eleven records before it acquires the background
// gallery.  Keep this as source data, not as a generated/DAT adapter.
inline constexpr std::array<MapRoom, 11> kInitialWorldRooms{{
    {{699, 453, 2480, 594}, 1},
    {{810, 683, 1869, 787}, 0},
    {{760, 310, 1073, 434}, 1},
    {{819, 858, 998, 994}, 0},
    {{1255, 971, 2927, 1082}, 0},
    {{1469, 798, 1854, 928}, 0},
    {{2164, 604, 2681, 738}, 0},
    {{1972, 757, 5874, 933}, 1},
    {{4390, 960, 5067, 1132}, 0},
    {{4067, 367, 5855, 526}, 1},
    {{5668, 544, 6266, 664}, 1},
}};

constexpr std::uint32_t kUsesMapAmbientTemperature = 1u;

// The executable stores ten five-byte records. Profiles 0..3 are read from
// rows 0..3 for wind and rows 5..8 for temperature; rows 4 and 9 are retained
// file bytes but are not addressed by the confirmed index range.
struct AmbientEnvironmentRecord {
    std::int8_t wind_delta = 0;
    std::uint8_t reserved_1 = 0;
    std::uint8_t reserved_2 = 0;
    std::int8_t temperature_delta_raw = 0;
    std::uint8_t reserved_4 = 0;
};

// Exact bytes from Creatures.exe g_ambient_environment_records at
// 0x00457028.  The table is intentionally retained as typed source data;
// the two rows outside the confirmed runtime index range are part of the
// executable's data and must not be silently discarded.
inline constexpr std::array<AmbientEnvironmentRecord, 10>
    kAmbientEnvironmentRecords{{
        {1, 2, 1, 1, 2},
        {1, 3, 2, 1, 1},
        {-1, 254, 254, -1, 255},
        {-1, 253, 255, -1, 254},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 1},
        {-1, 1, 0, 0, 2},
        {-1, 1, 0, 0, 3},
        {0, 1, 0, 0, 0},
        {0, 1, 255, -1, 255},
    }};

struct AmbientLightProfile {
    std::uint8_t light_level = 0;
    std::array<std::uint8_t, 3> reserved{};
};

// Exact C1AmbientLightProfile[4] bytes at
// 0x004542f0 (g_ambient_light_level_by_environment).  Only light_level is
// consumed by Creature::UpdateEnvironmentAndLifeStage; the three trailing
// bytes preserve the native four-byte record width.
inline constexpr std::array<AmbientLightProfile, 4>
    kAmbientLightProfiles{{
        {150, {0, 0, 0}},
        {200, {0, 0, 0}},
        {255, {0, 0, 0}},
        {64, {0, 0, 0}},
    }};

struct AmbientEnvironmentTable {
    const AmbientEnvironmentRecord* records = nullptr;
    std::size_t record_count = 0;
};

// Non-owning view of the room table used by map-boundary helpers.
struct MapRoomTable {
    const MapRoom* rooms = nullptr;
    std::size_t room_count = 0;
};

// The archive adapter combines the existing gallery/image protocol with the
// byte protocol used by Bacterium records. Its framing, dynamic-object
// versioning, and exception behavior remain outside the map policy.
class MapDataArchive : public display::ImageArchive,
                       public creatures1::creatures::BacteriumArchive {
public:
    ~MapDataArchive() override = default;
    virtual bool is_loading() const override = 0;
    virtual std::uint8_t read_byte() override = 0;
    virtual void write_byte(std::uint8_t value) override = 0;
};

// Typed owner boundary for MapData's persisted state. The native MapData
// object owns these records; this interface keeps that storage independent of
// the serialization algorithm and avoids reproducing its MFC layout.
class MapDataSerializationHost {
public:
    virtual ~MapDataSerializationHost() = default;
    virtual std::int32_t& save_format_version() = 0;
    virtual std::uint32_t& ambient_environment_index() = 0;
    virtual display::Gallery* background_gallery() const = 0;
    virtual void set_background_gallery(display::Gallery* gallery) = 0;
    virtual std::int32_t& room_count() = 0;
    virtual MapRoom& room_at(std::size_t index) = 0;
    virtual std::int32_t& ground_height_at(std::size_t index) = 0;
    virtual creatures1::creatures::Bacterium& bacterium_at(
        std::size_t index) = 0;
};

// Concrete owner for the recovered MapData state.  The native object is an
// MFC runtime-class allocation of 0xbf8 bytes containing forty room records,
// 261 ground-height values, one hundred CBacterium records, the ambient
// environment selector, and a borrowed CGallery reference.  The clean owner
// keeps the semantic fields together; framework allocation and archive
// object identity remain adapter responsibilities.
class MapData final : public MapDataSerializationHost {
public:
    static constexpr std::size_t kRoomCapacity = 0x28;
    static constexpr std::size_t kGroundHeightCount = 0x105;
    static constexpr std::size_t kBacteriumCount = 100;

    MapData() = default;
    ~MapData() override = default;

    MapData(const MapData&) = delete;
    MapData& operator=(const MapData&) = delete;

    // SFCDoc::OnNewDocument copies only the initial room table, sets all
    // ground heights to 600, selects environment zero, and attaches the
    // already-acquired background gallery.
    void initialize_new_world(const MapRoom* initial_rooms,
                              std::size_t initial_room_count,
                              display::Gallery* background_gallery,
                              std::int32_t default_ground_height = 600);

    // Deletes the document-owned map payload without destroying the enclosing
    // WorldRuntime. This is the MapData half of SFCDoc::DeleteContents; the
    // gallery registry releases the referenced gallery separately.
    void reset_for_document_delete();

    std::uint32_t ambient_environment_index_value() const {
        return ambient_environment_index_;
    }
    std::int32_t save_format_version_value() const {
        return save_format_version_;
    }
    void set_save_format_version(std::int32_t value) {
        save_format_version_ = value;
    }
    std::int32_t& save_format_version() override {
        return save_format_version_;
    }
    void set_ambient_environment_index(std::uint32_t value) {
        ambient_environment_index_ = value;
    }
    display::Gallery* background_gallery_value() const {
        return background_gallery_;
    }
    void set_background_gallery_value(display::Gallery* gallery) {
        background_gallery_ = gallery;
    }
    std::int32_t room_count_value() const { return room_count_; }
    MapRoomTable room_table() const {
        return {rooms_.data(), static_cast<std::size_t>(room_count_)};
    }
    std::int32_t ground_height(std::size_t index) const {
        return ground_height_by_x_block_[index];
    }

    std::uint32_t& ambient_environment_index() override {
        return ambient_environment_index_;
    }
    display::Gallery* background_gallery() const override {
        return background_gallery_;
    }
    void set_background_gallery(display::Gallery* gallery) override {
        background_gallery_ = gallery;
    }
    std::int32_t& room_count() override { return room_count_; }
    MapRoom& room_at(std::size_t index) override { return rooms_[index]; }
    std::int32_t& ground_height_at(std::size_t index) override {
        return ground_height_by_x_block_[index];
    }
    creatures1::creatures::Bacterium& bacterium_at(
        std::size_t index) override {
        return bacteria_[index];
    }

private:
    std::int32_t save_format_version_ = 0;
    std::uint32_t ambient_environment_index_ = 0;
    display::Gallery* background_gallery_ = nullptr;
    std::int32_t room_count_ = 0;
    std::array<MapRoom, kRoomCapacity> rooms_{};
    std::array<std::int32_t, kGroundHeightCount> ground_height_by_x_block_{};
    std::array<creatures1::creatures::Bacterium, kBacteriumCount> bacteria_{};
};

void serialize_map_data(MapDataSerializationHost& host,
                        MapDataArchive& archive);

// Selects the room spanning world_x whose bottom edge is nearest world_y.
// When no room qualifies, only output_bounds.bottom is set to the executable's
// sentinel value; the other output fields retain their prior contents.
void find_nearest_room_bounds_at_point(const MapRoomTable& room_table,
                                       std::int32_t world_x,
                                       std::int32_t world_y,
                                       MapRectangle& output_bounds);

// Returns the first serialized room containing the point.  The inclusive
// edge rules belong to the Win32 PtInRect boundary supplied by the platform.
const MapRoom* find_room_containing_point(
    const MapRoomTable& room_table,
    std::int32_t world_x,
    std::int32_t world_y,
    const platform::RectangleHitTestApi& hit_test);

// Returns the raw signed temperature adjustment selected by the map profile.
// A point outside the room table uses the profile; a containing room uses it
// only when its serialized room_type is kUsesMapAmbientTemperature.
int get_ambient_temperature_at_point(
    const MapRoomTable& room_table,
    std::uint32_t ambient_environment_index,
    const AmbientEnvironmentTable& environment_table,
    std::int32_t world_x,
    std::int32_t world_y,
    const platform::RectangleHitTestApi& hit_test);

} // namespace creatures1::world
