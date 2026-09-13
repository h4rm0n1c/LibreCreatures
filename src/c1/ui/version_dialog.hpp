#pragma once

#include <string_view>

namespace creatures1::ui {

// The dialog resource, MFC modal loop, CString storage, and UpdateData call
// are supplied by the native UI adapter.  C1 owns only the user-visible
// version-dialog policy.
class VersionDialogPlatform {
public:
    virtual ~VersionDialogPlatform() = default;
    virtual void show_version_dialog_modal() = 0;
    virtual void initialise_dialog_base() = 0;
    virtual void set_version_text(std::string_view text) = 0;
    virtual void update_dialog_data() = 0;
};

void show_version_dialog(VersionDialogPlatform& platform);
void initialise_community_edition_version_dialog(
    VersionDialogPlatform& platform);

} // namespace creatures1::ui
