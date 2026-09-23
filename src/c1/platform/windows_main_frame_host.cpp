#include "windows_embedded_kit_host.hpp"
#include "windows_creature_transfer_host.hpp"
#include "windows_macro_host.hpp"
#include "windows_creature_hosts.hpp"
#include "windows_shell.hpp"
#include "windows_com_host.hpp"

#include <stdexcept>

namespace creatures1::platform {

// Owns a closed main frame between PostNcDestroy and the end of ExitInstance.
std::unique_ptr<C1MainFrame>& deferred_main_frame_release() {
    static std::unique_ptr<C1MainFrame> frame;
    return frame;
}

// The native publishes the constructed frame into g_CMainFrame (and the event
// bar and toolbar into their own globals) at the end of CreateObject, and
// clears it first thing in the destructor.  The port keeps one pointer: the
// event bar and toolbar are reached through the frame that owns them.
namespace {
C1MainFrame* g_active_main_frame = nullptr;
} // namespace

C1MainFrame::C1MainFrame() {
    // CreateObject is the allocation-time factory.  MFC has already run the
    // CFrameWnd base and every control member by the time this body executes,
    // so construct_frame_controls() records that step rather than repeating it.
    frame_policy_ = creatures1::application::CMainFrame::CreateObject(*this);
}

C1MainFrame::~C1MainFrame() {
    // Reset explicitly: ~CMainFrame calls back through this object, which must
    // still be complete when it does.
    frame_policy_.reset();
}

void C1MainFrame::construct_frame_controls() {
    embedded_kit_dispatch_ = {};
    embedded_kit_process_handles_ = {};
    active_embedded_tool_count_ = 0;
}

C1MainFrame* active_main_frame() { return g_active_main_frame; }

void C1MainFrame::publish_main_frame() { g_active_main_frame = this; }

void C1MainFrame::unpublish_main_frame() {
    if (g_active_main_frame == this) {
        g_active_main_frame = nullptr;
    }
}

void C1MainFrame::destroy_frame_controls() {
    // The controls are C1MainFrame data members, so MFC destroys them with the
    // frame.  The native's explicit teardown order is the compiler's here.
}

void C1MainFrame::terminate_launched_kit_processes() {
    KitProcessAdapter adapter(*this);
    creatures1::application::terminate_launched_kit_processes(adapter, 0);
}

std::string C1MainFrame::embedded_tool_message(std::size_t tool_index) const {
    return tool_index < embedded_kit_definitions_.size()
               ? embedded_kit_definitions_[tool_index].display_name
               : std::string();
}

std::string C1MainFrame::forward_default_message(
    std::uint32_t command_id) const {
    CString message;
    CFrameWnd::GetMessageString(static_cast<UINT>(command_id), message);
    return std::string(message.GetString());
}

creatures1::application::MainFrameWindowRect C1MainFrame::window_rect() const {
    RECT native_rect{};
    GetWindowRect(&native_rect);
    return {native_rect.left, native_rect.top, native_rect.right,
            native_rect.bottom};
}

void C1MainFrame::stop_pipe_server() {
    if (pipe_server_boundary_ != nullptr) {
        pipe_server_boundary_->stop();
    }
}

void C1MainFrame::revoke_ole_factories() { COleObjectFactory::RevokeAll(); }

void C1MainFrame::forward_default_close() { CFrameWnd::OnClose(); }

bool C1MainFrame::window_still_exists() const {
    return ::IsWindow(GetSafeHwnd()) != FALSE;
}

void C1MainFrame::post_quit_message(int exit_code) {
    // CMainFrame::OnClose @ 0x00421900 ends with PostQuitMessage(0) and a
    // plain return, letting the message loop drain into ExitInstance.
    // Calling ExitProcess here skipped SFCApp::ExitInstance outright, so the
    // kit sweep, the pipe-server stop and the sound teardown never ran.
    ::PostQuitMessage(exit_code);
}

void C1MainFrame::shutdown_embedded_kit_tool(std::size_t tool_index) {
    WindowsEmbeddedKitHost host(*this);
    creatures1::application::shutdown_embedded_kit_tool(
        host, tool_index, active_embedded_tool_count_);
}

void C1MainFrame::update_document_world() {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr) {
        return;
    }
    try {
        document->update_world_tick();
    } catch (const std::exception& error) {
        document->recover_world_update_after_boundary_failure(error);
    } catch (CException* error) {
        char message[512] = {};
        if (error != nullptr) {
            error->GetErrorMessage(message, sizeof(message));
            error->Delete();
        }
        document->recover_world_update_after_boundary_failure(
            std::runtime_error(message[0] == '\0'
                                   ? "MFC world-update exception"
                                   : message));
    } catch (...) {
        document->recover_world_update_after_boundary_failure(
            std::runtime_error("unknown world-update exception"));
    }
    refresh_event_bar_status(*document);
}

// --- MainFrameActivationPlatform, folded onto the frame -------------------

void C1MainFrame::forward_activate_frame(int show_command) {
    activate_frame_native(show_command);
}

void C1MainFrame::rebuild_creature_selection_menu() {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr) {
        document->rebuild_creature_selection_menu();
    }
}

std::size_t C1MainFrame::favourite_place_count() const {
    const C1WindowsDocument* document = DYNAMIC_DOWNCAST(
        C1WindowsDocument,
        const_cast<C1MainFrame*>(this)->GetActiveDocument());
    return document == nullptr ? 0 : document->favourite_place_count();
}

std::string C1MainFrame::favourite_place_name(std::size_t index) const {
    const C1WindowsDocument* document = DYNAMIC_DOWNCAST(
        C1WindowsDocument,
        const_cast<C1MainFrame*>(this)->GetActiveDocument());
    return document == nullptr ? std::string()
                               : document->favourite_place_name(index);
}

creatures1::ui::MenuHandle* C1MainFrame::favourite_places_menu() {
    // find_camera_submenu hands back a handle owned by the menu platform, and
    // the platform dies with this call.  Returning that pointer left every
    // caller holding a dangling handle that happened to work until the stack
    // was reused -- removing a middle favourite place relabelled one entry and
    // then failed to remove the last.  Copy the HMENU into a frame-owned handle.
    C1NativeCreatureSelectionMenuPlatform menu_platform(*this);
    const auto* found = dynamic_cast<const C1NativeMenuHandle*>(
        creatures1::ui::find_camera_submenu(menu_platform));
    if (found == nullptr) {
        return nullptr;
    }
    camera_menu_handle_ = C1NativeMenuHandle(found->native_menu());
    return &camera_menu_handle_;
}

void C1MainFrame::show_missing_favourite_places_menu_warning() {
    AfxMessageBox("Creatures could not find the Camera menu.\n",
                  MB_ICONWARNING, 0);
}

void C1MainFrame::append_favourite_place_menu_item(
    creatures1::ui::MenuHandle& menu, std::uint32_t command_id,
    std::string_view name) {
    C1NativeCreatureSelectionMenuPlatform menu_platform(*this);
    menu_platform.append_item(menu, command_id, name);
}

// --- KitProcessAdapter ----------------------------------------------------

bool C1MainFrame::KitProcessAdapter::main_frame_exists() const {
    return g_active_main_frame != nullptr;
}

bool C1MainFrame::KitProcessAdapter::take_launched_kit_process(
    std::size_t slot) {
    if (slot >= creatures1::application::kEmbeddedKitSlotCount) {
        return false;
    }
    taken_process_ = frame_.embedded_kit_process_handle(slot);
    frame_.embedded_kit_process_handle(slot) = 0;
    return taken_process_ != 0;
}

bool C1MainFrame::KitProcessAdapter::query_taken_process_exit_code(
    std::uint32_t& exit_code) const {
    DWORD native_exit_code = 0;
    if (GetExitCodeProcess(reinterpret_cast<HANDLE>(taken_process_),
                           &native_exit_code) == FALSE) {
        return false;
    }
    exit_code = static_cast<std::uint32_t>(native_exit_code);
    return true;
}

bool C1MainFrame::KitProcessAdapter::debug_console_exists() const {
    return active_debug_console() != nullptr;
}

void C1MainFrame::KitProcessAdapter::log_kit_termination(std::size_t slot) {
    // TerminateLaunchedKitProcesses @ 00444b50 logs under category 0x2000.
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(*console, 0x2000,
                                      "ToolTerminateAll: Terminating kit %d\n",
                                      static_cast<int>(slot));
    }
}

void C1MainFrame::KitProcessAdapter::terminate_taken_kit_process(
    std::uint32_t exit_code) {
    TerminateProcess(reinterpret_cast<HANDLE>(taken_process_),
                     static_cast<UINT>(exit_code));
}

void C1MainFrame::KitProcessAdapter::close_taken_kit_process() {
    CloseHandle(reinterpret_cast<HANDLE>(taken_process_));
    taken_process_ = 0;
}

bool C1MainFrame::create() {
    const RECT initial_rect{CW_USEDEFAULT, CW_USEDEFAULT, 640, 480};
    return Create(
               nullptr, "Creatures", WS_OVERLAPPEDWINDOW, initial_rect,
               nullptr, MAKEINTRESOURCEA(128), 0, nullptr) != FALSE;
}

void C1MainFrame::bind_pipe_server_boundary( creatures1::platform::WindowsPipeServerBoundary* boundary) {
    pipe_server_boundary_ = boundary;
}

BOOL C1MainFrame::PreCreateWindow(CREATESTRUCT& create_struct) {
    pending_pre_create_struct_ = &create_struct;
    pending_pre_create_result_ = FALSE;
    creatures1::application::MainFrameCreateParameters parameters;
    creatures1::application::prepare_main_frame_window(*this,
                                                        parameters);
    pending_pre_create_struct_ = nullptr;
    return pending_pre_create_result_;
}

void C1MainFrame::OnDestroy() {
    CFrameWnd::OnDestroy();
}

afx_msg void C1MainFrame::OnClose() {
    if (frame_policy_ == nullptr) {
        CFrameWnd::OnClose();
        return;
    }
    frame_policy_->OnClose();
}

afx_msg void C1MainFrame::OnViewToolbar() {
    ShowControlBar(&main_toolbar_, !main_toolbar_.IsWindowVisible(),
                   FALSE);
}

afx_msg void C1MainFrame::OnUpdateViewToolbar(CCmdUI* command_ui) {
    command_ui->SetCheck(main_toolbar_.IsWindowVisible());
}

afx_msg void C1MainFrame::OnViewStatusBar() {
    ShowControlBar(&event_bar_, !event_bar_.IsWindowVisible(), FALSE);
}

afx_msg void C1MainFrame::OnUpdateViewStatusBar(CCmdUI* command_ui) {
    command_ui->SetCheck(event_bar_.IsWindowVisible());
}

void C1MainFrame::PostNcDestroy() {
    // CFrameWnd::OnClose goes on using the frame after the document close
    // has destroyed its window, and CWnd::DestroyWindow is virtual, so
    // releasing the frame here leaves that tail call reading a freed vtable
    // and jumping to whatever the released memory holds.  The frame is handed
    // to the application instead and released once the message loop has
    // unwound, which is where the native still has it allocated.
    deferred_main_frame_release().reset(this);
}

creatures1::application::MainFrameWindowRect C1MainFrame::default_window_rect() const {
    return {0, 0, 640, 480};
}

bool C1MainFrame::read_saved_window_rect( creatures1::application::MainFrameWindowRect& rect) const {
    HKEY registry_key = nullptr;
    if (!open_c1_secondary_registry(registry_key, KEY_READ)) {
        return false;
    }
    RECT native_rect{};
    const bool result = read_c1_window_rect(registry_key, native_rect);
    RegCloseKey(registry_key);
    if (result) {
        rect = {native_rect.left, native_rect.top, native_rect.right,
                native_rect.bottom};
    }
    return result;
}

void C1MainFrame::save_window_rect( const creatures1::application::MainFrameWindowRect& rect) {
    HKEY registry_key = nullptr;
    if (!open_c1_secondary_registry(registry_key, KEY_SET_VALUE)) {
        return;
    }
    const RECT native_rect{rect.left, rect.top, rect.right, rect.bottom};
    write_c1_window_rect(registry_key, native_rect);
    RegCloseKey(registry_key);
}

int C1MainFrame::minimum_window_x() const {
    return GetSystemMetrics(SM_CXSCREEN);
}

int C1MainFrame::minimum_window_y() const {
    return GetSystemMetrics(SM_CYSCREEN);
}

void C1MainFrame::forward_default_pre_create( creatures1::application::MainFrameCreateParameters& parameters) {
    if (pending_pre_create_struct_ == nullptr) {
        return;
    }
    pending_pre_create_struct_->x = parameters.x;
    pending_pre_create_struct_->y = parameters.y;
    pending_pre_create_struct_->cx = parameters.width;
    pending_pre_create_struct_->cy = parameters.height;
    pending_pre_create_struct_->style = parameters.style;
    pending_pre_create_result_ =
        CFrameWnd::PreCreateWindow(*pending_pre_create_struct_);
}

std::string C1MainFrame::dispatch_pipe_command( std::string_view command) {
    return pipe_server_boundary_ == nullptr
               ? std::string("ERROR\x1eServer unavailable")
               : pipe_server_boundary_->dispatch_command(command);
}

void C1MainFrame::signal_pipe_server_command_complete() {
    if (pipe_server_boundary_ != nullptr) {
        pipe_server_boundary_->signal_command_complete();
    }
}

BEGIN_MESSAGE_MAP(C1MainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_CLOSE()
    ON_COMMAND(0xe800, &C1MainFrame::OnViewToolbar)
    ON_UPDATE_COMMAND_UI(0xe800, &C1MainFrame::OnUpdateViewToolbar)
    ON_COMMAND(0xe801, &C1MainFrame::OnViewStatusBar)
    ON_UPDATE_COMMAND_UI(0xe801, &C1MainFrame::OnUpdateViewStatusBar)
    ON_COMMAND(107, &C1MainFrame::OnTipOfDay)
    // Recovered ToggleDebugLogCategory @ 00434a10 selects the mask bit with
    // (command_id - 0x8028) & 0x1f, so the Log popup is one contiguous range.
    ON_COMMAND_RANGE(32808, 32823, &C1MainFrame::OnToggleDebugLogCategory)
    ON_UPDATE_COMMAND_UI_RANGE(32808, 32823,
                               &C1MainFrame::OnUpdateToggleDebugLogCategory)
    ON_COMMAND(32870, &C1MainFrame::OnToggleDebugLogging)
    ON_UPDATE_COMMAND_UI(32870, &C1MainFrame::OnUpdateToggleDebugLogging)
    ON_COMMAND(32786, &C1MainFrame::OnToggleCreaturesBurble)
    ON_UPDATE_COMMAND_UI(32786, &C1MainFrame::OnUpdateToggleCreaturesBurble)
    ON_COMMAND(32840, &C1MainFrame::OnEuthanasia)
    ON_COMMAND(32865, &C1MainFrame::OnForceAgeing)
    ON_UPDATE_COMMAND_UI(32865, &C1MainFrame::OnUpdateForceAgeing)
    ON_COMMAND(32807, &C1MainFrame::OnInfectCurrentNorn)
    ON_COMMAND(32864, &C1MainFrame::OnImportCreature)
    ON_COMMAND(32863, &C1MainFrame::OnExportCurrentCreature)
    ON_UPDATE_COMMAND_UI(32863, &C1MainFrame::OnUpdateExportCurrentCreature)
    ON_COMMAND(32804, &C1MainFrame::OnCreateMaleNorn)
    ON_COMMAND(32805, &C1MainFrame::OnCreateFemaleNorn)
    ON_COMMAND(32897, &C1MainFrame::OnMuteCreatureVoices)
    ON_UPDATE_COMMAND_UI(32897, &C1MainFrame::OnUpdateMuteCreatureVoices)
    ON_COMMAND(32839, &C1MainFrame::OnAddFavouritePlace)
    ON_COMMAND(32867, &C1MainFrame::OnRemoveFavouritePlace)
    ON_COMMAND(32896, &C1MainFrame::OnShowCaosConsole)
    ON_UPDATE_COMMAND_UI(32896, &C1MainFrame::OnUpdateShowCaosConsole)
    ON_COMMAND(32869, &C1MainFrame::OnShowDebugConsole)
    ON_UPDATE_COMMAND_UI(32869, &C1MainFrame::OnUpdateShowDebugConsole)
    ON_COMMAND(32771, &C1MainFrame::OnToggleEyeView)
    ON_UPDATE_COMMAND_UI(32771, &C1MainFrame::OnUpdateToggleEyeView)
    ON_COMMAND(32777, &C1MainFrame::OnInstantVerbVocabulary)
    ON_COMMAND_RANGE(0x8086, 0x8099, &C1MainFrame::OnEmbeddedKitTool)
    ON_UPDATE_COMMAND_UI_RANGE(0x8086, 0x8099,
                               &C1MainFrame::OnUpdateEmbeddedKitTool)
    ON_COMMAND_RANGE(40000, 40099, &C1MainFrame::OnSelectCreature)
    ON_COMMAND_RANGE(0x8053, 0x805e, &C1MainFrame::OnFavouritePlace)
    ON_COMMAND(32783, &C1MainFrame::OnCameraTrackSelectedCreature)
    ON_UPDATE_COMMAND_UI(32783,
                         &C1MainFrame::OnUpdateCameraTrackSelectedCreature)
    ON_COMMAND(32774, &C1MainFrame::OnInfiniteWorld)
    ON_UPDATE_COMMAND_UI(32774, &C1MainFrame::OnUpdateInfiniteWorld)
    ON_COMMAND(32866, &C1MainFrame::OnSmoothScrolling)
    ON_UPDATE_COMMAND_UI(32866, &C1MainFrame::OnUpdateSmoothScrolling)
    ON_COMMAND(32891, &C1MainFrame::OnMuteSounds)
    ON_COMMAND(32892, &C1MainFrame::OnInformativeCreaturesMenu)
    ON_UPDATE_COMMAND_UI(32891, &C1MainFrame::OnUpdateMuteSounds)
    ON_UPDATE_COMMAND_UI(32892, &C1MainFrame::OnUpdateInformativeCreaturesMenu)
    ON_COMMAND(32894, &C1MainFrame::OnVolume)
    ON_COMMAND(32832, &C1MainFrame::OnWebConnect)
    ON_COMMAND(57664, &C1MainFrame::OnAbout)
    ON_WM_TIMER()
    ON_WM_QUERYNEWPALETTE()
    ON_WM_PALETTECHANGED()
    ON_WM_GETMINMAXINFO()
    ON_WM_INITMENUPOPUP()
    ON_MESSAGE(0x402, &C1MainFrame::OnPipeServerCommand)
    ON_MESSAGE(0x401, &C1MainFrame::OnShutdownEmbeddedKitTool)
END_MESSAGE_MAP()

int C1MainFrame::OnCreate(LPCREATESTRUCT create_struct) {
    pending_create_struct_ = create_struct;
    const int result = creatures1::application::initialize_main_frame(*this);
    pending_create_struct_ = nullptr;
    return result;
}

LRESULT C1MainFrame::OnPipeServerCommand(WPARAM wparam, LPARAM) {
    auto* posted_command =
        reinterpret_cast<creatures1::scripting::PipeServerCommandHandle*>(
            wparam);
    return creatures1::application::handle_pipe_server_command(
        *this, posted_command);
}

LRESULT C1MainFrame::OnShutdownEmbeddedKitTool(WPARAM tool_index, LPARAM) {
    // Native: `if (tool_index < 0x14) { ShutdownEmbeddedKitTool(tool_index); }
    // return 0;` -- 20 embedded-tool slots, same bound as the rest of the
    // embedded-kit machinery.
    if (tool_index < 0x14) {
        shutdown_embedded_kit_tool(static_cast<std::size_t>(tool_index));
    }
    return 0;
}

int C1MainFrame::create_frame_base() {
    return pending_create_struct_ == nullptr
               ? -1
               : CFrameWnd::OnCreate(pending_create_struct_);
}

creatures1::application::MainMenuHandle* C1MainFrame::root_menu() {
    CMenu* menu = GetMenu();
    main_menu_handle_.reset(menu == nullptr ? nullptr : menu->GetSafeHmenu());
    return main_menu_handle_.native_menu() == nullptr ? nullptr
                                                       : &main_menu_handle_;
}

std::size_t C1MainFrame::menu_item_count(
    const creatures1::application::MainMenuHandle& menu) const {
    const auto* native = dynamic_cast<const C1NativeMainMenuHandle*>(&menu);
    if (native == nullptr || native->native_menu() == nullptr) {
        return 0;
    }
    const int count = ::GetMenuItemCount(native->native_menu());
    return count < 0 ? 0 : static_cast<std::size_t>(count);
}

const char* C1MainFrame::menu_item_text(
    const creatures1::application::MainMenuHandle& menu,
    std::size_t position) const {
    const auto* native = dynamic_cast<const C1NativeMainMenuHandle*>(&menu);
    if (native == nullptr || native->native_menu() == nullptr ||
        position > static_cast<std::size_t>(UINT_MAX)) {
        return nullptr;
    }
    std::array<char, 256> text{};
    const int length = ::GetMenuStringA(
        native->native_menu(), static_cast<UINT>(position), text.data(),
        static_cast<int>(text.size()), MF_BYPOSITION);
    if (length <= 0) {
        main_menu_item_text_.clear();
        return nullptr;
    }
    main_menu_item_text_.assign(text.data(), static_cast<std::size_t>(length));
    return main_menu_item_text_.c_str();
}

bool C1MainFrame::replace_menu_item_text(
    creatures1::application::MainMenuHandle& menu, std::size_t position,
    std::uint32_t string_resource_id) {
    auto* native = dynamic_cast<C1NativeMainMenuHandle*>(&menu);
    if (native == nullptr || native->native_menu() == nullptr ||
        position > static_cast<std::size_t>(UINT_MAX)) {
        return false;
    }
    CStringA localized_text;
    if (!localized_text.LoadStringA(static_cast<UINT>(string_resource_id))) {
        return false;
    }
    return ::ModifyMenuA(native->native_menu(), static_cast<UINT>(position),
                          MF_BYPOSITION | MF_STRING, 0,
                          localized_text.GetString()) != FALSE;
}

void C1MainFrame::show_missing_root_menu_warning() {
    AfxMessageBox("Creatures could not find root menu.\n", MB_ICONWARNING, 0);
}

void C1MainFrame::show_unsupported_language_warning() {
    AfxMessageBox(
        "Creatures failed to localise all menu items.\n"
        "This version does not support your Window's language.\n"
        "Contact CyberLife Technology Limited for information on supported "
        "languages.",
        MB_ICONWARNING, 0);
}

[[noreturn]] void C1MainFrame::terminate_process(int exit_code) {
    ::ExitProcess(static_cast<UINT>(exit_code));
}

BEGIN_MESSAGE_MAP(C1MainToolBar, CToolBar)
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL C1MainToolBar::OnEraseBkgnd(CDC* dc) {
    // Native's MyToolBar leaves erasing to the system ToolbarWindow32 class,
    // which on Windows fills COLOR_BTNFACE.  Wine's comctl32 toolbar erases
    // nothing, so the bar showed black and a button changing state (the eye
    // view check) kept its old pixels under the new glyph.  Fill explicitly.
    // CControlBar::EraseNonClient sends this with a window DC clipped to
    // the border area, so fill whatever the DC exposes, not just the client.
    CRect exposed;
    dc->GetClipBox(&exposed);
    dc->FillSolidRect(&exposed, ::GetSysColor(COLOR_BTNFACE));
    return TRUE;
}

bool C1MainFrame::WindowsToolbarPlatform::create_toolbar(
    std::uintptr_t parent_window, std::uint32_t window_style,
    std::uint32_t control_id) {
    return frame_.main_toolbar_.Create(reinterpret_cast<CWnd*>(parent_window),
                                       window_style, control_id) != FALSE;
}

bool C1MainFrame::WindowsToolbarPlatform::load_toolbar_bitmap(
    std::uint32_t resource_id) {
    return frame_.main_toolbar_.LoadBitmap(resource_id) != FALSE;
}

bool C1MainFrame::WindowsToolbarPlatform::set_button_count(
    std::uint32_t count) {
    return frame_.main_toolbar_.SetButtons(nullptr,
                                           static_cast<int>(count)) != FALSE;
}

bool C1MainFrame::WindowsToolbarPlatform::set_button(
    std::uint32_t index, std::uint32_t command_id, std::uint32_t style,
    std::uint32_t image_index) {
    frame_.main_toolbar_.SetButtonInfo(
        static_cast<int>(index), static_cast<UINT>(command_id),
        static_cast<UINT>(style), static_cast<int>(image_index));
    return true;
}

std::uint32_t C1MainFrame::WindowsToolbarPlatform::button_command(
    std::uint32_t index) const {
    TBBUTTON button_info{};
    if (!frame_.main_toolbar_.GetToolBarCtrl().GetButton(
            static_cast<int>(index), &button_info)) {
        return 0;
    }
    return static_cast<std::uint32_t>(button_info.idCommand);
}

void C1MainFrame::WindowsToolbarPlatform::remove_button(std::uint32_t index) {
    frame_.main_toolbar_.GetToolBarCtrl().DeleteButton(
        static_cast<int>(index));
}

creatures1::ui::ToolbarRect C1MainFrame::WindowsToolbarPlatform::item_rect(
    std::uint32_t index) const {
    CRect rect;
    frame_.main_toolbar_.GetItemRect(static_cast<int>(index), &rect);
    return {rect.left, rect.top, rect.right, rect.bottom};
}

bool C1MainFrame::WindowsToolbarPlatform::create_creature_selector(
    const creatures1::ui::ToolbarRect& rect,
    std::uintptr_t /*parent_window*/, std::uint32_t control_id) {
    // MyToolBar::Create parents the selector to the toolbar itself, not to
    // whatever parent_window the frame passed into create_toolbar -- this
    // adapter already knows that toolbar, so the generic handle argument
    // is unused here.
    CRect selector_rect(rect.left, rect.top, rect.right, rect.bottom);
    return frame_.creature_selector_.Create(
               WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWN, selector_rect,
               &frame_.main_toolbar_,
               static_cast<UINT>(control_id)) != FALSE;
}

void C1MainFrame::WindowsToolbarPlatform::
    populate_embedded_kit_menu_and_toolbar() {
    frame_.populate_embedded_kit_menu_and_toolbar();
}

bool C1MainFrame::WindowsToolbarPlatform::uses_system_gui_font() const {
    return GetSystemMetrics(0x2a) != 0;
}

bool C1MainFrame::WindowsToolbarPlatform::set_selector_font(
    const creatures1::ui::ToolbarFontSpec& font) {
    frame_.selector_font_.CreateFontA(
        font.height, 0, 0, 0, font.weight, FALSE, FALSE, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        FF_DONTCARE, std::string(font.family).c_str());
    if (frame_.selector_font_.GetSafeHandle() == nullptr) {
        return false;
    }
    frame_.creature_selector_.SetFont(&frame_.selector_font_, TRUE);
    return true;
}

void C1MainFrame::WindowsToolbarPlatform::use_default_selector_font() {
    frame_.creature_selector_.SetFont(
        CFont::FromHandle(static_cast<HFONT>(::GetStockObject(SYSTEM_FONT))),
        TRUE);
}

void C1MainFrame::WindowsToolbarPlatform::apply_selector_font() {
    // Both branches above already call SetFont at the point they resolve
    // their font, matching MyToolBar::Create exactly; there is no separate
    // native step left to perform here.
}

bool C1MainFrame::WindowsToolbarPlatform::file_exists(
    std::string_view path) const {
    return GetFileAttributesA(std::string(path).c_str()) !=
           INVALID_FILE_ATTRIBUTES;
}

creatures1::ui::BitmapHandle
C1MainFrame::WindowsToolbarPlatform::load_bitmap_file(std::string_view path,
                                                       std::uint32_t width,
                                                       std::uint32_t height) {
    const HBITMAP bitmap = static_cast<HBITMAP>(
        LoadImageA(nullptr, std::string(path).c_str(), IMAGE_BITMAP,
                  static_cast<int>(width), static_cast<int>(height),
                  LR_LOADFROMFILE));
    return reinterpret_cast<creatures1::ui::BitmapHandle>(bitmap);
}

std::int32_t C1MainFrame::WindowsToolbarPlatform::add_bitmap_to_toolbar(
    creatures1::ui::BitmapHandle bitmap) {
    return frame_.main_toolbar_.GetToolBarCtrl().AddBitmap(
        1, CBitmap::FromHandle(reinterpret_cast<HBITMAP>(bitmap)));
}

void C1MainFrame::WindowsToolbarPlatform::destroy_bitmap(
    creatures1::ui::BitmapHandle bitmap) {
    DeleteObject(reinterpret_cast<HBITMAP>(bitmap));
}

bool C1MainFrame::create_main_toolbar() {
    // MyToolBar::Create's button/trim/selector/font policy now lives in
    // creatures1::ui::MainToolbar (ui/toolbars.cpp), exercised through the
    // WindowsToolbarPlatform adapter above; this function only supplies the
    // parent handle the policy needs to create the real CToolBar.
    WindowsToolbarPlatform platform(*this);
    return creatures1::ui::MainToolbar(platform).create(
        reinterpret_cast<std::uintptr_t>(static_cast<CWnd*>(this)));
}

bool C1MainFrame::set_status_bar_indicators(
    const creatures1::application::MainFrameStatusIndicatorState& indicators) {
    std::array<UINT, 14> native_indicators{};
    for (std::size_t index = 0; index < native_indicators.size(); ++index) {
        native_indicators[index] = static_cast<UINT>(indicators.command_ids[index]);
    }
    return event_bar_.Create(this, 0x50008200, 0xe801) != FALSE &&
           event_bar_.SetIndicators(native_indicators.data(),
                                    static_cast<int>(native_indicators.size())) != FALSE;
}

creatures1::application::MainFrameStatusPaneInfo C1MainFrame::status_pane_info(
    std::size_t pane_index) const {
    if (pane_index > static_cast<std::size_t>(INT_MAX)) {
        return {};
    }
    UINT command_id = 0;
    UINT style = 0;
    int width = 0;
    event_bar_.GetPaneInfo(static_cast<int>(pane_index), command_id, style,
                           width);
    return {command_id, style, width};
}

void C1MainFrame::set_status_pane_info(
    std::size_t pane_index,
    const creatures1::application::MainFrameStatusPaneInfo& info) {
    if (pane_index > static_cast<std::size_t>(INT_MAX)) {
        return;
    }
    event_bar_.SetPaneInfo(static_cast<int>(pane_index),
                           static_cast<UINT>(info.command_id),
                           static_cast<UINT>(info.style_flags), info.width);
}

std::uint32_t C1MainFrame::privilege_level() const {
    return g_active_app_state == nullptr
               ? 0
               : static_cast<std::uint32_t>(
                     g_active_app_state->privilege_level);
}

void C1MainFrame::remove_menu_item_by_position(
    creatures1::application::MainMenuHandle& menu, std::size_t position) {
    auto* native = dynamic_cast<C1NativeMainMenuHandle*>(&menu);
    if (native != nullptr && native->native_menu() != nullptr &&
        position <= static_cast<std::size_t>(UINT_MAX)) {
        ::RemoveMenu(native->native_menu(), static_cast<UINT>(position),
                     MF_BYPOSITION);
        DrawMenuBar();
    }
}

void C1MainFrame::populate_embedded_kit_menu_and_toolbar() {
    C1NativeEmbeddedKitMenuPlatform platform(*this);
    creatures1::application::populate_embedded_kit_menu_and_toolbar(platform);
}

void C1MainFrame::publish_embedded_kit_definition(
    std::size_t tool_index,
    const creatures1::application::EmbeddedKitToolDefinition& definition,
    std::uint8_t /*adjusted_type_code*/) {
    if (tool_index < embedded_kit_definitions_.size()) {
        embedded_kit_definitions_[tool_index] = definition;
    }
}

void C1MainFrame::set_embedded_kit_toolbar_button(
    std::size_t tool_index, std::uint32_t command_id, int image_index) {
    if (tool_index >= 20 || image_index < 0) {
        return;
    }
    const int button_index = 0x0f + static_cast<int>(tool_index);
    main_toolbar_.SetButtonInfo(button_index, static_cast<UINT>(command_id),
                                 TBBS_BUTTON, image_index);
}

int C1MainFrame::add_kit_toolbar_bitmap(std::string_view prog_id) {
    // This was previously a permanent stub returning -1, so every kit --
    // on every platform, not only under Wine -- fell straight through to
    // the shared toolbar bitmap's type-code slot, which is a different
    // image than the kit's own icon. The four-step native sequence
    // (resolve the COM local server path, swap its extension for .bmp,
    // load that file, add it to the toolbar) is implemented once as
    // creatures1::ui::add_kit_toolbar_bitmap (ui/toolbars.cpp); this wires
    // it to the real toolbar and COM resolver instead of duplicating it.
    WindowsToolbarPlatform platform(*this);
    WindowsComLocalServer com_resolver;
    return creatures1::ui::add_kit_toolbar_bitmap(platform, com_resolver,
                                                  prog_id);
}

void C1MainFrame::GetMessageString(UINT command_id, CString& message) const {
    if (frame_policy_ == nullptr) {
        CFrameWnd::GetMessageString(command_id, message);
        return;
    }
    message = frame_policy_->GetMessageString(
                                 static_cast<std::uint32_t>(command_id))
                  .c_str();
}

void C1MainFrame::refresh_event_bar_status(C1WindowsDocument& document) {
    C1EventBarStatusAdapter status(event_bar_, document);
    C1EventBarObjectAdapter objects(event_bar_, document);
    event_bar_.refresh_object_display_panes(status, objects);
}

void C1MainFrame::add_event_bar_object(
    C1WindowsDocument& document, creatures1::objects::Object* object) {
    // The EventBar owns duplicate suppression, capacity eviction, pane refresh,
    // and rejection of null references before touching the native panes.
    C1EventBarObjectAdapter objects(event_bar_, document);
    event_bar_.add_object(object, objects);
}

void C1MainFrame::remove_event_bar_object(
    C1WindowsDocument& document, creatures1::objects::Object& object,
    bool remove_all_entries) {
    C1EventBarObjectAdapter objects(event_bar_, document);
    event_bar_.remove_object(&object, remove_all_entries, objects);
}

void C1MainFrame::bind_event_bar_document(C1WindowsDocument* document) {
    event_bar_.bind_document(document);
}

void C1MainFrame::serialize_main_toolbar(
    creatures1::ui::ToolbarArchiveApi& archive) {
    class Selector final : public creatures1::ui::CreatureSelectorApi {
    public:
        explicit Selector(CComboBox& combo) : combo_(combo) {}

        void clear() override { combo_.ResetContent(); }
        std::size_t count() const override {
            const int count = combo_.GetCount();
            return count < 0 ? 0 : static_cast<std::size_t>(count);
        }
        std::string text_at(std::size_t index) const override {
            if (index >= count()) {
                return {};
            }
            CStringA text;
            combo_.GetLBText(static_cast<int>(index), text);
            return text.GetString();
        }
        void add(std::string_view value) override {
            combo_.AddString(CStringA(std::string(value).c_str()));
        }

    private:
        CComboBox& combo_;
    } selector(creature_selector_);

    creatures1::ui::serialize_main_toolbar(archive, selector);
}

void C1MainFrame::serialize_event_bar(
    creatures1::ui::EventBarArchiveApi& archive,
    creatures1::ui::EventBarLegacyArchiveWords& words) {
    event_bar_.semantic_event_bar().serialize(archive, words);
}

void C1MainFrame::set_coordinate_status(std::string_view text) {
    event_bar_.SetPaneText(0, CStringA(std::string(text).c_str()));
}

void C1MainFrame::clear_coordinate_status() {
    event_bar_.SetPaneText(0, "");
}

void C1MainFrame::invalidate_main_toolbar() {
    if (main_toolbar_.GetSafeHwnd() != nullptr) {
        main_toolbar_.Invalidate(FALSE);
    }
}

void C1MainFrame::forward_default_query_new_palette() {
    // KNOWN DIVERGENCE, not yet fixed: native CMainFrame::OnQueryNewPalette
    // (0x004218a0) does NOT call CFrameWnd::OnQueryNewPalette() on itself --
    // the raw disassembly loads g_SFCView into ECX before the call, so it
    // invokes CWnd::Default() on the SFCView instead, then unconditionally
    // tail-jumps into CWorldRenderer::RealizePalette() and returns its
    // result directly (no "was anything realized" branch at all). This
    // forward-on-self-and-conditionally-invalidate shape is this project's
    // own approximation, not a match for that unusual cross-window forward.
    // Left as documented rather than guessed at: the behavior only differs
    // under legacy 8-bit-palette video modes, which no modern display uses,
    // and MFC's Default() semantics when called through a different
    // window's `this` than the one currently dispatching the message are
    // easy to model wrong without being able to observe it running.
    static_cast<void>(CFrameWnd::OnQueryNewPalette());
}

std::uint32_t C1MainFrame::realize_world_renderer_palette() {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    return document == nullptr ? 0 : document->query_new_palette();
}

void C1MainFrame::invalidate_world_renderer_window() {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr) {
        document->invalidate_renderer_view();
    }
}

void C1MainFrame::forward_default_palette_changed(
    const void* palette_focus_window) {
    const CWnd* focus = static_cast<const CWnd*>(palette_focus_window);
    CFrameWnd::OnPaletteChanged(const_cast<CWnd*>(focus));
    // KNOWN DIVERGENCE, not yet fixed: same shape as
    // forward_default_query_new_palette above.  Native
    // CMainFrame::OnPaletteChanged (0x004218c0) additionally calls
    // CWnd::Default() on g_SFCView, unconditionally, whenever the focus
    // window isn't the frame itself -- independent of whether it goes on
    // to realize the palette.  Not replicated here for the same reason:
    // 8-bit-palette-only effect, and cross-window Default() semantics
    // this project can't currently observe running to confirm against.
}

bool C1MainFrame::palette_focus_is_this_frame(
    const void* palette_focus_window) const {
    return palette_focus_window == this;
}

bool C1MainFrame::palette_focus_is_sfc_view(
    const void* palette_focus_window) const {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(
            C1WindowsDocument,
            const_cast<C1MainFrame*>(this)->GetActiveDocument());
    return document != nullptr && document->palette_focus_is_view(
                                       static_cast<const CWnd*>(
                                           palette_focus_window));
}

bool C1MainFrame::palette_focus_is_world_renderer_owner(
    const void* /*palette_focus_window*/) const {
    // C1's renderer is owned by C1WindowsView; there is no separate native
    // palette-owner window in this executable boundary.
    return false;
}

BOOL C1MainFrame::OnQueryNewPalette() {
    return creatures1::application::query_new_main_frame_palette(*this) != 0
               ? TRUE
               : FALSE;
}

void C1MainFrame::OnPaletteChanged(CWnd* palette_focus_window) {
    creatures1::application::notify_main_frame_palette_changed(
        *this, palette_focus_window);
}

void C1MainFrame::OnGetMinMaxInfo(MINMAXINFO* min_max_info) {
    pending_min_max_info_ = min_max_info;
    creatures1::application::update_main_frame_min_max(
        *this, min_max_update_in_progress_);
    pending_min_max_info_ = nullptr;
}

void C1MainFrame::forward_default_min_max_info() {
    if (pending_min_max_info_ != nullptr) {
        CFrameWnd::OnGetMinMaxInfo(pending_min_max_info_);
    }
}

bool C1MainFrame::world_renderer_exists() const {
    C1WindowsDocument* document = DYNAMIC_DOWNCAST(
        C1WindowsDocument, const_cast<C1MainFrame*>(this)->GetActiveDocument());
    return document != nullptr && document->world_renderer_exists();
}

bool C1MainFrame::window_is_iconic() const {
    return IsIconic() != FALSE;
}

void C1MainFrame::OnInitMenuPopup(CMenu* popup_menu, UINT menu_index,
                                  BOOL system_menu) {
    if (system_menu || popup_menu == nullptr) {
        CFrameWnd::OnInitMenuPopup(popup_menu, menu_index, system_menu);
        return;
    }
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr) {
        // Rebuild first so MFC's command-update pass sees the same filtered
        // entries and captions that the popup is about to display.  This
        // keeps the pregnancy marker and selected-creature checkmark live.
        document->rebuild_creature_selection_menu();
    }
    CFrameWnd::OnInitMenuPopup(popup_menu, menu_index, system_menu);
}

void C1MainFrame::OnSelectCreature(UINT command_id) {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr) {
        document->select_creature_by_menu_command(
            static_cast<int>(command_id));
    }
}

void C1MainFrame::OnFavouritePlace(UINT command_id) {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr && command_id >= 0x8053 && command_id <= 0x805e) {
        document->request_favourite_place(
            static_cast<std::size_t>(command_id - 0x8053));
    }
}

void C1MainFrame::activate_frame_native(int show_command) {
    CFrameWnd::ActivateFrame(show_command);
}

void C1MainFrame::ActivateFrame(int show_command) {
    if (frame_policy_ == nullptr) {
        activate_frame_native(show_command);
        return;
    }
    frame_policy_->ActivateFrame(show_command);
}

void C1MainFrame::OnCameraTrackSelectedCreature() {
    C1WindowsView* view = active_c1_view(*this);
    if (view != nullptr) {
        view->toggle_camera_tracking();
    }
}

void C1MainFrame::OnInfiniteWorld() {
    C1WindowsView* view = active_c1_view(*this);
    if (view != nullptr) {
        view->toggle_infinite_world();
    }
}

void C1MainFrame::OnSmoothScrolling() {
    C1WindowsView* view = active_c1_view(*this);
    if (view != nullptr) {
        view->toggle_smooth_scrolling();
    }
}

void C1MainFrame::OnMuteSounds() {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr) {
        toggle_native_mute(*document);
    }
}

void C1MainFrame::OnInformativeCreaturesMenu() {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr && document->semantic_document_mutable() != nullptr) {
        document->semantic_document_mutable()->toggle_informative_selection_menu(
            *document);
    }
}

void C1MainFrame::OnVolume() {
    if (g_active_sound_manager == nullptr) {
        return;
    }
    if (volume_dialog_host_ == nullptr) {
        volume_dialog_host_ = std::make_unique<C1NativeVolumeDialogHost>(*this);
    }
    creatures1::application::show_volume_dialog(*volume_dialog_host_);
}

void C1MainFrame::OnWebConnect() {
    C1NativeWebpageShortcut platform;
    creatures1::application::open_webpage_url_shortcut(platform);
}

void C1MainFrame::OnAbout() {
    C1NativeVersionDialog dialog(this);
    creatures1::ui::show_version_dialog(dialog);
}

void C1MainFrame::OnUpdateCameraTrackSelectedCreature(
    CCmdUI* command_ui) {
    C1WindowsView* view = active_c1_view(*this);
    if (view == nullptr) {
        command_ui->Enable(FALSE);
        command_ui->SetCheck(FALSE);
        return;
    }
    command_ui->SetCheck(view->camera_tracks_selected_creature());
    command_ui->Enable(view->selected_creature_exists_for_command() &&
                       view->world_update_timer_is_running_for_command());
}

void C1MainFrame::OnUpdateInfiniteWorld(CCmdUI* command_ui) {
    C1WindowsView* view = active_c1_view(*this);
    if (view == nullptr) {
        command_ui->Enable(FALSE);
        command_ui->SetCheck(FALSE);
        return;
    }
    command_ui->SetCheck(view->infinite_world_enabled());
    command_ui->Enable(view->world_update_timer_is_running_for_command());
}

void C1MainFrame::OnUpdateSmoothScrolling(CCmdUI* command_ui) {
    C1WindowsView* view = active_c1_view(*this);
    if (view == nullptr) {
        command_ui->Enable(FALSE);
        command_ui->SetCheck(FALSE);
        return;
    }
    command_ui->SetCheck(view->smooth_scrolling_enabled());
    command_ui->Enable(TRUE);
}

namespace {

class DocumentCommandUi final
    : public creatures1::application::DocumentCommandUpdateHost {
public:
    explicit DocumentCommandUi(CCmdUI& command_ui) : command_ui_(command_ui) {}
    void set_checked(bool checked) override {
        command_ui_.SetCheck(checked ? 1 : 0);
    }
    void set_enabled(bool enabled) override {
        command_ui_.Enable(enabled ? TRUE : FALSE);
    }

private:
    CCmdUI& command_ui_;
};

} // namespace

void C1MainFrame::OnUpdateMuteSounds(CCmdUI* command_ui) {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr || document->semantic_document() == nullptr) {
        command_ui->SetCheck(FALSE);
        command_ui->Enable(FALSE);
        return;
    }
    DocumentCommandUi ui(*command_ui);
    document->semantic_document()->update_mute_menu(
        ui, document->world_timer_is_armed());
}

void C1MainFrame::OnUpdateInformativeCreaturesMenu(CCmdUI* command_ui) {
    C1WindowsDocument* document =
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr || document->semantic_document() == nullptr) {
        command_ui->SetCheck(FALSE);
        command_ui->Enable(FALSE);
        return;
    }
    DocumentCommandUi ui(*command_ui);
    document->semantic_document()->update_informative_selection_menu(
        ui, document->world_timer_is_armed());
}

void C1MainFrame::OnTimer(UINT_PTR timer_id) {
    if (timer_id == 1 && frame_policy_ != nullptr) {
        frame_policy_->OnTimer();
    }
    CFrameWnd::OnTimer(timer_id);
}

IMPLEMENT_DYNCREATE(C1MainFrame, CFrameWnd)

void C1MainFrame::OnTipOfDay() {
    const std::string primary_directory =
        g_active_primary_directories != nullptr &&
                !g_active_primary_directories->paths[0].empty()
            ? g_active_primary_directories->paths[0]
            : current_directory_with_separator();
    C1TipDialogPlatform platform(primary_directory, this);
    creatures1::ui::show_startup_tip_dialog(platform);
}

} // namespace creatures1::platform

namespace {

// Adapts MFC's CCmdUI to the semantic CommandUi contract.
// The recovered update handlers enable the Log entries only while the world
// timer is running; that state belongs to the active document.
bool world_is_running(CFrameWnd& frame) {
    const auto* document = DYNAMIC_DOWNCAST(
        creatures1::platform::C1WindowsDocument, frame.GetActiveDocument());
    return document != nullptr && document->world_timer_is_armed();
}

// The burble flag lives in the same C1 user registry key the settings host
// reads, so the command can own it without reaching into the application.
class RegistryBurbleStore final
    : public creatures1::application::BurbleSettingStore {
public:
    bool burble_is_enabled() const override {
        HKEY key = nullptr;
        std::uint32_t value = 0;
        if (creatures1::platform::open_c1_secondary_registry(key, KEY_READ)) {
            creatures1::platform::read_registry_dword(key, "Burble", value);
            RegCloseKey(key);
        }
        return value != 0;
    }

    void set_burble_enabled(bool enabled) override {
        HKEY key = nullptr;
        if (creatures1::platform::open_c1_secondary_registry(key,
                                                             KEY_SET_VALUE)) {
            creatures1::platform::write_registry_dword(key, "Burble",
                                                       enabled ? 1u : 0u);
            RegCloseKey(key);
        }
    }
};

class FrameCommandUi final : public creatures1::application::CommandUi {
public:
    explicit FrameCommandUi(CCmdUI* command_ui) : command_ui_(command_ui) {}
    void set_enabled(bool enabled) override {
        command_ui_->Enable(enabled ? TRUE : FALSE);
    }
    void set_checked(bool checked) override {
        command_ui_->SetCheck(checked ? 1 : 0);
    }

private:
    CCmdUI* command_ui_;
};

} // namespace

namespace creatures1::platform {

void C1MainFrame::OnToggleDebugLogCategory(UINT command_id) {
    creatures1::application::toggle_debug_log_category(debug_log_state_,
                                                       command_id);
}

void C1MainFrame::OnUpdateToggleDebugLogCategory(CCmdUI* command_ui) {
    FrameCommandUi ui(command_ui);
    creatures1::application::update_debug_log_category_command(
        ui, debug_log_state_, command_ui->m_nID, world_is_running(*this));
}

void C1MainFrame::OnInstantVerbVocabulary() {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr || frame_policy_ == nullptr) {
        return;
    }
    WindowsAgeCommandPlatform platform(*document);
    frame_policy_->OnAgeSelectedCreatureAndSeedWords(platform);
}

void C1MainFrame::OnInfectCurrentNorn() {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr || document->world_runtime() == nullptr) {
        return;
    }
    WindowsInfectCreatureHost host(*document);
    creatures1::application::infect_selected_creature_with_random_bacterium(
        host);
}

void C1MainFrame::OnForceAgeing() {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr) {
        return;
    }
    WindowsForceAgeHost host(*document);
    creatures1::application::force_age_selected_creature_one_stage(host);
}

void C1MainFrame::OnUpdateForceAgeing(CCmdUI* command_ui) {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    command_ui->Enable(document != nullptr &&
                       document->selected_creature() != nullptr);
}

void C1MainFrame::OnEuthanasia() {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr) {
        return;
    }
    WindowsEuthanasiaHost host(*document);
    creatures1::application::request_euthanasia(host);
}

void C1MainFrame::OnImportCreature() {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr) {
        return;
    }
    WindowsCreatureImportHost host(*document);
    creatures1::application::import_creature(host);
}

void C1MainFrame::OnExportCurrentCreature() {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr) {
        return;
    }
    WindowsCreatureExportHost host(*document);
    creatures1::application::export_current_creature(host);
}

void C1MainFrame::OnUpdateExportCurrentCreature(CCmdUI* command_ui) {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    command_ui->Enable(document != nullptr &&
                       document->selected_creature() != nullptr);
}

namespace {

void create_generated_norn(
    C1WindowsDocument* document,
    creatures1::creatures::CreatureConstructionSex construction_sex) {
    if (document == nullptr) {
        return;
    }
    WindowsGeneratedCreatureHost host(*document);
    creatures1::application::create_and_select_generated_creature(
        host, construction_sex);
}

} // namespace

void C1MainFrame::OnCreateMaleNorn() {
    create_generated_norn(
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument()),
        creatures1::creatures::CreatureConstructionSex::male);
}

void C1MainFrame::OnCreateFemaleNorn() {
    create_generated_norn(
        DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument()),
        creatures1::creatures::CreatureConstructionSex::female);
}

void C1MainFrame::OnMuteCreatureVoices() {
    // Creature::Speak @ 0040b5f0 gates the whole Voice path on the burble
    // setting, so muting creature voices is that setting turned off.  The
    // Testing menu exposes the same flag positively at 32786; this Options
    // entry is its inverse, and the port keeps one stored value.
    RegistryBurbleStore store;
    creatures1::application::toggle_burble_setting(store);
}

void C1MainFrame::OnUpdateMuteCreatureVoices(CCmdUI* command_ui) {
    RegistryBurbleStore store;
    command_ui->SetCheck(store.burble_is_enabled() ? 0 : 1);
}

void C1MainFrame::OnAddFavouritePlace() {
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr) {
        document->add_favourite_place_from_viewport();
    }
}

void C1MainFrame::OnRemoveFavouritePlace() {
    // show_remove_favourite_place_dialog @ the recovered application policy is
    // a single host call; dialog 143 lists the stored places and returns the
    // one to drop.
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document == nullptr) {
        return;
    }
    C1RemoveFavouritePlaceDialog dialog(*document);
    if (dialog.DoModal() == IDOK && dialog.removed_index() >= 0) {
        document->remove_favourite_place_at(
            static_cast<std::size_t>(dialog.removed_index()));
    }
}

void C1MainFrame::OnShowCaosConsole() {
    // ShowCAOSConsoleDialog @ 00434d20.  The singleton decision is recovered
    // application policy, so it runs there rather than being repeated here.
    creatures1::application::show_caos_console_dialog(*this);
}

void C1MainFrame::OnUpdateShowCaosConsole(CCmdUI* command_ui) {
    command_ui->SetCheck(caos_console_exists() ? 1 : 0);
}

void C1MainFrame::OnShowDebugConsole() {
    // EnsureDebugConsoleDialog @ 00434ac0: the dialog is a singleton, so an
    // existing one is re-shown rather than replaced.
    ensure_debug_console_dialog();
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        console->set_debug_log_state(&debug_log_state_);
    }
}

void C1MainFrame::OnUpdateShowDebugConsole(CCmdUI* command_ui) {
    // OnUpdateShowDebugConsole @ 00434ce0.
    command_ui->SetCheck(active_debug_console() != nullptr ? 1 : 0);
}

void C1MainFrame::OnToggleEyeView() {
    // ToggleEyeView @ 00432140.  The native holds the eye view in a process
    // global; the port hangs it off the document that owns the world.
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    if (document != nullptr) {
        creatures1::application::toggle_eye_view(*document);
    }
}

void C1MainFrame::OnUpdateToggleEyeView(CCmdUI* command_ui) {
    // OnUpdateToggleEyeView @ 004322d0: enabled while a living creature is
    // selected and the world is running, checked while the view exists.
    auto* document = DYNAMIC_DOWNCAST(C1WindowsDocument, GetActiveDocument());
    FrameCommandUi ui(command_ui);
    const creatures1::creatures::Creature* creature =
        document == nullptr ? nullptr : document->selected_creature();
    const bool alive =
        creature != nullptr &&
        creature->life_state() == creatures1::creatures::CreatureLifeState::alive;
    creatures1::application::update_eye_view_command(
        ui, alive, world_is_running(*this),
        document != nullptr && document->eye_view_exists());
}

void C1MainFrame::OnToggleCreaturesBurble() {
    RegistryBurbleStore store;
    creatures1::application::toggle_burble_setting(store);
}

void C1MainFrame::OnUpdateToggleCreaturesBurble(CCmdUI* command_ui) {
    RegistryBurbleStore store;
    FrameCommandUi ui(command_ui);
    // The recovered Testing entry is checked when burble is OFF.
    creatures1::application::update_burble_command(
        ui, store.burble_is_enabled(), world_is_running(*this));
}

void C1MainFrame::OnToggleDebugLogging() {
    creatures1::application::toggle_debug_logging(debug_log_state_);
}

void C1MainFrame::OnUpdateToggleDebugLogging(CCmdUI* command_ui) {
    FrameCommandUi ui(command_ui);
    creatures1::application::update_debug_logging_command(
        ui, debug_log_state_, world_is_running(*this));
}


namespace {

// CMainFrame::ToggleEmbeddedKitToolForCommand @ 0x00421670 and
// CMainFrame::OnUpdateEmbeddedKitToolCommand @ 0x004216d0.  Both policies
// already live in application/main_frame.cpp; these carry the frame's own
// per-tool records, the MaxKits registry value and the CCmdUI across to them.
//
// "Running" is the first word of the per-tool record at 0x1cc + index*0xb4 --
// the tool's COleDispatchDriver::m_lpDispatch.
class FrameEmbeddedKitToggle final
    : public creatures1::application::MainFrameEmbeddedKitTogglePlatform {
public:
    explicit FrameEmbeddedKitToggle(C1MainFrame& frame) : frame_(frame) {}

    bool embedded_kit_is_running(std::size_t tool_index) const override {
        return frame_.embedded_kit_dispatch(tool_index).m_lpDispatch !=
               nullptr;
    }

    void execute_embedded_kit_tool(std::size_t tool_index) override {
        WindowsEmbeddedKitHost host(frame_);
        creatures1::application::execute_embedded_kit_tool(host, tool_index);
    }

    void shutdown_embedded_kit_tool(std::size_t tool_index) override {
        WindowsEmbeddedKitHost host(frame_);
        creatures1::application::shutdown_embedded_kit_tool(
            host, tool_index, frame_.active_embedded_tool_count());
    }

    void flush_funeral_kit_document_state() override {
        C1WindowsDocument* document =
            DYNAMIC_DOWNCAST(C1WindowsDocument, frame_.GetActiveDocument());
        if (document != nullptr) {
            document->flush_funeral_state();
        }
    }

    void invalidate_main_toolbar() override {
        frame_.invalidate_main_toolbar();
    }

private:
    C1MainFrame& frame_;
};

class FrameEmbeddedKitUpdate final
    : public creatures1::application::MainFrameEmbeddedKitUpdatePlatform {
public:
    FrameEmbeddedKitUpdate(C1MainFrame& frame, CCmdUI& command_ui)
        : frame_(frame), command_ui_(command_ui) {}

    bool embedded_kit_is_running(std::size_t tool_index) const override {
        return frame_.embedded_kit_dispatch(tool_index).m_lpDispatch !=
               nullptr;
    }

    void set_command_checked(bool checked) override {
        command_ui_.SetCheck(checked ? 1 : 0);
    }

    void set_command_enabled(bool enabled) override {
        command_ui_.Enable(enabled ? TRUE : FALSE);
    }

    std::size_t ensure_max_embedded_kit_count() override {
        // The native caches this in a global and writes the default back when
        // the value is absent, so the registry is read once per run.
        static std::uint32_t cached_max_kits = 0;
        if (cached_max_kits != 0) {
            return cached_max_kits;
        }
        HKEY key = nullptr;
        std::uint32_t value = 0;
        if (open_c1_secondary_registry(key, KEY_READ | KEY_SET_VALUE)) {
            if (!read_registry_dword(key, "MaxKits", value) || value == 0) {
                value = 4;
                write_registry_dword(key, "MaxKits", value);
            }
            RegCloseKey(key);
        }
        cached_max_kits = value == 0 ? 4 : value;
        return cached_max_kits;
    }

    std::size_t active_embedded_kit_count() const override {
        const int count = frame_.active_embedded_tool_count();
        return count < 0 ? 0 : static_cast<std::size_t>(count);
    }

    bool world_update_is_running() const override {
        C1WindowsView* view = active_c1_view(frame_);
        return view != nullptr &&
               view->world_update_timer_is_running_for_command();
    }

    std::size_t selected_creature_count() const override {
        // g_creature_selection_array.m_nSize, which is NOT the creature
        // registry.  Native keeps two lists: g_creature_registry holds every
        // creature, and g_creature_selection_array holds only the
        // tick-enabled ones, rebuilt by RebuildCreatureSelectionMenu
        // @0x00422250.  CMainFrame::OnUpdateEmbeddedKitToolCommand
        // @0x004216d0 reads the selection count at 0x004217a0 -- the same
        // array SelectCreatureByMenuIndex @0x00433920 and the `getb ovvd`
        // walk index.  Returning the registry count here over-counts by
        // every creature that is not tick-enabled, so the hatchery gates
        // itself off against MaxNorns too early.
        C1WindowsDocument* document =
            DYNAMIC_DOWNCAST(C1WindowsDocument, frame_.GetActiveDocument());
        return document == nullptr ? 0 : document->selected_creature_count();
    }

    std::size_t max_norn_count() const override {
        C1WindowsDocument* document =
            DYNAMIC_DOWNCAST(C1WindowsDocument, frame_.GetActiveDocument());
        return document == nullptr
                   ? 0
                   : static_cast<std::size_t>(document->max_norns_setting());
    }

    bool injector_allows_without_subject() const override {
        HKEY key = nullptr;
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegOpenKeyExA(HKEY_CURRENT_USER,
                          "SOFTWARE\\Gameware Development\\Creatures\\"
                          "Injector Kit\\2.0",
                          0, KEY_READ, &key) != ERROR_SUCCESS) {
            return false;
        }
        const LSTATUS status =
            RegQueryValueExA(key, "AllowWithoutSubject", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(key);
        return status == ERROR_SUCCESS && value != 0;
    }

    bool selected_creature_is_alive() const override {
        C1WindowsDocument* document =
            DYNAMIC_DOWNCAST(C1WindowsDocument, frame_.GetActiveDocument());
        if (document == nullptr) {
            return false;
        }
        const creatures1::creatures::Creature* creature =
            document->selected_creature();
        return creature != nullptr && creature->death_state() == 0;
    }

private:
    C1MainFrame& frame_;
    CCmdUI& command_ui_;
};

} // namespace

void C1MainFrame::OnEmbeddedKitTool(UINT command_id) {
    FrameEmbeddedKitToggle platform(*this);
    creatures1::application::toggle_embedded_kit_tool(
        platform, static_cast<std::uint32_t>(command_id));
}

void C1MainFrame::OnUpdateEmbeddedKitTool(CCmdUI* command_ui) {
    if (command_ui == nullptr) {
        return;
    }
    FrameEmbeddedKitUpdate platform(*this, *command_ui);
    creatures1::application::update_embedded_kit_tool_command(
        platform, static_cast<std::uint32_t>(command_ui->m_nID));
}


} // namespace creatures1::platform
