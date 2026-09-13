#pragma once

#include <string_view>

namespace creatures1::ui {

class MainWindowApi {
public:
    virtual ~MainWindowApi() = default;

    virtual void set_window_title(std::string_view title) = 0;
};

class SelectedCreatureTitleSource {
public:
    virtual ~SelectedCreatureTitleSource() = default;

    virtual std::string_view display_name() const = 0;
};

void update_main_window_title_for_selected_creature(
    MainWindowApi& window,
    const SelectedCreatureTitleSource* selected_creature);

} // namespace creatures1::ui
