#include "place_dialog.hpp"

#include <algorithm>

namespace creatures1::ui {

void PlaceDialog::exchange_data(PlaceDialogDataExchange& exchange) {
    exchange.bind_text(kPlaceNameControlId, place_name_);
    exchange.validate_maximum_characters(kPlaceNameControlId, place_name_,
                                         kPlaceNameMaximumCharacters);
}

void PlaceDialog::on_place_name_changed(PlaceDialogPlatform& platform) {
    platform.update_data(true);
}

bool PlaceDialog::on_init_dialog(PlaceDialogPlatform& platform) {
    if (!platform.base_on_init_dialog()) {
        return false;
    }

    place_name_ = "<Place Name>";
    platform.update_data(false);
    return true;
}

void RemovePlaceDialog::on_remove_place_list_selection_changed(
    RemovePlaceDialogPlatform& platform) {
    platform.update_data(false);
    const int selected_list_index = platform.selected_favourite_list_index();
    for (std::size_t list_index = 0; list_index < favourite_list_count_;
         ++list_index) {
        if (selected_list_index == favourite_list_indices_[list_index]) {
            // The original dialog deliberately excludes the document's
            // first (built-in) place, so its displayed entries map to
            // one-based document indices.
            platform.remove_favourite_place_at_index(list_index + 1);
            platform.finish_selection_change();
            return;
        }
    }
}

bool RemovePlaceDialog::on_init_dialog(RemovePlaceDialogPlatform& platform) {
    if (!platform.base_on_init_dialog()) {
        return false;
    }

    const std::size_t place_count = platform.favourite_place_count();
    if (place_count > 1) {
        const std::size_t inserted_count =
            std::min(place_count - 1, kMaximumRemovableFavouritePlaces);
        for (std::size_t place_index = 0; place_index < inserted_count;
             ++place_index) {
            // CRemovePlaceDlg::OnInitDialog @ 0x0042f5e0 starts at the
            // second record's name (document offset 300 = places[1]); the
            // built-in first place is never listed, so row r is place r+1.
            const int inserted_list_index =
                platform.insert_favourite_name_at_front(
                    platform.favourite_place_name(place_index + 1));

            for (std::size_t prior_index = 0; prior_index < place_index;
                 ++prior_index) {
                if (inserted_list_index <=
                    favourite_list_indices_[prior_index]) {
                    ++favourite_list_indices_[prior_index];
                }
            }

            favourite_list_indices_[place_index] = inserted_list_index;
            ++favourite_list_count_;
        }
    }

    platform.select_first_favourite_name();
    platform.update_data(true);
    return true;
}

} // namespace creatures1::ui
