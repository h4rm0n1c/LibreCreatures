#pragma once

#include "windows_prelude.hpp"
#include <afxwin.h>

#include "../ui/version_dialog.hpp"

namespace creatures1::platform {

// Concrete MFC version/about dialog behind ui::VersionDialogPlatform.
class C1NativeVersionDialog final
    : public CDialog, public creatures1::ui::VersionDialogPlatform {
public:
    explicit C1NativeVersionDialog(CWnd* owner)
        : CDialog(MAKEINTRESOURCEA(138), owner) {}

    BOOL OnInitDialog() override {
        creatures1::ui::initialise_community_edition_version_dialog(*this);
        return TRUE;
    }

    void show_version_dialog_modal() override { DoModal(); }

    void initialise_dialog_base() override {
        CDialog::OnInitDialog();
    }

    void set_version_text(std::string_view text) override {
        SetDlgItemTextA(1010, std::string(text).c_str());
    }

    void update_dialog_data() override { UpdateData(FALSE); }
};

} // namespace creatures1::platform
