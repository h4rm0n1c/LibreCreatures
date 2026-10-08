// KitApp: start-up, OLE server registration and the Communicate route.
// Follows Observation's COverviewApp::InitInstance @ 0x00401cd0 and the
// application constructor @ 0x00401a90.

#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_art.hpp"

#include <cstdio>

namespace c1kitshell {

KitApp::KitApp() = default;

BOOL KitApp::InitInstance() {
    const KitDefinition& kit = kit_definition();

    // A copy started by a crash only shows the report (and sets no crash
    // handlers of its own, so a failure there cannot start another).
    const c1kit::LaunchArgs launch =
        c1kit::parse_launch_args(CStringA(m_lpCmdLine).GetString());
    if (!launch.crash_report.empty()) {
        show_crash_report_file(launch.crash_report);
        return FALSE;
    }
    install_crash_handlers();

    // The game's Main Directory becomes the working directory, where kits
    // find their art (LoadOverviewMainDirectory @ 0x00402040, which read
    // only HKLM, and only when the HKCU key opened as well).  The originals
    // warned when it was missing; here the current directory is kept, and a
    // kit that then cannot find a file says which one.
    if (kit.use_main_directory) {
        const CString directory = game_directory_setting("Main Directory");
        if (!directory.IsEmpty()) {
            SetCurrentDirectoryA(directory);
        }
    }

    if (!AfxOleInit()) {
        AfxMessageBox(kit.ole_init_failed_string);
        return FALSE;
    }

    const c1kit::LaunchArgs& args = launch;
    if (!args.embedding && !args.automation) {
        // Run on its own: register the server and the Tools-menu entry, then
        // exit, as COleObjectFactory::UpdateRegistryAll plus
        // InitializeOverviewOleRegistration @ 0x00401f10 do.
        char exe_path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        c1kit::register_local_server(kit.identity, exe_path);
        c1kit::write_tool_registration(
            kit.tool_slot, kit.tool_value_prog_id,
            CStringA(load_string(kit.tool_name_string)).GetString(),
            CStringA(load_string(kit.tool_help_string)).GetString());
        return FALSE;
    }

    server_ = c1kit::create_kit_server(kit.identity, *this);
    long hresult = 0;
    if (server_ == nullptr || !server_->register_class_object(&hresult)) {
        AfxMessageBox(_T("Could not register OLE factories"));
        return FALSE;
    }

    // CreateOverviewDefaultFont @ 0x00402140.
    if (!default_font_.CreateFont(12, 6, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  ANSI_CHARSET, OUT_STROKE_PRECIS,
                                  CLIP_LH_ANGLES, PROOF_QUALITY,
                                  VARIABLE_PITCH | FF_SWISS,
                                  _T("MS Sans Serif"))) {
        return FALSE;
    }

    if (kit.create_main_window == nullptr) {
        return FALSE;
    }
    KitSheet* sheet = kit.create_main_window(default_font_);
    if (sheet == nullptr) {
        return FALSE;
    }
    m_pMainWnd = sheet;
    sheet->bring_to_front();
    run_crash_test_if_asked();
    return TRUE;
}

void KitApp::rebuild_main_window(KitSheet& old) {
    const KitDefinition& kit = kit_definition();
    if (kit.create_main_window == nullptr) {
        return;
    }
    KitSheet* next = kit.create_main_window(default_font_);
    if (next == nullptr) {
        return;
    }
    next->adopt_tool_id(old.tool_id());
    // The new window is the main one before the old goes, or MFC would take
    // the old one's destruction for the end of the application.
    m_pMainWnd = next;
    old.DestroyWindow();
    retired_windows_.emplace_back(&old);
}

int KitApp::ExitInstance() {
    if (server_ != nullptr) {
        server_->revoke_class_object();
        server_->release();
        server_ = nullptr;
    }
    return CWinApp::ExitInstance();
}

LRESULT KitApp::ProcessWndProcException(CException* error, const MSG* message) {
    // As CWinApp::ProcessWndProcException: a window that fails to be made
    // is refused, a paint is marked done, a command answers TRUE, and a
    // CUserException has already told the user.
    LRESULT result = 0;
    if (message->message == WM_CREATE) {
        result = -1;
    } else if (message->message == WM_PAINT) {
        ::ValidateRect(message->hwnd, nullptr);
    } else if (message->message == WM_COMMAND) {
        result = TRUE;
    }
    if (error->IsKindOf(RUNTIME_CLASS(CUserException))) {
        return result;
    }
    TCHAR text[512] = {};
    error->GetErrorMessage(text, 512);
    char detail[96];
    std::snprintf(detail, sizeof detail, " (%s, window message 0x%04X)",
                  error->GetRuntimeClass()->m_lpszClassName, message->message);
    report_carried_on_error(std::string(CStringA(text).GetString()) + detail);
    return result;
}

bool KitApp::on_communicate(std::int32_t header, std::int32_t payload) {
    auto* sheet = dynamic_cast<KitSheet*>(m_pMainWnd);
    if (sheet != nullptr && ::IsWindow(sheet->GetSafeHwnd())) {
        sheet->handle_kit_message(c1kit::decode_communicate(header, payload));
    }
    // The kits' Communicate handlers always answer TRUE.
    return true;
}

CString load_string(UINT id) {
    CString text;
    if (id != 0) {
        text.LoadString(id);
    }
    return text;
}

} // namespace c1kitshell

// The one application object; each kit supplies kit_definition().
c1kitshell::KitApp g_kit_app;
