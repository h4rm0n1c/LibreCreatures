#include "application.hpp"

#include "../scripting/classifier_scripts.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace creatures1::application {
namespace {

std::array<std::string_view, 4> split_classifier_record(
    const std::string& record) {
    // Every ClassifierNames.txt record is "family|genus|species|name|" -- all
    // 803 lines carry exactly four separators, so the name is delimited by the
    // fourth one, not "the rest of the line".  Taking the remainder left the
    // trailing bar on every display name, and those names reach the screen
    // through the view's classifier overlay.
    std::array<std::string_view, 4> fields{};
    std::size_t field_start = 0;
    for (std::size_t field_index = 0; field_index < fields.size();
         ++field_index) {
        const std::size_t separator = record.find('|', field_start);
        if (separator == std::string::npos) {
            fields[field_index] =
                std::string_view(record).substr(field_start);
            break;
        }
        fields[field_index] = std::string_view(record).substr(
            field_start, separator - field_start);
        field_start = separator + 1;
    }
    return fields;
}

scripting::ScriptClassifier unpack_script_classifier(std::uint32_t packed) {
    return scripting::ScriptClassifier{
        static_cast<scripting::ScriptEvent>(packed & 0xffu),
        static_cast<std::uint8_t>((packed >> 8) & 0xffu),
        static_cast<std::uint8_t>((packed >> 16) & 0xffu),
        static_cast<std::uint8_t>((packed >> 24) & 0xffu)};
}

}  // namespace

void update_eye_view_command(CommandUi& ui,
                             bool selected_creature_is_alive,
                             bool world_is_running,
                             bool eye_view_is_available) {
    ui.set_enabled(selected_creature_is_alive && world_is_running);
    ui.set_checked(eye_view_is_available);
}

void toggle_burble_setting(BurbleSettingStore& settings) {
    settings.set_burble_enabled(!settings.burble_is_enabled());
}

void update_burble_command(CommandUi& ui,
                           bool burble_is_enabled,
                           bool world_is_running) {
    ui.set_checked(!burble_is_enabled);
    ui.set_enabled(world_is_running);
}

void update_sfcapp_burble_command(CommandUi& ui,
                                  bool burble_is_enabled,
                                  bool world_is_running) {
    ui.set_checked(burble_is_enabled);
    ui.set_enabled(world_is_running);
}

void update_creature_selection_command(
    CommandUi& ui,
    std::size_t menu_index,
    const std::vector<CreatureSelectionCommandState>& selection,
    bool world_is_running) {
    if (menu_index >= selection.size()) {
        return;
    }

    const CreatureSelectionCommandState& entry = selection[menu_index];
    const bool enabled = entry.creature_is_present && world_is_running &&
                         !entry.is_below_world_boundary;
    ui.set_enabled(enabled);
    ui.set_checked(entry.is_selected);
}

void toggle_debug_log_category(DebugLogState& state,
                               std::uint32_t command_id) {
    const std::uint32_t category_bit = (command_id - 0x8028u) & 0x1fu;
    state.category_mask ^= (std::uint32_t{1} << category_bit);
}

void update_debug_log_category_command(CommandUi& ui,
                                       const DebugLogState& state,
                                       std::uint32_t command_id,
                                       bool world_is_running) {
    const std::uint32_t category_bit = (command_id - 0x8028u) & 0x1fu;
    ui.set_checked((state.category_mask & (std::uint32_t{1} << category_bit)) != 0);
    ui.set_enabled(world_is_running);
}

void toggle_debug_logging(DebugLogState& state) {
    state.logging_is_enabled = !state.logging_is_enabled;
}

void update_debug_logging_command(CommandUi& ui,
                                  const DebugLogState& state,
                                  bool world_is_running) {
    ui.set_checked(state.logging_is_enabled);
    ui.set_enabled(world_is_running);
}

void toggle_mute_and_stop_sounds(MuteControlHost& host) {
    const bool was_muted = host.mute_is_enabled();
    const bool is_muted = !was_muted;
    host.set_mute_enabled(is_muted);
    if (!was_muted) {
        host.stop_all_sounds();
    }
    host.persist_mute_enabled(is_muted);
}

void update_creature_name_combo_history(CreatureNameHistoryUi& ui,
                                        std::uint32_t world_tick,
                                        std::uint32_t& next_update_tick) {
    if (next_update_tick > world_tick) {
        return;
    }

    const std::string name = ui.current_creature_name();
    const std::size_t existing_index = ui.find_exact_name(name);
    if (existing_index != std::numeric_limits<std::size_t>::max()) {
        ui.remove_name_at(existing_index);
    }
    ui.insert_name_at_front(name);
    ui.select_first_name();

    const std::size_t count = ui.name_count();
    if (count > 10) {
        ui.remove_name_at(count - 1);
    }

    ui.update_pointer_tool_name(name);
    next_update_tick = world_tick + 10;
}

void show_volume_dialog(VolumeDialogHost& host) {
    if (!host.volume_dialog_exists()) {
        if (!host.create_volume_dialog()) {
            host.discard_failed_volume_dialog();
            return;
        }
    }
    host.show_volume_dialog();
    host.bring_volume_dialog_to_front();
}

void open_webpage_url_shortcut(WebpageShortcutHost& host) {
    const std::string directory = host.primary_resource_directory();
    const std::string webpage_path = directory + "\\webpage.url";
    if (!host.is_regular_file(webpage_path)) {
        host.show_missing_webpage_error();
        return;
    }
    host.launch_webpage_shortcut("webpage.url", directory);
}

void arm_world_update_timer(WorldUpdateTimerHost& host,
                            WorldUpdateTimerState& state,
                            std::uint32_t& interval_ms) {
    if (state != WorldUpdateTimerState::running) {
        host.broadcast_world_control_state(9);
        interval_ms = std::clamp(interval_ms, std::uint32_t{1}, std::uint32_t{300});
        host.set_world_update_timer(interval_ms);
        state = WorldUpdateTimerState::running;
    }
    host.invalidate_main_toolbar();
}

bool request_euthanasia(EuthanasiaHost& host) {
    if (!host.confirm_euthanasia() || !host.selected_creature_exists()) {
        return false;
    }
    // Creature::Die owns the alive/dead guard and the complete death
    // sequence.  The application handler only confirms and dispatches.
    host.kill_selected_creature();
    return true;
}

void ensure_debug_console_dialog(DebugConsoleHost& host) {
    if (host.debug_console_exists()) {
        if (!host.output_stream_is_open()) {
            host.close_output_stream();
        }
        host.show_debug_console();
        return;
    }
    host.create_debug_console();
}

void update_show_debug_console_command(CommandUi& ui,
                                       bool debug_console_exists,
                                       bool world_is_running) {
    ui.set_checked(debug_console_exists);
    ui.set_enabled(world_is_running);
}

void show_remove_favourite_place_dialog(FavouritePlaceDialogHost& host) {
    host.run_remove_favourite_place_dialog();
}

void apply_selected_creature(
    CreatureSelectionCycleHost& host,
    creatures1::creatures::CreatureSelectionEntry* creature,
    bool return_to_creature) {
    if (creature == host.selected_creature()) {
        return;
    }

    const std::uint32_t state_code = creature == nullptr ? 8u : 6u;
    host.set_selected_creature(creature);
    host.broadcast_selection_state(state_code);
    host.update_main_window_title();

    if (host.eye_view_exists()) {
        const bool dead = creature != nullptr &&
                          creature->life_state() !=
                              creatures1::creatures::CreatureLifeState::alive;
        if (creature == nullptr || dead) {
            host.close_eye_view();
        } else {
            host.update_eye_view_title();
            host.invalidate_eye_view_follow_position();
        }
    }

    if (creature != nullptr &&
        creature->life_state() !=
            creatures1::creatures::CreatureLifeState::alive) {
        host.broadcast_selection_state(8u);
    }
    if (return_to_creature) {
        host.return_viewport_navigation_to_selection();
    }
    host.refresh_event_bar();
    host.invalidate_main_toolbar();
}

void select_creature_by_menu_command(CreatureSelectionCycleHost& host,
                                     int menu_index) {
    const std::size_t count = host.selection_count();
    if (menu_index < 0 || static_cast<std::size_t>(menu_index) >= count) {
        host.throw_invalid_selection_argument();
    }

    apply_selected_creature(
        host, host.selection_at(static_cast<std::size_t>(menu_index)), true);
    // The original menu handler calls the viewport-return façade once more
    // after the selection transition, even when the transition already did
    // so. Preserve that observable ordering in the semantic policy.
    host.return_viewport_navigation_to_selection();
}

void select_next_creature(CreatureSelectionCycleHost& host) {
    const std::size_t count = host.selection_count();
    const auto* selected = host.selected_creature();
    std::size_t next_index = 0;

    if (selected != nullptr) {
        std::size_t scan_index = 0;
        std::size_t selected_index = count;
        while (scan_index < count) {
            if (host.selection_at(scan_index) == selected) {
                selected_index = scan_index;
                break;
            }
            ++scan_index;
        }

        if (selected_index < count) {
            next_index = selected_index + 1;
            if (next_index == count) {
                next_index = 0;
            }
        }
    } else if (count == 0) {
        return;
    }

    if (next_index >= count) {
        host.throw_invalid_selection_argument();
    }

    apply_selected_creature(host, host.selection_at(next_index), true);
}

void select_previous_creature(CreatureSelectionCycleHost& host) {
    const std::size_t count = host.selection_count();
    const auto* selected = host.selected_creature();
    if (count == 0) {
        return;
    }

    std::size_t previous_index = count - 1;
    if (selected != nullptr) {
        std::size_t scan_index = 0;
        while (scan_index < count && host.selection_at(scan_index) != selected) {
            ++scan_index;
        }
        if (scan_index < count && scan_index != 0) {
            previous_index = scan_index - 1;
        }
        // A missing selection and index zero both wrap to the last entry,
        // matching the executable's backward-search fallthrough.
    }

    apply_selected_creature(host, host.selection_at(previous_index), true);
}

void trigger_selected_creature_script_event(
    SelectedCreatureScriptCommandHost& host, int event_offset) {
    objects::Object* const selected = host.selected_creature_object();
    if (selected == nullptr) {
        return;
    }

    // The command carries the low-word event offset. The native integer add
    // is a packed classifier operation, so preserve its 32-bit wraparound
    // before unpacking the four classifier bytes.
    const std::uint32_t packed_classifier =
        static_cast<std::uint32_t>(event_offset) + 0xff8fu;
    host.execute_script_for_classifier(
        *selected, *selected, unpack_script_classifier(packed_classifier),
        true);
}

void import_creature(CreatureImportHost& host) {
    std::string path;
    if (!host.prompt_for_import_path(path)) {
        return;
    }

    host.log_import_started();
    std::unique_ptr<CreatureImportArchive> archive =
        host.begin_import_archive(path);
    if (archive == nullptr) {
        return;
    }

    creatures1::creatures::Creature* const creature = archive->read_creature();
    if (creature != nullptr) {
        archive->dispatch_after_bounds_update(*creature);
        if (archive->is_family_four(*creature)) {
            archive->deserialize_creature(*creature);
        }
    }
    host.remove_import_file(path);
}

void create_and_select_generated_creature(
    GeneratedCreatureCommandHost& host,
    creatures1::creatures::CreatureConstructionSex construction_sex) {
    const creatures1::creatures::GenomeFilenameId genome_source_filename =
        host.generate_test_offspring_genome();
    creatures1::creatures::Creature* const creature =
        host.create_generated_creature(genome_source_filename,
                                       construction_sex);
    host.set_selected_creature(creature);
    host.set_edit_object(creature);
    if (creature != nullptr) {
        creature->instinct_runtime_state().dream_countdown = 1;
    }
}

void infect_selected_creature_with_random_bacterium(
    InfectSelectedCreatureHost& host) {
    creatures1::creatures::BacteriumRandomSource& random =
        host.random_source();
    const std::uint32_t random_value = random.next();
    creatures1::creatures::Creature* const selected = host.selected_creature();
    if (selected != nullptr &&
        selected->bacterium().activity_state() <
            creatures1::creatures::BacteriumActivityState::active) {
        constexpr std::size_t kWorldBacteriumCount = 100;
        selected->bacterium().replicate_and_mutate(
            host.world_bacterium_at(random_value % kWorldBacteriumCount),
            random);
    }
    if (host.debug_console_visible()) {
        host.log_infection();
    }
}

void force_age_selected_creature_one_stage(
    ForceAgeSelectedCreatureHost& host) {
    creatures1::creatures::Creature* const selected = host.selected_creature();
    if (selected != nullptr) {
        selected->force_age_one_stage(host.environment(), host.debug_log_host());
    }
}

void update_idle_cadence(IdleCadenceState& state,
                         std::int32_t idle_cycle_index) {
    if (idle_cycle_index > 0) {
        state.current_idle_cycle = idle_cycle_index;
        return;
    }
    state.smoothed_idle_cycle =
        (state.current_idle_cycle + state.smoothed_idle_cycle * 4) / 5;
}

namespace {

std::uint32_t clamp_world_update_interval(std::uint32_t interval_ms) {
    return std::clamp(interval_ms, std::uint32_t{1}, std::uint32_t{300});
}

} // namespace

void handle_file_open(FileCommandHost& host, std::uint32_t& interval_ms) {
    host.stop_world_update_timer();
    host.invoke_base_file_open();
    interval_ms = clamp_world_update_interval(interval_ms);
    host.restart_world_update_timer(interval_ms);
}

void handle_file_new(FileCommandHost& host, std::uint32_t& interval_ms) {
    host.stop_world_update_timer();
    host.invoke_base_file_new();
    interval_ms = clamp_world_update_interval(interval_ms);
    host.restart_world_update_timer(interval_ms);
}

void export_current_creature(CreatureExportHost& host) {
    if (!host.selected_creature_exists()) {
        return;
    }

    std::string output_path;
    if (!host.prompt_for_export_path(output_path)) {
        return;
    }

    const CreatureExportState* creature =
        host.selected_creature_for_export();
    if (creature == nullptr) {
        return;
    }

    host.log_export_started();
    std::unique_ptr<CreatureExportArchive> archive =
        host.begin_export_archive(output_path);
    if (archive == nullptr) {
        return;
    }

    host.clear_selected_creature_references();
    archive->write_selected_creature();

    constexpr std::uint32_t kFamilyFourClassifier = 0x04000000;
    if (creature->classifier_family == kFamilyFourClassifier) {
        archive->write_genome(creature->genome_source_filename,
                              creature->genome_sex,
                              creature->genome_life_stage);
        if (creature->child_genome_source_filename != 0) {
            host.log_child_genome_export();
            archive->write_genome(
                creature->child_genome_source_filename,
                creatures1::creatures::GenomeSex::male,
                creatures1::creatures::GenomeLifeStage::stage_zero);
        }
    }

    host.restore_selected_creature_runtime_state();
}

void toggle_eye_view(EyeViewApplicationHost& host) {
    if (host.eye_view_exists()) {
        host.persist_eye_view_position();
        host.destroy_eye_view();
        return;
    }

    const EyeViewCreationParameters parameters{};
    host.create_eye_view(parameters,
                         host.selected_creature_sound_source_x(),
                         host.selected_creature_sound_source_y(),
                         host.eye_view_title());
}

void show_caos_console_dialog(CaosConsoleApplicationHost& host) {
    if (host.caos_console_exists()) {
        host.activate_caos_console();
        return;
    }

    host.create_caos_console();
    host.show_caos_console();
}

void load_sfc_app_classifier_names(SfcAppStartupHost& host,
                                   std::string_view primary_directory) {
    std::string classifier_file(primary_directory);
    classifier_file += "ClassifierNames.txt";

    std::vector<std::string> records;
    host.read_text_lines(classifier_file, records);
    for (const std::string& record : records) {
        const std::array<std::string_view, 4> fields =
            split_classifier_record(record);
        if (fields[3].empty()) {
            continue;
        }
        std::string key;
        key.reserve(fields[0].size() + fields[1].size() +
                    fields[2].size() + 4);
        key.append(fields[0]);
        key += ", ";
        key.append(fields[1]);
        key += ", ";
        key.append(fields[2]);
        host.publish_classifier_name(key, fields[3]);
    }
}

bool initialise_sfc_app(SfcAppStartupHost& host) {
    if (!host.initialize_ole()) {
        host.report_ole_initialization_failure();
        return false;
    }

    host.load_standard_profile_settings();
    host.install_document_template();
    host.connect_document_template_server();

    SfcAppResourceDirectories primary;
    if (!host.load_resource_directories(false, primary)) {
        host.report_resource_directory_failure();
    }

    SfcAppResourceDirectories secondary;
    if (!host.load_resource_directories(true, secondary)) {
        host.report_resource_directory_failure();
    }

    load_sfc_app_classifier_names(host, primary.paths[0]);

    const std::string& world_directory =
        secondary.paths[0].empty() ? primary.paths[0] : secondary.paths[0];
    host.set_world_save_path(world_directory + "World.sfc");
    host.publish_uninstall_command(primary.paths[0] + "\\Remove.exe");

    const SfcAppCommandLineMode command_line = host.parse_command_line();
    if (!command_line.run_embedded && !command_line.run_automated) {
        host.update_document_server_registry();
        host.update_ole_factory_registry();
        host.write_patch_registry_metadata(
            {"The App you've been waiting for!", "1.0.5", "Release"});
        host.delete_autorun_registration();
        return false;
    }

    host.register_ole_factories();
    // The native branch tests CWinApp::m_pMainWnd here.  When the host has
    // not yet materialized the frame, it dispatches either the USER-mode
    // frame creation or the normal World.sfc document open.  Both are one
    // platform operation because the choice depends on the host's recovered
    // privilege/document state, not on C1 startup policy.
    if (!host.main_window_exists() &&
        !host.prepare_embedded_main_window(world_directory + "World.sfc")) {
        return false;
    }
    host.accept_file_drops();
    host.ensure_sound_system();
    host.start_pipe_server_if_needed();
    host.show_startup_tip_dialog();
    return true;
}

void SFCApp::ExitInstance(SfcAppShutdownHost& host) {
    host.terminate_launched_kit_processes();
    host.stop_pipe_server();
    host.destroy_sound_manager();
    host.forward_default_exit_instance();
}

void initialise_sfc_app_state(SfcAppState& state,
                              SfcAppSettingsHost& settings) {
    state.max_norns_setting = 0;
    state.autosave_interval_ms = 360000;
    state.burble_enabled = true;
    state.privilege_level = PrivilegeLevel::unknown;
    settings.publish_sfc_app_instance(state);

    if (!settings.query_burble_enabled(state.burble_enabled)) {
        settings.write_burble_enabled(true);
        state.burble_enabled = true;
    }

    std::uint32_t persisted_interval = state.autosave_interval_ms;
    if (settings.query_autosave_interval_ms(persisted_interval)) {
        state.autosave_interval_ms = persisted_interval;
    }
    if (state.autosave_interval_ms == 0 ||
        state.autosave_interval_ms > 86400000) {
        state.autosave_interval_ms = 360000;
    }

    std::string privilege_name;
    if (!settings.query_privileges(privilege_name)) {
        privilege_name = "User";
        settings.write_default_privileges(privilege_name);
    }

    if (settings.is_user_privilege(privilege_name)) {
        state.privilege_level = PrivilegeLevel::user;
    }
    if (settings.is_elevated_privilege(privilege_name)) {
        state.privilege_level = PrivilegeLevel::elevated;
    }
    if (privilege_name == "GreenTea") {
        state.privilege_level = PrivilegeLevel::green_tea;
    }
}

} // namespace creatures1::application
