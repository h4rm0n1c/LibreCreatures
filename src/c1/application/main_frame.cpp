#include "main_frame.hpp"

#include <algorithm>
#include <cstring>

#include "../scripting/pipe_server.hpp"

namespace creatures1::application {
namespace {

constexpr std::uint32_t kCameraMenuResourceId = 0xef27;
constexpr std::uint32_t kNornsMenuResourceId = 0xef29;
constexpr std::uint32_t kToolsMenuResourceId = 0xef28;
constexpr std::size_t kLocalizedMenuCount = 3;
constexpr std::size_t kStatusPaneCount = 14;
constexpr std::uint32_t kStatusPaneStyle = 0x04000100;
constexpr std::uint32_t kMinimumPrivilegeForTestingMenu = 2;
constexpr std::uint32_t kFirstEmbeddedKitCommand = 0x8086;
constexpr std::size_t kEmbeddedKitCommandCount = 20;
constexpr std::size_t kFuneralKitIndex = 9;
constexpr std::size_t kInjectorKitIndex = 7;

struct MenuLocalizationEntry {
    const char* marker;
    std::uint32_t resource_id;
};

constexpr MenuLocalizationEntry kMenuLocalization[] = {
    {"_IDS_CAMERA_MENU_", kCameraMenuResourceId},
    {"_IDS_NORNS_MENU_", kNornsMenuResourceId},
    {"_IDS_TOOLS_MENU_", kToolsMenuResourceId},
};

MainFrameStatusIndicatorState make_status_indicator_state() {
    MainFrameStatusIndicatorState state{};
    state.command_ids[0] = 0;
    state.command_ids[1] = 0xef1b;
    state.command_ids[2] = 0xef1b;
    state.command_ids[3] = 0xef1b;
    // Ghidra's exact read of the four-word initializer at 0045d4d0 is
    // {0x0000ef1b, 0x0000ef1b, 0x0000ef1b, 0x0000ef1b}.
    state.command_ids[4] = 0xef1b;
    state.command_ids[5] = 0xef1b;
    state.command_ids[6] = 0xef1b;
    state.command_ids[7] = 0xef1b;
    state.command_ids[8] = 0xef1b;
    state.command_ids[9] = 0xef1b;
    state.command_ids[10] = 0xef1b;
    state.command_ids[11] = 0xef1a;
    state.command_ids[12] = 0xef19;
    state.command_ids[13] = 0xef20;
    return state;
}

std::size_t localize_root_menu(MainFrameCreatePlatform& platform,
                               MainMenuHandle& menu) {
    std::size_t localized_count = 0;
    const std::size_t initial_item_count = platform.menu_item_count(menu);
    for (std::size_t position = 0; position < initial_item_count; ++position) {
        const char* text = platform.menu_item_text(menu, position);
        for (const MenuLocalizationEntry& entry : kMenuLocalization) {
            if (text != nullptr && std::strcmp(text, entry.marker) == 0) {
                if (platform.replace_menu_item_text(menu, position,
                                                     entry.resource_id)) {
                    ++localized_count;
                }
            }
        }
        if (localized_count == kLocalizedMenuCount) {
            break;
        }
    }
    return localized_count;
}

} // namespace

CMainFrameEmbeddedRecord::CMainFrameEmbeddedRecord() {
    toolbar_bitmap_index = 0;
    is_available = false;
    ole_dispatch_driver.connected = false;
    tool_runtime.launched_process_handle = 0;
    registry_tool_name.clear();
    menu_display_name.clear();
    launch_command.clear();
}

std::unique_ptr<CMainFrame> CMainFrame::CreateObject(
    MainFrameLifecyclePlatform& platform) {
    auto frame = std::unique_ptr<CMainFrame>(new CMainFrame(platform));
    platform.construct_frame_controls();
    platform.publish_main_frame();
    return frame;
}

CMainFrame::~CMainFrame() {
    platform_.terminate_launched_kit_processes();
    platform_.unpublish_main_frame();
    platform_.destroy_frame_controls();
}

void CMainFrame::OnTimer() {
    platform_.update_document_world();
}

void CMainFrame::OnAgeSelectedCreatureAndSeedWords(
    MainFrameAgeCommandPlatform& platform) {
    creatures::Creature* selected = platform.selected_creature();
    if (selected != nullptr) {
        selected->apply_instant_verb_vocabulary(
            platform.creature_environment(), platform.text_api(),
            platform.speech_host());
    }
}

std::string CMainFrame::GetMessageString(std::uint32_t command_id) const {
    if (command_id >= kFirstEmbeddedKitCommand &&
        command_id < kFirstEmbeddedKitCommand + kEmbeddedKitCommandCount) {
        return platform_.embedded_tool_message(
            static_cast<std::size_t>(command_id - kFirstEmbeddedKitCommand));
    }
    return platform_.forward_default_message(command_id);
}

void CMainFrame::OnClose() {
    if (!platform_.window_is_iconic()) {
        platform_.save_window_rect(platform_.window_rect());
    }

    platform_.terminate_launched_kit_processes();
    platform_.stop_pipe_server();
    platform_.revoke_ole_factories();
    platform_.forward_default_close();
    if (!platform_.window_still_exists()) {
        platform_.post_quit_message(0);
    }
}

void CMainFrame::OnGetMinMaxInfo() {
    update_main_frame_min_max(platform_, min_max_update_in_progress_);
}

long CMainFrame::OnShutdownEmbeddedKitTool(std::int32_t tool_index) {
    if (tool_index >= 0 &&
        static_cast<std::size_t>(tool_index) < kEmbeddedKitCommandCount) {
        platform_.shutdown_embedded_kit_tool(
            static_cast<std::size_t>(tool_index));
    }
    return 0;
}

void CMainFrame::ActivateFrame(int show_command) {
    activate_main_frame(platform_, show_command);
}

void activate_main_frame(MainFrameActivationPlatform& platform,
                         int show_command) {
    platform.forward_activate_frame(show_command);
    platform.rebuild_creature_selection_menu();

    constexpr std::uint32_t kFavouritePlaceMenuIdBase = 0x8053;
    constexpr std::size_t kFavouritePlaceMenuCapacity = 12;
    const std::size_t place_count = std::min(
        platform.favourite_place_count(), kFavouritePlaceMenuCapacity);
    ui::MenuHandle* menu = platform.favourite_places_menu();
    if (menu == nullptr && place_count != 0) {
        platform.show_missing_favourite_places_menu_warning();
        platform.terminate_process(1);
    }

    if (menu != nullptr) {
        for (std::size_t index = 0; index < place_count; ++index) {
            platform.append_favourite_place_menu_item(
                *menu,
                kFavouritePlaceMenuIdBase +
                    static_cast<std::uint32_t>(index),
                platform.favourite_place_name(index));
        }
    }
}

void update_main_frame_min_max(MainFrameMinMaxPlatform& platform,
                               bool& min_max_update_in_progress) {
    platform.forward_default_min_max_info();
    if (!platform.world_renderer_exists()) {
        return;
    }

    if (min_max_update_in_progress) {
        min_max_update_in_progress = false;
        return;
    }

    if (platform.window_is_iconic()) {
        min_max_update_in_progress = true;
    }
}

int initialize_main_frame(MainFrameCreatePlatform& platform) {
    if (platform.create_frame_base() == -1) {
        return -1;
    }

    MainMenuHandle* menu = platform.root_menu();
    if (menu == nullptr) {
        platform.show_missing_root_menu_warning();
        platform.terminate_process(1);
    }

    if (localize_root_menu(platform, *menu) != kLocalizedMenuCount) {
        platform.show_unsupported_language_warning();
    }

    if (!platform.create_main_toolbar()) {
        return -1;
    }

    const MainFrameStatusIndicatorState indicators =
        make_status_indicator_state();
    if (!platform.set_status_bar_indicators(indicators)) {
        return -1;
    }

    for (std::size_t pane_index = 1; pane_index < 11; ++pane_index) {
        MainFrameStatusPaneInfo pane = platform.status_pane_info(pane_index);
        pane.style_flags = kStatusPaneStyle;
        pane.width = 0;
        platform.set_status_pane_info(pane_index, pane);
    }

    if (platform.privilege_level() < kMinimumPrivilegeForTestingMenu) {
        menu = platform.root_menu();
        if (menu != nullptr) {
            platform.remove_menu_item_by_position(*menu, 1);
            platform.remove_menu_item_by_position(*menu, 1);
        }
    }

    return 0;
}

void toggle_embedded_kit_tool(MainFrameEmbeddedKitTogglePlatform& platform,
                              std::uint32_t command_id) {
    const std::size_t tool_index =
        static_cast<std::size_t>(command_id - kFirstEmbeddedKitCommand);
    if (tool_index >= kEmbeddedKitCommandCount) {
        return;
    }

    if (!platform.embedded_kit_is_running(tool_index)) {
        platform.execute_embedded_kit_tool(tool_index);
        if (tool_index == kFuneralKitIndex) {
            platform.flush_funeral_kit_document_state();
        }
    } else {
        platform.shutdown_embedded_kit_tool(tool_index);
    }
    platform.invalidate_main_toolbar();
}

void update_embedded_kit_tool_command(
    MainFrameEmbeddedKitUpdatePlatform& platform, std::uint32_t command_id) {
    if (command_id < kFirstEmbeddedKitCommand ||
        command_id >= kFirstEmbeddedKitCommand + kEmbeddedKitCommandCount) {
        return;
    }

    const std::size_t tool_index = static_cast<std::size_t>(
        command_id - kFirstEmbeddedKitCommand);
    const bool running = platform.embedded_kit_is_running(tool_index);
    platform.set_command_checked(running);

    const std::size_t max_embedded_kits =
        platform.ensure_max_embedded_kit_count();
    if (!running && max_embedded_kits <=
                        platform.active_embedded_kit_count()) {
        platform.set_command_enabled(false);
        return;
    }

    if (tool_index == 0) {
        platform.set_command_enabled(
            platform.world_update_is_running() &&
            platform.selected_creature_count() < platform.max_norn_count());
        return;
    }

    if (tool_index == 6 || tool_index == 8 || tool_index == 9) {
        platform.set_command_enabled(platform.world_update_is_running());
        return;
    }

    // CMainFrame::OnUpdateEmbeddedKitToolCommand @ 0x004216d0: when the
    // Injector Kit's AllowWithoutSubject preference is set, the JNZ at
    // 0x00421840 jumps to 0x0042187e -- the same tail slots 6, 8 and 9 use,
    // which enables on the world timer alone.  It does NOT enable
    // unconditionally; the preference only drops the creature requirement.
    // Ghidra collapses the preference value and the HKEY onto one stack slot
    // here, which is what made the decompiled form look like a discarded
    // read; the instruction stream keeps them apart (pre-zeroed local at
    // ESP+0xc, the key at ESP+0x1c).
    if (tool_index == kInjectorKitIndex &&
        platform.injector_allows_without_subject()) {
        platform.set_command_enabled(platform.world_update_is_running());
        return;
    }

    platform.set_command_enabled(platform.selected_creature_is_alive() &&
                                 platform.world_update_is_running());
}

long handle_pipe_server_command(
    MainFramePipeServerPlatform& platform,
    scripting::PipeServerCommandContext* command_context) {
    if (command_context != nullptr) {
        command_context->response =
            platform.dispatch_pipe_command(command_context->command);
    }
    platform.signal_pipe_server_command_complete();
    return 0;
}

std::uint32_t query_new_main_frame_palette(MainFramePalettePlatform& platform) {
    platform.forward_default_query_new_palette();
    const std::uint32_t realized_entries =
        platform.realize_world_renderer_palette();
    if (realized_entries != 0) {
        platform.invalidate_world_renderer_window();
    }
    return realized_entries;
}

void notify_main_frame_palette_changed(
    MainFramePalettePlatform& platform, const void* palette_focus_window) {
    platform.forward_default_palette_changed(palette_focus_window);
    if (platform.palette_focus_is_this_frame(palette_focus_window)) {
        return;
    }
    if (platform.palette_focus_is_sfc_view(palette_focus_window)) {
        return;
    }
    if (!platform.palette_focus_is_world_renderer_owner(palette_focus_window)) {
        static_cast<void>(platform.realize_world_renderer_palette());
    }
}

void prepare_main_frame_window(MainFrameWindowCreationPlatform& platform,
                               MainFrameCreateParameters& parameters) {
    MainFrameWindowRect rect = platform.default_window_rect();
    if (!platform.read_saved_window_rect(rect)) {
        platform.save_window_rect(rect);
    }

    if (!(rect.left < platform.minimum_window_x() &&
          rect.top < platform.minimum_window_y())) {
        rect = {0, 0, 0x280, 0x1e0};
    }

    parameters.x = rect.left;
    parameters.y = rect.top;
    parameters.width = rect.right - rect.left;
    parameters.height = rect.bottom - rect.top;
    parameters.style = 0x00cf0000;
    platform.forward_default_pre_create(parameters);
}

} // namespace creatures1::application
