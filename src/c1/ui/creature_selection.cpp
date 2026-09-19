#include "creature_selection.hpp"

#include <algorithm>
#include <cstdio>
#include <array>
#include <string>
#include <string_view>

namespace creatures1::ui {
namespace {

constexpr std::uint32_t kCreaturesMenuCaptionResource = 0xef29;
constexpr std::uint32_t kEmptySelectionCaptionResource = 0x8024;
constexpr std::uint32_t kEmptySelectionCommand = 0x8023;
constexpr std::uint32_t kMaleStatusCaptionResource = 0xef37;
constexpr std::uint32_t kFemaleStatusCaptionResource = 0xef36;
constexpr std::uint32_t kDeadStatusCaptionResource = 0xef38;
constexpr std::uint32_t kSelectionCommandBase = 40000;
constexpr std::uint32_t kSelectionCommandLimit = 0x9c5f;
constexpr std::uint32_t kInformativeChemicalIndex = 0x3b;
constexpr std::size_t kMenuTextCapacity = 64;

std::string_view species_suffix(std::uint32_t family) {
    switch (static_cast<CreatureSpeciesFamily>(family)) {
    case CreatureSpeciesFamily::norn:
        return " Norn";
    case CreatureSpeciesFamily::grendel:
        return " Grendel";
    case CreatureSpeciesFamily::ettin:
        return " Ettin";
    }
    return " Geat";
}

std::uint32_t status_caption_resource(
    const creatures1::creatures::CreatureSelectionEntry& creature) {
    if (creature.life_state() !=
        creatures1::creatures::CreatureLifeState::alive) {
        return kDeadStatusCaptionResource;
    }
    return creature.gender() == creatures1::creatures::CreatureGender::male
               ? kMaleStatusCaptionResource
               : kFemaleStatusCaptionResource;
}

MenuHandle* find_creatures_menu(const CreatureSelectionMenuPlatform& platform) {
    MenuHandle* main_menu = platform.main_window_menu();
    if (main_menu == nullptr) {
        return nullptr;
    }

    const std::string creatures_caption =
        platform.load_string(kCreaturesMenuCaptionResource);
    std::size_t item_index = 0;
    std::size_t item_count = platform.item_count(*main_menu);
    while (item_index < item_count) {
        std::array<char, kMenuTextCapacity> item_text{};
        if (platform.item_text(*main_menu,
                               item_index,
                               item_text.data(),
                               item_text.size()) &&
            std::string_view(item_text.data()) == creatures_caption) {
            return platform.submenu(*main_menu, item_index);
        }
        ++item_index;
        item_count = platform.item_count(*main_menu);
    }
    return nullptr;
}

} // namespace

bool rebuild_creature_selection_menu(
    creatures1::creatures::CreatureSelectionState& selection,
    const creatures1::creatures::CreatureRegistryView& registry,
    CreatureSelectionMenuPlatform& platform,
    bool informative_menu) {
    MenuHandle* creatures_menu = find_creatures_menu(platform);
    if (creatures_menu == nullptr) {
        return false;
    }

    while (platform.item_count(*creatures_menu) > 1) {
        if (!platform.delete_item_by_position(*creatures_menu, 1)) {
            return false;
        }
    }

    selection.clear();
    const std::size_t creature_count = registry.creature_count();
    for (std::size_t index = 0; index < creature_count; ++index) {
        creatures1::creatures::CreatureSelectionEntry* creature =
            registry.creature_at(index);
        if (creature != nullptr && creature->tick_enabled()) {
            selection.add(creature);
        }
    }

    platform.invalidate_main_toolbar();

    if (selection.size() == 0) {
        const std::string caption =
            platform.load_string(kEmptySelectionCaptionResource);
        if (!platform.replace_item_by_position(*creatures_menu,
                                               0,
                                               kEmptySelectionCommand,
                                               caption)) {
            return false;
        }
    } else {
        for (std::size_t index = 0; index < selection.size(); ++index) {
            creatures1::creatures::CreatureSelectionEntry* creature =
                selection.at(index);
            const std::uint32_t command_id =
                kSelectionCommandBase + static_cast<std::uint32_t>(index);
            creature->set_selection_menu_command_id(command_id);

            if (command_id >= kSelectionCommandLimit) {
                continue;
            }

            std::string caption = creature->display_name();
            if (informative_menu &&
                creature->gender() !=
                    creatures1::creatures::CreatureGender::male &&
                creature->has_child_genome_source()) {
                caption.insert(0, "*");
            }

            std::string status =
                platform.load_string(status_caption_resource(*creature));
            status += species_suffix(creature->classifier_species_family());
            caption += " (";
            caption += status;
            caption += ")";

            if (informative_menu) {
                // RebuildCreatureSelectionMenu @ 00422250 (004225df..0042263f):
                // " [%02d:%02d - %d%%]" with hours = age / 36000,
                // minutes = age / 600 - hours * 60 (age in 0.1 s ticks), and
                // chemical 0x3b as concentration * 100 / 255.
                const int age = static_cast<int>(creature->age_in_ticks());
                const int hours = age / 36000;
                const int minutes = age / 600 - hours * 60;
                const int percent = static_cast<int>(
                    creature->chemical_concentration(kInformativeChemicalIndex) *
                    100U / 255U);
                char informative[48];
                std::snprintf(informative, sizeof(informative),
                              " [%02d:%02d - %d%%]", hours, minutes, percent);
                caption += informative;
            }

            const bool menu_updated =
                index == 0
                    ? platform.replace_item_by_position(*creatures_menu,
                                                        0,
                                                        command_id,
                                                        caption)
                    : platform.append_item(*creatures_menu,
                                           command_id,
                                           caption);
            if (!menu_updated) {
                return false;
            }
        }
    }

    platform.draw_main_menu_bar();
    return true;
}

void set_selected_creature(
    application::CreatureSelectionCycleHost& host,
    creatures1::creatures::CreatureSelectionEntry* creature,
    bool return_to_creature) {
    application::apply_selected_creature(host, creature, return_to_creature);
}

void select_creature_by_menu_index(
    application::CreatureSelectionCycleHost& host, int menu_index) {
    application::select_creature_by_menu_command(host, menu_index);
}

} // namespace creatures1::ui
