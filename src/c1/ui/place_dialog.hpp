#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::ui {

constexpr std::uint32_t kPlaceNameControlId = 0x3f4;
constexpr std::size_t kPlaceNameMaximumCharacters = 0x14;
constexpr std::size_t kMaximumRemovableFavouritePlaces = 5;

// MFC's dialog object, DDX/DDV helpers, and the native edit control are
// supplied by the UI adapter. The recovered dialog owns only the text and
// its validation policy.
class PlaceDialogDataExchange {
public:
    virtual ~PlaceDialogDataExchange() = default;

    virtual void bind_text(std::uint32_t control_id, std::string& value) = 0;
    virtual void validate_maximum_characters(std::uint32_t control_id,
                                             std::string_view value,
                                             std::size_t maximum) = 0;
};

class PlaceDialogPlatform {
public:
    virtual ~PlaceDialogPlatform() = default;

    virtual bool base_on_init_dialog() = 0;
    virtual void update_data(bool save_and_validate) = 0;
};

class PlaceDialog {
public:
    void exchange_data(PlaceDialogDataExchange& exchange);
    void on_place_name_changed(PlaceDialogPlatform& platform);
    bool on_init_dialog(PlaceDialogPlatform& platform);

    std::string& place_name() { return place_name_; }
    const std::string& place_name() const { return place_name_; }

private:
    std::string place_name_;
};

// The document remains the owner of favourite-place records. This adapter
// exposes the record names and the list-box messages used by the original
// dialog without leaking CString, HWND, or LPARAM into the clean source.
class RemovePlaceDialogPlatform {
public:
    virtual ~RemovePlaceDialogPlatform() = default;

    virtual bool base_on_init_dialog() = 0;
    virtual std::size_t favourite_place_count() const = 0;
    virtual std::string_view favourite_place_name(std::size_t index) const = 0;
    virtual int insert_favourite_name_at_front(std::string_view name) = 0;
    virtual int selected_favourite_list_index() const = 0;
    virtual void remove_favourite_place_at_index(std::size_t index) = 0;
    virtual void finish_selection_change() = 0;
    virtual void select_first_favourite_name() = 0;
    virtual void update_data(bool save_and_validate) = 0;
};

class RemovePlaceDialog {
public:
    void on_remove_place_list_selection_changed(
        RemovePlaceDialogPlatform& platform);
    bool on_init_dialog(RemovePlaceDialogPlatform& platform);

    std::size_t favourite_list_count() const {
        return favourite_list_count_;
    }
    int favourite_list_index(std::size_t index) const {
        return favourite_list_indices_[index];
    }

private:
    std::array<int, kMaximumRemovableFavouritePlaces>
        favourite_list_indices_{};
    std::size_t favourite_list_count_ = 0;
};

} // namespace creatures1::ui
