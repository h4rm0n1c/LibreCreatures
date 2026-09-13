#pragma once

#include "menus.hpp"

#include "../application/application.hpp"
#include "../creatures/selection.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace creatures1::ui {

// Packed classifier values use the upper two bytes for the species family.
// These are the values used by the original menu caption policy.
enum class CreatureSpeciesFamily : std::uint32_t {
    norn = 0x04010000,
    grendel = 0x04020000,
    ettin = 0x04030000,
};

// Extends the common menu lookup boundary with the menu mutations and frame
// notifications needed by the creature-selection command.
class CreatureSelectionMenuPlatform : public MenuPlatform {
public:
    ~CreatureSelectionMenuPlatform() override = default;

    virtual bool delete_item_by_position(MenuHandle& menu,
                                         std::size_t item_index) = 0;
    virtual bool replace_item_by_position(MenuHandle& menu,
                                          std::size_t item_index,
                                          std::uint32_t command_id,
                                          std::string_view caption) = 0;
    virtual bool append_item(MenuHandle& menu,
                             std::uint32_t command_id,
                             std::string_view caption) = 0;
    virtual void invalidate_main_toolbar() = 0;
    virtual void draw_main_menu_bar() = 0;
};

// Rebuilds the dynamic Creatures menu from the live registry. The platform
// object supplies resource/menu/window operations; all selection filtering,
// command numbering, caption construction, and failure policy belong here.
bool rebuild_creature_selection_menu(
    creatures1::creatures::CreatureSelectionState& selection,
    const creatures1::creatures::CreatureRegistryView& registry,
    CreatureSelectionMenuPlatform& platform,
    bool informative_menu);

// These are application selection entry points reached by the MFC command
// wrappers.  The transition itself is shared with next/previous navigation;
// this layer only resolves the menu index and preserves the binary's extra
// viewport-return call after menu dispatch.
void set_selected_creature(
    application::CreatureSelectionCycleHost& host,
    creatures1::creatures::CreatureSelectionEntry* creature,
    bool return_to_creature);

void select_creature_by_menu_index(
    application::CreatureSelectionCycleHost& host, int menu_index);

} // namespace creatures1::ui
