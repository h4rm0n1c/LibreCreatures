#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "../brain/classifiers.hpp"
#include "../creatures/bacterium.hpp"
#include "../creatures/creature.hpp"
#include "../creatures/genome.hpp"
#include "../creatures/selection.hpp"

namespace creatures1::objects {
class Object;
}

namespace creatures1::scripting {
struct ScriptClassifier;
}

namespace creatures1::application {

class CommandUi {
public:
    virtual ~CommandUi() = default;

    virtual void set_enabled(bool enabled) = 0;
    virtual void set_checked(bool checked) = 0;
};

class BurbleSettingStore {
public:
    virtual ~BurbleSettingStore() = default;

    virtual bool burble_is_enabled() const = 0;
    virtual void set_burble_enabled(bool enabled) = 0;
};

struct CreatureSelectionCommandState {
    bool creature_is_present;
    bool is_below_world_boundary;
    bool is_selected;
};

struct DebugLogState {
    std::uint32_t category_mask;
    bool logging_is_enabled;
};

void update_eye_view_command(CommandUi& ui,
                             bool selected_creature_is_alive,
                             bool world_is_running,
                             bool eye_view_is_available);

void toggle_burble_setting(BurbleSettingStore& settings);

void update_burble_command(CommandUi& ui,
                           bool burble_is_enabled,
                           bool world_is_running);

// SFCApp has a second, receiver-based command path in the binary.  It uses
// the stored value directly for the check state; the CApplication global
// wrapper above intentionally preserves its opposite check-state policy.
void update_sfcapp_burble_command(CommandUi& ui,
                                  bool burble_is_enabled,
                                  bool world_is_running);

// The binary contains one MFC update wrapper per menu index.  The source
// contract is the shared policy, parameterized by the selected index.
void update_creature_selection_command(
    CommandUi& ui,
    std::size_t menu_index,
    const std::vector<CreatureSelectionCommandState>& selection,
    bool world_is_running);

void toggle_debug_log_category(DebugLogState& state,
                               std::uint32_t command_id);

void update_debug_log_category_command(CommandUi& ui,
                                       const DebugLogState& state,
                                       std::uint32_t command_id,
                                       bool world_is_running);

void toggle_debug_logging(DebugLogState& state);

void update_debug_logging_command(CommandUi& ui,
                                  const DebugLogState& state,
                                  bool world_is_running);

// CApplication::ToggleMuteAndStopSounds owns the state transition and the
// stop-on-unmute edge.  SoundManager and registry DWORD persistence are host
// services; neither their concrete ABI nor their Windows handles belong in
// the application policy.
class MuteControlHost {
public:
    virtual ~MuteControlHost() = default;

    virtual bool mute_is_enabled() const = 0;
    virtual void set_mute_enabled(bool enabled) = 0;
    virtual void stop_all_sounds() = 0;
    virtual void persist_mute_enabled(bool enabled) = 0;
};

void toggle_mute_and_stop_sounds(MuteControlHost& host);

// The following interfaces are the application-owned policies behind the
// CApplication/SFCApp handlers.  Their implementations are deliberately
// independent of MFC, USER32, SHELL32, and MSVC's CString/stream layout.
// Platform adapters provide the operations at the boundary.
class CreatureNameHistoryUi {
public:
    virtual ~CreatureNameHistoryUi() = default;

    virtual std::string current_creature_name() const = 0;
    virtual std::size_t find_exact_name(std::string_view name) const = 0;
    virtual void remove_name_at(std::size_t index) = 0;
    virtual void insert_name_at_front(std::string_view name) = 0;
    virtual void select_first_name() = 0;
    virtual std::size_t name_count() const = 0;
    virtual void update_pointer_tool_name(std::string_view name) = 0;
};

void update_creature_name_combo_history(CreatureNameHistoryUi& ui,
                                        std::uint32_t world_tick,
                                        std::uint32_t& next_update_tick);

class VolumeDialogHost {
public:
    virtual ~VolumeDialogHost() = default;

    virtual bool volume_dialog_exists() const = 0;
    virtual bool create_volume_dialog() = 0;
    virtual void discard_failed_volume_dialog() = 0;
    virtual void show_volume_dialog() = 0;
    virtual void bring_volume_dialog_to_front() = 0;
};

void show_volume_dialog(VolumeDialogHost& host);

class WebpageShortcutHost {
public:
    virtual ~WebpageShortcutHost() = default;

    virtual std::string primary_resource_directory() const = 0;
    virtual bool is_regular_file(std::string_view path) const = 0;
    virtual void show_missing_webpage_error() = 0;
    virtual void launch_webpage_shortcut(std::string_view shortcut_name,
                                         std::string_view working_directory) = 0;
};

void open_webpage_url_shortcut(WebpageShortcutHost& host);

enum class WorldUpdateTimerState : std::uint32_t {
    paused = 0,
    running = 1,
};

class WorldUpdateTimerHost {
public:
    virtual ~WorldUpdateTimerHost() = default;

    virtual void broadcast_world_control_state(std::uint32_t command) = 0;
    virtual void set_world_update_timer(std::uint32_t interval_ms) = 0;
    virtual void invalidate_main_toolbar() = 0;
};

void arm_world_update_timer(WorldUpdateTimerHost& host,
                            WorldUpdateTimerState& state,
                            std::uint32_t& interval_ms);

class EuthanasiaHost {
public:
    virtual ~EuthanasiaHost() = default;

    virtual bool confirm_euthanasia() = 0;
    virtual bool selected_creature_exists() const = 0;
    virtual void kill_selected_creature() = 0;
};

bool request_euthanasia(EuthanasiaHost& host);

class DebugConsoleHost {
public:
    virtual ~DebugConsoleHost() = default;

    virtual bool debug_console_exists() const = 0;
    virtual bool output_stream_is_open() const = 0;
    virtual void close_output_stream() = 0;
    virtual void show_debug_console() = 0;
    virtual void create_debug_console() = 0;
};

void ensure_debug_console_dialog(DebugConsoleHost& host);

void update_show_debug_console_command(CommandUi& ui,
                                       bool debug_console_exists,
                                       bool world_is_running);

class FavouritePlaceDialogHost {
public:
    virtual ~FavouritePlaceDialogHost() = default;

    virtual void run_remove_favourite_place_dialog() = 0;
};

void show_remove_favourite_place_dialog(FavouritePlaceDialogHost& host);

// Advances the application selection array and performs the recovered UI
// synchronization.  Selection storage, Creature lifetime, eye-view/window
// controls, and event-bar rendering are supplied by the owning adapters.
class CreatureSelectionCycleHost {
public:
    virtual ~CreatureSelectionCycleHost() = default;

    virtual creatures1::creatures::CreatureSelectionEntry*
        selected_creature() const = 0;
    virtual std::size_t selection_count() const = 0;
    virtual creatures1::creatures::CreatureSelectionEntry*
        selection_at(std::size_t index) const = 0;
    virtual void set_selected_creature(
        creatures1::creatures::CreatureSelectionEntry* creature) = 0;
    // The binary passes unrelated caller context in ECX to the embedded-kit
    // broadcaster.  The broadcaster ignores it; the only C1-owned value is
    // the published selection state code.
    virtual void broadcast_selection_state(std::uint32_t state_code) = 0;
    virtual void update_main_window_title() = 0;
    virtual bool eye_view_exists() const = 0;
    virtual void close_eye_view() = 0;
    virtual void update_eye_view_title() = 0;
    virtual void invalidate_eye_view_follow_position() = 0;
    virtual void return_viewport_navigation_to_selection() = 0;
    virtual void refresh_event_bar() = 0;
    virtual void invalidate_main_toolbar() = 0;
    [[noreturn]] virtual void throw_invalid_selection_argument() = 0;
};

// Applies the common selected-creature transition used by menu selection and
// next/previous navigation.  Eye-view, viewport, event-bar, toolbar, and
// embedded-kit implementations remain host-owned platform/UI boundaries.
void apply_selected_creature(CreatureSelectionCycleHost& host,
                              creatures1::creatures::CreatureSelectionEntry*
                                  creature,
                              bool return_to_creature);

void select_creature_by_menu_command(CreatureSelectionCycleHost& host,
                                     int menu_index);

void select_next_creature(CreatureSelectionCycleHost& host);
void select_previous_creature(CreatureSelectionCycleHost& host);

// These three native bodies are message-map thunks whose only C1 operation
// is the shared index policy. The MFC command-map entries remain framework
// metadata; the clean source has one parameterized implementation.

class SelectedCreatureScriptCommandHost {
public:
    virtual ~SelectedCreatureScriptCommandHost() = default;
    virtual objects::Object* selected_creature_object() const = 0;
    virtual void execute_script_for_classifier(
        objects::Object& script_owner,
        objects::Object& from_object,
        scripting::ScriptClassifier classifier,
        bool force_restart) = 0;
};

void trigger_selected_creature_script_event(
    SelectedCreatureScriptCommandHost& host, int event_offset);

class CreatureImportArchive {
public:
    virtual ~CreatureImportArchive() = default;
    virtual creatures1::creatures::Creature* read_creature() = 0;
    virtual void dispatch_after_bounds_update(
        creatures1::creatures::Creature& creature) = 0;
    virtual bool is_family_four(
        const creatures1::creatures::Creature& creature) const = 0;
    virtual void deserialize_creature(
        creatures1::creatures::Creature& creature) = 0;
};

class CreatureImportHost {
public:
    virtual ~CreatureImportHost() = default;
    virtual bool prompt_for_import_path(std::string& path) = 0;
    virtual void log_import_started() = 0;
    virtual std::unique_ptr<CreatureImportArchive>
        begin_import_archive(std::string_view path) = 0;
    virtual void remove_import_file(std::string_view path) = 0;
};

void import_creature(CreatureImportHost& host);

class GeneratedCreatureCommandHost {
public:
    virtual ~GeneratedCreatureCommandHost() = default;
    virtual creatures1::creatures::GenomeFilenameId
        generate_test_offspring_genome() = 0;
    virtual creatures1::creatures::Creature* create_generated_creature(
        creatures1::creatures::GenomeFilenameId genome_source_filename,
        creatures1::creatures::CreatureConstructionSex construction_sex) = 0;
    virtual void set_selected_creature(
        creatures1::creatures::Creature* creature) = 0;
    virtual void set_edit_object(
        creatures1::creatures::Creature* creature) = 0;
};

void create_and_select_generated_creature(
    GeneratedCreatureCommandHost& host,
    creatures1::creatures::CreatureConstructionSex construction_sex);

class InfectSelectedCreatureHost {
public:
    virtual ~InfectSelectedCreatureHost() = default;
    virtual creatures1::creatures::Creature* selected_creature() const = 0;
    virtual creatures1::creatures::BacteriumRandomSource& random_source() = 0;
    virtual creatures1::creatures::Bacterium& world_bacterium_at(
        std::size_t index) = 0;
    virtual bool debug_console_visible() const = 0;
    virtual void log_infection() = 0;
};

void infect_selected_creature_with_random_bacterium(
    InfectSelectedCreatureHost& host);

class ForceAgeSelectedCreatureHost {
public:
    virtual ~ForceAgeSelectedCreatureHost() = default;
    virtual creatures1::creatures::Creature* selected_creature() const = 0;
    virtual creatures1::creatures::CreatureEnvironmentHost& environment() = 0;
    virtual common::DebugLogHost* debug_log_host() = 0;
};

void force_age_selected_creature_one_stage(
    ForceAgeSelectedCreatureHost& host);

struct IdleCadenceState {
    std::int32_t current_idle_cycle = 0;
    std::int32_t smoothed_idle_cycle = 0;
};

void update_idle_cadence(IdleCadenceState& state,
                         std::int32_t idle_cycle_index);

class FileCommandHost {
public:
    virtual ~FileCommandHost() = default;

    virtual void stop_world_update_timer() = 0;
    virtual void invoke_base_file_open() = 0;
    virtual void invoke_base_file_new() = 0;
    virtual void restart_world_update_timer(std::uint32_t interval_ms) = 0;
};

void handle_file_open(FileCommandHost& host, std::uint32_t& interval_ms);
void handle_file_new(FileCommandHost& host, std::uint32_t& interval_ms);

struct CreatureExportState {
    std::uint32_t classifier_family = 0;
    creatures1::creatures::GenomeFilenameId genome_source_filename = 0;
    creatures1::creatures::GenomeSex genome_sex =
        creatures1::creatures::GenomeSex::male;
    creatures1::creatures::GenomeLifeStage genome_life_stage =
        creatures1::creatures::GenomeLifeStage::stage_zero;
    creatures1::creatures::GenomeFilenameId child_genome_source_filename = 0;
};

// The export handler owns the C1-specific inclusion/order policy.  CFile,
// CArchive, runtime-class dispatch, Genome construction, debug-dialog
// logging, and the selected Creature's complete storage are supplied by the
// platform/creature adapter behind this interface.
class CreatureExportArchive {
public:
    virtual ~CreatureExportArchive() = default;

    virtual void write_selected_creature() = 0;
    virtual void write_genome(
        creatures1::creatures::GenomeFilenameId source_filename,
        creatures1::creatures::GenomeSex sex,
        creatures1::creatures::GenomeLifeStage life_stage) = 0;
};

class CreatureExportHost {
public:
    virtual ~CreatureExportHost() = default;

    virtual bool selected_creature_exists() const = 0;
    virtual const CreatureExportState* selected_creature_for_export() const = 0;
    virtual bool prompt_for_export_path(std::string& output_path) = 0;
    virtual void log_export_started() = 0;
    virtual std::unique_ptr<CreatureExportArchive>
        begin_export_archive(std::string_view output_path) = 0;
    virtual void clear_selected_creature_references() = 0;
    virtual void restore_selected_creature_runtime_state() = 0;
    virtual void log_child_genome_export() = 0;
};

void export_current_creature(CreatureExportHost& host);

struct EyeViewCreationParameters {
    std::int32_t viewport_width = 0x80;
    std::int32_t viewport_height = 0x60;
    bool smooth_scrolling = false;
    std::uint32_t overlay_gallery_identifier = 0x62627562;
};

class EyeViewApplicationHost {
public:
    virtual ~EyeViewApplicationHost() = default;

    // Allocation, MFC window lifetime, and selected-Creature virtual
    // dispatch stay in the platform/UI adapter.
    virtual bool eye_view_exists() const = 0;
    virtual void persist_eye_view_position() = 0;
    virtual void destroy_eye_view() = 0;
    virtual std::int32_t selected_creature_sound_source_x() const = 0;
    virtual std::int32_t selected_creature_sound_source_y() const = 0;
    virtual std::string eye_view_title() const = 0;
    virtual void create_eye_view(const EyeViewCreationParameters& parameters,
                                 std::int32_t initial_viewport_left,
                                 std::int32_t initial_viewport_top,
                                 std::string_view title) = 0;
};

void toggle_eye_view(EyeViewApplicationHost& host);

class CaosConsoleApplicationHost {
public:
    virtual ~CaosConsoleApplicationHost() = default;

    // Dialog allocation, MFC construction, resource creation, and native
    // show/activation calls stay in the platform/UI adapter.
    virtual bool caos_console_exists() const = 0;
    virtual void activate_caos_console() = 0;
    virtual void create_caos_console() = 0;
    virtual void show_caos_console() = 0;
};

void show_caos_console_dialog(CaosConsoleApplicationHost& host);

constexpr std::size_t kSfcAppResourceDirectoryCount = 8;

struct SfcAppResourceDirectories {
    std::array<std::string, kSfcAppResourceDirectoryCount> paths{};
};

struct SfcAppCommandLineMode {
    bool run_embedded = false;
    bool run_automated = false;
};

struct SfcAppPatchMetadata {
    std::string_view product_message;
    std::string_view build_id;
    std::string_view build_type;
};

// InitInstance is an application coordinator, not a source-level owner of
// MFC's document-template, registry, window, OLE, or thread layouts.  This
// boundary names each recovered startup effect so those platform details do
// not leak into the reconstructed C1 source.
class SfcAppStartupHost {
public:
    virtual ~SfcAppStartupHost() = default;

    virtual bool initialize_ole() = 0;
    virtual void report_ole_initialization_failure() = 0;
    virtual void load_standard_profile_settings() = 0;
    virtual void install_document_template() = 0;
    virtual void connect_document_template_server() = 0;

    // Loading a set also performs the binary's registry defaults.  The return
    // value reports only whether its first (Main Directory) entry could be
    // made the process current directory.
    virtual bool load_resource_directories(
        bool secondary,
        SfcAppResourceDirectories& directories) = 0;
    virtual void report_resource_directory_failure() = 0;
    virtual void read_text_lines(std::string_view path,
                                 std::vector<std::string>& lines) const = 0;

    virtual void publish_classifier_name(std::string_view key,
                                         std::string_view display_name) = 0;
    virtual void set_world_save_path(std::string_view path) = 0;
    virtual void publish_uninstall_command(std::string_view path) = 0;

    virtual SfcAppCommandLineMode parse_command_line() = 0;
    virtual void update_document_server_registry() = 0;
    virtual void update_ole_factory_registry() = 0;
    virtual void write_patch_registry_metadata(
        const SfcAppPatchMetadata& metadata) = 0;
    virtual void delete_autorun_registration() = 0;

    virtual void register_ole_factories() = 0;
    // This is the recovered CWinApp::m_pMainWnd test.  If it is false,
    // prepare_embedded_main_window performs the host's USER-mode frame
    // creation or normal World.sfc document open.
    virtual bool main_window_exists() const = 0;
    // The host performs the recovered privilege split: USER shows the frame,
    // other modes open World.sfc and report failure.  Returning false means
    // InitInstance must abort embedded startup.
    virtual bool prepare_embedded_main_window(std::string_view world_path) = 0;
    virtual void accept_file_drops() = 0;
    virtual void ensure_sound_system() = 0;
    virtual void start_pipe_server_if_needed() = 0;
    virtual void show_startup_tip_dialog() = 0;
};

void load_sfc_app_classifier_names(SfcAppStartupHost& host,
                                   std::string_view primary_directory);

bool initialise_sfc_app(SfcAppStartupHost& host);

// SFCApp shutdown owns the ordering of process-wide services.  The concrete
// MFC exit call, pipe handles, and SoundManager global lifetime stay in the
// application host; they are not reconstructed as loose decompiler helpers.
class SfcAppShutdownHost {
public:
    virtual ~SfcAppShutdownHost() = default;

    virtual void terminate_launched_kit_processes() = 0;
    virtual void stop_pipe_server() = 0;
    virtual void destroy_sound_manager() = 0;
    virtual void forward_default_exit_instance() = 0;
};

class SFCApp {
public:
    static void ExitInstance(SfcAppShutdownHost& host);
};

enum class PrivilegeLevel : std::uint32_t {
    unknown = 0,
    green_tea = 1,
    elevated = 2,
    user = 3,
};

struct SfcAppState {
    // SFCApp::OnIdle @ 0x0043f360 keeps the idle cadence in two process
    // globals; they are app-scoped state, so they live with the rest of it.
    IdleCadenceState idle_cadence{};
    std::uint32_t max_norns_setting = 0;
    std::uint32_t autosave_interval_ms = 360000;
    bool burble_enabled = true;
    PrivilegeLevel privilege_level = PrivilegeLevel::unknown;
};

class SfcAppSettingsHost {
public:
    virtual ~SfcAppSettingsHost() = default;

    // Registry handles, MFC base construction, OLE storage, and the global
    // SFCApp instance are framework boundaries. The two privilege matchers
    // own the binary's encoded labels without exposing opaque fragments in
    // reconstructed source.
    virtual void publish_sfc_app_instance(SfcAppState& state) = 0;
    virtual bool query_burble_enabled(bool& enabled) const = 0;
    virtual void write_burble_enabled(bool enabled) = 0;
    virtual bool query_autosave_interval_ms(
        std::uint32_t& interval_ms) const = 0;
    virtual bool query_privileges(std::string& privilege_name) const = 0;
    virtual void write_default_privileges(std::string_view privilege_name) = 0;
    virtual bool is_user_privilege(std::string_view privilege_name) const = 0;
    virtual bool is_elevated_privilege(
        std::string_view privilege_name) const = 0;
};

void initialise_sfc_app_state(SfcAppState& state,
                              SfcAppSettingsHost& settings);

} // namespace creatures1::application
