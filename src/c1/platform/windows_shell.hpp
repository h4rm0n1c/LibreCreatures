#pragma once

#include <winsock2.h>
#include <windows.h>
#include <afxcmn.h>
#include <afxext.h>
#include <afxwin.h>
#include <afxole.h>
#include <shellapi.h>

#include <optional>
#include <utility>
#include <fstream>
#include <functional>

#include "../application/application.hpp"
#include "../application/document.hpp"
#include "../application/embedded_kits.hpp"
#include "../application/kit_processes.hpp"
#include "../application/main_frame.hpp"
#include "../application/resource_hosts.hpp"
#include "../display/font.hpp"
#include "../display/gallery.hpp"
#include "../display/palette.hpp"
#include "../display/rendering.hpp"
#include "../display/sprite_cache.hpp"
#include "../brain/blackboard.hpp"
#include "../creatures/creature.hpp"
#include "../creatures/owner.hpp"
#include "../objects/bubble.hpp"
#include "../objects/call_button.hpp"
#include "../objects/lift.hpp"
#include "../objects/simple_object.hpp"
#include "../objects/scenery.hpp"
#include "../objects/events.hpp"
#include "../objects/lifecycle.hpp"
#include "../objects/vehicle.hpp"
#include "../scripting/classifier_scripts.hpp"
#include "../scripting/dde.hpp"
#include "../scripting/tables.hpp"
#include "../sound/sound.hpp"
#include "../ui/tools.hpp"
#include "../common/logging.hpp"
#include "../ui/caos_console.hpp"
#include "../ui/classifier_tip.hpp"
#include "../ui/debug_console.hpp"
#include "../ui/main_window.hpp"
#include "../ui/windows.hpp"
#include "../ui/eye_view.hpp"
#include "../ui/world_statistics.hpp"
#include "../ui/magic_profiler.hpp"
#include "../ui/views.hpp"
#include "../ui/creature_selection.hpp"
#include "../world/map.hpp"
#include "../world/runtime.hpp"
#include "../world/update_timer.hpp"
#include "../world/tick.hpp"
#include "../world/geometry.hpp"
#include "windows_app_state.hpp"
#include "windows_version_dialog_host.hpp"
#include "windows_volume_dialog_host.hpp"
#include "windows_application_host.hpp"
#include "windows_environment_host.hpp"
#include "windows_filesystem_host.hpp"
#include "windows_gdi_host.hpp"
#include "windows_registry_host.hpp"
#include "windows_main_frame_host.hpp"
#include "windows_menus_host.hpp"
#include "mfc_document_archives.hpp"
#include "mfc_creature_archives.hpp"
#include "mfc_map_archive.hpp"
#include "mfc_object_archive.hpp"
#include "creature_resource_hosts.hpp"
#include "creature_text_platform.hpp"
#include "mfc_adapters.hpp"
#include "windows_palette_host.hpp"
#include "windows_pipe_server_boundary.hpp"
#include "windows_sound_host.hpp"
#include "../archive/backup.hpp"

#include "../scripting/pipe_server.hpp"
#include "../ui/tip_dialog.hpp"
#include "../ui/event_bar.hpp"
#include "../ui/volume_dialog.hpp"
#include "../ui/version_dialog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>


namespace creatures1::platform {


using creatures1::application::SfcAppCommandLineMode;
using creatures1::application::SfcAppPatchMetadata;
using creatures1::application::SfcAppResourceDirectories;
using creatures1::application::SfcAppStartupHost;

inline constexpr std::size_t kResourceDirectoryCount = 8;

inline constexpr std::array<const char*, kResourceDirectoryCount>
    kResourceDirectoryValueNames = {
        "Main Directory", "Sound Directory", "Macro Directory",
        "Palette Directory", "Image Directory", "Genetics Directory",
        "Body Data", "Programs"};

// The startup host is created before MFC asks the document template to create
// its document.  Keep only a borrowed view of the already-recovered primary
// directory slots here; the document copies the values into its own resource
// owner before doing any game initialization.

class C1WindowsDocument;
class C1WindowsView;
void set_world_view_safe_frame(C1WindowsView* view,
                               std::uint32_t frame_count);

void toggle_native_mute(C1WindowsDocument& document);
bool native_mute_enabled(const C1WindowsDocument& document);
bool native_mute_command_enabled(const C1WindowsDocument& document);

inline constexpr CLSID kSfcDocumentClsid = {
    0x380459a0,
    0x3587,
    0x11cf,
    {0x94, 0xb8, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

class C1TipDialogWindow;

// The original executable constructs its SFCApp (a CWinApp-derived MFC
// object) during CRT static initialization.  AfxWinMain retrieves that
// process-global object before dispatching InitInstance; forwarding WinMain
// without constructing it leaves MFC's current-app pointer null and crashes
// in AfxWinMain before any C1 policy can run.


// The main toolbar: CToolBar with the button-face erase the system class
// provides on Windows.
class C1MainToolBar final : public CToolBar {
protected:
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    DECLARE_MESSAGE_MAP()
};

class C1EventBar final : public CStatusBar,
                         public creatures1::ui::EventBarWindowApi {
public:
    void bind_document(C1WindowsDocument* document);

    void update_status_panes(creatures1::ui::EventBarStatusApi& status);

    void refresh_object_display_panes(creatures1::ui::EventBarStatusApi& status, const creatures1::ui::EventBarObjectPolicyApi& policy);

    void add_object(creatures1::objects::Object* object, creatures1::ui::EventBarObjectPolicyApi& policy);

    void remove_object(creatures1::objects::Object* object, bool record_auxiliary_state, creatures1::ui::EventBarObjectPolicyApi& policy);

    creatures1::ui::EventBar& semantic_event_bar();

    creatures1::ui::StatusBarPaneInfo pane_info(std::uint32_t pane_index) const override;

    creatures1::ui::StatusBarClientRect client_rect() const override;

    bool has_parent_window() const override;

    bool parent_is_zoomed() const override;

    std::uint32_t window_style() const override;

    std::int32_t system_metric(std::int32_t metric) const override;

    void def_window_proc(std::uint32_t message, std::uintptr_t pane_count, std::intptr_t pane_right_edges) override;

protected:
    LRESULT WindowProc(UINT message, WPARAM w_param, LPARAM l_param) override;

    afx_msg void OnLButtonDown(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    creatures1::ui::EventBar event_bar_;
    LRESULT deferred_result_ = 0;
    C1WindowsDocument* document_ = nullptr;
};


class C1NativeVolumeDialogHost final
    : public creatures1::application::VolumeDialogHost {
public:
    explicit C1NativeVolumeDialogHost(CWnd& owner);

    bool volume_dialog_exists() const override;

    bool create_volume_dialog() override;

    void discard_failed_volume_dialog() override;

    void show_volume_dialog() override;

    void bring_volume_dialog_to_front() override;

private:
    CWnd& owner_;
    std::unique_ptr<C1NativeVolumeDialog> dialog_;
};


class C1MainFrame final
    : public CFrameWnd,
      public creatures1::application::MainFrameWindowCreationPlatform,
      public creatures1::application::MainFrameCreatePlatform,
      public creatures1::application::MainFramePalettePlatform,
      public creatures1::application::CaosConsoleApplicationHost,
      public creatures1::application::MainFrameLifecyclePlatform,
      public creatures1::application::MainFramePipeServerPlatform {
public:
    DECLARE_DYNCREATE(C1MainFrame)

    C1MainFrame();
    ~C1MainFrame() override;

    bool create();

    void refresh_event_bar_status(C1WindowsDocument& document);
    void bind_event_bar_document(C1WindowsDocument* document);
    void remove_event_bar_object(C1WindowsDocument& document,
                                 creatures1::objects::Object& object,
                                 bool remove_all_entries);
    void add_event_bar_object(C1WindowsDocument& document,
                              creatures1::objects::Object* object);
    void set_coordinate_status(std::string_view text);
    void clear_coordinate_status();
    void invalidate_main_toolbar();
    void serialize_main_toolbar(creatures1::ui::ToolbarArchiveApi& archive);
    void serialize_event_bar(creatures1::ui::EventBarArchiveApi& archive,
                             creatures1::ui::EventBarLegacyArchiveWords& words);
    void populate_embedded_kit_menu_and_toolbar();
    void publish_embedded_kit_definition(std::size_t tool_index,
        const creatures1::application::EmbeddedKitToolDefinition& definition,
        std::uint8_t adjusted_type_code);
    void set_embedded_kit_toolbar_button(std::size_t tool_index,
                                         std::uint32_t command_id,
                                         int image_index);
    void GetMessageString(UINT command_id, CString& message) const override;
    // The native frame carries one COleDispatchDriver per embedded-kit slot;
    // a slot is connected when its driver holds an IDispatch.  Population is
    // the kit connection path, which the port does not implement yet.
    COleDispatchDriver& embedded_kit_dispatch(std::size_t tool_index) {
        return embedded_kit_dispatch_[tool_index];
    }

    // The native frame also holds, per slot, the launched kit's process
    // handle, plus a count of currently active kits.
    std::uintptr_t& embedded_kit_process_handle(std::size_t tool_index) {
        return embedded_kit_process_handles_[tool_index];
    }

    int& active_embedded_tool_count() { return active_embedded_tool_count_; }
    // UpdateCreatureNameComboHistory @ 00432360 drives this control, and
    // the command that does so is routed to the document.
    CComboBox& creature_selector() { return creature_selector_; }

    // The native keeps the debug category mask and logging flag in process
    // globals; the clean policy takes them as explicit state.
    creatures1::application::DebugLogState& debug_log_state() {
        return debug_log_state_;
    }

    // The embedded-kit tool commands (0x8086 upward).  Without these the
    // frame has no handler for the range, so MFC's automatic menu
    // enabling greys every registered kit out.
    afx_msg void OnEmbeddedKitTool(UINT command_id);
    afx_msg void OnUpdateEmbeddedKitTool(CCmdUI* command_ui);
    afx_msg void OnToggleDebugLogCategory(UINT command_id);
    afx_msg void OnUpdateToggleDebugLogCategory(CCmdUI* command_ui);
    afx_msg void OnToggleDebugLogging();
    afx_msg void OnUpdateToggleDebugLogging(CCmdUI* command_ui);
    afx_msg void OnToggleCreaturesBurble();
    afx_msg void OnUpdateToggleCreaturesBurble(CCmdUI* command_ui);
    afx_msg void OnEuthanasia();
    afx_msg void OnForceAgeing();
    afx_msg void OnUpdateForceAgeing(CCmdUI* command_ui);
    afx_msg void OnInfectCurrentNorn();
    afx_msg void OnImportCreature();
    afx_msg void OnExportCurrentCreature();
    afx_msg void OnUpdateExportCurrentCreature(CCmdUI* command_ui);
    afx_msg void OnCreateMaleNorn();
    afx_msg void OnCreateFemaleNorn();
    afx_msg void OnMuteCreatureVoices();
    afx_msg void OnUpdateMuteCreatureVoices(CCmdUI* command_ui);
    afx_msg void OnAddFavouritePlace();
    afx_msg void OnRemoveFavouritePlace();
    afx_msg void OnShowCaosConsole();
    afx_msg void OnUpdateShowCaosConsole(CCmdUI* command_ui);
    afx_msg void OnShowDebugConsole();
    afx_msg void OnUpdateShowDebugConsole(CCmdUI* command_ui);
    afx_msg void OnToggleEyeView();
    afx_msg void OnUpdateToggleEyeView(CCmdUI* command_ui);
    afx_msg void OnInstantVerbVocabulary();

    const auto& embedded_kit_definitions() const {
        return embedded_kit_definitions_;
    }
    void rebuild_creature_selector(const creatures1::creatures::CreatureSelectionState& selection,
        const creatures1::creatures::CreatureSelectionEntry* selected);
    void bind_pipe_server_boundary(creatures1::platform::WindowsPipeServerBoundary* boundary);

    void activate_frame_native(int show_command);
    void ActivateFrame(int show_command) override;

protected:
    BOOL PreCreateWindow(CREATESTRUCT& create_struct) override;

    int OnCreate(LPCREATESTRUCT create_struct);

    afx_msg LRESULT OnPipeServerCommand(WPARAM wparam, LPARAM lparam);

    // CMainFrame::OnShutdownEmbeddedKitTool @ 0x00421a10, message 0x401
    // (WM_USER+1) -- a kit's own shutdown request.  The message-map entry
    // was never added, so an embedded kit asking to be shut down went to
    // the default window proc and was silently dropped.
    afx_msg LRESULT OnShutdownEmbeddedKitTool(WPARAM tool_index, LPARAM);

    void OnDestroy();

    afx_msg void OnClose();

    afx_msg void OnViewToolbar();

    afx_msg void OnUpdateViewToolbar(CCmdUI* command_ui);

    afx_msg void OnViewStatusBar();

    afx_msg void OnUpdateViewStatusBar(CCmdUI* command_ui);

    afx_msg void OnTipOfDay();
    afx_msg void OnSelectCreature(UINT command_id);
    afx_msg void OnCreatureSelectorChanged();
    afx_msg void OnFavouritePlace(UINT command_id);
    afx_msg void OnCameraTrackSelectedCreature();
    afx_msg void OnInfiniteWorld();
    afx_msg void OnSmoothScrolling();
    afx_msg void OnMuteSounds();
    afx_msg void OnInformativeCreaturesMenu();
    afx_msg void OnVolume();
    afx_msg void OnWebConnect();
    afx_msg void OnAbout();
    afx_msg void OnUpdateCameraTrackSelectedCreature(CCmdUI* command_ui);
    afx_msg void OnUpdateInfiniteWorld(CCmdUI* command_ui);
    afx_msg void OnUpdateSmoothScrolling(CCmdUI* command_ui);
    afx_msg void OnUpdateMuteSounds(CCmdUI* command_ui);
    afx_msg void OnUpdateInformativeCreaturesMenu(CCmdUI* command_ui);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg BOOL OnQueryNewPalette();
    afx_msg void OnPaletteChanged(CWnd* palette_focus_window);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* min_max_info);
    afx_msg void OnInitMenuPopup(CMenu* popup_menu, UINT menu_index,
                                 BOOL system_menu);

    void PostNcDestroy() override;

    DECLARE_MESSAGE_MAP()

private:
    creatures1::application::MainFrameWindowRect default_window_rect() const override;

    bool read_saved_window_rect(creatures1::application::MainFrameWindowRect& rect) const override;

    void save_window_rect(const creatures1::application::MainFrameWindowRect& rect) override;

    int minimum_window_x() const override;

    int minimum_window_y() const override;

    void forward_default_pre_create(creatures1::application::MainFrameCreateParameters& parameters) override;

    int create_frame_base() override;
    creatures1::application::MainMenuHandle* root_menu() override;
    std::size_t menu_item_count(const creatures1::application::MainMenuHandle& menu) const override;
    const char* menu_item_text(const creatures1::application::MainMenuHandle& menu,
        std::size_t position) const override;
    bool replace_menu_item_text(creatures1::application::MainMenuHandle& menu, std::size_t position,
        std::uint32_t string_resource_id) override;
    void show_missing_root_menu_warning() override;
    void show_unsupported_language_warning() override;
    [[noreturn]] void terminate_process(int exit_code) override;
    bool create_main_toolbar() override;
    bool set_status_bar_indicators(const creatures1::application::MainFrameStatusIndicatorState& indicators)
        override;
    creatures1::application::MainFrameStatusPaneInfo status_pane_info(std::size_t pane_index) const override;
    void set_status_pane_info(std::size_t pane_index,
        const creatures1::application::MainFrameStatusPaneInfo& info) override;
    std::uint32_t privilege_level() const override;
    void remove_menu_item_by_position(creatures1::application::MainMenuHandle& menu,
        std::size_t position) override;

    void forward_default_query_new_palette() override;
    std::uint32_t realize_world_renderer_palette() override;
    void invalidate_world_renderer_window() override;
    void forward_default_palette_changed(const void* palette_focus_window) override;
    bool palette_focus_is_this_frame(const void* palette_focus_window) const override;
    bool palette_focus_is_sfc_view(const void* palette_focus_window) const override;
    bool palette_focus_is_world_renderer_owner(const void* palette_focus_window) const override;
    void forward_default_min_max_info() override;
    bool world_renderer_exists() const override;
    bool window_is_iconic() const override;
    std::string dispatch_pipe_command(std::string_view command) override;

    void signal_pipe_server_command_complete() override;

    // MainFrameActivationPlatform, reached through MainFrameLifecyclePlatform.
    // The document-dependent members answer emptily when no document is
    // active; the native reaches its document through the same frame.
    void forward_activate_frame(int show_command) override;
    void rebuild_creature_selection_menu() override;
    std::size_t favourite_place_count() const override;
    std::string favourite_place_name(std::size_t index) const override;
    creatures1::ui::MenuHandle* favourite_places_menu() override;
    void show_missing_favourite_places_menu_warning() override;
    void append_favourite_place_menu_item(
        creatures1::ui::MenuHandle& menu, std::uint32_t command_id,
        std::string_view name) override;

    // MainFrameLifecyclePlatform.
    void construct_frame_controls() override;
    void publish_main_frame() override;
    void unpublish_main_frame() override;
    void destroy_frame_controls() override;
    void terminate_launched_kit_processes() override;
    std::string embedded_tool_message(std::size_t tool_index) const override;
    std::string forward_default_message(
        std::uint32_t command_id) const override;
    creatures1::application::MainFrameWindowRect window_rect() const override;
    void stop_pipe_server() override;
    void revoke_ole_factories() override;
    void forward_default_close() override;
    bool window_still_exists() const override;
    void post_quit_message(int exit_code) override;
    void shutdown_embedded_kit_tool(std::size_t tool_index) override;
    void update_document_world() override;

    // CaosConsoleApplicationHost: the recovered singleton policy owns the
    // exists/activate/create ordering; these are the native operations.
    bool caos_console_exists() const override;
    void activate_caos_console() override;
    void create_caos_console() override;
    void show_caos_console() override;

    // The recovered TerminateLaunchedKitProcesses sweep is its own policy; the
    // frame owns the slot array it walks.
    class KitProcessAdapter final
        : public creatures1::application::KitProcessHost {
    public:
        explicit KitProcessAdapter(C1MainFrame& frame) : frame_(frame) {}

        bool main_frame_exists() const override;
        bool take_launched_kit_process(std::size_t slot) override;
        bool query_taken_process_exit_code(
            std::uint32_t& exit_code) const override;
        bool debug_console_exists() const override;
        void log_kit_termination(std::size_t slot) override;
        void terminate_taken_kit_process(std::uint32_t exit_code) override;
        void close_taken_kit_process() override;

    private:
        C1MainFrame& frame_;
        std::uintptr_t taken_process_ = 0;
    };

    std::unique_ptr<creatures1::application::CMainFrame> frame_policy_;
    C1MainToolBar main_toolbar_;
    C1EventBar event_bar_;
    CComboBox creature_selector_;
    CFont selector_font_;
    std::unique_ptr<C1NativeVolumeDialogHost> volume_dialog_host_;
    bool min_max_update_in_progress_ = false;
    CREATESTRUCT* pending_pre_create_struct_ = nullptr;
    BOOL pending_pre_create_result_ = FALSE;
    LPCREATESTRUCT pending_create_struct_ = nullptr;
    MINMAXINFO* pending_min_max_info_ = nullptr;
    mutable C1NativeMainMenuHandle main_menu_handle_;
    mutable std::string main_menu_item_text_;
    creatures1::platform::WindowsPipeServerBoundary* pipe_server_boundary_ =
        nullptr;
    std::array<creatures1::application::EmbeddedKitToolDefinition, 20>
        embedded_kit_definitions_{};
    std::array<COleDispatchDriver,
               creatures1::application::kEmbeddedKitSlotCount>
        embedded_kit_dispatch_{};
    std::array<std::uintptr_t,
               creatures1::application::kEmbeddedKitSlotCount>
        embedded_kit_process_handles_{};
    int active_embedded_tool_count_ = 0;
    creatures1::application::DebugLogState debug_log_state_{0, false};
};


class C1NativeEmbeddedKitMenuPlatform final
    : public creatures1::application::EmbeddedKitMenuApi {
public:
    explicit C1NativeEmbeddedKitMenuPlatform(C1MainFrame& frame);

    bool tools_menu_exists() const override;

    bool read_tool_registry_value(std::size_t tool_index, std::string& value) const override;

    void publish_tool_definition(std::size_t tool_index, const creatures1::application::EmbeddedKitToolDefinition& definition, std::uint8_t adjusted_type_code) override;

    int add_toolbar_bitmap(std::string_view /*prog_id*/) override;

    void set_toolbar_button(std::size_t tool_index, std::uint32_t command_id, int image_index) override;

    void append_tool_menu_item(std::uint32_t command_id, std::string_view display_name) override;

    void invalidate_toolbar_and_menu() override;

private:
    C1MainFrame& frame_;
    mutable HMENU tools_menu_ = nullptr;
};


class C1NativeCreatureSelectionMenuPlatform final
    : public creatures1::ui::CreatureSelectionMenuPlatform {
public:
    explicit C1NativeCreatureSelectionMenuPlatform(C1MainFrame& frame);

    creatures1::ui::MenuHandle* main_window_menu() const override;

    std::string load_string(std::uint32_t resource_id) const override;

    std::size_t item_count(const creatures1::ui::MenuHandle& menu) const override;

    bool item_text(const creatures1::ui::MenuHandle& menu, std::size_t item_index, char* text, std::size_t text_capacity) const override;

    creatures1::ui::MenuHandle* submenu(const creatures1::ui::MenuHandle& menu, std::size_t item_index) const override;

    bool delete_item_by_position(creatures1::ui::MenuHandle& menu, std::size_t item_index) override;

    bool replace_item_by_position(creatures1::ui::MenuHandle& menu, std::size_t item_index, std::uint32_t command_id, std::string_view caption) override;

    bool append_item(creatures1::ui::MenuHandle& menu, std::uint32_t command_id, std::string_view caption) override;

    void invalidate_main_toolbar() override;

    void draw_main_menu_bar() override;

private:
    static HMENU native_handle(const creatures1::ui::MenuHandle& menu);

    C1MainFrame& frame_;
    mutable C1NativeMenuHandle root_;
    mutable std::vector<std::unique_ptr<C1NativeMenuHandle>> submenus_;
};


class C1EyeViewWindow;
class WindowsMacroHost;

class C1WindowsDocument final
    : public CDocument,
      public creatures1::application::DocumentOpenHost,
      public creatures1::application::DocumentNewWorldHost,
      public creatures1::application::DocumentInformativeSelectionHost,
      public creatures1::application::DocumentContentsHost,
      public creatures1::application::DocumentCloseHost,
      public creatures1::application::DocumentSaveHost,
      public creatures1::application::DocumentTimerHost,
      public creatures1::creatures::CreatureRegistryMutation,
      public creatures1::creatures::CreatureObjectIdentityHost,
      public creatures1::creatures::Creature::InitializationHost,
      public creatures1::application::CreatureSelectionCycleHost,
      public creatures1::scripting::ScriptDefinitionInstallHost,
      public creatures1::application::EyeViewApplicationHost,
      public creatures1::objects::SimpleObjectBubbleHost,
      public creatures1::objects::ImagePreloadHost,
      public creatures1::objects::EntityRasterHost,
      public creatures1::objects::BubbleConstructionHost,
      public creatures1::objects::SimpleObjectPlacementHost,
      public creatures1::objects::ObjectMovementBoundsHost,
      public creatures1::objects::ObjectDeletionHost,
      public creatures1::objects::SceneryMoveRedrawHost,
      public creatures1::objects::CompoundObjectMoveRedrawHost,
      public creatures1::creatures::BodySpriteFileHost,
      public creatures1::objects::ObjectLifetimeHost,
      public creatures1::objects::CompoundObjectLifetimeHost,
      public creatures1::creatures::SkeletonLifetimeHost,
      public creatures1::display::WorldRendererHost,
      public creatures1::application::DocumentWorldUpdateHost,
      public creatures1::creatures::MultibyteTextApi,
      public creatures1::platform::RectangleHitTestApi {
public:
    DECLARE_DYNCREATE(C1WindowsDocument)

    void Serialize(CArchive& archive) override;

    BOOL OnOpenDocument(LPCTSTR path) override;

    BOOL OnSaveDocument(LPCTSTR path) override;

    BOOL OnNewDocument() override;

    void DeleteContents() override;

    void OnCloseDocument() override;

    void set_full_redraw_pending(bool pending) override;

    bool open_framework_document(creatures1::application::Document& /*document*/, std::string_view path) override;

    void validate_creature_body_sprites(std::size_t index) override;

    void refresh_temporary_world_backup() override;

    void refresh_event_bar_object_display_panes() override;

    void update_event_bar_status_panes() override;

    void update_main_window_title_for_selected_creature() override;

    std::uint32_t max_norns_setting() const override;

    bool read_max_norns_setting(std::uint32_t& value) override;

    void set_max_norns_setting(std::uint32_t value) override;

    void write_max_norns_setting(std::uint32_t value) override;

    std::string body_sprite_path(std::string_view image_directory, std::uint32_t genome_filename) const override;

    bool read_sprite_header(std::string_view path, std::uint16_t& image_count, std::uint32_t& first_frame_offset, std::uint16_t& first_frame_width, std::uint16_t& first_frame_height) override;

    void destroy_limb(creatures1::creatures::LimbPart& limb) override;
    void stop_continuous_sound(int sound_handle) override;
    void remove_from_renderable_set(
        creatures1::creatures::Skeleton& skeleton) override;
    void remove_from_renderable_set(
        creatures1::objects::CompoundObject& object) override;
    void unregister_from_object_registry(
        creatures1::creatures::Skeleton& skeleton) override;
    void unregister_from_object_registry(
        creatures1::objects::CompoundObject& object) override;
    void release_gallery(creatures1::display::Gallery& gallery) override;

    void persist_and_close_eye_view() override;

    std::uint32_t privilege_level() const override;

    std::size_t world_object_count() const override;

    bool world_object_can_be_destroyed(std::size_t index) const override;

    bool world_object_is_generated(std::size_t /*index*/) const override;

    void initialize_generated_object_runtime(std::size_t /*index*/) override;

    std::string generated_image_filename(std::size_t /*index*/) const override;

    void remove_generated_image(std::string_view filename) override;

    void destroy_world_object(std::size_t index) override;

    void remove_world_object(std::size_t index) override;

    void reset_world_tick_count() override;

    void clear_favourite_place_names(creatures1::application::Document& /*document*/) override;

    void promote_temporary_world_backup() override;

    void save_framework_document(creatures1::application::Document& /*document*/, std::string_view path) override;

    std::size_t object_count() const override;

    creatures1::objects::Object* object_at(std::size_t index) const override;

    void report_invalid_index() const override;

    std::size_t running_macro_count() const override;

    const creatures1::scripting::MacroObjectContext* running_macro_at(std::size_t index) const override;

    std::size_t immediate_event_count() const override;

    const creatures1::objects::QueuedObjectEvent* immediate_event_at(std::size_t index) const override;

    std::size_t delayed_event_count() const override;

    const creatures1::objects::QueuedObjectEvent* delayed_event_at(std::size_t index) const override;

    bool delayed_event_is_active(const creatures1::objects::QueuedObjectEvent& event) const override;

    std::size_t queued_stimulus_count() const override;

    const creatures1::objects::QueuedCreatureStimulus* queued_stimulus_at(std::size_t index) const override;

    bool stimulus_targets_object(const creatures1::objects::QueuedCreatureStimulus& stimulus, const creatures1::objects::Object& object) const override;

    bool save_for_close(creatures1::application::Document& /*document*/) override;

    void report_save_failure() override;

    void close_framework_document(creatures1::application::Document& /*document*/) override;

    std::size_t scenery_count() const override;

    creatures1::objects::Scenery* scenery_at(std::size_t index) const;

    void delete_first_non_scenery_object() override;

    void delete_first_scenery() override;

    bool map_loaded() const override;

    void delete_map() override;

    void clear_sprite_file_cache() override;

    void clear_charset_glyph_cache() override;

    std::size_t gallery_count() const override;

    void release_first_gallery() override;

    void remove_running_macros() override;

    void clear_script_definitions() override;

    void clear_object_registries() override;

    void clear_renderable_objects() override;

    void clear_document_selection() override;

    void delete_framework_contents(creatures1::application::Document& /*document*/) override;

    const creatures1::application::Document* semantic_document() const;

    creatures1::application::Document* semantic_document_mutable();

    // CreatureRegistryMutation is the native owner-facing view over the
    // world runtime's borrowed creature pointers.  Creature allocation and
    // destruction remain separate hosts; this boundary only preserves the
    // registry insertion/removal contract used by SFCDoc and the menus.
    std::size_t creature_count() const override;

    creatures1::creatures::CreatureSelectionEntry* creature_at(std::size_t index) const override;

    bool remove_at(std::size_t index) override;

    creatures1::creatures::Creature* selected_creature() const override;

    const creatures1::creatures::Creature* creature_for_object(const creatures1::objects::Object& object) const override;

    creatures1::creatures::Creature* mutable_creature_for_object(creatures1::objects::Object& object);

    void append_funeral_state_word(std::uint32_t value);

    std::size_t funeral_state_word_count() const;

    void flush_funeral_state();

    bool viewport_navigation_is_disabled() const;

    void disable_viewport_navigation();

    void request_event_bar_viewport_origin(int world_x, int world_y);

    creatures1::objects::Object& object_for_creature(creatures1::creatures::Creature& creature) const override;


    std::size_t selection_count() const override;

    creatures1::creatures::CreatureSelectionEntry* selection_at(std::size_t index) const override;

    void set_selected_creature(creatures1::creatures::CreatureSelectionEntry* creature) override;

    void broadcast_selection_state(std::uint32_t /*state_code*/) override;

    void persist_informative_menu_setting(bool enabled) override;

    void rebuild_informative_selection_menu(bool enabled) override;

    void update_main_window_title() override;

    bool eye_view_exists() const override;

    void close_eye_view() override;

    void update_eye_view_title() override;

    void invalidate_eye_view_follow_position() override;

    void request_viewport_origin_for_selected_creature();
    void return_viewport_navigation_to_selection() override;

    void refresh_event_bar() override;

    void invalidate_main_toolbar() override;

    [[noreturn]] void throw_invalid_selection_argument() override;

    void rebuild_creature_selection_menu();

    creatures1::world::WorldRuntime* world_runtime() const {
        return world_runtime_.get();
    }

    // Assembles the recovered skeleton sprite-build services from the owners
    // the document already holds.  The gallery lifetime host is supplied by
    // the caller because it is the one piece the document does not own.
    creatures1::creatures::SkeletonSpriteBuildServices skeleton_services(
        creatures1::creatures::SkeletonLifetimeHost& lifetime_host);

    creatures1::creatures::GenomeFileStore& genome_files() {
        return *creature_resources_;
    }
    creatures1::creatures::VoiceFileStore& voice_files() {
        return *creature_resources_;
    }

    const creatures1::creatures::StimulusContext* default_stimulus_context(std::size_t index) const override;

    std::string localized_birthplace() const override;

    std::string format_moniker(std::uint32_t identifier) const override;

    void select_creature_by_menu_command(int command_id);

    std::uint32_t world_tick_count() const override;

    bool world_timer_is_armed() const;

    void update_world_tick();

    void recover_world_update_after_boundary_failure(const std::exception& error);

    bool world_update_in_progress() const override;

    void set_world_update_in_progress(bool in_progress) override;

    bool has_edit_object() const override;

    void place_edit_object_at_pointer() override;

    bool pending_right_button() const override;

    void clear_pending_input() override;

    void finalize_edit_object() override;

    bool non_scenery_object_tick_enabled(std::size_t index) const override;

    void tick_non_scenery_object(std::size_t index) override;

    bool pop_text_input_character(char& character) override;
    // SFCView::OnCharQueueTextInput @ 0x00437270 writes the 16-byte key ring
    // (0x467d34..0x467d43); a full ring drops the character.
    bool enqueue_text_input(char character);

    std::size_t text_input_length() const override;

    std::size_t text_input_max_length() const override;

    std::uint32_t text_input_allowed_character_flags() const override;

    bool text_input_character_allowed(char character, std::uint32_t allowed_flags) const override;

    void commit_text_input() override;

    void erase_last_text_input_character() override;

    void append_text_input_character(char character) override;

    void update_text_input_target() override;

    // Blackboard's runtime host drives these; see text_input_target_.
    void configure_text_input(creatures1::objects::Object* target,
                              std::size_t maximum_length,
                              std::uint32_t allowed_characters);
    void reset_text_input_configuration();
    // g_text_input_buffer[0] = 0; g_text_input_length = 0 -- the global typed
    // text, not the pending key ring and not the pointer's saved text.
    void clear_text_input_buffer();

    creatures1::world::BacteriumServicePhase& bacterium_service_phase() {
        return bacterium_service_phase_;
    }

    creatures1::objects::Lift* active_lift() const { return active_lift_; }
    void set_active_lift(creatures1::objects::Lift* lift) {
        active_lift_ = lift;
    }
    creatures1::objects::Object* text_input_target() const {
        return text_input_target_;
    }

    void update_selected_creature_follow_viewport() override;

    void execute_running_macro(std::size_t index) override;

    std::uint32_t creature_update_cohort() const override;

    void set_creature_update_cohort(std::uint32_t cohort) override;

    bool creature_tick_enabled(std::size_t index) const override;

    bool creature_is_dreaming(std::size_t index) const override;

    bool creature_is_alive(std::size_t index) const override;

    bool selected_action_is_in_range(std::size_t index) const override;

    void boost_selected_action_activation(std::size_t index) override;

    void update_creature_brain(std::size_t index) override;

    void update_creature_biochemistry(std::size_t index) override;

    void update_creature_action_selection(std::size_t index) override;

    void increment_creature_biochemistry_tick(std::size_t index) override;

    void process_creature_dreaming(std::size_t index) override;

    bool sound_manager_available() const override;

    creatures1::world::ViewportBounds sound_viewport() const override;

    bool sounds_muted() const override;

    creatures1::sound::SoundManager& sound_manager() override;

    bool debug_console_available() const override;

    void log_sound_event(creatures1::objects::ObjectSoundPlaybackHost::LogEvent event, creatures1::sound::SoundId sound_id, int value) override;

    const char* find_character(const char* text, unsigned char character) const override;

    const char* find_substring(const char* text, const char* fragment) const override;

    const char* next_character(const char* text) const override;

    int compare_strings(const char* left, const char* right) const override;

    void copy_string(char* destination, std::size_t destination_capacity, const char* source) const override;

    void append_string(char* destination, std::size_t destination_capacity, const char* source) const override;

    bool contains(const creatures1::platform::Rectangle& rectangle, std::int32_t x, std::int32_t y) const override;

    void update_sound_system() override;

    std::uint32_t ambient_sound_cooldown_ticks() const override;

    void set_ambient_sound_cooldown_ticks(std::uint32_t ticks) override;

    bool sound_muted() const override;

    std::uint32_t random_ambient_sound_index() override;

    std::uint32_t ambient_sound_descriptor_override() const override;

    bool sound_mixer_ready() const override;

    bool load_ambient_sound(std::uint32_t sound_id) override;

    void start_ambient_sound(std::uint32_t sound_id) override;

    void update_keyboard_scroll() override;
    void apply_view_sound_policy(creatures1::ui::SfcViewSoundPolicy policy);
    void pan_view_to_selected_creature();
    bool advance_smooth_scroll() override;

    bool manual_viewport_navigation() const override;
    bool selected_creature_in_safe_area() const override;

    void reset_manual_navigation_safe_frame_count() override;
    std::uint32_t manual_navigation_safe_frame_count() const override;
    void set_manual_navigation_safe_frame_count(std::uint32_t frame_count) override;

    void reset_world_scrollbars() override;
    void follow_selected_creature_viewport() override;

    std::uint32_t world_tick_phase() const override;

    void set_world_tick_phase(std::uint32_t phase) override;

    void dispatch_world_tick_phase(std::uint32_t /*phase*/) override;

    void begin_deferred_dirty_rectangles() override;
    void flush_deferred_dirty_rectangles() override;

    std::size_t selected_creature_count() const override;

    void set_world_tick_count(std::uint32_t count) override;

    void publish_periodic_score_to_embedded_control(const creatures1::application::DocumentScore& /*score*/) override;

    std::uint32_t current_time_ms() const override;

    std::uint32_t autosave_interval_ms() const override;

    void broadcast_embedded_control_state(std::uint32_t /*state*/) override;

    std::string capture_main_window_title() const override;

    void set_temporary_main_window_title() override;

    void set_application_busy(bool busy) override;

    bool save_for_autosave(creatures1::application::Document& document) override;

    void restore_main_window_title(std::string_view title) override;

    void kill_world_update_timer() override;

    bool initialize_framework_new_document(creatures1::application::Document& /*document*/) override;

    bool initialize_game_palette() override;

    void seed_random_from_current_time() override;

    void load_charset_data() override;

    void create_legacy_world() override;

    void create_pointer_tool(const creatures1::application::PointerToolInitialization& init) override;

    void install_builtin_script(creatures1::scripting::ScriptClassifier classifier, std::string_view text) override;

    std::uint32_t world_update_timer_interval_ms() const override;

    void set_world_update_timer_interval_ms(std::uint32_t interval_ms) override;

    bool has_main_frame() const override;

    void arm_world_update_timer(std::uint32_t interval_ms) override;

    // DocumentTimerHost: world pause/resume.  The paused latch mirrors
    // g_world_update_timer_state, which is what every "is the world running"
    // test in the native reads; the interval is kept separately so a pause
    // does not lose the speed the world was running at.
    bool world_update_timer_is_running() const override;
    void mark_world_update_timer_paused() override;
    void mark_world_update_timer_running() override;
    std::int32_t object_sound_channel(std::size_t index) const override;
    void release_object_sound_channel(std::size_t index) override;
    bool sound_mixer_is_suspended() const override;
    void clear_sound_channel_active(std::size_t channel) override;
    void stop_sound_channel(std::size_t channel) override;
    bool debug_logging_available() const override;
    void stop_all_sounds() override;
    void log_continuous_sound_stop(std::int32_t channel) override;
    void update_world() override;

    // The two world-timer commands: toolbar Play (0x8045) and Pause (0x8046).
    afx_msg void OnWorldPlay();
    afx_msg void OnWorldPause();
    afx_msg void OnUpdateWorldPlay(CCmdUI* command_ui);
    afx_msg void OnUpdateWorldPause(CCmdUI* command_ui);
    afx_msg void OnSelectNextCreature();
    afx_msg void OnSelectPreviousCreature();
    // The shared "only while the world is running" gate: nineteen commands in
    // SFCDoc's map share one update handler at 004335d0, Enable(state == 0).
    afx_msg void OnUpdateRequiresRunningWorld(CCmdUI* command_ui);
    afx_msg void OnSpeakCreatureName();
    afx_msg void OnTriggerSelectedCreatureScriptEvent(UINT command_id);
    afx_msg void OnUpdateSelectCreatureByMenuIndex(CCmdUI* command_ui);
    // DECLARE_MESSAGE_MAP expands with a trailing `protected:`, so the
    // access level has to be restored for the rest of the class.
    DECLARE_MESSAGE_MAP()
public:

    void report_archive_not_loading() override;

    bool confirm_script_replacement(creatures1::scripting::ScriptClassifier /*classifier*/, std::string_view /*old_text*/, std::string_view /*new_text*/) override;

    void report_script_table_full() override;

    creatures1::display::Gallery* acquire_gallery(std::uint32_t sprite_file_id, int header_record_index, std::uint32_t image_count, bool cache_protected) override;

    creatures1::objects::EntityRegistryHost& entity_registry() override;

    void update_movement_bounds(creatures1::objects::SimpleObject& object) override;

    creatures1::objects::ObjectRenderableSetHost& renderables() override;

    // SimpleObjectBubbleHost: with the five BubbleConstructionHost bases below
    // already satisfied, the document is the complete construction host that
    // SimpleObject::create_bubble needs.
    int viewport_left() const override;
    int viewport_right() const override;
    creatures1::objects::BubbleConstructionHost& bubble_construction() override;
    // The world runtime owns every top-level object; a speech bubble
    // enters it the same way a loaded one does.
    // EyeViewApplicationHost: eye_view_exists() and destroy_eye_view() are
    // the same lifetime the four held methods above use.
    void persist_eye_view_position() override;
    void destroy_eye_view() override;
    std::int32_t selected_creature_sound_source_x() const override;
    std::int32_t selected_creature_sound_source_y() const override;
    std::string eye_view_title() const override;
    void create_eye_view(
        const creatures1::application::EyeViewCreationParameters& parameters,
        std::int32_t initial_viewport_left,
        std::int32_t initial_viewport_top,
        std::string_view title) override;

    void adopt_speech_bubble(
        std::unique_ptr<creatures1::objects::Bubble> bubble);
    // Sleep indicators are top-level SimpleObjects in the native world. Keep
    // them in WorldRuntime rather than in the short-lived attention host that
    // creates them; creatures retain the raw identity between ticks.
    void adopt_sleep_indicator(
        std::unique_ptr<creatures1::objects::SimpleObject> indicator);
    bool burble_is_enabled() const;


    // EntityRasterHost: the charset the document already loads, plus the
    // resident pixels of an entity's current gallery image.  Entity owns the
    // glyph blitting; this supplies only the two buffers it writes through.
    std::uint8_t* current_image_pixels(creatures1::objects::Entity& entity,
                                       int& out_width,
                                       int& out_height) override;
    void preload_image(const creatures1::display::Image& image) override;
    const std::uint8_t* charset_glyph_rows(
        std::uint8_t character_code) const override;
    int charset_glyph_advance_width(
        std::uint8_t character_code) const override;

    // BubbleRedrawHost / BubbleTextHost.  With these the document satisfies
    // all five BubbleConstructionHost bases.
    void invoke_bubble_deleting(creatures1::objects::Bubble& bubble,
                                std::uint32_t deletion_flags) override;
    void redraw_after_bubble_deleting(
        const creatures1::world::WorldRect& bubble_bounds) override;
    std::uint16_t measure_bubble_text_width(
        std::string_view text) const override;
    void clear_bubble_text_band(creatures1::objects::Bubble& bubble) override;
    void draw_bubble_text(creatures1::objects::Bubble& bubble, int x, int y,
                          std::string_view text, std::uint8_t background,
                          std::uint8_t foreground,
                          std::uint8_t shadow) override;
    void redraw_after_bubble_text(
        const creatures1::world::WorldRect& bubble_bounds) override;

    void redraw_after_simple_object_move(creatures1::objects::SimpleObject& /*object*/, const creatures1::world::WorldRect& /*old_bounds*/, const creatures1::world::WorldRect& /*new_bounds*/) override;
    void redraw_after_compound_object_move(
        creatures1::objects::CompoundObject& object,
        const creatures1::world::WorldRect& old_bounds,
        const creatures1::world::WorldRect& new_bounds) override;
    void queue_compound_object_dirty_rect(
        creatures1::objects::CompoundObject& object,
        const creatures1::world::WorldRect& primary_bounds) override;
    void move_by_and_redraw(creatures1::objects::Object& object, int delta_x,
                            int delta_y);

    int mouse_client_x() const;
    int mouse_client_y() const;
    int mouse_world_x() const override;

    int mouse_world_y() const override;

    void clear_edit_object() override;
    // CAOS `edit` publishes its target as the application edit object.
    void set_edit_object(creatures1::objects::Object* object);
    // CAOS `evnt` appends to the event bar; the frame owns the control and
    // the adapter that supplies its object policy.
    void add_to_event_bar(creatures1::objects::Object* object);
    // Map and score reads used by the CAOS runtime family.  The world runtime
    // and semantic document own the storage; these expose the three values the
    // recovered commands read.
    std::int32_t map_ground_height(std::uint32_t x_block) const;
    std::uint32_t map_room_count() const;
    std::int32_t map_room_value(std::uint32_t room_index,
                                std::uint32_t field) const;
    std::uint32_t document_score_value(std::uint32_t index) const;
    // Creature import increments the living-norn tally.
    void adjust_document_score(creatures1::scripting::DdeScoreCounter counter,
                               std::int32_t delta);
    void increment_living_norn_score();
    // Room lookup by world position, used by the goal-direction host.
    std::int32_t map_room_index_at(int world_x, int world_y) const;
    // The fourteen values CreateSystemInfoData @ 00410270 reports; the
    // System Information window and the DDE item share this snapshot.
    creatures1::scripting::DdeSystemInfoSnapshot system_info_snapshot()
        const;
    void set_renderer_viewport_origin(int world_x, int world_y);
    // The world runtime is the ObjectRegistryHost; several recovered
    // Creature policies walk the non-scenery registry through it.
    creatures1::objects::ObjectRegistryHost& object_registry();
    // The document owns the event scheduler; this is the immediate ring.
    void queue_immediate_object_event(
        creatures1::objects::Object& source,
        creatures1::objects::Object& target,
        creatures1::objects::ObjectEventId event_id,
        std::uint32_t argument);

    // The speech-range fan-out schedules against the Voice delay, so it needs
    // the delayed ring rather than the immediate one.  A zero delay lands
    // back in the immediate queue, which is what the native call does too.
    void queue_object_event_with_delay(
        creatures1::objects::Object& source,
        creatures1::objects::Object& target,
        creatures1::objects::ObjectEventId event_id,
        std::uint32_t argument,
        std::int32_t delay_world_ticks);

    void queue_creature_stimulus(
        const creatures1::objects::QueuedCreatureStimulus& stimulus);

    bool is_edit_object(const creatures1::objects::Object& object) const override;

    void remove_from_event_bar(creatures1::objects::Object& object, bool remove_all_entries) override;

    void purge_destroy_when_finished_macros(creatures1::objects::Object& object) override;

    void move_to_and_redraw(creatures1::objects::Object& object, int world_x, int world_y) override;

    bool is_selected_creature(const creatures1::objects::Object& object) const override;

    void clear_selected_creature(bool notify) override;

    void report_creature_base_function_misuse() override;

    void add_to_world_object_registry(creatures1::objects::Object& object) override;

    // ObjectDeletionHost: Creature vtable slot 16 (0040e9f0), the permanent
    // delete `kill` and Export run on a creature.
    bool selected_creature_exists() const override;
    void clear_references_from_other_object(
        creatures1::objects::Object& object,
        creatures1::objects::Object& deleted_object) override;
    void remove_from_creature_selection(
        creatures1::objects::Object& object) override;
    bool remove_from_creature_registry(
        creatures1::objects::Object& object) override;
    void delete_object(creatures1::objects::Object& object) override;
    void delete_creature(creatures1::creatures::Creature& creature);
    // Creature::RemoveFromWorld (0040e0d0) drops one living norn from the score.
    void decrement_living_norn_score();
    void remove_from_selection(creatures1::creatures::Creature& creature);

    void move_scenery_to_and_redraw(creatures1::objects::Scenery& /*scenery*/, creatures1::objects::Entity& entity, int world_x, int world_y) override;

    void find_nearest_room_bounds_at_point(int world_x, int world_y, creatures1::world::WorldRect& out_bounds) const override;

    creatures1::world::WorldRect vehicle_local_bounds(const creatures1::objects::Object& /*vehicle*/) const override;

    int vehicle_primary_entity_x(const creatures1::objects::Object& /*vehicle*/) const override;

    int vehicle_primary_entity_y(const creatures1::objects::Object& /*vehicle*/) const override;

    creatures1::objects::Object* edit_object() const override;

    void bind_renderer_view(CWnd& view);

    void bind_world_view(C1WindowsView* view);

    void create_world_renderer_for_view(bool smooth_scrolling_enabled);

    void destroy_world_renderer();

    void reset_renderer_navigation();

    void scroll_renderer_viewport(int& delta_x, int& delta_y);

    int renderer_viewport_left() const;

    int renderer_viewport_top() const;

    int renderer_viewport_width() const;

    int renderer_viewport_height() const;

    void request_renderer_origin(int world_x, int world_y);
    void set_renderer_debug_highlight_rect(int left, int top, int right,
                                          int bottom);
    void queue_renderer_dirty_world_rect(
        const creatures1::world::WorldRect& rect);

    void set_renderer_smooth_scrolling(bool enabled);

    void present_renderer_view(void* device_context);

    void fill_view_background_black(void* device_context) const;

    // Object-pointer validation for CAOS values; see MacroRuntimeHost.
    bool is_live_object(const creatures1::objects::Object* object) const;

    std::size_t non_scenery_object_count() const;

    creatures1::objects::Object* non_scenery_object_at(std::size_t index) const;

    creatures1::objects::Object* pointer_tool() const;

    bool has_favourite_place(std::size_t index) const;

    std::size_t favourite_place_count() const;

    std::string favourite_place_name(std::size_t index) const;

    void request_favourite_place(std::size_t index);
    // Add and Remove Favourite Place both mutate the document's bounded
    // place array; the semantic Document owns the add ordering.
    void add_favourite_place_from_viewport();
    void remove_favourite_place_at(std::size_t index);

    bool read_view_setting(std::string_view name, std::uint32_t& value, std::uint32_t default_value) const;

    void write_view_setting(std::string_view name, std::uint32_t value);


    void resize_renderer_for_view(CWnd& view, int client_width, int client_height);

    void invalidate_renderer_view();

    bool world_renderer_exists() const;

    std::uint32_t query_new_palette();

    void realize_world_renderer_palette();

    bool palette_focus_is_view(const CWnd* palette_focus_window) const;

    void* game_palette() const override;

    std::uint32_t realize_palette(void* owner_window, void* palette) override;

    void invalidate_window(void* owner_window) override;

    bool owner_is_minimized(void* owner_window) const override;

    creatures1::display::Gallery* background_gallery() const override;

    std::size_t entity_count() const override;

    const creatures1::objects::Entity* entity_at(std::size_t index) const override;

    void report_invalid_render_registry_index() const override;

    void blit_image_to_dib(creatures1::display::Image& image, std::uint8_t* dib_pixels, int world_x, int world_y, const creatures1::world::WorldRect& clip_rect, const creatures1::world::WorldRect& view_rect, bool direct_copy) override;

    creatures1::display::Gallery* acquire_overlay_gallery(std::uint32_t gallery_identifier) override;

    void* create_memory_dc() override;

    void* create_indexed_dib(void* memory_dc, int width, int height, std::uint8_t*& pixels) override;

    void* select_bitmap(void* memory_dc, void* bitmap) override;

    void delete_object(void* object) override;

    void delete_dc(void* memory_dc) override;

    void bit_blt(void* target_context, int destination_x, int destination_y, int width, int height, void* source_context, int source_x, int source_y, std::uint32_t raster_operation) override;

    void draw_dirty_world_outline(void* target_context, const creatures1::world::WorldRect& dirty_world_rect, const creatures1::world::WorldRect& viewport_rect) override;

    std::array<creatures1::display::RendererPaletteEntry, 0x100> read_palette_entries(void* palette) const override;

    void set_dib_colour_table(void* memory_dc, const std::array<creatures1::display::RendererDibColour, 0x100>& colours) override;

    void fill_client_background_black(void* device_context) override;

    void present_dirty_world_rect(
        void* owner_window, const creatures1::world::WorldRect& world_rect,
        const creatures1::world::WorldRect& viewport_rect) override;

    void present_current_view(void* /*owner_window*/, const creatures1::world::WorldRect& viewport_rect) override;

    void move_renderable_objects(int /*delta_x*/, int /*delta_y*/) override;

    void update_view_anchored_objects() override;

    bool selected_creature_is_edit_object() const override;

    bool selected_creature_is_bounded() const override;

    bool selected_creature_down_foot(int& world_x, int& world_y) const override;

    creatures1::world::WorldRect navigation_bounds() const override;

    bool contains_point(const creatures1::world::WorldRect& bounds, int world_x, int world_y) const override;

public:
    // The creature export path serializes one dynamic object through the same
    // host the world save uses, so it needs the type by name.
    class ArchiveHost;

private:
    friend class ArchiveHost;

    static constexpr UINT_PTR kWorldUpdateTimerId = 1;
    static constexpr std::size_t kPaletteDirectoryIndex = 3;
    static constexpr std::size_t kImageDirectoryIndex = 4;

    void ensure_resource_hosts();

    // World.sfc archives keep creature body pixels in the Images directory
    // belonging to the save.  Keep that directory as the secondary lazy
    // sprite source while the configured install Images directory remains
    // the primary source for ordinary game assets and generated bodies.
    creatures1::display::SpriteFileSearchPaths sprite_file_search_paths()
        const;

    // The launcher keeps the selected world and its writable generated
    // resources in the secondary tree under the user's Documents folder.
    // When a World.sfc is opened directly, its parent is the authoritative
    // per-world fallback even if the process registry still names another
    // world. Primary resources remain install-owned and read-only.
    std::string secondary_resource_directory(std::size_t index) const;

    static constexpr std::size_t kMainDirectoryIndex = 0;
    static constexpr std::size_t kGeneticsDirectoryIndex = 5;
    static constexpr std::size_t kBodyDataDirectoryIndex = 6;

    void ensure_renderer(CWnd& view, bool smooth_scrolling_enabled = false);

    void present_renderer_rect(const creatures1::world::WorldRect& rect);

    creatures1::application::StandardResourceFileBackend files_;
    std::array<std::string, kResourceDirectoryCount> resource_paths_{};
    std::string save_world_directory_;
    std::string save_image_directory_;
    std::unique_ptr<creatures1::platform::C1CreatureResourceHost>
        creature_resources_;
    std::unique_ptr<creatures1::application::C1ResourceHost> resources_;
    std::unique_ptr<creatures1::application::C1PaletteDtaHost> palette_files_;
    std::unique_ptr<creatures1::platform::WindowsPalettePlatform>
        palette_platform_;
    creatures1::display::GamePaletteState game_palette_;
    // The eye view is created on demand by the Norns > Eye View command
    // and destroyed with the document or the selected creature.
    std::unique_ptr<C1EyeViewWindow> eye_view_;
    creatures1::display::CharsetData charset_;
    creatures1::display::ImageCacheState image_cache_;
    creatures1::display::PixelCacheState pixel_cache_;
    std::uint32_t palette_build_count_ = 0;
    std::unique_ptr<creatures1::world::WorldRuntime> world_runtime_;
    // Borrowed: the world runtime owns every non-scenery object.
    creatures1::ui::PointerTool* pointer_tool_ = nullptr;
    std::unique_ptr<creatures1::platform::WindowsWorldRendererGdiHost>
        gdi_host_;
    std::unique_ptr<creatures1::display::WorldRenderer> renderer_;
    // Saved camera origin requested by Serialize before the renderer exists.
    std::optional<std::pair<int, int>> pending_renderer_origin_;
    CWnd* renderer_view_ = nullptr;
    std::unique_ptr<creatures1::application::Document> semantic_document_;
    creatures1::display::SpriteFileCache sprite_files_;
    creatures1::objects::ObjectEventScheduler event_scheduler_;
    creatures1::creatures::CreatureSelectionState selection_;
    creatures1::creatures::CreatureSelectionEntry* selected_creature_entry_ =
        nullptr;
    std::uint32_t world_update_timer_interval_ms_ = 0;
    // g_world_update_timer_state: 0 == RUNNING in the native, so the
    // world starts running and a pause raises this.
    bool world_update_paused_ = false;
    std::uint32_t world_tick_count_ = 0;
    // g_pointer_tool_name_update_deadline: the combo history is only
    // refreshed every ten world ticks.
    std::uint32_t pointer_tool_name_update_deadline_ = 0;
    bool world_update_in_progress_ = false;
    std::uint32_t creature_update_cohort_ = 0;
    std::uint32_t ambient_sound_cooldown_ticks_ = 0;
    std::uint32_t manual_navigation_safe_frame_count_ = 0;
    std::uint32_t world_tick_phase_ = 0;
    std::deque<char> pending_text_input_;
    // Blackboard edit mode retargets the shared text editor away from the
    // pointer tool, so the target, its length limit and its allowed character
    // set are state rather than pointer-tool constants.  Null target means
    // "not configured", and the accessors fall back to the pointer tool's
    // values, which is what every path did before edit mode existed.
    // Lift::initialize_state publishes itself as the active lift; `new: cbtn`
    // reads it back to seed the button's lift reference, which is what the
    // native g_Lift global carries.  Borrowed, never owned.
    creatures1::objects::Lift* active_lift_ = nullptr;
    creatures1::objects::Object* text_input_target_ = nullptr;
    // SFCDoc's global typed-text buffer (0x00467d50, 0x50 bytes) and length
    // (0x00467d48).  Separate from PointerTool::text_buffer, which only
    // receives committed speech and is saved with the world.
    std::array<char, 0x50> text_input_buffer_{};
    std::size_t text_input_buffer_length_ = 0;
    std::size_t text_input_max_length_ = 0;
    std::uint32_t text_input_allowed_flags_ = 0;
    creatures1::objects::Object* edit_object_ = nullptr;
    bool world_update_boundary_hold_reported_ = false;
    // World tick phase 15 is still held; it reports once per run rather than
    // on every tick it fires.
    // The native keeps this in g_bacterium_service_phase; the document owns
    // the world tick phase cursor already, so the sub-service cursor sits
    // beside it.
    creatures1::world::BacteriumServicePhase bacterium_service_phase_ =
        creatures1::world::BacteriumServicePhase::idle_0;
    C1WindowsView* world_view_ = nullptr;
    bool viewport_navigation_disabled_ = false;
    bool framework_opening_ = false;
    C1NativeBackupFileSystem native_backup_files_;
    creatures1::platform::MfcArchiveStream* active_archive_stream_ = nullptr;
    creatures1::platform::MfcDynamicObjectTable* active_object_table_ = nullptr;
};




class C1EventBarStatusAdapter final
    : public creatures1::ui::EventBarStatusApi {
public:
    C1EventBarStatusAdapter(C1EventBar& event_bar, const C1WindowsDocument& document);

    std::string load_string(std::uint32_t resource_id) const override;

    std::string object_pane_text(const creatures1::objects::Object& /*object*/, std::uint32_t resource_id) const override;


    creatures1::ui::StatusBarTextMetrics measure_text(std::string_view text) const override;

    void set_pane_text(std::uint32_t pane_index, std::string_view text) override;

    void set_pane_width(std::uint32_t pane_index, std::int32_t width) override;

    void set_pane_disabled(std::uint32_t pane_index, bool disabled) override;

    void invalidate_status_bar() override;

    bool selected_creature_exists() const override;

    bool selected_creature_is_dead() const override;

    std::uint8_t selected_creature_glycogen() const override;

    creatures1::ui::EventBarScoreSnapshot score_snapshot() const override;

    std::uint32_t world_tick_count() const override;

private:
    C1EventBar& event_bar_;
    const C1WindowsDocument& document_;
};


class C1EventBarObjectAdapter final
    : public creatures1::ui::EventBarObjectPolicyApi,
      public creatures1::ui::EventBarInteractionApi,
      public creatures1::application::EmbeddedKitGateway {
public:
    C1EventBarObjectAdapter(C1EventBar& event_bar, C1WindowsDocument& document);

    bool is_creature(const creatures1::objects::Object& object) const override;

    bool creature_is_dead(const creatures1::objects::Object& object) const override;

    std::uint32_t genome_filename_id(const creatures1::objects::Object& object) const override;

    std::size_t funeral_state_word_count() const override;

    void append_funeral_state_word(std::uint32_t value) override;

    void flush_funeral_kit_state() override;

    void disable_viewport_navigation() override;

    void refresh_display_panes() override;

    bool point_is_inside_pane(std::uint32_t pane_index, long x, long y) const override;

    int viewport_width() const override;

    int viewport_height() const override;

    void request_viewport_origin(int x, int y) override;

    bool viewport_navigation_is_disabled() const override;

    void select_creature(creatures1::objects::Object& object) override;

    void notify_embedded_kit_of_death(creatures1::objects::Object& /*object*/) override;

    void flush_funeral_kit_document_state() override;

    std::string object_display_name(const creatures1::objects::Object& object) const override;

    std::string unnamed_creature_label() const override;

    bool embedded_kit_is_connected(std::size_t /*tool_index*/) const override;

    void execute_embedded_kit_tool(std::size_t /*tool_index*/) override;

    bool send_kit_message(
        std::size_t tool_index,
        const creatures1::application::EmbeddedKitMessage& message) override;

private:
    C1EventBar& event_bar_;
    C1WindowsDocument& document_;
};


// CClassifierTip @ 004362a0: a popup CWnd registered with CS_SAVEBITS over
// the COLOR_INFOBK brush, holding the cached classifier text it displays.
// CEyeView @ 00417440: a WS_POPUP|WS_CAPTION|WS_VISIBLE top-level window,
// registered CS_HREDRAW|CS_VREDRAW with the app icon and arrow cursor,
// parented to the main frame.  It owns its own WorldRenderer, which follows
// the selected creature rather than the player viewport.
// CAOSConsoleDlg: dialog resource 148 ("CAOS Console"), already compiled in
// from the recovered PE resource tree.  Created modeless with IDD 0x94 by
// ShowCAOSConsoleDialog @ 00434d20.  Command execution reuses the pipe
// server's CAOS dispatch rather than a second interpreter path.
class C1CaosConsoleDialog final
    : public CDialog,
      public creatures1::ui::CaosConsoleHost,
      public creatures1::ui::CaosScriptLoadHost {
public:
    enum : UINT {
        kOutputEdit = 1035,
        kCommandInputEdit = 1036,
        kSendButton = 1037,
        kAlwaysOnTopCheckBox = 1038,
        kClearButton = 1039,
        kLoadButton = 1040,
    };

    C1CaosConsoleDialog();

    bool create_modeless();
    void bind_dispatch(C1MainFrame* frame) { frame_ = frame; }

    // CaosConsoleHost.
    void initialise_base_dialog(creatures1::ui::CaosConsoleState& state)
        override;
    creatures1::ui::CaosRect client_rect() const override;
    bool control_exists(creatures1::ui::CaosControl control) const override;
    creatures1::ui::CaosRect control_screen_rect(
        creatures1::ui::CaosControl control) const override;
    creatures1::ui::CaosRect screen_to_client(
        creatures1::ui::CaosRect rect) const override;
    creatures1::ui::CaosRect dialog_screen_rect() const override;
    bool main_window_client_rect_in_screen(
        creatures1::ui::CaosRect& rect) const override;
    void position_dialog(long x, long y) override;
    void set_always_on_top_checked(bool checked) override;
    void apply_always_on_top(bool enabled) override;
    void focus_command_input() override;
    std::string command_input_text() const override;
    void echo_submitted_command(std::string_view command) override;
    creatures1::ui::CaosExecutionResult execute_command(
        std::string_view command) override;
    void set_control_text(creatures1::ui::CaosControl control,
                          std::string_view text) override;
    std::size_t output_text_length() const override;
    std::string output_text() const override;
    void select_output(std::size_t start, std::size_t end) override;
    void replace_output_selection(std::string_view text) override;
    void move_output_caret_to_end() override;
    void set_control_position(creatures1::ui::CaosControl control, long x,
                              long y, long width, long height,
                              unsigned flags) override;
    void invalidate_dialog() override;
    void set_minimum_tracking_size(long width, long height) override;
    void default_window_message() override;
    void handle_console_shortcut() override;
    bool default_pre_translate(
        const creatures1::ui::CaosKeyMessage& message) override;

    // CaosScriptLoadHost.
    bool choose_script_file(std::string& selected_path) override;
    bool read_script_lines(std::string_view selected_path,
                           std::vector<std::string>& lines) override;
    void report_script_open_failure() override;
    void set_script_editor_text(std::string_view text) override;
    void focus_script_editor() override;
    void select_all_script_editor_text() override;

protected:
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG* message) override;
    void PostNcDestroy() override;
    afx_msg void OnSend();
    afx_msg void OnClear();
    afx_msg void OnLoad();
    afx_msg void OnToggleAlwaysOnTop();
    afx_msg void OnSize(UINT size_type, int client_width, int client_height);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* min_max_info);
    DECLARE_MESSAGE_MAP()

private:
    UINT control_id(creatures1::ui::CaosControl control) const;

    creatures1::ui::CaosConsoleState state_;
    C1MainFrame* frame_ = nullptr;
    MINMAXINFO* pending_min_max_info_ = nullptr;
};


// DebugConsoleDialog: dialog resource 144 ("Log information"), already
// compiled into the executable from the recovered PE resource tree.  Created
// modeless by EnsureDebugConsoleDialog @ 00434ac0.  Control ids come from the
// dialog leaf and match the recovered policy exactly.
class C1DebugConsoleDialog final
    : public CDialog,
      public creatures1::ui::DebugConsoleHost,
      public creatures1::common::DebugLogHost,
      public creatures1::ui::WindowPlatform {
public:
    enum : UINT {
        kLogOutputEdit = 0x3f7,       // 1015
        kClearLogButton = 0x3f8,      // 1016
        kStepFunctionsList = 0x3fc,   // 1020
        kPauseCheckBox = 0x3fe,       // 1022 "Step"
        kWorldUpdateControl = 0x3ff,  // 1023 "Next"
        kFilterTextEdit = 0x401,      // 1025
        kCloseButton = 0x402,         // 1026
        kTopmostCheckBox = 0x404,     // 1028
        kFilterCheckBox = 0x405,      // 1029
        kWholeLogButton = 0x406,      // 1030
        kThisPageButton = 0x407,      // 1031
        kMirrorButton = 0x409,        // 1033 "To Log.txt"
    };

    C1DebugConsoleDialog();
    ~C1DebugConsoleDialog() override;

    bool create_modeless();
    creatures1::ui::DebugConsoleState& state() { return state_; }

    // DebugConsoleHost.
    void initialise_base_dialog() override;
    void set_checkbox_checked(std::uint32_t control_id, bool checked) override;
    void set_dialog_always_on_top(bool enabled) override;
    void update_dialog_data(bool save_and_validate) override;
    void bind_control(std::uint32_t control_id) override;
    void exchange_log_output_text(std::string& value,
                                  bool save_and_validate) override;
    void exchange_world_update_paused(bool& value,
                                      bool save_and_validate) override;
    void set_log_output_text(std::string_view text) override;
    void move_log_output_caret_to_end() override;
    bool pause_checkbox_checked() const override;
    void service_world_update_timer() override;
    void set_world_update_control_enabled(bool enabled) override;
    void arm_world_update_timer() override;
    bool debug_log_category_enabled(std::uint32_t category) const override;
    void disable_debug_log_category(std::uint32_t category) override;
    std::string read_filter_control_text() const override;
    void set_filter_control_enabled(bool enabled) override;
    std::size_t first_visible_log_line() const override;
    std::size_t log_line_start_offset(std::size_t line) const override;
    void select_log_output(std::size_t start, std::size_t end) override;
    void scroll_log_output_caret() override;
    void copy_log_output_selection() override;
    bool log_file_is_open() const override;
    void open_log_file(std::string_view path) override;
    void close_log_file() override;
    void set_mirror_button_checked(bool checked) override;
    void set_dialog_title(std::string_view title) override;
    void start_log_flush_timer(std::uint32_t interval_ms) override;
    void stop_log_flush_timer() override;
    void flash_dialog() override;

    // DebugLogHost.
    std::string format_message(const char* format,
                               std::va_list arguments) override;
    std::string format_line_prefix(std::string_view category_tag,
                                   std::uint32_t world_tick) override;
    std::string category_tag(std::size_t highest_set_bit) const override;
    std::uint32_t world_tick() const override;
    bool debug_category_enabled(std::uint32_t category_mask) const override;
    void handle_oversized_log_line(std::string_view line,
                                   std::size_t capacity) override;
    bool log_filter_accepts(std::string_view line) const override;
    void append_console_text(std::string_view line) override;
    std::size_t console_text_length() const override;
    void retain_newest_console_text(std::size_t character_count) override;
    void mark_console_log_dirty() override;
    void refresh_console_output() override;
    bool log_file_is_open_for_logging() const { return log_file_.is_open(); }
    void mirror_to_log_file(std::string_view line) override;

    void set_debug_log_state(creatures1::application::DebugLogState* state) {
        debug_log_state_ = state;
    }

    // WindowPlatform.  DebugConsoleDialog::OnActivate @ 0x00410bf0 flashes
    // the window on activation-code 1, then always forwards to the base
    // class -- previously ported to ui/windows.cpp's on_activate_flash_window
    // but never wired to a real WM_ACTIVATE handler here.
    void set_window_always_on_top(bool /*enabled*/) override {}
    void flash_window() override { FlashWindow(TRUE); }
    void forward_default_activation(int /*activation_code*/) override {
        Default();
    }

protected:
    void DoDataExchange(CDataExchange* exchange) override;
    BOOL OnInitDialog() override;
    void PostNcDestroy() override;
    afx_msg void OnClearLog();
    afx_msg void OnTogglePause();
    afx_msg void OnFilterTextChanged();
    afx_msg void OnToggleFilter();
    afx_msg void OnCopyWholeLog();
    afx_msg void OnCopyThisPage();
    afx_msg void OnToggleMirror();
    afx_msg void OnCloseConsole();
    afx_msg void OnActivate(UINT activation_state, CWnd* other_window,
                            BOOL minimized);
    DECLARE_MESSAGE_MAP()

private:
    creatures1::ui::DebugConsoleState state_;
    CDataExchange* exchange_ = nullptr;
    CEdit log_output_edit_;
    CEdit filter_text_edit_;
    CListBox step_functions_list_;
    CButton pause_checkbox_;
    CButton world_update_control_;
    CButton topmost_checkbox_;
    CButton filter_checkbox_;
    CButton mirror_button_;
    CString log_output_binding_;
    BOOL world_update_paused_binding_ = FALSE;
    std::ofstream log_file_;
    creatures1::application::DebugLogState* debug_log_state_ = nullptr;
};

// EnsureDebugConsoleDialog @ 00434ac0 keeps the dialog as a singleton and
// reopens its log stream when it already exists.
C1DebugConsoleDialog* active_debug_console();
void ensure_debug_console_dialog();


class C1EyeViewWindow final : public CWnd,
                              public creatures1::ui::EyeViewHost,
                              public creatures1::ui::FollowViewportHost {
public:
    explicit C1EyeViewWindow(C1WindowsDocument& document)
        : document_(document) {}
    ~C1EyeViewWindow() override;

    bool create(
        std::string_view title,
        const creatures1::application::EyeViewCreationParameters& parameters,
        std::int32_t initial_viewport_left,
        std::int32_t initial_viewport_top);

    void redraw_full_view();

    // EyeViewHost.
    std::string localized_eye_view_title() const override;
    std::string selected_creature_name() const override;
    void set_window_title(std::string_view title) override;
    creatures1::ui::EyeViewPosition window_position() const override;
    void persist_eye_view_position(
        const creatures1::ui::EyeViewPosition& position) override;
    void forward_default_window_operation() override;
    void forward_default_size(unsigned size_type, int client_width,
                              int client_height) override;
    bool palette_focus_is_this_window(
        const void* palette_focus_window) const override;
    bool palette_focus_owns_renderer(
        const void* palette_focus_window) const override;
    std::uint32_t realize_world_renderer_palette() override;
    void resize_world_renderer(int client_width,
                               int client_height) override;
    bool create_native_eye_window(std::string_view title,
                                  std::uint32_t style) override;
    creatures1::ui::EyeViewRect default_eye_view_rect() const override;
    bool read_saved_eye_view_position(
        creatures1::ui::EyeViewPosition& position) const override;
    creatures1::ui::EyeViewPosition eye_view_position_limits() const override;
    void move_eye_view_window(int x, int y, int width, int height,
                              bool repaint) override;

    // FollowViewportHost.
    bool selected_creature(
        creatures1::ui::FollowViewportTarget& target) const override;
    int follow_center_x() const override { return follow_center_x_; }
    int follow_center_y() const override { return follow_center_y_; }
    bool follow_position_valid() const override { return follow_valid_; }
    void set_follow_position(int center_x, int center_y, bool valid) override;
    void publish_follow_viewport(
        const creatures1::world::ViewportBounds& bounds) override;
    void queue_dirty_world_rect(
        const creatures1::world::WorldRect& rect) override;

    void invalidate_follow_position() { follow_valid_ = false; }

    // Presents into THIS window's own renderer/gdi_host, as opposed to
    // C1WindowsDocument::present_renderer_rect, which always targets the
    // main game view. The eye view's WorldRenderer is constructed with
    // the document as its WorldRendererHost (the same interface the main
    // view's renderer uses), so its present_current_view callback would
    // otherwise silently draw into the main view instead of here -- this
    // is the actual repaint path for this window.
    void present_world_rect(const creatures1::world::WorldRect& rect);

protected:
    afx_msg void OnSize(UINT size_type, int client_width, int client_height);
    afx_msg void OnClose();
    afx_msg void OnPaint();
    afx_msg void OnPaletteChanged(CWnd* palette_focus_window);
    afx_msg BOOL OnQueryNewPalette();
    DECLARE_MESSAGE_MAP()

private:
    C1WindowsDocument& document_;
    std::unique_ptr<creatures1::display::WorldRenderer> renderer_;
    std::unique_ptr<creatures1::platform::WindowsWorldRendererGdiHost>
        gdi_host_;
    std::int32_t initial_viewport_left_ = 0;
    std::int32_t initial_viewport_top_ = 0;
    std::int32_t viewport_width_ = 0x80;
    std::int32_t viewport_height_ = 0x60;
    bool smooth_scrolling_enabled_ = false;
    int overlay_gallery_identifier_ = 0x62627562;
    int follow_center_x_ = 0;
    int follow_center_y_ = 0;
    bool follow_valid_ = false;
};


// CSystemInfoWnd: the developer System Information window, opened with
// Ctrl+period per SFCView::OnKeyDown @ 00437b30 and released through
// ReleaseSystemInfoWindowSingleton @ 00449f10, which clears the global then
// destroys the window.  Ghidra recovered no methods for the native class, so
// its visual layout is not evidenced; the fourteen values it reports are, and
// are the same set DDEServiceItem::CreateSystemInfoData @ 00410270 formats.
// Dialog 142 "Add Favourite Place": a single edit control for the name.
class C1AddFavouritePlaceDialog final : public CDialog {
public:
    enum : UINT { kNameEdit = 1012 };

    C1AddFavouritePlaceDialog() : CDialog(142, nullptr) {}
    const CString& place_name() const { return place_name_; }

protected:
    void DoDataExchange(CDataExchange* exchange) override;

private:
    CString place_name_;
};

// Dialog 143 "Add/Remove Favorite Places": a list of the stored places and a
// Remove button.  The Remove button carries id 1015 in the recovered leaf.
class C1RemoveFavouritePlaceDialog final : public CDialog {
public:
    enum : UINT { kPlaceList = 1013, kRemoveButton = 1015 };

    explicit C1RemoveFavouritePlaceDialog(C1WindowsDocument& document)
        : CDialog(143, nullptr), document_(document) {}

    int removed_index() const { return removed_index_; }

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnRemove();
    DECLARE_MESSAGE_MAP()

private:
    C1WindowsDocument& document_;
    CListBox place_list_;
    int removed_index_ = -1;
};


class C1SystemInfoWindow final : public CFrameWnd {
public:
    bool create_for(CWnd& parent);
    void set_snapshot(const creatures1::scripting::DdeSystemInfoSnapshot& info);

protected:
    afx_msg void OnPaint();
    void PostNcDestroy() override;
    DECLARE_MESSAGE_MAP()

private:
    creatures1::scripting::DdeSystemInfoSnapshot info_{};
};

// CWorldStatisticsFrame @ 0x00449b90-0x00449ef0.  Native's own creation
// call site could not be found (no menu entry, no OnKeyDown case, no
// direct or vtable-only xref to its constructor/OnCreate/CloseAndDestroy)
// despite a real search -- this mirrors the earlier, already-documented
// OnAgeSelectedCreatureAndSeedWords case: real, working logic with an
// unconfirmed trigger, possibly an internal QA-only path never wired to
// a shipped menu or key. The window itself and every value it displays
// are verified against the decompile; only the "how does a player open
// it" question is open.  Exposed here via Ctrl+Shift+W, a port-only
// binding consistent with the existing Ctrl+Shift+M/T/X debug shortcuts,
// pending a real trigger being found.
class C1WorldStatisticsFrame final : public CFrameWnd {
public:
    bool create_for(CWnd& parent);

    creatures1::ui::WorldStatisticsHost& host() { return host_; }

    // CFrameWnd::OnCreate/Default/PreTranslateMessage are protected; the
    // nested ConcreteHost below needs to forward through them the same way
    // C1TipDialogWindow::ForwardDefaultCtlColor does for CDialog::OnCtlColor.
    int ForwardBaseOnCreate(LPCREATESTRUCT create_struct) {
        return CFrameWnd::OnCreate(create_struct);
    }
    void ForwardDefaultMessage() { Default(); }
    BOOL ForwardBasePreTranslateMessage(MSG* message) {
        return CFrameWnd::PreTranslateMessage(message);
    }

protected:
    int OnCreate(LPCREATESTRUCT create_struct);
    afx_msg void OnTimer(UINT_PTR timer_id);
    BOOL PreTranslateMessage(MSG* message) override;
    void PostNcDestroy() override;
    DECLARE_MESSAGE_MAP()

private:
    class ConcreteHost final : public creatures1::ui::WorldStatisticsHost {
    public:
        explicit ConcreteHost(C1WorldStatisticsFrame& owner) : owner_(owner) {}

        int initialise_base_frame() override;
        creatures1::ui::WorldStatisticsRect client_rect() const override;
        void create_display(std::string_view initial_text,
                            const creatures1::ui::WorldStatisticsRect& bounds,
                            std::uint32_t control_id) override;
        void apply_statistics_font(int point_size) override;
        creatures1::ui::WorldStatisticsSnapshot read_snapshot() const override;
        void set_display_text(std::string_view text) override;
        void start_timer(std::uint32_t timer_id,
                         std::uint32_t interval_ms) override;
        void kill_timer(std::uint32_t timer_id) override;
        void close_frame() override;
        void default_window_message() override;
        bool base_pre_translate(
            const creatures1::ui::WorldStatisticsKeyMessage& message) override;

        LPCREATESTRUCT pending_create_struct = nullptr;
        MSG* pending_message = nullptr;

    private:
        C1WorldStatisticsFrame& owner_;
    };

    ConcreteHost host_{*this};
    CStatic statistics_display_;
    CFont statistics_font_;

public:
    // This frame has no doc/view relationship of its own -- it is a plain
    // top-level CFrameWnd, not attached to any document template, so
    // CFrameWnd::GetActiveDocument() on itself always returns null.  The
    // view that opened it (create_for's parent) is captured here instead,
    // the same way open_or_activate_system_information() hands its
    // document snapshot down rather than expecting the info window to
    // reach back into the doc/view hierarchy itself.
    C1WindowsView* owning_view = nullptr;
};

// The frame published by CMainFrame::CreateObject, mirroring the native
// g_CMainFrame.  MFC only assigns m_pMainWnd after OnOpenDocument, so a
// document loading from the archive must reach the frame this way.
C1MainFrame* active_main_frame();

// Startup diagnostics.  A failure during InitInstance happens before any
// window exists, so it is written to the debugger and to a log beside the
// executable as well as shown.
void report_startup_failure(const char* detail);


class C1ClassifierTipWindow final : public CWnd {
public:
    bool create_for(CWnd& parent);
    const CString& cached_text() const { return cached_text_; }
    void set_cached_text(const CString& text) { cached_text_ = text; }

protected:
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()

private:
    CString cached_text_;
};


class C1WindowsView final : public CView,
                            public creatures1::ui::SfcViewHost,
                            public creatures1::ui::ClassifierTipHost,
                            public creatures1::ui::TextInputQueue {
public:
    DECLARE_DYNCREATE(C1WindowsView)

    void toggle_camera_tracking();

    void toggle_infinite_world();

    void toggle_smooth_scrolling();

    bool camera_tracks_selected_creature() const;

    bool selected_creature_exists_for_command() const;

    bool world_update_timer_is_running_for_command() const;

    bool infinite_world_enabled() const;

    bool smooth_scrolling_enabled() const;

    void initialize_view_base() override;

    void apply_sound_policy(creatures1::ui::SfcViewSoundPolicy policy);
    void set_navigation_mode(creatures1::ui::ViewportNavigationMode mode);
    void update_keyboard_scroll_for_world_tick();

    bool manual_navigation_for_world_tick() const;

    std::uint32_t manual_navigation_safe_frame_count_for_world_tick() const;

    void set_manual_navigation_safe_frame_count_for_world_tick(std::uint32_t frame_count);

    void reset_world_scrollbars_for_world_tick();

    void load_view_settings(creatures1::ui::WorldViewSettings& settings, creatures1::ui::SfcViewState& state) override;

    void persist_view_settings(const creatures1::ui::WorldViewSettings& settings, const creatures1::ui::SfcViewState& state) override;

    bool read_dword_setting(std::string_view name, std::uint32_t& value, std::uint32_t default_value) override;

    void write_dword_setting(std::string_view name, std::uint32_t value) override;

    void create_world_renderer(int, int, bool smooth_scrolling_enabled) override;

    void destroy_world_renderer() override;

    void initialize_classifier_tip() override;

    void destroy_classifier_tip() override;

    void forward_set_focus_message() override;

    void forward_kill_focus_message() override;

    void restore_sound_mixer() override;

    void suspend_sound_mixer() override;

    int scroll_position(unsigned axis) const override;

    bool main_frame_is_active() const override;

    bool key_is_down(std::uint32_t virtual_key) const override;

    void scroll_viewport(int& delta_x, int& delta_y) override;

    void reset_renderer_navigation() override;

    bool selected_creature_exists() const override;

    void set_view_window_class() override;

    // PreCreateWindow owns the CREATESTRUCT for the duration of the call; the
    // host method needs it to publish the registered class name, so the view
    // carries it transiently exactly as the frame carries MINMAXINFO.
    CREATESTRUCT* pending_create_struct_ = nullptr;

    // SetCapture returns the window that held the capture; the recovered
    // handler materialises it into MFC's temporary map.
    HWND previous_capture_window_ = nullptr;

    // The native interval lives in a process global; the clean policy takes it
    // as explicit state, so the view owns it.
    creatures1::world::UpdateTimerState update_timer_state_{};

    void create_classifier_tip_window() override;

    void forward_default_mouse_message(std::uint32_t, int, int) override;

    void forward_default_size(unsigned, int, int) override;

    void release_mouse_capture() override;

    void capture_mouse_for_drag() override;

    void materialize_previous_capture_window() override;

    void set_scroll_range(unsigned axis, int minimum, int maximum, bool redraw) override;

    void set_scroll_position(unsigned axis, int position, bool redraw) override;

    void set_default_arrow_cursor() override;

    void set_coordinate_status(std::string_view text) override;
    int viewport_left() const override;

    int viewport_top() const override;

    std::size_t non_scenery_object_count() const override;

    creatures1::objects::Object* non_scenery_object_at(std::size_t index) const override;

    void report_invalid_non_scenery_object_index() const override;

    std::size_t scenery_object_count() const override;

    creatures1::objects::Object* scenery_object_at(std::size_t index) const override;

    void report_invalid_scenery_object_index() const override;

    creatures1::objects::Object* pointer_tool() const override;

    bool point_in_world_rect(const creatures1::world::WorldRect& bounds, int world_x, int world_y) const override;

    std::string classifier_name(const creatures1::brain::ClassifierId&) const override;

    void update_classifier_tip(std::string_view, int, int) override;

    void clear_classifier_tip_text() override;

    void update_pointer_tool_unbounded_position_and_redraw() override;

    void fill_client_background_black(void* device_context) override;

    void mark_renderer_full_redraw() override;

    void present_renderer_view(void* device_context) override;

    void resize_renderer_for_viewport(int client_width, int client_height) override;

    void request_renderer_origin_for_selected_creature() override;

    void invalidate_main_toolbar() override;

    void open_or_activate_system_information() override;

    // Port-only addition; see C1WorldStatisticsFrame's own class comment.
    void open_or_activate_world_statistics();

    void configure_world_update_timer(std::uint32_t) override;

    bool has_favourite_place(std::size_t index) const override;

    void request_renderer_origin_for_favourite_place(std::size_t index) override;

    void generate_profiler_report() override;

    void hide_classifier_tip() override;
    bool classifier_tip_visible() const override;
    void hide_classifier_tip_window() override;
    bool classifier_tip_text_matches(std::string_view text) const override;
    void set_classifier_tip_text(std::string_view text) override;
    creatures1::ui::ClassifierTipExtent measure_classifier_tip(
        int content_width, int content_height) override;
    creatures1::ui::ClassifierTipExtent measure_classifier_tip_text() override;
    void move_classifier_tip(int screen_x, int screen_y) override;
    void resize_classifier_tip(int screen_x, int screen_y, int width,
                               int height) override;
    void show_classifier_tip_if_hidden() override;

    C1ClassifierTipWindow classifier_tip_;

    void clear_coordinate_status() override;
    void forward_default_key_down(std::uint32_t virtual_key, std::uint32_t repeat_count, std::uint32_t key_flags) override;
    bool control_key_is_down() const override;
    void forward_default_character_message(std::uint32_t character_code) override;
    bool try_enqueue(char character) override;

protected:
    BOOL PreCreateWindow(CREATESTRUCT& create_struct) override;

    void OnInitialUpdate() override;

    void OnDraw(CDC* device_context) override;

    afx_msg void OnSize(UINT resize_type, int client_width, int client_height);
    afx_msg void OnDestroy();

    afx_msg void OnSetFocus(CWnd* old_focus);

    afx_msg void OnKillFocus(CWnd* new_focus);

    afx_msg void OnMouseMove(UINT flags, CPoint point);

    afx_msg void OnLButtonDown(UINT flags, CPoint point);

    afx_msg void OnRButtonDown(UINT flags, CPoint point);

    afx_msg void OnLButtonUp(UINT flags, CPoint point);

    afx_msg void OnRButtonUp(UINT flags, CPoint point);

    afx_msg void OnHScroll(UINT code, UINT position, CScrollBar*);

    afx_msg void OnVScroll(UINT code, UINT position, CScrollBar*);

    afx_msg void OnKeyDown(UINT virtual_key, UINT repeat_count, UINT flags);
    afx_msg void OnChar(UINT character_code, UINT repeat_count, UINT flags);

public:
    // C1WorldStatisticsFrame is a separate top-level window with no doc/view
    // template of its own; it reaches this view's document through here
    // rather than via CFrameWnd::GetActiveDocument() on itself.
    C1WindowsDocument* document() const;

private:
    creatures1::ui::WorldViewSettings view_settings_{};
    creatures1::ui::SfcViewState view_state_{};

public:
    // The pointer-tool runtime host reads and clears the pending input
    // flags the view accumulates from mouse messages.
    creatures1::ui::SfcViewState& view_state() { return view_state_; }
private:

    DECLARE_MESSAGE_MAP()
};


class C1NativeMuteControl final
    : public creatures1::application::MuteControlHost {
public:
    explicit C1NativeMuteControl(C1WindowsDocument& document);

    bool mute_is_enabled() const override;

    void set_mute_enabled(bool enabled) override;

    void stop_all_sounds() override;

    void persist_mute_enabled(bool enabled) override;

private:
    C1WindowsDocument& document_;
};


class C1NativeTipFile final : public creatures1::ui::TipFile {
public:
    explicit C1NativeTipFile(std::string path);

    ~C1NativeTipFile() override;

    bool is_open() const;

    bool read_line(std::string& line) override;

    bool rewind() override;

    bool seek(std::int32_t position) override;

    std::int32_t position() const override;

    std::string modification_timestamp() const override;

private:
    std::string path_;
    std::FILE* file_ = nullptr;
};


class C1TipDialogPlatform final : public creatures1::ui::TipDialogPlatform {
public:
    C1TipDialogPlatform(std::string primary_directory, CWnd* owner);

    bool read_registry_dword(std::string_view name, std::uint32_t& value) const override;

    void write_registry_dword(std::string_view name, std::uint32_t value) override;

    bool read_registry_string(std::string_view name, std::string& value) const override;

    void write_registry_string(std::string_view name, std::string_view value) override;

    std::string primary_resource_directory() const override;

    std::unique_ptr<creatures1::ui::TipFile> open_tips_file(std::string_view path) const override;

    std::string load_string(std::uint32_t resource_id) const override;

    void show_resource_message(std::uint32_t resource_id) override;

    void update_data(bool save_and_validate) override;
    void forward_default_timer() override;

    bool is_tip_control(std::uint32_t control_id) const override;

    void use_tip_control_brush() override;

    void forward_default_control_color() override;

    void paint_tip(std::string_view text,
                   void* paint_device_context = nullptr) override;
    int show_modal(creatures1::ui::TipDialog& dialog) override;

    void close_dialog() override;

    HBRUSH requested_brush() const;

    void bind(C1TipDialogWindow* dialog, creatures1::ui::TipDialog* semantic_dialog);

    CWnd* owner() const;

    // Stashed by C1TipDialogWindow::OnCtlColor before it calls
    // dialog_.on_ctl_color(), so forward_default_control_color() can hand
    // the real WM_CTLCOLOR* parameters to the base class instead of
    // guessing a brush.  CTipDlg::OnCtlColor @ 0x00443ac0 forwards every
    // non-tip-text control to CWnd::OnCtlColor and returns *that* brush,
    // not a fixed one.
    void set_ctl_color_context(CDC* device_context, CWnd* control,
                              UINT control_type);

private:
    bool open_key(REGSAM access, HKEY& key) const;

    std::string primary_directory_;
    CWnd* owner_ = nullptr;
    C1TipDialogWindow* native_dialog_ = nullptr;
    creatures1::ui::TipDialog* semantic_dialog_ = nullptr;
    HBRUSH pending_brush_ = nullptr;
    CDC* ctl_color_dc_ = nullptr;
    CWnd* ctl_color_control_ = nullptr;
    UINT ctl_color_type_ = 0;
};


class C1TipDialogWindow final : public CDialog {
public:
    // CWnd::Default is protected; the platform adapter needs it to forward the
    // recovered one-call default-message handlers.
    void forward_default_message() { Default(); }

    // CDialog::OnCtlColor is protected; the platform adapter needs the real
    // default brush (not a guessed one) for every control CTipDlg::OnCtlColor
    // doesn't special-case.
    HBRUSH ForwardDefaultCtlColor(CDC* device_context, CWnd* control,
                                  UINT control_type) {
        return CDialog::OnCtlColor(device_context, control, control_type);
    }

    C1TipDialogWindow(C1TipDialogPlatform& platform, creatures1::ui::TipDialog& dialog);

    BOOL OnInitDialog() override;

    afx_msg void OnTimer(UINT_PTR timer_id);

    afx_msg void OnNextTip();

    afx_msg void OnPaint();

    afx_msg HBRUSH OnCtlColor(CDC* device_context, CWnd* control, UINT control_type);

    void OnOK() override;

private:
    C1TipDialogPlatform& platform_;
    creatures1::ui::TipDialog& dialog_;

    DECLARE_MESSAGE_MAP()
};


class C1StartupHost final : public SfcAppStartupHost {
public:
    explicit C1StartupHost(creatures1::application::SfcAppState& app_state);

    ~C1StartupHost() override;

    bool initialize_ole() override;

    void report_ole_initialization_failure() override;

    void load_standard_profile_settings() override;

    void install_document_template() override;

    void connect_document_template_server() override;

    bool load_resource_directories(bool secondary, SfcAppResourceDirectories& directories) override;

    void report_resource_directory_failure() override;

    void read_text_lines(std::string_view path, std::vector<std::string>& lines) const override;

    void publish_classifier_name(std::string_view /*key*/, std::string_view /*display_name*/) override;

    void set_world_save_path(std::string_view path) override;

    void publish_uninstall_command(std::string_view path) override;

    SfcAppCommandLineMode parse_command_line() override;

    void update_document_server_registry() override;

    void update_ole_factory_registry() override;

    // Removes the InprocServer32 registration only when it names this
    // executable, so a Community Edition install's OLEKitProxy registration
    // survives.
    void delete_own_sfc_inproc_server_registration();

    void write_patch_registry_metadata(const SfcAppPatchMetadata& /*metadata*/) override;

    void delete_autorun_registration() override;

    void register_ole_factories() override;

    bool main_window_exists() const override;

    bool prepare_embedded_main_window(std::string_view world_path) override;

    void accept_file_drops() override;

    void ensure_sound_system() override;

    void start_pipe_server_if_needed() override;

    void show_startup_tip_dialog() override;

    void destroy_sound_manager_native();

    // CWinApp::LoadStdProfileSettings is protected, so only the concrete
    // application subclass can call it.  It installs this hook rather than
    // running the call itself, which keeps the recovered ordering: the native
    // loads profile settings inside the startup sequence, after OLE init.
    void set_profile_settings_loader(std::function<void()> loader) {
        profile_settings_loader_ = std::move(loader);
    }

    void stop_pipe_server_native();

    // The 20 launched-kit slots live on the frame, so the sweep runs
    // there; the shutdown host only orders it.
    void terminate_launched_kit_processes_native();

private:
    creatures1::application::SfcAppState& app_state_;
    C1MainFrame* main_frame_ = nullptr;
    CSingleDocTemplate* document_template_ = nullptr;
    COleTemplateServer document_server_;
    std::string world_save_path_;
    creatures1::application::SfcAppResourceDirectories primary_directories_;
    creatures1::application::SfcAppResourceDirectories secondary_directories_;
    creatures1::brain::ClassifierNameMap classifier_names_;
    std::function<void()> profile_settings_loader_;
    std::unique_ptr<creatures1::platform::WindowsSoundSystemHost> sound_host_;
    std::unique_ptr<creatures1::sound::SoundManager> sound_manager_;
    // The macro host outlives each MacroHolder it is handed to; the holder
    // keeps a reference, so it cannot be a temporary.
    std::unique_ptr<WindowsMacroHost> macro_host_;
    std::unique_ptr<creatures1::platform::WindowsPipeServerBoundary>
        pipe_server_;
};


class C1ShutdownHost final
    : public creatures1::application::SfcAppShutdownHost {
public:
    C1ShutdownHost(C1StartupHost& startup_host, CWinApp& application);

    // CWinApp::ExitInstance is the last step of the recovered shutdown order,
    // so the host performs it and the application returns its result rather
    // than calling the base a second time.
    int exit_code() const { return exit_code_; }

    void terminate_launched_kit_processes() override;

    void stop_pipe_server() override;

    void destroy_sound_manager() override;

    void forward_default_exit_instance() override;

private:
    C1StartupHost& startup_host_;
    CWinApp& application_;
    int exit_code_ = 0;
};


class C1WindowsDocument::ArchiveHost final
    : public creatures1::application::DocumentSerializationHost {
public:
    ArchiveHost(C1WindowsDocument& document, CArchive& archive);

    ~ArchiveHost() override;

    bool loading() const override;

    void serialize_map_data() override;
    // The payload half, driven from the dynamic object table once the
    // MapData class record has been consumed.
    void serialize_map_data_payload();

    std::size_t non_scenery_object_count() const override;

    std::int32_t read_non_scenery_object_count() override;

    void write_non_scenery_object_count(std::int32_t count) override;

    void serialize_non_scenery_object(std::size_t index) override;

    std::size_t scenery_object_count() const override;

    std::int32_t read_scenery_object_count() override;

    void write_scenery_object_count(std::int32_t count) override;

    void serialize_scenery_object(std::size_t index) override;

    void serialize_classifier_scripts() override;

    std::int32_t read_viewport_origin_x() override;

    std::int32_t read_viewport_origin_y() override;

    std::int32_t viewport_origin_x() const override;

    std::int32_t viewport_origin_y() const override;

    void write_viewport_origin(std::int32_t x, std::int32_t y) override;

    void serialize_selected_creature() override;

    void serialize_favourite_place(creatures1::application::FavouritePlace& place) override;

    void serialize_main_toolbar() override;

    std::size_t running_macro_count() const override;

    std::int32_t read_running_macro_count() override;

    void write_running_macro_count(std::int32_t count) override;

    void serialize_running_macro(std::size_t index) override;

    std::size_t world_object_count() const override;

    std::int32_t read_world_object_count() override;

    void write_world_object_count(std::int32_t count) override;

    void serialize_world_object(std::size_t index) override;

    void disable_loaded_world_object_ticks(std::size_t index) override;

    void serialize_event_bar() override;

    void serialize_score(creatures1::application::DocumentScore& score) override;

    std::uint32_t world_tick_count() const override;

    void write_world_tick_count(std::uint32_t count) override;

    std::uint32_t read_world_tick_count() override;

    void set_world_tick_count(std::uint32_t count) override;

    std::uint32_t read_document_state_word_count() override;

    void write_document_state_word_count(std::uint32_t count) override;

    std::uint32_t read_document_state_word() override;

    void write_document_state_word(std::uint32_t word) override;

    bool viewport_navigation_requires_manual_reset() const override;

    void reset_viewport_navigation_after_load() override;

    void request_viewport_origin(std::int32_t x, std::int32_t y) override;

    void load_default_first_favourite_place_name(creatures1::application::FavouritePlace& /*place*/) override;

    void dispatch_loaded_non_scenery_bounds_update(std::size_t index) override;

    void rebuild_missing_unbounded_renderable_entry(std::size_t /*index*/) override;

    bool has_unbounded_stage_one_object() const override;

    void queue_pointer_tool_event_four() override;

    void rebuild_creature_selection_menu() override;

private:
    // Creature is not an Object here: the archive identifies a Creature
    // record by the Skeleton the registry holds, and these convert.
    creatures1::creatures::Creature* creature_for_archive_object(
        void* object) const;
    const void* archive_object_for_creature(
        const creatures1::creatures::Creature* creature) const;
    void require_runtime() const;

    creatures1::platform::MfcObjectArchive* object_archive();

    void* create_object(std::string_view name, std::uint16_t schema);

    static std::uint16_t class_schema(std::string_view name);

    static bool compatible_class(std::string_view actual, std::string_view requested);

    std::string runtime_class(const void* object, std::string_view requested) const;

    void read_object(void* object, std::string_view name, creatures1::platform::MfcObjectArchive& archive);

    void write_object(const void* object, std::string_view name, creatures1::platform::MfcObjectArchive& archive);

    // Not static: PointerTool's record needs the document's placement and
    // redraw services to restore itself.
    void serialize_object_payload(void* object, std::string_view name,
                                  creatures1::platform::MfcObjectArchive& archive);

    C1WindowsDocument& document_;
    creatures1::platform::MfcArchiveStream stream_;
    std::unique_ptr<creatures1::platform::MfcDynamicObjectTable> objects_;
    std::unique_ptr<creatures1::platform::MfcObjectArchive> object_archive_;

public:
    // Creature export and import serialize a single dynamic object
    // through the same table the world save uses.
    creatures1::platform::MfcDynamicObjectTable& dynamic_objects() {
        return *objects_;
    }
private:
    std::vector<std::unique_ptr<creatures1::brain::Instinct>>
        archive_instincts_;
    // COwner arrives through the same DYNCREATE stream as CInstinct: it is
    // not an Object, so the archive host owns the instances it creates.
    std::vector<std::unique_ptr<creatures1::creatures::Owner>>
        archive_owners_;
    creatures1::ui::EventBarLegacyArchiveWords event_bar_words_{};
};


void set_world_view_safe_frame(C1WindowsView* view,
                               std::uint32_t frame_count);
C1WindowsView* active_c1_view(C1MainFrame& frame);
void toggle_native_mute(C1WindowsDocument& document);
bool native_mute_enabled(const C1WindowsDocument& document);
bool native_mute_command_enabled(const C1WindowsDocument& document);


// Registry side of the CAOS `tool` command; defined next to the kit menu
// platform that reads the records back.
bool register_embedded_kit_tool_for_frame(
    C1MainFrame* frame,
    const creatures1::application::EmbeddedKitToolRegistration& registration);

} // namespace creatures1::platform
