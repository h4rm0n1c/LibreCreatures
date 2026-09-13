#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "../archive/string_archive.hpp"

namespace creatures1::application {

struct FavouritePlace {
    std::string name;
    std::int16_t viewport_origin_x = 0;
    std::int16_t viewport_origin_y = 0;

    void serialize(class FavouritePlaceArchive& archive);
};

// The document owns the fixed six-entry favourite-place array.  Menu handles
// and camera-submenu lookup are UI/MFC services; the removal policy stays in
// the world/document module and receives only typed operations at that
// boundary.
class FavouritePlaceRemovalHost {
public:
    virtual ~FavouritePlaceRemovalHost() = default;

    virtual std::size_t favourite_place_count() const = 0;
    virtual FavouritePlace& favourite_place_at(std::size_t index) = 0;
    virtual void set_favourite_place_count(std::size_t count) = 0;
    virtual void modify_favourite_place_menu(std::uint32_t menu_id,
                                             std::string_view name) = 0;
    virtual void remove_favourite_place_menu(std::uint32_t menu_id) = 0;
};

void remove_favourite_place_at_index(FavouritePlaceRemovalHost& host,
                                     int index);

// C1 writes the MFC string record followed by two little-endian signed
// 16-bit viewport coordinates. Stream framing and exception behavior stay in
// the archive adapter.
class FavouritePlaceArchive : public archive::StringArchive {
public:
    ~FavouritePlaceArchive() override = default;
    virtual std::int16_t read_int16() = 0;
    virtual void write_int16(std::int16_t value) = 0;
};

} // namespace creatures1::application
