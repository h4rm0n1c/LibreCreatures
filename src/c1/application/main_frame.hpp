#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "../creatures/creature.hpp"
#include "../ui/menus.hpp"

namespace creatures1::scripting {
struct PipeServerCommandContext;
}

namespace creatures1::application {

class MainMenuHandle {
public:
    virtual ~MainMenuHandle() = default;
};

struct MainFrameStatusPaneInfo {
    std::uint32_t command_id = 0;
    std::uint32_t style_flags = 0;
    std::int32_t width = 0;
};

struct MainFrameStatusIndicatorState {
    std::array<std::uint32_t, 14> command_ids{};
};

struct MainFrameWindowRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct MainFrameCreateParameters {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::uint32_t style = 0;
};

// This is the C1-owned part of the 0xb4-byte embedded-kit record.  The
// dispatch driver and process handle themselves remain host objects; naming
// them as typed state keeps the frame model honest without inventing an MFC
// layout in the clean source.
struct MainFrameOleDispatchState {
    bool connected = false;
};

struct MainFrameEmbeddedToolRuntime {
    std::uintptr_t launched_process_handle = 0;
};

class CMainFrameEmbeddedRecord {
public:
    CMainFrameEmbeddedRecord();

    std::uint32_t toolbar_bitmap_index = 0;
    bool is_available = false;
    MainFrameOleDispatchState ole_dispatch_driver;
    MainFrameEmbeddedToolRuntime tool_runtime;
    std::string registry_tool_name;
    std::string menu_display_name;
    std::string launch_command;
};

// Activation is shared by the clean application model and the MFC adapter.
// The host supplies only native activation/menu operations; favourite-place
// limits, command ids, and ordering remain application policy.
class MainFrameActivationPlatform {
public:
    virtual ~MainFrameActivationPlatform() = default;

    virtual void forward_activate_frame(int show_command) = 0;
    virtual void rebuild_creature_selection_menu() = 0;
    virtual std::size_t favourite_place_count() const = 0;
    virtual std::string favourite_place_name(std::size_t index) const = 0;
    virtual ui::MenuHandle* favourite_places_menu() = 0;
    virtual void show_missing_favourite_places_menu_warning() = 0;
    [[noreturn]] virtual void terminate_process(int exit_code) = 0;
    virtual void append_favourite_place_menu_item(
        ui::MenuHandle& menu,
        std::uint32_t command_id,
        std::string_view name) = 0;
};

// The iconic-window guard is shared policy.  The host supplies the native
// MINMAXINFO forwarding and renderer/window predicates; the re-entry flag is
// state owned by the frame policy instance.
class MainFrameMinMaxPlatform {
public:
    virtual ~MainFrameMinMaxPlatform() = default;

    virtual void forward_default_min_max_info() = 0;
    virtual bool world_renderer_exists() const = 0;
    virtual bool window_is_iconic() const = 0;
};

// CFrameWnd, CEventBar, CMainToolbar, OLE revocation, and USER32 message
// plumbing belong to this adapter.  CMainFrame owns the fixed tool-record
// collection and the recovered lifetime/menu policy.
class MainFrameLifecyclePlatform : public MainFrameActivationPlatform,
                                   public MainFrameMinMaxPlatform {
public:
    virtual ~MainFrameLifecyclePlatform() = default;

    // CreateObject is the allocation-time factory, not window creation: the
    // native constructs the CFrameWnd base, the 20 embedded records, the event
    // bar, the toolbar, and the selector combo/font, then publishes the frame,
    // event-bar, and toolbar globals.  None of that can fail once the
    // allocation succeeds, so these are void.  Window creation and toolbar
    // Create() belong to MainFrameCreatePlatform / initialize_main_frame.
    virtual void construct_frame_controls() = 0;
    virtual void publish_main_frame() = 0;
    virtual void unpublish_main_frame() = 0;
    virtual void destroy_frame_controls() = 0;

    virtual void terminate_launched_kit_processes() = 0;
    virtual std::string embedded_tool_message(std::size_t tool_index) const = 0;
    virtual std::string forward_default_message(
        std::uint32_t command_id) const = 0;

    virtual bool window_is_iconic() const = 0;
    virtual MainFrameWindowRect window_rect() const = 0;
    virtual void save_window_rect(const MainFrameWindowRect& rect) = 0;
    virtual void stop_pipe_server() = 0;
    virtual void revoke_ole_factories() = 0;
    virtual void forward_default_close() = 0;
    virtual bool window_still_exists() const = 0;
    virtual void post_quit_message(int exit_code) = 0;

    virtual void shutdown_embedded_kit_tool(std::size_t tool_index) = 0;

    // WM_TIMER delivery is an MFC boundary. The document owns the world
    // tick; the frame only forwards the timer event to that application
    // service.
    virtual void update_document_world() = 0;
};

// The testing command is frame-owned dispatch, Creature-owned vocabulary and
// life-stage policy, and runtime-owned genome reload/speech/CRT services.
class MainFrameAgeCommandPlatform {
public:
    virtual ~MainFrameAgeCommandPlatform() = default;
    virtual creatures::Creature* selected_creature() = 0;
    virtual creatures::CreatureEnvironmentHost&
    creature_environment() = 0;
    virtual const creatures::MultibyteTextApi& text_api() const = 0;
    virtual creatures::Creature::CreatureSpeechHost& speech_host() = 0;
};

class CMainFrame {
public:
    static std::unique_ptr<CMainFrame> CreateObject(
        MainFrameLifecyclePlatform& platform);
    ~CMainFrame();

    std::string GetMessageString(std::uint32_t command_id) const;
    void OnClose();
    void OnGetMinMaxInfo();
    void OnTimer();
    void OnAgeSelectedCreatureAndSeedWords(
        MainFrameAgeCommandPlatform& platform);
    long OnShutdownEmbeddedKitTool(std::int32_t tool_index);
    void ActivateFrame(int show_command);

    const std::array<CMainFrameEmbeddedRecord, 20>& embedded_tools() const {
        return embedded_tools_;
    }

private:
    explicit CMainFrame(MainFrameLifecyclePlatform& platform)
        : platform_(platform) {}

    MainFrameLifecyclePlatform& platform_;
    std::array<CMainFrameEmbeddedRecord, 20> embedded_tools_{};
    bool min_max_update_in_progress_ = false;
};

// Registry, USER32 metrics, and the CFrameWnd base call stay in the host;
// this interface exposes only the recovered window-placement policy.
class MainFrameWindowCreationPlatform {
public:
    virtual ~MainFrameWindowCreationPlatform() = default;
    virtual MainFrameWindowRect default_window_rect() const = 0;
    virtual bool read_saved_window_rect(MainFrameWindowRect& rect) const = 0;
    virtual void save_window_rect(const MainFrameWindowRect& rect) = 0;
    virtual int minimum_window_x() const = 0;
    virtual int minimum_window_y() const = 0;
    virtual void forward_default_pre_create(
        MainFrameCreateParameters& parameters) = 0;
};

// CMainFrame owns the command-level toggle, while embedded-kit activation,
// shutdown, and the Funeral Kit document flush remain platform/record
// operations.  The command id is intentionally kept at this boundary: the
// original menu range is a fixed 20-slot application convention.
class MainFrameEmbeddedKitTogglePlatform {
public:
    virtual ~MainFrameEmbeddedKitTogglePlatform() = default;
    virtual bool embedded_kit_is_running(std::size_t tool_index) const = 0;
    virtual void execute_embedded_kit_tool(std::size_t tool_index) = 0;
    virtual void shutdown_embedded_kit_tool(std::size_t tool_index) = 0;
    virtual void flush_funeral_kit_document_state() = 0;
    virtual void invalidate_main_toolbar() = 0;
};

// CCmdUI, the embedded-record array, lazy MaxKits registry state, and the
// Injector Kit preference are host-owned.  This interface exposes the
// recovered enablement policy without reproducing MFC/UI or registry ABI.
class MainFrameEmbeddedKitUpdatePlatform {
public:
    virtual ~MainFrameEmbeddedKitUpdatePlatform() = default;
    virtual bool embedded_kit_is_running(std::size_t tool_index) const = 0;
    virtual void set_command_checked(bool checked) = 0;
    virtual void set_command_enabled(bool enabled) = 0;
    virtual std::size_t ensure_max_embedded_kit_count() = 0;
    virtual std::size_t active_embedded_kit_count() const = 0;
    virtual bool world_update_is_running() const = 0;
    virtual std::size_t selected_creature_count() const = 0;
    virtual std::size_t max_norn_count() const = 0;
    virtual bool injector_allows_without_subject() const = 0;
    // The adapter returns false when no creature is selected or when the
    // selected creature's death state is not ALIVE.
    virtual bool selected_creature_is_alive() const = 0;
};

// The private pipe message is delivered by MFC, but command execution and
// completion signaling are application policy.  The adapter supplies the
// existing scripting dispatcher and event primitive.
class MainFramePipeServerPlatform {
public:
    virtual ~MainFramePipeServerPlatform() = default;
    virtual std::string dispatch_pipe_command(std::string_view command) = 0;
    virtual void signal_pipe_server_command_complete() = 0;
};

// MFC CWnd/CFrameWnd and the WorldRenderer palette DC are host concerns.  The
// recovered frame policy is the forwarding and focus/renderer decision below.
class MainFramePalettePlatform {
public:
    virtual ~MainFramePalettePlatform() = default;
    virtual void forward_default_query_new_palette() = 0;
    virtual std::uint32_t realize_world_renderer_palette() = 0;
    virtual void invalidate_world_renderer_window() = 0;
    virtual void forward_default_palette_changed(
        const void* palette_focus_window) = 0;
    virtual bool palette_focus_is_this_frame(
        const void* palette_focus_window) const = 0;
    virtual bool palette_focus_is_sfc_view(
        const void* palette_focus_window) const = 0;
    virtual bool palette_focus_is_world_renderer_owner(
        const void* palette_focus_window) const = 0;
};

// The adapter owns CFrameWnd/CMenu/CStatusBar, resource-string loading,
// USER32 menu calls, and process termination.  This interface contains only
// the C1 startup policy recovered from CMainFrame::OnCreate.
class MainFrameCreatePlatform {
public:
    virtual ~MainFrameCreatePlatform() = default;

    virtual int create_frame_base() = 0;
    virtual MainMenuHandle* root_menu() = 0;
    virtual std::size_t menu_item_count(const MainMenuHandle& menu) const = 0;
    virtual const char* menu_item_text(const MainMenuHandle& menu,
                                       std::size_t position) const = 0;
    virtual bool replace_menu_item_text(MainMenuHandle& menu,
                                        std::size_t position,
                                        std::uint32_t string_resource_id) = 0;
    virtual void show_missing_root_menu_warning() = 0;
    virtual void show_unsupported_language_warning() = 0;
    [[noreturn]] virtual void terminate_process(int exit_code) = 0;

    virtual bool create_main_toolbar() = 0;
    virtual bool set_status_bar_indicators(
        const MainFrameStatusIndicatorState& indicators) = 0;
    virtual MainFrameStatusPaneInfo status_pane_info(
        std::size_t pane_index) const = 0;
    virtual void set_status_pane_info(std::size_t pane_index,
                                      const MainFrameStatusPaneInfo& info) = 0;
    virtual std::uint32_t privilege_level() const = 0;
    virtual void remove_menu_item_by_position(MainMenuHandle& menu,
                                              std::size_t position) = 0;
};

int initialize_main_frame(MainFrameCreatePlatform& platform);
void activate_main_frame(MainFrameActivationPlatform& platform,
                         int show_command);
void update_main_frame_min_max(MainFrameMinMaxPlatform& platform,
                               bool& min_max_update_in_progress);
void toggle_embedded_kit_tool(MainFrameEmbeddedKitTogglePlatform& platform,
                              std::uint32_t command_id);
void update_embedded_kit_tool_command(
    MainFrameEmbeddedKitUpdatePlatform& platform, std::uint32_t command_id);
long handle_pipe_server_command(
    MainFramePipeServerPlatform& platform,
    scripting::PipeServerCommandContext* command_context);
std::uint32_t query_new_main_frame_palette(MainFramePalettePlatform& platform);
void notify_main_frame_palette_changed(
    MainFramePalettePlatform& platform, const void* palette_focus_window);
void prepare_main_frame_window(MainFrameWindowCreationPlatform& platform,
                               MainFrameCreateParameters& parameters);

} // namespace creatures1::application
