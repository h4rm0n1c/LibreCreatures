#include "main_window.hpp"

#include <string>

namespace creatures1::ui {

void update_main_window_title_for_selected_creature(
    MainWindowApi& window,
    const SelectedCreatureTitleSource* selected_creature) {
    std::string title = "Creatures";
    if (selected_creature != nullptr) {
        title += " - ";
        title += selected_creature->display_name();
    }
    window.set_window_title(title);
}

} // namespace creatures1::ui
