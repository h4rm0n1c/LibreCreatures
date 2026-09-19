#include "version_dialog.hpp"

#include <string>

namespace creatures1::ui {
namespace {

constexpr std::string_view kVersionMajor = "1.0";
constexpr std::string_view kVersionMinor = "5";
constexpr std::string_view kCommunityEditionVersion = "10.3";

} // namespace

void show_version_dialog(VersionDialogPlatform& platform) {
    platform.show_version_dialog_modal();
}

void initialise_community_edition_version_dialog(
    VersionDialogPlatform& platform) {
    platform.initialise_dialog_base();

    std::string version_text = "Version ";
    version_text.append(kVersionMajor);
    version_text.push_back('.');
    version_text.append(kVersionMinor);
    version_text.append(" (Release)\r\nCommunity Release ");
    version_text.append(kCommunityEditionVersion);
    version_text.append("\r\nLibreCreatures Refactor Build");
    platform.set_version_text(version_text);
    platform.update_dialog_data();
}

} // namespace creatures1::ui
