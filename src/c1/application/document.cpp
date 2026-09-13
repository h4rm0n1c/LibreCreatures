#include "document.hpp"

#include <array>
#include <new>

#include "../scripting/classifier_scripts.hpp"

namespace creatures1::application {

namespace {

constexpr std::uint32_t kFavouritePlaceNavigationMode = 1;
constexpr std::uint32_t kWorldUpdateTimerRunning = 1;
constexpr std::uint32_t kWorldUpdateControlState = 9;
constexpr std::uint32_t kMaxWorldUpdateIntervalMs = 300;
constexpr std::size_t kSoundChannelCount = 0x20;
constexpr std::uint32_t kFavouritePlaceMenuIdBase = 0x8053;
constexpr std::uint32_t kMinimumWorldUpdateIntervalMs = 1;
constexpr std::uint32_t kMaximumWorldUpdateIntervalMs = 300;
constexpr std::uint32_t kDefaultMaxNorns = 10;
constexpr std::uint32_t kWorldTickPhaseCount = 16;
constexpr std::uint32_t kCreatureUpdateCohortCount = 5;
constexpr std::uint32_t kAmbientSoundCount = 0x1c;
constexpr std::uint32_t kAmbientSoundCooldownMinimum = 0x32;
constexpr std::uint32_t kAmbientSoundCooldownRange = 0x32;
constexpr std::uint32_t kPeriodicWorldUpdateCount = 0x4b0;

constexpr std::string_view kMuteSettingName = "Mute";
constexpr std::string_view kInformativeMenuSettingName = "InformativeMenu";

struct BuiltinScript {
    std::uint32_t packed_classifier;
    std::string_view text;
};

scripting::ScriptClassifier unpack_classifier(std::uint32_t packed) {
    return scripting::ScriptClassifier{
        static_cast<scripting::ScriptEvent>(packed & 0xffU),
        static_cast<std::uint8_t>((packed >> 8) & 0xffU),
        static_cast<std::uint8_t>((packed >> 16) & 0xffU),
        static_cast<std::uint8_t>((packed >> 24) & 0xffU),
    };
}

// These are the executable's built-in CAOS definitions. Keeping them as
// source data makes the recovered startup policy reviewable; parsing,
// storage, and replacement behavior remain in the scripting host.
constexpr std::array<BuiltinScript, 32> kBuiltinScripts{{
    {0x02010132, "anim [233320]"},
    {0x02010133, "anim [233320]"},
    {0x02010134, "anim [233320]"},
    {0x02010135, "pose 6"},
    {0x02010136, "pose 0"},
    {0x02020001, "pose 1,sndv [Bing]"},
    {0x02020000, "pose 0,sndv [Bing]"},
    {0x04000001, "doif from eq pntr,stm# writ targ 1,else,stm# writ targ 2,endi,doif aslp eq 0,say$ [Ooh!],endi"},
    {0x04000000, "doif from eq pntr,stm# writ targ 3,else,stm# writ targ 4,endi,doif aslp eq 0,say$ [Ouch!],endi"},
    {0x04000010, "impt 0,aim: 0,pose 12,loop,wait 50,stm# writ targ 12,ever"},
    {0x04000020, "impt 0,loop,rndv var0 0 1,doif var0 eq 0,pose 57,rndv var0 20 40,wait var0,else,pose 59,rndv var0 1 10,wait var0,pose 60,rndv var0 1 10,wait var0,endi,stm# writ targ 12,ever"},
    {0x04000011, "impt 3,aim: 0,appr,touc,wait 4,mesg writ _it_ 0,stm# writ targ 13,wait 4,pose 12,wait 20,done"},
    {0x02060011, "impt 4,aim: 0,appr,touc,mesg writ _it_ 4,wait 2,reps 3,pose 73,mesg writ _it_ 0,pose 74,wait 4,repe,pose 74,stm# writ targ 13,wait 20,done"},
    {0x020d0211, "impt 3,aim: 0,appr,touc,mesg writ _it_ 4,wait 4,pose 66,mesg writ _it_ 5,wait 4,pose 12,stm# writ targ 13,wait 20,done"},
    {0x04000012, "impt 3,aim: 1,appr,touc,wait 4,mesg writ _it_ 1,stm# writ targ 14,wait 4,pose 12,wait 20,done"},
    {0x04000013, "impt 3,aim: 2,appr,touc,wait 4,mesg writ _it_ 2,stm# writ targ 15,wait 4,pose 12,wait 20,done"},
    {0x04000014, "impt 2,aim: 0,loop,appr,stim writ targ 0 255 0 0 0 0 0 0 0 0 0 0,impt 1,pose 12,wait 30,stm# writ targ 16,ever"},
    {0x04000015, "doif driv 0 le 0,doif driv 8 gt 0,impt 4,aim: 0,anim [49505152R],stm# writ targ 17,stop,endi,else,impt 4,pose 39,wait 7,doif driv 9 gt 0,anim [53545556R],wait 40,endi,pose 12,endi,stm# writ targ 17,wait 20,done"},
    {0x04000016, "impt 3,aim: 0,appr,touc,wait 4,mesg writ _it_ 4,stm# writ targ 18,wait 4,pose 12,wait 20,done"},
    {0x04000017, "appr,drop,stm# writ targ 19,wait 20,done"},
    {0x04000027, "drop,stm# writ targ 19,wait 20,done"},
    {0x04000018, "impt 2,aim: 0,sayn,pose 12,stm# writ targ 20,wait 20,done"},
    {0x04000028, "impt 3,sayn,pose 34,stm# writ targ 20,wait 40,done"},
    {0x04000029, "loop,doif driv 6 gt 0,impt 3,pose 57,wait 25,stm# writ targ 21,impt 4,aslp 1,pose 58,wait 60,stm# writ targ 22,impt 15,pose 58,wait 255,stm# writ targ 22,impt 5,pose 58,wait 200,stm# writ targ 22,impt 4,aslp 0,pose 58,wait 25,stm# writ targ 22,else,doif driv 5 gt 0,impt 3,pose 58,wait 120,stm# writ targ 21,else,impt 1,pose 57,wait 50,stm# writ targ 21,endi,endi,ever"},
    {0x0400002a, "loop,impt 1,pose 60,anim [61626364R],wait 20,stm# writ targ 23,ever"},
    {0x0400001a, "loop,impt 1,pose 60,anim [61626364R],wait 20,stm# writ targ 23,ever"},
    {0x0400002b, "loop,impt 1,pose 59,anim [61626364R],wait 20,stm# writ targ 23,ever"},
    {0x0400001b, "loop,impt 1,pose 59,anim [61626364R],wait 20,stm# writ targ 23,ever"},
    {0x04000040, "pose 75,wait 8"},
    {0x04000048, "setv norn targ,sys: wtop,pose 77,sndv [dead.wav],wait 50,exec [Funeral Kit.exe] [new],wait 3000,kill ownr"},
    {0x04000032, "anim [232323230],sndv [Tckl]"},
    {0x04000034, "anim [1710],sndv [Spnk]"},
}};

void go_to_favourite_place(DocumentViewportHost& host,
                           const FavouritePlace& place) {
    host.set_viewport_navigation_mode(kFavouritePlaceNavigationMode);
    host.request_viewport_origin(
        static_cast<std::uint32_t>(static_cast<std::uint16_t>(
            place.viewport_origin_x)),
        static_cast<std::uint32_t>(static_cast<std::uint16_t>(
            place.viewport_origin_y)));
}

} // namespace

std::unique_ptr<Document> Document::create(DocumentConstructionHost& host) {
    auto document = std::unique_ptr<Document>(new (std::nothrow) Document());
    if (!document) {
        return nullptr;
    }

    host.initialize_framework_document(*document);
    host.enable_automation(*document);
    host.lock_ole_application();
    host.set_caos_language_version(5);
    document->last_autosave_time_ms = host.current_time_ms();
    document->favourite_place_count = 0;

    if (!host.read_boolean_setting(kMuteSettingName,
                                   document->mute_setting)) {
        document->mute_setting = false;
        host.write_boolean_setting(kMuteSettingName, false);
    }
    if (!host.read_boolean_setting(kInformativeMenuSettingName,
                                   document->informative_menu_setting)) {
        document->informative_menu_setting = false;
        host.write_boolean_setting(kInformativeMenuSettingName, false);
    }
    host.set_active_document(*document);
    return document;
}

void Document::destroy(DocumentDestructionHost& host) {
    host.unlock_ole_application();
    host.destroy_framework_document(*this);
}

void Document::delete_contents(DocumentContentsHost& host) {
    host.persist_and_close_eye_view();

    while (host.non_scenery_object_count() != 0) {
        host.delete_first_non_scenery_object();
    }
    while (host.scenery_count() != 0) {
        host.delete_first_scenery();
    }
    if (host.map_loaded()) {
        host.delete_map();
    }

    host.clear_sprite_file_cache();
    host.clear_charset_glyph_cache();
    while (host.gallery_count() != 0) {
        host.release_first_gallery();
    }
    host.remove_running_macros();
    host.clear_script_definitions();
    host.clear_object_registries();
    host.clear_renderable_objects();
    host.delete_framework_contents(*this);
    host.clear_document_selection();
}

void Document::close(DocumentCloseHost& host) {
    host.kill_world_update_timer();
    host.persist_and_close_eye_view();
    if (host.privilege_level() < 2 && !host.save_for_close(*this)) {
        host.report_save_failure();
    }
    host.close_framework_document(*this);
}

bool Document::save(DocumentSaveHost& host, std::string_view path) {
    host.clear_sprite_file_cache();

    for (std::size_t remaining = host.world_object_count(); remaining != 0;
         --remaining) {
        const std::size_t index = remaining - 1;
        if (!host.world_object_can_be_destroyed(index)) {
            continue;
        }
        if (host.world_object_is_generated(index)) {
            host.initialize_generated_object_runtime(index);
            host.remove_generated_image(host.generated_image_filename(index));
        }
        host.destroy_world_object(index);
        host.remove_world_object(index);
    }

    if (host.privilege_level() > 1) {
        host.reset_world_tick_count();
        host.clear_favourite_place_names(*this);
    }
    host.promote_temporary_world_backup();
    host.save_framework_document(*this, path);

    // The native SFCDoc path completes through the MFC save hook and returns
    // success from this wrapper. Failure reporting belongs to that adapter.
    return true;
}

bool Document::open_document(DocumentOpenHost& host, std::string_view path) {
    host.kill_world_update_timer();
    host.set_full_redraw_pending(false);

    if (!host.open_framework_document(*this, path) ||
        !host.initialize_game_palette()) {
        return false;
    }

    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        host.validate_creature_body_sprites(index);
    }

    host.refresh_temporary_world_backup();
    host.load_charset_data();
    host.refresh_event_bar_object_display_panes();
    host.update_event_bar_status_panes();

    std::uint32_t interval_ms = host.world_update_timer_interval_ms();
    if (interval_ms == 0) {
        interval_ms = kMinimumWorldUpdateIntervalMs;
    } else if (interval_ms > kMaximumWorldUpdateIntervalMs) {
        interval_ms = kMaximumWorldUpdateIntervalMs;
    }
    host.set_world_update_timer_interval_ms(interval_ms);

    if (host.has_main_frame()) {
        host.arm_world_update_timer(interval_ms);
    }
    host.set_full_redraw_pending(true);
    host.update_main_window_title_for_selected_creature();

    if (host.max_norns_setting() < 1) {
        std::uint32_t max_norns = 0;
        if (!host.read_max_norns_setting(max_norns)) {
            max_norns = kDefaultMaxNorns;
            host.write_max_norns_setting(max_norns);
        }
        host.set_max_norns_setting(max_norns);
    }
    return true;
}

bool Document::on_new_document(DocumentNewWorldHost& host) {
    host.kill_world_update_timer();
    if (!host.initialize_framework_new_document(*this) ||
        !host.initialize_game_palette()) {
        return false;
    }

    host.seed_random_from_current_time();
    host.load_charset_data();
    host.create_legacy_world();

    const PointerToolInitialization pointer_tool{
        0x74737973, 0, 9, false, 0xa0, 100, 9999, 0,
        0x02010100, 0xff, 0, 0, 0, 0x18, 0x1f};
    host.create_pointer_tool(pointer_tool);
    for (const BuiltinScript& script : kBuiltinScripts) {
        host.install_builtin_script(unpack_classifier(script.packed_classifier),
                                    script.text);
    }

    std::uint32_t interval_ms = host.world_update_timer_interval_ms();
    if (interval_ms == 0) {
        interval_ms = kMinimumWorldUpdateIntervalMs;
    } else if (interval_ms > kMaximumWorldUpdateIntervalMs) {
        interval_ms = kMaximumWorldUpdateIntervalMs;
    }
    host.set_world_update_timer_interval_ms(interval_ms);
    if (host.has_main_frame()) {
        host.arm_world_update_timer(interval_ms);
    }
    return true;
}

void Document::update_world(DocumentWorldUpdateHost& host) {
    if (host.world_update_in_progress()) {
        return;
    }
    host.set_world_update_in_progress(true);

    if (host.has_edit_object()) {
        host.place_edit_object_at_pointer();
        if (host.pending_right_button()) {
            host.clear_pending_input();
            host.finalize_edit_object();
            host.clear_edit_object();
        }
    }

    // Native enables deferred dirty-rectangle rendering here, immediately
    // before the object registry walk, and drains it after the tick-phase
    // dispatch below.
    host.begin_deferred_dirty_rectangles();

    // Non-scenery objects can add or remove registry entries while ticking;
    // re-read the count after each dispatch just as the native index walk did.
    for (std::size_t index = 0;
         index < host.non_scenery_object_count(); ++index) {
        if (host.non_scenery_object_tick_enabled(index)) {
            host.tick_non_scenery_object(index);
        }
    }

    char input_character = '\0';
    if (host.pop_text_input_character(input_character)) {
        if (input_character == '\r') {
            host.commit_text_input();
        } else if (input_character == '\b') {
            if (host.text_input_length() != 0) {
                host.erase_last_text_input_character();
            }
            host.update_text_input_target();
        } else if (static_cast<unsigned char>(input_character - 0x20) <
                       0x5b &&
                   host.text_input_length() < host.text_input_max_length() &&
                   host.text_input_character_allowed(
                       input_character,
                       host.text_input_allowed_character_flags())) {
            host.append_text_input_character(input_character);
            host.update_text_input_target();
        } else {
            host.update_text_input_target();
        }
    }

    host.update_selected_creature_follow_viewport();

    for (std::size_t index = 0; index < host.running_macro_count(); ++index) {
        // ExecuteInterpreter remains a separate held semantic row; this
        // adapter is its typed world-scheduler boundary.
        host.execute_running_macro(index);
    }

    const std::uint32_t cohort = host.creature_update_cohort();
    // Creature updates can change the registry.  The native index walk
    // re-read its current size after each update, so keep the bound live
    // instead of freezing the count at the start of the cohort.
    for (std::size_t index = cohort; index < host.creature_count();
         index += 4) {
        if (!host.creature_tick_enabled(index) ||
            host.creature_is_dreaming(index)) {
            continue;
        }
        if (host.creature_is_alive(index)) {
            if (host.selected_action_is_in_range(index)) {
                host.boost_selected_action_activation(index);
            }
            host.update_creature_brain(index);
        }
        host.update_creature_biochemistry(index);
        if (host.creature_is_alive(index)) {
            host.update_creature_action_selection(index);
        }
        host.increment_creature_biochemistry_tick(index);
    }

    host.set_creature_update_cohort(
        cohort + 1 < kCreatureUpdateCohortCount
            ? cohort + 1
            : 0);

    for (std::size_t index = 0; index < host.creature_count(); ++index) {
        if (host.creature_tick_enabled(index)) {
            host.process_creature_dreaming(index);
        }
    }

    if (host.sound_manager_available()) {
        host.update_sound_system();
        const std::uint32_t cooldown = host.ambient_sound_cooldown_ticks();
        if (cooldown == 0) {
            if (!mute_setting && !host.sound_muted()) {
                const std::uint32_t random_index =
                    host.random_ambient_sound_index() % kAmbientSoundCount + 1;
                std::uint32_t sound_id =
                    host.ambient_sound_descriptor_override();
                if (sound_id == 0) {
                    sound_id =
                        (((random_index % 10) << 8) | (random_index / 10)) <<
                            16;
                    sound_id += 0x3030554d;
                }
                if (host.sound_mixer_ready() &&
                    host.load_ambient_sound(sound_id)) {
                    host.start_ambient_sound(sound_id);
                }
                host.set_ambient_sound_cooldown_ticks(
                    host.random_ambient_sound_index() %
                        kAmbientSoundCooldownRange +
                    kAmbientSoundCooldownMinimum);
            } else {
                host.set_ambient_sound_cooldown_ticks(1);
            }
        } else {
            host.set_ambient_sound_cooldown_ticks(cooldown - 1);
        }
    }

    host.update_keyboard_scroll();
    if (!host.advance_smooth_scroll()) {
        if (host.manual_viewport_navigation()) {
            if (!host.selected_creature_in_safe_area()) {
                host.reset_manual_navigation_safe_frame_count();
            } else {
                const std::uint32_t safe_frames =
                    host.manual_navigation_safe_frame_count() + 1;
                host.set_manual_navigation_safe_frame_count(safe_frames);
                if (safe_frames == 0x14) {
                    host.reset_world_scrollbars();
                }
            }
        } else {
            host.follow_selected_creature_viewport();
        }
    }

    std::uint32_t phase = host.world_tick_phase() + 1;
    if (phase >= kWorldTickPhaseCount) {
        phase = 0;
    }
    host.set_world_tick_phase(phase);
    host.dispatch_world_tick_phase(phase);
    host.flush_deferred_dirty_rectangles();

    const std::uint32_t world_tick_count = host.world_tick_count();
    if (world_tick_count % kPeriodicWorldUpdateCount ==
        kPeriodicWorldUpdateCount - 1) {
        score.population_time_accumulator +=
            static_cast<std::uint32_t>(host.selected_creature_count());
        host.publish_periodic_score_to_embedded_control(score);
    }
    if (world_tick_count % 100 == 99) {
        host.update_event_bar_status_panes();
    }

    const std::uint32_t now = host.current_time_ms();
    const std::uint32_t elapsed = now - last_autosave_time_ms;
    if ((elapsed >= host.autosave_interval_ms() ||
         now < last_autosave_time_ms) && host.privilege_level() < 2) {
        const std::string old_title = host.capture_main_window_title();
        host.set_temporary_main_window_title();
        host.set_application_busy(true);
        host.promote_temporary_world_backup();
        if (host.save_for_autosave(*this)) {
            host.refresh_temporary_world_backup();
        }
        host.set_application_busy(false);
        host.restore_main_window_title(old_title);
        host.broadcast_embedded_control_state(kWorldUpdateControlState);
        last_autosave_time_ms = host.current_time_ms();
    }

    host.set_world_tick_count(world_tick_count + 1);
    host.set_world_update_in_progress(false);
}

void Document::serialize(DocumentSerializationHost& host) {
    if (!host.loading()) {
        host.serialize_map_data();

        host.write_non_scenery_object_count(
            static_cast<std::int32_t>(host.non_scenery_object_count()));
        for (std::size_t index = 0; index < host.non_scenery_object_count();
             ++index) {
            host.serialize_non_scenery_object(index);
        }

        host.write_scenery_object_count(
            static_cast<std::int32_t>(host.scenery_object_count()));
        for (std::size_t index = 0; index < host.scenery_object_count();
             ++index) {
            host.serialize_scenery_object(index);
        }

        host.serialize_classifier_scripts();
        host.write_viewport_origin(host.viewport_origin_x(),
                                   host.viewport_origin_y());
        host.serialize_selected_creature();
        for (FavouritePlace& place : favourite_places) {
            host.serialize_favourite_place(place);
        }
        host.serialize_main_toolbar();

        host.write_running_macro_count(
            static_cast<std::int32_t>(host.running_macro_count()));
        for (std::size_t index = 0; index < host.running_macro_count(); ++index) {
            host.serialize_running_macro(index);
        }

        host.write_world_object_count(
            static_cast<std::int32_t>(host.world_object_count()));
        for (std::size_t index = 0; index < host.world_object_count(); ++index) {
            host.serialize_world_object(index);
        }
        host.serialize_event_bar();
        host.serialize_score(score);
        host.write_world_tick_count(host.world_tick_count());

        serialized_document_state_word_count = static_cast<std::uint32_t>(
            serialized_document_state_words.size());
        host.write_document_state_word_count(
            serialized_document_state_word_count);
        for (std::uint32_t word : serialized_document_state_words) {
            host.write_document_state_word(word);
        }
        host.rebuild_creature_selection_menu();
        return;
    }

    host.serialize_map_data();
    const std::int32_t non_scenery_count =
        host.read_non_scenery_object_count();
    for (std::int32_t index = 0; index < non_scenery_count; ++index) {
        host.serialize_non_scenery_object(static_cast<std::size_t>(index));
    }

    const std::int32_t scenery_count = host.read_scenery_object_count();
    for (std::int32_t index = 0; index < scenery_count; ++index) {
        host.serialize_scenery_object(static_cast<std::size_t>(index));
    }

    host.serialize_classifier_scripts();
    const std::int32_t viewport_x = host.read_viewport_origin_x();
    const std::int32_t viewport_y = host.read_viewport_origin_y();
    host.serialize_selected_creature();
    if (host.viewport_navigation_requires_manual_reset()) {
        host.reset_viewport_navigation_after_load();
    }
    host.request_viewport_origin(viewport_x, viewport_y);

    favourite_place_count = 0;
    for (std::size_t index = 0; index < favourite_places.size(); ++index) {
        FavouritePlace& place = favourite_places[index];
        host.serialize_favourite_place(place);
        if (!place.name.empty()) {
            favourite_place_count = index + 1;
        }
    }
    host.load_default_first_favourite_place_name(favourite_places.front());
    host.serialize_main_toolbar();

    const std::int32_t running_macro_count = host.read_running_macro_count();
    for (std::int32_t index = 0; index < running_macro_count; ++index) {
        host.serialize_running_macro(static_cast<std::size_t>(index));
    }

    const std::int32_t world_object_count = host.read_world_object_count();
    for (std::int32_t index = 0; index < world_object_count; ++index) {
        const std::size_t object_index = static_cast<std::size_t>(index);
        host.serialize_world_object(object_index);
        host.disable_loaded_world_object_ticks(object_index);
    }
    host.serialize_event_bar();

    for (std::size_t index = 0; index < host.non_scenery_object_count(); ++index) {
        host.dispatch_loaded_non_scenery_bounds_update(index);
        host.rebuild_missing_unbounded_renderable_entry(index);
    }
    host.serialize_score(score);
    host.set_world_tick_count(host.read_world_tick_count());

    serialized_document_state_word_count =
        host.read_document_state_word_count();
    serialized_document_state_words.clear();
    serialized_document_state_words.reserve(
        serialized_document_state_word_count);
    for (std::uint32_t index = 0;
         index < serialized_document_state_word_count; ++index) {
        serialized_document_state_words.push_back(
            host.read_document_state_word());
    }

    if (host.has_unbounded_stage_one_object()) {
        return;
    }
    host.queue_pointer_tool_event_four();
}

void Document::toggle_informative_selection_menu(
    DocumentInformativeSelectionHost& host) {
    informative_menu_setting = !informative_menu_setting;
    host.persist_informative_menu_setting(informative_menu_setting);
    host.rebuild_informative_selection_menu(informative_menu_setting);
}

DocumentAdapter* Document::get_document_adapter(DocumentAdapterHost& host) {
    if (document_adapter != nullptr) {
        host.add_reference(*document_adapter);
        return document_adapter;
    }
    document_adapter = host.create_document_adapter(*this);
    return document_adapter;
}

const DocumentRuntimeClass& Document::runtime_class() {
    static const DocumentRuntimeClass descriptor{"SFCDoc"};
    return descriptor;
}

void Document::update_informative_selection_menu(
    DocumentCommandUpdateHost& command_ui, bool world_update_running) const {
    command_ui.set_checked(informative_menu_setting);
    command_ui.set_enabled(world_update_running);
}

void Document::update_mute_menu(DocumentCommandUpdateHost& command_ui,
                                bool world_update_running) const {
    command_ui.set_checked(mute_setting);
    command_ui.set_enabled(world_update_running);
}

void Document::add_favourite_place_from_current_viewport(
    DocumentFavouritePlaceHost& host) {
    std::string name;
    if (!host.prompt_for_favourite_place_name(name) ||
        favourite_place_count == kFavouritePlaceCapacity) {
        return;
    }

    FavouritePlace& place = favourite_places[favourite_place_count];
    place.name = name;
    place.viewport_origin_x = host.current_viewport_origin_x();
    place.viewport_origin_y = host.current_viewport_origin_y();

    const std::uint32_t command_id =
        kFavouritePlaceMenuIdBase +
        static_cast<std::uint32_t>(favourite_place_count);
    if (!host.append_favourite_place_menu(command_id, place.name)) {
        place = {};
        return;
    }
    ++favourite_place_count;
}

void go_to_first_favourite_place(DocumentViewportHost& host,
                                 const FavouritePlace& place) {
    go_to_favourite_place(host, place);
}

void go_to_second_favourite_place(DocumentViewportHost& host,
                                  const FavouritePlace& place) {
    go_to_favourite_place(host, place);
}

void go_to_third_favourite_place(DocumentViewportHost& host,
                                 const FavouritePlace& place) {
    go_to_favourite_place(host, place);
}

void go_to_fourth_favourite_place(DocumentViewportHost& host,
                                  const FavouritePlace& place) {
    go_to_favourite_place(host, place);
}

void go_to_fifth_favourite_place(DocumentViewportHost& host,
                                 const FavouritePlace& place) {
    go_to_favourite_place(host, place);
}

void go_to_sixth_favourite_place(DocumentViewportHost& host,
                                 const FavouritePlace& place) {
    go_to_favourite_place(host, place);
}

void service_world_update_timer(DocumentTimerHost& host) {
    if (host.world_update_timer_is_running()) {
        host.broadcast_embedded_control_state(kWorldUpdateControlState);
        host.kill_world_update_timer();

        std::size_t object_index = 0;
        while (object_index < host.non_scenery_object_count()) {
            const std::int32_t channel =
                host.object_sound_channel(object_index);
            if (channel >= 0) {
                if (host.debug_logging_available()) {
                    host.log_continuous_sound_stop(channel);
                }
                if (!host.sound_mixer_is_suspended() &&
                    static_cast<std::size_t>(channel) < kSoundChannelCount) {
                    host.clear_sound_channel_active(
                        static_cast<std::size_t>(channel));
                    host.stop_sound_channel(
                        static_cast<std::size_t>(channel));
                }
                host.release_object_sound_channel(object_index);
            }
            ++object_index;
        }
        host.stop_all_sounds();
        host.mark_world_update_timer_paused();
    } else if (host.privilege_level() > 1) {
        host.update_world();
    }

    host.invalidate_main_toolbar();
}

void arm_world_update_timer(DocumentTimerHost& host) {
    // CApplication::ArmWorldUpdateTimer @ 0x004337e0.  Resume is a no-op when
    // the world is already running, and the interval survives the pause -- it
    // is only clamped, never cleared, so the world comes back at the speed it
    // was stopped at.
    if (!host.world_update_timer_is_running()) {
        host.broadcast_embedded_control_state(kWorldUpdateControlState);

        std::uint32_t interval_ms = host.world_update_timer_interval_ms();
        if (interval_ms == 0) {
            interval_ms = 1;
        } else if (interval_ms > kMaxWorldUpdateIntervalMs) {
            interval_ms = kMaxWorldUpdateIntervalMs;
        }
        host.set_world_update_timer_interval_ms(interval_ms);
        host.arm_world_update_timer(interval_ms);
        host.mark_world_update_timer_running();
    }

    host.invalidate_main_toolbar();
}

} // namespace creatures1::application
