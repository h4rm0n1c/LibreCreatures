#pragma once

#include "windows_prelude.hpp"
#include <afxwin.h>

#include "../common/lab_trace.hpp"
#include "../ui/version_dialog.hpp"

#include <chrono>

namespace creatures1::platform {

// Concrete MFC version/about dialog behind ui::VersionDialogPlatform.
class C1NativeVersionDialog final
    : public CDialog, public creatures1::ui::VersionDialogPlatform {
public:
    explicit C1NativeVersionDialog(CWnd* owner)
        : CDialog(MAKEINTRESOURCEA(138), owner) {}

    BOOL OnInitDialog() override {
        // Not native: see windows_fixes_dialog.cpp's identical pair of
        // lines -- this dialog is a .rc resource, not an in-code template,
        // so if it shows the same begin/end gap as LibreFixes, the delay is
        // somewhere shared (DoModal/window creation), not in either
        // dialog's own body.
        creatures1::common::lab_trace(creatures1::common::LabTrace::timing,
                                      "About: OnInitDialog begin");
        const auto start = std::chrono::steady_clock::now();
        creatures1::ui::initialise_community_edition_version_dialog(*this);
        const double body_ms = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        creatures1::common::lab_trace(
            creatures1::common::LabTrace::timing,
            "About: OnInitDialog end (body took %.1fms)", body_ms);
        return TRUE;
    }

    void show_version_dialog_modal() override {
        creatures1::common::lab_trace(creatures1::common::LabTrace::timing,
                                      "About: show_version_dialog_modal "
                                      "entered (about to DoModal)");
        DoModal();
    }

    void initialise_dialog_base() override {
        CDialog::OnInitDialog();
    }

    void set_version_text(std::string_view text) override {
        SetDlgItemTextA(1010, std::string(text).c_str());
    }

    void update_dialog_data() override { UpdateData(FALSE); }
};

} // namespace creatures1::platform
