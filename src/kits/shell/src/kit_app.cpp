// KitApp: start-up, OLE server registration and the Communicate route.
// Follows Observation's COverviewApp::InitInstance @ 0x00401cd0 and the
// application constructor @ 0x00401a90.

#include "c1kitshell/kit_shell.hpp"

namespace c1kitshell {
namespace {

// The game's Main Directory (LoadOverviewMainDirectory @ 0x00402040 reads
// only HKLM ...\Creatures 1\1.0, and only when the HKCU key opens as well).
// Here the machine-wide install is tried first, then the per-user key the
// game and its launcher also keep; empty when neither has one.
CString load_main_directory() {
    CString directory;
    c1kit::KitSettings* settings = c1kit::open_kit_settings(
        "Gameware Development", "Creatures 1", "1.0",
        c1kit::SettingsOpenPolicy::user_key_only);
    if (settings == nullptr) {
        return directory;
    }
    char buffer[MAX_PATH] = {};
    if (settings->read_string(c1kit::SettingsScope::machine, "Main Directory",
                              buffer, sizeof(buffer)) ||
        settings->read_string(c1kit::SettingsScope::user, "Main Directory",
                              buffer, sizeof(buffer))) {
        directory = buffer;
    }
    settings->release();
    return directory;
}

} // namespace

KitApp::KitApp() = default;

BOOL KitApp::InitInstance() {
    const KitDefinition& kit = kit_definition();

    // The originals do this during static construction of the app object
    // and warn when it fails.  Nothing the kits read depends on it being
    // found, so a missing directory keeps the current one without a
    // message box.
    if (kit.use_main_directory) {
        const CString directory = load_main_directory();
        if (!directory.IsEmpty()) {
            SetCurrentDirectoryA(directory);
        }
    }

    if (!AfxOleInit()) {
        AfxMessageBox(kit.ole_init_failed_string);
        return FALSE;
    }

    const c1kit::LaunchArgs args =
        c1kit::parse_launch_args(CStringA(m_lpCmdLine).GetString());
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
    return TRUE;
}

int KitApp::ExitInstance() {
    if (server_ != nullptr) {
        server_->revoke_class_object();
        server_->release();
        server_ = nullptr;
    }
    return CWinApp::ExitInstance();
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
