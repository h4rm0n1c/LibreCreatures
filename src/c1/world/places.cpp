#include "places.hpp"

namespace creatures1::application {

namespace {

constexpr std::uint32_t kFavouritePlaceMenuIdBase = 0x8053;
constexpr std::size_t kFavouritePlaceMenuCapacity = 6;

} // namespace

void FavouritePlace::serialize(FavouritePlaceArchive& archive) {
    if (archive.is_loading()) {
        name = archive.read_string();
        viewport_origin_x = archive.read_int16();
        viewport_origin_y = archive.read_int16();
        return;
    }

    archive.write_string(name);
    archive.write_int16(viewport_origin_x);
    archive.write_int16(viewport_origin_y);
}

void remove_favourite_place_at_index(FavouritePlaceRemovalHost& host,
                                     int index) {
    const std::size_t current_count = host.favourite_place_count();
    const std::size_t remaining_count = current_count - 1;
    host.set_favourite_place_count(remaining_count);

    if (index < static_cast<int>(remaining_count)) {
        std::size_t destination_index = static_cast<std::size_t>(index);
        while (destination_index < remaining_count) {
            FavouritePlace& destination =
                host.favourite_place_at(destination_index);
            const FavouritePlace& source =
                host.favourite_place_at(destination_index + 1);
            destination.name = source.name;
            destination.viewport_origin_x = source.viewport_origin_x;
            destination.viewport_origin_y = source.viewport_origin_y;
            host.modify_favourite_place_menu(
                kFavouritePlaceMenuIdBase +
                    static_cast<std::uint32_t>(destination_index),
                destination.name);
            ++destination_index;
        }
    }

    host.remove_favourite_place_menu(
        kFavouritePlaceMenuIdBase + static_cast<std::uint32_t>(remaining_count));

    if (remaining_count < kFavouritePlaceMenuCapacity) {
        for (std::size_t index_to_clear = remaining_count;
             index_to_clear < kFavouritePlaceMenuCapacity; ++index_to_clear) {
            host.favourite_place_at(index_to_clear).name.clear();
        }
    }
}

} // namespace creatures1::application
