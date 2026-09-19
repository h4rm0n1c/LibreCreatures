#include <cstdio>
#include "windows_sfc_ole_host.hpp"
#include "windows_macro_host.hpp"
#include "windows_shell.hpp"
#include "windows_dde_host.hpp"

#include <cstring>
#include <fstream>

namespace creatures1::platform {

C1NativeVolumeDialogHost::C1NativeVolumeDialogHost(CWnd& owner) : owner_(owner) {}


bool C1NativeVolumeDialogHost::volume_dialog_exists() const {
    return dialog_ != nullptr && dialog_->GetSafeHwnd() != nullptr;
}

bool C1NativeVolumeDialogHost::create_volume_dialog() {
    if (volume_dialog_exists()) {
        return true;
    }
    dialog_ = std::make_unique<C1NativeVolumeDialog>(&owner_);
    if (!dialog_->Create(MAKEINTRESOURCEA(147), &owner_)) {
        dialog_.reset();
        return false;
    }
    return true;
}

void C1NativeVolumeDialogHost::discard_failed_volume_dialog() { dialog_.reset(); }


void C1NativeVolumeDialogHost::show_volume_dialog() {
    if (volume_dialog_exists()) {
        dialog_->ShowWindow(SW_SHOW);
    }
}

void C1NativeVolumeDialogHost::bring_volume_dialog_to_front() {
    if (volume_dialog_exists()) {
        dialog_->SetForegroundWindow();
    }
}

C1NativeMuteControl::C1NativeMuteControl(C1WindowsDocument& document) : document_(document) {}


bool C1NativeMuteControl::mute_is_enabled() const {
    const creatures1::application::Document* semantic =
        document_.semantic_document();
    return semantic != nullptr && semantic->mute_setting;
}

void C1NativeMuteControl::set_mute_enabled(bool enabled) {
    creatures1::application::Document* semantic =
        document_.semantic_document_mutable();
    if (semantic != nullptr) {
        semantic->mute_setting = enabled;
    }
}

void C1NativeMuteControl::stop_all_sounds() {
    if (g_active_sound_manager != nullptr) {
        g_active_sound_manager->stop_all_sounds();
    }
}

void C1NativeMuteControl::persist_mute_enabled(bool enabled) {
    HKEY registry_key = nullptr;
    if (open_c1_secondary_registry(registry_key, KEY_SET_VALUE)) {
        write_registry_dword(registry_key, "Mute", enabled ? 1u : 0u);
        RegCloseKey(registry_key);
    }
}

C1StartupHost::C1StartupHost( creatures1::application::SfcAppState& app_state) : app_state_(app_state) {
    g_active_primary_directories = &primary_directories_;
    g_active_app_state = &app_state_;
}

C1StartupHost::~C1StartupHost() {
    if (g_active_world_save_path == &world_save_path_) {
        g_active_world_save_path = nullptr;
    }
    if (g_active_classifier_names == &classifier_names_) {
        g_active_classifier_names = nullptr;
    }
    if (g_active_primary_directories == &primary_directories_) {
        g_active_primary_directories = nullptr;
    }
    if (g_active_secondary_directories == &secondary_directories_) {
        g_active_secondary_directories = nullptr;
    }
    if (g_active_app_state == &app_state_) {
        g_active_app_state = nullptr;
    }
}

bool C1StartupHost::initialize_ole() {
    return AfxOleInit() != FALSE;
}

void C1StartupHost::report_ole_initialization_failure() {
    ::MessageBoxA(nullptr, "Creatures could not initialize OLE.",
                  "Creatures", MB_OK | MB_ICONERROR);
}

void C1StartupHost::load_standard_profile_settings() {
    // InitInstance @ 0043e1d0 loads profile settings as part of the startup
    // sequence, not before it.  The concrete application installs the hook
    // because CWinApp::LoadStdProfileSettings is protected.
    if (profile_settings_loader_) {
        profile_settings_loader_();
    }
}

void C1StartupHost::install_document_template() {
    CWinApp* application = AfxGetApp();
    if (application == nullptr || document_template_ != nullptr) {
        return;
    }

    // This is the native MFC shell described by the recovered
    // SFCApp::InitInstance.  C1's clean Document remains the semantic
    // owner; these three classes only satisfy MFC's document-template
    // and OLE construction ABI at the executable boundary.
    document_template_ = new CSingleDocTemplate(
        128, RUNTIME_CLASS(C1WindowsDocument), RUNTIME_CLASS(C1MainFrame),
        RUNTIME_CLASS(C1WindowsView));
    application->AddDocTemplate(document_template_);
}

void C1StartupHost::connect_document_template_server() {
    if (document_template_ != nullptr) {
        document_server_.ConnectTemplate(kSfcDocumentClsid,
                                          document_template_, TRUE, FALSE);
    }
}

bool C1StartupHost::load_resource_directories( bool secondary, SfcAppResourceDirectories& directories) {
    directories.paths.fill({});

    HKEY registry_key = nullptr;
    const bool opened = secondary
                            ? open_c1_secondary_registry(
                                  registry_key, KEY_READ | KEY_SET_VALUE)
                            : open_c1_primary_registry(registry_key);
    if (opened) {
        for (std::size_t index = 0; index < kResourceDirectoryCount;
             ++index) {
            if (!read_registry_string(registry_key,
                                      kResourceDirectoryValueNames[index],
                                      directories.paths[index])) {
                // SFCApp::InitInstance @ 0043e1d0 writes a blank REG_SZ
                // when a path is absent and continues with the blank.  It
                // does so for both keys, but OpenCreaturesRegistryKeys
                // @ 0042f360 opens the HKLM key KEY_READ (0x20019), so the
                // HKLM write always fails, even with administrator rights.
                // Only the HKCU write can take effect.
                if (secondary) {
                    write_empty_registry_string(
                        registry_key,
                        kResourceDirectoryValueNames[index]);
                }
                directories.paths[index].clear();
            }
        }
        RegCloseKey(registry_key);
    } else {
        // A clean-room install may not have run the original installer.
        // Keep the recovered fallback useful while retaining the native
        // registry schema whenever it exists.
        const std::string directory = current_directory_with_separator();
        if (directory.empty()) {
            return false;
        }
        for (std::string& path : directories.paths) {
            path = directory;
        }
    }

    // SFCApp changes the process current directory to each set's Main
    // Directory, blank or not; a failure shows 0xef32 and startup goes on.
    // Native stores all eight paths either way, so a bad Main Directory is
    // kept and still used (World.sfc, the uninstall string).
    if (!secondary) {
        primary_directories_ = directories;
    } else {
        secondary_directories_ = directories;
        g_active_secondary_directories = &secondary_directories_;
    }
    return SetCurrentDirectoryA(directories.paths[0].c_str()) != FALSE;
}

void C1StartupHost::report_resource_directory_failure() {
    // InitInstance @ 0043e1d0 shows string resource 0xef32 when the main
    // directory cannot be made current: "Creatures could not locate its Main
    // Directory from the registry. The registry maybe corrupt. Using current
    // directory instead."  Startup continues, which is why the earlier note
    // here mistook the fallback for silence.
    CStringA message;
    message.LoadStringA(0xef32);
    AfxMessageBox(message.GetString(), MB_OK, 0);
}

void C1StartupHost::read_text_lines(std::string_view path, std::vector<std::string>& lines) const {
    lines.clear();
    HANDLE file = CreateFileA(std::string(path).c_str(), GENERIC_READ,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    std::string contents;
    std::array<char, 4096> buffer{};
    DWORD bytes_read = 0;
    while (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()),
                    &bytes_read, nullptr) && bytes_read != 0) {
        contents.append(buffer.data(), bytes_read);
    }
    CloseHandle(file);

    std::size_t line_start = 0;
    while (line_start <= contents.size()) {
        const std::size_t line_end = contents.find_first_of("\r\n",
                                                             line_start);
        if (line_end == std::string::npos) {
            if (line_start < contents.size()) {
                lines.emplace_back(contents.substr(line_start));
            }
            break;
        }
        lines.emplace_back(contents.substr(line_start,
                                           line_end - line_start));
        line_start = line_end + 1;
        if (line_start < contents.size() &&
            contents[line_start - 1] == '\r' &&
            contents[line_start] == '\n') {
            ++line_start;
        }
    }
}

void C1StartupHost::publish_classifier_name(std::string_view key,
                                            std::string_view display_name) {
    // The native writes g_classifier_name_map here; ResolveClassifierDisplayName
    // @ 00439310 reads it back with the same "family, genus, species" key that
    // load_sfc_app_classifier_names builds.
    classifier_names_[std::string(key)] = std::string(display_name);
    g_active_classifier_names = &classifier_names_;
}


void C1StartupHost::set_world_save_path(std::string_view path) {
    world_save_path_ = path;
    g_active_world_save_path = &world_save_path_;
}

void C1StartupHost::publish_uninstall_command(std::string_view path) {
    HKEY uninstall_key = nullptr;
    constexpr char uninstall_path[] =
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
        "CreaturesDeinstKey";
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, uninstall_path, 0,
                      KEY_SET_VALUE, &uninstall_key) != ERROR_SUCCESS) {
        return;
    }
    const std::string command(path);
    RegSetValueExA(uninstall_key, "UninstallString", 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(command.c_str()),
                   static_cast<DWORD>(command.size() + 1));
    RegCloseKey(uninstall_key);
}

SfcAppCommandLineMode C1StartupHost::parse_command_line() {
    const char* command_line = GetCommandLineA();
    const std::string_view arguments = command_line == nullptr
                                           ? std::string_view{}
                                           : std::string_view(command_line);
    return {
        command_line_contains(arguments, "/embedding"),
        command_line_contains(arguments, "/automation"),
    };
}

void C1StartupHost::update_document_server_registry() {
    // SFCApp::InitInstance @ 0x0043e1d0 calls UpdateRegistry((OLE_APPTYPE)3,
    // 0, 0, 1) -- OAT_DISPATCH_OBJECT.  Registering as OAT_INPLACE_SERVER
    // instead adds the embedding shape the native never writes: an
    // Insertable key and a protocol\StdFileEditing verb table.
    document_server_.UpdateRegistry(OAT_DISPATCH_OBJECT);
}

void C1StartupHost::update_ole_factory_registry() {
    // The SFC.OLE factory has to exist before either registry sweep sees it;
    // the native build gets that from a CRT static initialiser.
    register_sfc_ole_object_factory();
    COleObjectFactory::UpdateRegistryAll(TRUE);

    // UpdateRegistryAll files this executable under InprocServer32, so COM
    // looks for a DllGetClassObject that an .exe does not export and
    // CoCreateInstance answers REGDB_E_CLASSNOTREG -- which is every external
    // kit's first call.  An out-of-process server belongs under
    // LocalServer32; write it, and drop the in-process key that cannot work.
    HKEY clsid_key = nullptr;
    if (RegCreateKeyExA(
            HKEY_CLASSES_ROOT,
            "CLSID\\{77C733E1-6797-11CF-BBF2-0020AF71E433}\\LocalServer32",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &clsid_key, nullptr) ==
        ERROR_SUCCESS) {
        char module_path[MAX_PATH]{};
        if (GetModuleFileNameA(nullptr, module_path, MAX_PATH) != 0) {
            const std::string quoted = std::string("\"") + module_path + "\"";
            RegSetValueExA(clsid_key, nullptr, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(quoted.c_str()),
                           static_cast<DWORD>(quoted.size() + 1));
        }
        RegCloseKey(clsid_key);
        delete_own_sfc_inproc_server_registration();
    }
}

void C1StartupHost::delete_own_sfc_inproc_server_registration() {
    // Only the key UpdateRegistryAll just wrote -- this executable -- may be
    // removed.  On a Community Edition install OLEKitProxy.dll registers
    // itself here so that a kit launched under Wine gets the in-process proxy
    // that forwards SFC.OLE to the running game over its pipe; deleting that
    // registration unconditionally left every kit's CreateDispatch("SFC.OLE")
    // failing with an empty COleException.
    static const char* const inproc_key =
        "CLSID\\{77C733E1-6797-11CF-BBF2-0020AF71E433}\\InprocServer32";

    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, inproc_key, 0, KEY_QUERY_VALUE,
                      &key) != ERROR_SUCCESS) {
        return;
    }
    char registered[MAX_PATH]{};
    DWORD size = sizeof(registered);
    DWORD type = 0;
    const LONG read = RegQueryValueExA(key, nullptr, nullptr, &type,
                                       reinterpret_cast<BYTE*>(registered),
                                       &size);
    RegCloseKey(key);
    if (read != ERROR_SUCCESS || type != REG_SZ) {
        return;
    }

    char module_path[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, module_path, MAX_PATH) == 0) {
        return;
    }
    std::string value(registered);
    if (!value.empty() && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    if (_stricmp(value.c_str(), module_path) == 0) {
        RegDeleteKeyA(HKEY_CLASSES_ROOT, inproc_key);
    }
}

void C1StartupHost::write_patch_registry_metadata(
    const SfcAppPatchMetadata& metadata) {
    // Recovered SFCApp::InitInstance @ 0043e1d0 opens the "Patch" subkey of the
    // Creatures registry key and writes three REG_SZ values: "P&G" carries the
    // product message, "Build ID" the version, "Build type" the build.  The
    // native builds its version with snprintf("%s.%s", "1.0", "5"); the clean
    // policy supplies the same string already joined.
    HKEY patch_key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER,
                        (std::string(kC1UserRegistryPath) + "\\Patch").c_str(),
                        0, nullptr, 0, KEY_SET_VALUE, nullptr, &patch_key,
                        nullptr) != ERROR_SUCCESS) {
        return;
    }

    const auto write = [patch_key](const char* name, std::string_view value) {
        const std::string text(value);
        RegSetValueExA(patch_key, name, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(text.c_str()),
                       static_cast<DWORD>(text.size() + 1));
    };
    write("P&G", metadata.product_message);
    write("Build ID", metadata.build_id);
    write("Build type", metadata.build_type);
    RegCloseKey(patch_key);
}


void C1StartupHost::delete_autorun_registration() {
    // The recovered startup removes the installer's auto-run key outright:
    // RegDeleteKeyA(HKEY_LOCAL_MACHINE,
    //               "Software\\Gameware Development\\CreaturesAutoRun").
    RegDeleteKeyA(HKEY_LOCAL_MACHINE,
                  "Software\\Gameware Development\\CreaturesAutoRun");
}


void C1StartupHost::register_ole_factories() {
    register_sfc_ole_object_factory();
    COleObjectFactory::RegisterAll();
}

bool C1StartupHost::main_window_exists() const {
    const CWinApp* application = AfxGetApp();
    return application != nullptr && application->m_pMainWnd != nullptr;
}

bool C1StartupHost::prepare_embedded_main_window(std::string_view world_path) {
    if (document_template_ == nullptr) {
        return false;
    }

    // Ghidra's recovered virtual calls are CWinApp::OnFileNew for USER
    // privilege and CWinApp::OpenDocumentFile(World.sfc) otherwise.
    // Route both through the registered CSingleDocTemplate.  That keeps
    // document, view, frame, main-window, and OnNewDocument/serialization
    // behavior in MFC's actual template machinery instead of fabricating
    // a frame beside the document model.
    CDocument* document = nullptr;
    if (app_state_.privilege_level ==
        creatures1::application::PrivilegeLevel::user) {
        document = document_template_->OpenDocumentFile(nullptr);
    } else {
        const CString world_file(world_path.data(),
                                 static_cast<int>(world_path.size()));
        document = document_template_->OpenDocumentFile(world_file);
    }
    if (document == nullptr) {
        return false;
    }

    CWinApp* application = AfxGetApp();
    main_frame_ = application == nullptr
                      ? nullptr
                      : DYNAMIC_DOWNCAST(C1MainFrame,
                                         application->m_pMainWnd);
    return main_frame_ != nullptr;
}

void C1StartupHost::accept_file_drops() {
    if (main_frame_ != nullptr) {
        main_frame_->DragAcceptFiles(TRUE);
    }
}

void C1StartupHost::ensure_sound_system() {
    if (sound_manager_ != nullptr) {
        return;
    }
    const HWND owner_window = main_frame_ == nullptr
                                  ? nullptr
                                  : main_frame_->GetSafeHwnd();
    sound_host_ = std::make_unique<creatures1::platform::WindowsSoundSystemHost>(
        owner_window, primary_directories_.paths[1]);
    sound_manager_ = creatures1::sound::initialize_sound_system(*sound_host_);
    g_active_sound_manager = sound_manager_.get();
}

void C1StartupHost::start_pipe_server_if_needed() {
    if (main_frame_ == nullptr || pipe_server_ != nullptr) {
        return;
    }

    // InitializeDdeService @ 0x00401090 publishes the "Vivarium" service the
    // kits converse over; the native runs it from a CRT initialiser, which is
    // before there is a document for the conversation macros to own, so it
    // starts here with the rest of the external transports.
    if (auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument,
                                          main_frame_->GetActiveDocument())) {
        start_dde_service(*document);
    }

    auto selected_creature_query = [this]() -> creatures1::objects::Object* {
        if (main_frame_ == nullptr) {
            return nullptr;
        }
        auto* document = DYNAMIC_DOWNCAST(
            C1WindowsDocument, main_frame_->GetActiveDocument());
        if (document == nullptr) {
            return nullptr;
        }
        auto* creature = document->selected_creature();
        if (creature == nullptr) {
            return nullptr;
        }
        try {
            return &document->object_for_creature(*creature);
        } catch (const std::out_of_range&) {
            return nullptr;
        }
    };

    // The macro chain is document-scoped, so the factory resolves the active
    // document per call rather than capturing one that may be closed.
    auto macro_holder_factory =
        [this](creatures1::scripting::MacroExecutionMode mode)
        -> std::unique_ptr<creatures1::scripting::MacroHolder> {
        if (main_frame_ == nullptr) {
            return nullptr;
        }
        auto* document = DYNAMIC_DOWNCAST(
            C1WindowsDocument, main_frame_->GetActiveDocument());
        if (document == nullptr) {
            return nullptr;
        }
        macro_host_ = std::make_unique<WindowsMacroHost>(*document);
        return std::make_unique<creatures1::scripting::MacroHolder>(
            mode, *macro_host_);
    };

    pipe_server_ = std::make_unique<
        creatures1::platform::WindowsPipeServerBoundary>(
        main_frame_->GetSafeHwnd(),
        std::move(macro_holder_factory),
        std::move(selected_creature_query),
        creatures1::platform::WindowsPipeServerBoundary::KitShutdown{});
    main_frame_->bind_pipe_server_boundary(pipe_server_.get());
    if (!pipe_server_->start()) {
        main_frame_->bind_pipe_server_boundary(nullptr);
        pipe_server_.reset();
    }
}

void C1StartupHost::show_startup_tip_dialog() {
    C1TipDialogPlatform platform(
        primary_directories_.paths[0], AfxGetMainWnd());
    // Startup honours "Show Tips on StartUp"; Help > Tip of the Day does not.
    creatures1::ui::show_tip_dialog_if_enabled_at_startup(platform);
}

void C1StartupHost::destroy_sound_manager_native() {
    if (g_active_sound_manager == sound_manager_.get()) {
        g_active_sound_manager = nullptr;
    }
    sound_manager_.reset();
    sound_host_.reset();
}

void C1StartupHost::terminate_launched_kit_processes_native() {
    if (main_frame_ != nullptr) {
        // The sweep is a MainFrameLifecyclePlatform operation; reach it
        // through that interface rather than widening the frame's own access.
        static_cast<creatures1::application::MainFrameLifecyclePlatform&>(
            *main_frame_)
            .terminate_launched_kit_processes();
    }
}

void C1StartupHost::stop_pipe_server_native() {
    stop_dde_service();
    if (pipe_server_ == nullptr) {
        return;
    }
    pipe_server_->stop();
    if (main_frame_ != nullptr) {
        main_frame_->bind_pipe_server_boundary(nullptr);
    }
    pipe_server_.reset();
}

C1ShutdownHost::C1ShutdownHost(C1StartupHost& startup_host, CWinApp& application)
    : startup_host_(startup_host), application_(application) {}


void report_startup_failure(const char* detail) {
    const std::string message =
        std::string("C1 startup failed: ") + (detail == nullptr ? "" : detail);
    ::OutputDebugStringA((message + "\n").c_str());
    {
        // Close the log before the modal box: a startup failure with no user
        // to dismiss the dialog must still leave the diagnostic on disk.
        std::ofstream log("Creatures.startup.log", std::ios::app);
        if (log.is_open()) {
            log << message << '\n';
        }
    }
    AfxMessageBox(message.c_str(), MB_OK | MB_ICONERROR, 0);
}

void C1ShutdownHost::terminate_launched_kit_processes() {
    startup_host_.terminate_launched_kit_processes_native();
}


void C1ShutdownHost::stop_pipe_server() {
    startup_host_.stop_pipe_server_native();
}

void C1ShutdownHost::destroy_sound_manager() {
    startup_host_.destroy_sound_manager_native();
}

void C1ShutdownHost::forward_default_exit_instance() {
    exit_code_ = application_.CWinApp::ExitInstance();
}


} // namespace creatures1::platform
