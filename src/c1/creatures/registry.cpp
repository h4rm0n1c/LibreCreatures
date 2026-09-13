#include "registry.hpp"

#include <array>
#include <ctime>

#include "../common/time.hpp"

namespace creatures1::creatures {
namespace {

void initialize_history_birthday(std::string& birthday) {
    common::Time64 current_time = 0;
    common::write_current_time(&current_time);

    const std::time_t calendar_time = static_cast<std::time_t>(current_time);
    const std::tm* local_time = std::localtime(&calendar_time);
    if (local_time == nullptr) {
        birthday.clear();
        return;
    }

    std::array<char, 128> formatted_time{};
    const std::size_t length =
        std::strftime(formatted_time.data(), formatted_time.size(),
                      "%H:%M %b %d %Y", local_time);
    if (length == 0) {
        birthday.clear();
        return;
    }
    birthday.assign(formatted_time.data(), length);
}

} // namespace

void remove_creature_from_registry(
    CreatureRegistryMutation& registry,
    CreatureSelectionEntry* creature,
    const std::function<void()>& refresh_selection_menu) {
    for (std::size_t index = 0; index < registry.creature_count(); ++index) {
        if (registry.creature_at(index) != creature) {
            continue;
        }

        if (registry.remove_at(index)) {
            refresh_selection_menu();
        }
        return;
    }
}

CreatureRegister::CreatureRegister() {
    initialize_history_birthday(history_.birthday);
}

CreatureRegister::~CreatureRegister() {
    // The recovered destructor explicitly empties every CString before the
    // normal member destruction path.  std::string owns the storage and then
    // supplies the ordinary C++ destruction boundary.
    for (std::string* entry : history_entries()) {
        entry->clear();
    }
}

std::array<std::string*, 10> CreatureRegister::history_entries() {
    return {&history_.genome_moniker,
            &history_.display_name,
            &history_.father_moniker,
            &history_.mother_moniker,
            &history_.birthday,
            &history_.birthplace,
            &history_.history_entry_06,
            &history_.history_entry_07,
            &history_.history_entry_08,
            &history_.history_entry_09};
}

std::array<const std::string*, 10> CreatureRegister::history_entries() const {
    return {&history_.genome_moniker,
            &history_.display_name,
            &history_.father_moniker,
            &history_.mother_moniker,
            &history_.birthday,
            &history_.birthplace,
            &history_.history_entry_06,
            &history_.history_entry_07,
            &history_.history_entry_08,
            &history_.history_entry_09};
}

void CreatureRegister::serialize(archive::StringArchive& archive) {
    if (archive.is_loading()) {
        for (std::string* entry : history_entries()) {
            *entry = archive.read_string();
        }
        return;
    }

    for (const std::string* entry : history_entries()) {
        archive.write_string(*entry);
    }
}

} // namespace creatures1::creatures
