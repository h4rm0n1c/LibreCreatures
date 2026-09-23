#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <memory>
#include <vector>

#include "../scripting/classifier_scripts.hpp"
#include "../world/places.hpp"

namespace creatures1::application {

class Document;

struct DocumentScore {
    std::uint32_t hatchery_eggs_used = 0;
    std::uint32_t natural_eggs_laid = 0;
    std::uint32_t dead_norns = 0;
    std::uint32_t living_norns = 0;
    std::uint32_t population_time_accumulator = 0;
};

// Framework construction, registry settings, and process-wide application
// state are adapters. The document keeps the recovered game-owned state and
// construction order in one place.
class DocumentConstructionHost {
public:
    virtual ~DocumentConstructionHost() = default;
    virtual void initialize_framework_document(Document& document) = 0;
    virtual void enable_automation(Document& document) = 0;
    virtual void lock_ole_application() = 0;
    virtual std::uint32_t current_time_ms() const = 0;
    virtual bool read_boolean_setting(std::string_view name,
                                      bool& value) const = 0;
    virtual void write_boolean_setting(std::string_view name, bool value) = 0;
    virtual void set_active_document(Document& document) = 0;
    virtual void set_caos_language_version(std::uint32_t version) = 0;
};

class DocumentDestructionHost {
public:
    virtual ~DocumentDestructionHost() = default;
    virtual void unlock_ole_application() = 0;
    virtual void destroy_framework_document(Document& document) = 0;
};

class DocumentContentsHost {
public:
    virtual ~DocumentContentsHost() = default;
    virtual void persist_and_close_eye_view() = 0;
    virtual std::size_t non_scenery_object_count() const = 0;
    virtual void delete_first_non_scenery_object() = 0;
    virtual std::size_t scenery_count() const = 0;
    virtual void delete_first_scenery() = 0;
    virtual bool map_loaded() const = 0;
    virtual void delete_map() = 0;
    virtual void clear_sprite_file_cache() = 0;
    virtual void clear_charset_glyph_cache() = 0;
    virtual std::size_t gallery_count() const = 0;
    virtual void release_first_gallery() = 0;
    virtual void remove_running_macros() = 0;
    virtual void clear_script_definitions() = 0;
    virtual void clear_object_registries() = 0;
    virtual void clear_renderable_objects() = 0;
    virtual void clear_document_selection() = 0;
    virtual void delete_framework_contents(Document& document) = 0;
};

class DocumentCloseHost {
public:
    virtual ~DocumentCloseHost() = default;
    virtual void kill_world_update_timer() = 0;
    virtual void persist_and_close_eye_view() = 0;
    virtual std::uint32_t privilege_level() const = 0;
    virtual bool save_for_close(Document& document) = 0;
    virtual void report_save_failure() = 0;
    virtual void close_framework_document(Document& document) = 0;
};

class DocumentSaveHost {
public:
    virtual ~DocumentSaveHost() = default;
    virtual void clear_sprite_file_cache() = 0;
    virtual std::uint32_t privilege_level() const = 0;
    virtual std::size_t world_object_count() const = 0;
    virtual bool world_object_can_be_destroyed(std::size_t index) const = 0;
    virtual bool world_object_is_generated(std::size_t index) const = 0;
    virtual void initialize_generated_object_runtime(std::size_t index) = 0;
    virtual std::string generated_image_filename(std::size_t index) const = 0;
    virtual void remove_generated_image(std::string_view filename) = 0;
    virtual void destroy_world_object(std::size_t index) = 0;
    virtual void remove_world_object(std::size_t index) = 0;
    virtual void reset_world_tick_count() = 0;
    virtual void clear_favourite_place_names(Document& document) = 0;
    virtual void promote_temporary_world_backup() = 0;
    virtual void save_framework_document(Document& document,
                                         std::string_view path) = 0;
};

// Opening a world is document policy surrounded by framework, renderer,
// registry, and UI adapters.  The host owns those mechanisms; the document
// owns the recovered ordering and success/failure boundary.
class DocumentOpenHost {
public:
    virtual ~DocumentOpenHost() = default;
    virtual void kill_world_update_timer() = 0;
    virtual void set_full_redraw_pending(bool pending) = 0;
    virtual bool open_framework_document(Document& document,
                                         std::string_view path) = 0;
    virtual bool initialize_game_palette() = 0;
    virtual std::size_t creature_count() const = 0;
    virtual void validate_creature_body_sprites(std::size_t index) = 0;
    virtual void refresh_temporary_world_backup() = 0;
    virtual void load_charset_data() = 0;
    virtual void refresh_event_bar_object_display_panes() = 0;
    virtual void update_event_bar_status_panes() = 0;
    virtual std::uint32_t world_update_timer_interval_ms() const = 0;
    virtual void set_world_update_timer_interval_ms(
        std::uint32_t interval_ms) = 0;
    virtual bool has_main_frame() const = 0;
    virtual void arm_world_update_timer(std::uint32_t interval_ms) = 0;
    virtual void update_main_window_title_for_selected_creature() = 0;
    virtual std::uint32_t max_norns_setting() const = 0;
    virtual bool read_max_norns_setting(std::uint32_t& value) = 0;
    virtual void set_max_norns_setting(std::uint32_t value) = 0;
    virtual void write_max_norns_setting(std::uint32_t value) = 0;
};

struct PointerToolInitialization {
    std::uint32_t sprite_file_id = 0;
    std::int32_t header_record_index = 0;
    std::uint32_t image_count = 0;
    bool cache_protected = false;
    std::int32_t initial_x = 0;
    std::int32_t initial_y = 0;
    std::int32_t render_plane = 0;
    std::uint8_t bounds_flags = 0;
    std::uint32_t classifier = 0;
    std::uint8_t click_event_selector = 0xff;
    std::uint32_t reserved_word_0 = 0;
    std::uint32_t reserved_word_1 = 0;
    std::uint8_t interaction_event_flags = 0;
    std::uint32_t text_input_max_length = 0;
    std::uint32_t text_input_allowed_characters = 0;
};

// OnNewDocument is game-world initialization after the framework document
// already exists.  The adapter owns MFC/CRT allocation, the global world
// registries, resource loading, and HWND timer calls; this interface keeps
// the recovered game ordering and literal initialization contract visible.
class DocumentNewWorldHost {
public:
    virtual ~DocumentNewWorldHost() = default;
    virtual void kill_world_update_timer() = 0;
    virtual bool initialize_framework_new_document(Document& document) = 0;
    virtual bool initialize_game_palette() = 0;
    virtual void seed_random_from_current_time() = 0;
    virtual void load_charset_data() = 0;
    virtual void create_legacy_world() = 0;
    virtual void create_pointer_tool(const PointerToolInitialization& init) = 0;
    virtual void install_builtin_script(scripting::ScriptClassifier classifier,
                                        std::string_view text) = 0;
    virtual std::uint32_t world_update_timer_interval_ms() const = 0;
    virtual void set_world_update_timer_interval_ms(
        std::uint32_t interval_ms) = 0;
    virtual bool has_main_frame() const = 0;
    virtual void arm_world_update_timer(std::uint32_t interval_ms) = 0;
};

// Serialization is a single recovered record sequence.  Archive bytes,
// dynamic-object identity, collection allocation, renderer/UI state, and
// error policy remain in this typed host; Document owns branch order and the
// post-load invariants.  The count methods intentionally distinguish reads
// from writes because the native archive stores each collection count.
class DocumentSerializationHost {
public:
    virtual ~DocumentSerializationHost() = default;
    virtual bool loading() const = 0;
    virtual void serialize_map_data() = 0;
    virtual std::size_t non_scenery_object_count() const = 0;
    virtual std::int32_t read_non_scenery_object_count() = 0;
    virtual void write_non_scenery_object_count(std::int32_t count) = 0;
    virtual void serialize_non_scenery_object(std::size_t index) = 0;
    virtual std::size_t scenery_object_count() const = 0;
    virtual std::int32_t read_scenery_object_count() = 0;
    virtual void write_scenery_object_count(std::int32_t count) = 0;
    virtual void serialize_scenery_object(std::size_t index) = 0;
    virtual void serialize_classifier_scripts() = 0;
    virtual std::int32_t read_viewport_origin_x() = 0;
    virtual std::int32_t read_viewport_origin_y() = 0;
    virtual std::int32_t viewport_origin_x() const = 0;
    virtual std::int32_t viewport_origin_y() const = 0;
    virtual void write_viewport_origin(std::int32_t origin_x,
                                       std::int32_t origin_y) = 0;
    virtual void serialize_selected_creature() = 0;
    virtual void serialize_favourite_place(FavouritePlace& place) = 0;
    virtual void serialize_main_toolbar() = 0;
    virtual std::size_t running_macro_count() const = 0;
    virtual std::int32_t read_running_macro_count() = 0;
    virtual void write_running_macro_count(std::int32_t count) = 0;
    virtual void serialize_running_macro(std::size_t index) = 0;
    virtual std::size_t world_object_count() const = 0;
    virtual std::int32_t read_world_object_count() = 0;
    virtual void write_world_object_count(std::int32_t count) = 0;
    virtual void serialize_world_object(std::size_t index) = 0;
    virtual void disable_loaded_world_object_ticks(std::size_t index) = 0;
    virtual void serialize_event_bar() = 0;
    virtual void serialize_score(DocumentScore& score) = 0;
    virtual std::uint32_t world_tick_count() const = 0;
    virtual void write_world_tick_count(std::uint32_t count) = 0;
    virtual std::uint32_t read_world_tick_count() = 0;
    virtual void set_world_tick_count(std::uint32_t count) = 0;
    virtual std::uint32_t read_document_state_word_count() = 0;
    virtual void write_document_state_word_count(std::uint32_t count) = 0;
    virtual std::uint32_t read_document_state_word() = 0;
    virtual void write_document_state_word(std::uint32_t word) = 0;
    virtual bool viewport_navigation_requires_manual_reset() const = 0;
    virtual void reset_viewport_navigation_after_load() = 0;
    virtual void request_viewport_origin(std::int32_t origin_x,
                                         std::int32_t origin_y) = 0;
    virtual void load_default_first_favourite_place_name(
        FavouritePlace& place) = 0;
    virtual void dispatch_loaded_non_scenery_bounds_update(
        std::size_t index) = 0;
    virtual void rebuild_missing_unbounded_renderable_entry(
        std::size_t index) = 0;
    virtual bool has_unbounded_stage_one_object() const = 0;
    virtual void queue_pointer_tool_event_four() = 0;
    virtual void rebuild_creature_selection_menu() = 0;
};

class DocumentInformativeSelectionHost {
public:
    virtual ~DocumentInformativeSelectionHost() = default;
    virtual void persist_informative_menu_setting(bool enabled) = 0;
    virtual void rebuild_informative_selection_menu(bool enabled) = 0;
};

// UpdateWorld is the document-owned world-tick coordinator.  The native
// method interleaves game policy with registry mutation, the CAOS interpreter,
// renderer/DC work, WinMM, MFC automation, and the autosave window.  Keep that
// ordering here while making each non-document mechanism an explicit adapter;
// none of those framework objects are reconstructed as C1 game state.
class DocumentWorldUpdateHost {
public:
    virtual ~DocumentWorldUpdateHost() = default;

    virtual bool world_update_in_progress() const = 0;
    virtual void set_world_update_in_progress(bool in_progress) = 0;

    virtual bool has_edit_object() const = 0;
    virtual void place_edit_object_at_pointer() = 0;
    virtual bool pending_right_button() const = 0;
    virtual void clear_pending_input() = 0;
    virtual void finalize_edit_object() = 0;
    virtual void clear_edit_object() = 0;

    virtual std::size_t non_scenery_object_count() const = 0;
    virtual bool non_scenery_object_tick_enabled(std::size_t index) const = 0;
    virtual void tick_non_scenery_object(std::size_t index) = 0;

    virtual bool pop_text_input_character(char& character) = 0;
    virtual std::size_t text_input_length() const = 0;
    virtual std::size_t text_input_max_length() const = 0;
    virtual std::uint32_t text_input_allowed_character_flags() const = 0;
    virtual bool text_input_character_allowed(
        char character, std::uint32_t allowed_flags) const = 0;
    virtual void commit_text_input() = 0;
    virtual void erase_last_text_input_character() = 0;
    virtual void append_text_input_character(char character) = 0;
    virtual void update_text_input_target() = 0;

    virtual void update_selected_creature_follow_viewport() = 0;
    virtual std::size_t running_macro_count() const = 0;
    virtual void execute_running_macro(std::size_t index) = 0;

    virtual std::size_t creature_count() const = 0;
    virtual std::uint32_t creature_update_cohort() const = 0;
    virtual void set_creature_update_cohort(std::uint32_t cohort) = 0;
    virtual bool creature_tick_enabled(std::size_t index) const = 0;
    virtual bool creature_is_dreaming(std::size_t index) const = 0;
    virtual bool creature_is_alive(std::size_t index) const = 0;
    virtual bool selected_action_is_in_range(std::size_t index) const = 0;
    virtual void boost_selected_action_activation(std::size_t index) = 0;
    virtual void update_creature_brain(std::size_t index) = 0;
    virtual void update_creature_biochemistry(std::size_t index) = 0;
    virtual void update_creature_action_selection(std::size_t index) = 0;
    virtual void increment_creature_biochemistry_tick(std::size_t index) = 0;
    virtual void process_creature_dreaming(std::size_t index) = 0;

    virtual bool sound_manager_available() const = 0;
    virtual void update_sound_system() = 0;
    virtual std::uint32_t ambient_sound_cooldown_ticks() const = 0;
    virtual void set_ambient_sound_cooldown_ticks(std::uint32_t ticks) = 0;
    virtual bool sound_muted() const = 0;
    virtual std::uint32_t random_ambient_sound_index() = 0;
    virtual std::uint32_t ambient_sound_descriptor_override() const = 0;
    virtual bool sound_mixer_ready() const = 0;
    virtual bool load_ambient_sound(std::uint32_t sound_id) = 0;
    virtual void start_ambient_sound(std::uint32_t sound_id) = 0;

    virtual void update_keyboard_scroll() = 0;
    virtual bool advance_smooth_scroll() = 0;
    virtual bool manual_viewport_navigation() const = 0;
    virtual bool follows_selected_creature_viewport() const = 0;
    virtual bool selected_creature_in_safe_area() const = 0;
    virtual void reset_manual_navigation_safe_frame_count() = 0;
    virtual std::uint32_t manual_navigation_safe_frame_count() const = 0;
    virtual void set_manual_navigation_safe_frame_count(
        std::uint32_t frame_count) = 0;
    virtual void reset_world_scrollbars() = 0;
    virtual void resume_following_selected_creature() = 0;
    virtual void follow_selected_creature_viewport() = 0;
    virtual void invalidate_main_toolbar() = 0;

    virtual std::uint32_t world_tick_phase() const = 0;
    virtual void set_world_tick_phase(std::uint32_t phase) = 0;
    virtual void dispatch_world_tick_phase(std::uint32_t phase) = 0;
    // Paired with flush_deferred_dirty_rectangles: the world update batches
    // its dirty rectangles instead of blitting each one as it is produced.
    virtual void begin_deferred_dirty_rectangles() = 0;
    virtual void flush_deferred_dirty_rectangles() = 0;
    virtual std::size_t selected_creature_count() const = 0;
    virtual std::uint32_t world_tick_count() const = 0;
    virtual void set_world_tick_count(std::uint32_t count) = 0;
    virtual void publish_periodic_score_to_embedded_control(
        const DocumentScore& score) = 0;
    virtual void update_event_bar_status_panes() = 0;

    virtual std::uint32_t current_time_ms() const = 0;
    virtual std::uint32_t autosave_interval_ms() const = 0;
    virtual std::uint32_t privilege_level() const = 0;
    virtual void broadcast_embedded_control_state(std::uint32_t state) = 0;
    virtual std::string capture_main_window_title() const = 0;
    virtual void set_temporary_main_window_title() = 0;
    virtual void set_application_busy(bool busy) = 0;
    virtual void promote_temporary_world_backup() = 0;
    virtual bool save_for_autosave(Document& document) = 0;
    virtual void refresh_temporary_world_backup() = 0;
    virtual void restore_main_window_title(std::string_view title) = 0;
};

class DocumentAdapter {
public:
    virtual ~DocumentAdapter() = default;
};

struct DocumentRuntimeClass {
    std::string_view name;
};

class DocumentCommandUpdateHost {
public:
    virtual ~DocumentCommandUpdateHost() = default;
    virtual void set_checked(bool checked) = 0;
    virtual void set_enabled(bool enabled) = 0;
};

class DocumentFavouritePlaceHost {
public:
    virtual ~DocumentFavouritePlaceHost() = default;
    virtual bool prompt_for_favourite_place_name(std::string& out_name) = 0;
    virtual std::int16_t current_viewport_origin_x() const = 0;
    virtual std::int16_t current_viewport_origin_y() const = 0;
    virtual bool append_favourite_place_menu(std::uint32_t command_id,
                                              const std::string& name) = 0;
};

// The native SFCDoc owns these six records and the two menu settings. MFC
// command objects, the place dialog, and the camera submenu are host policy.
class Document {
public:
    static constexpr std::size_t kFavouritePlaceCapacity = 6;

    static std::unique_ptr<Document> create(DocumentConstructionHost& host);
    void destroy(DocumentDestructionHost& host);
    void delete_contents(DocumentContentsHost& host);
    void close(DocumentCloseHost& host);
    bool save(DocumentSaveHost& host, std::string_view path);
    bool open_document(DocumentOpenHost& host, std::string_view path);
    bool on_new_document(DocumentNewWorldHost& host);
    void serialize(DocumentSerializationHost& host);
    void update_world(DocumentWorldUpdateHost& host);
    void toggle_informative_selection_menu(
        DocumentInformativeSelectionHost& host);
    static const DocumentRuntimeClass& runtime_class();

    void update_informative_selection_menu(
        DocumentCommandUpdateHost& command_ui, bool world_update_running) const;
    void update_mute_menu(DocumentCommandUpdateHost& command_ui,
                          bool world_update_running) const;
    void add_favourite_place_from_current_viewport(
        DocumentFavouritePlaceHost& host);

    bool mute_setting = false;
    bool informative_menu_setting = false;
    std::array<FavouritePlace, kFavouritePlaceCapacity> favourite_places{};
    std::size_t favourite_place_count = 0;
    DocumentScore score{};
    std::uint32_t last_autosave_time_ms = 0;
    // A failed autosave must not hammer the world file on every timer pass.
    // Manual save/close remains available; reopening the world re-enables
    // autosave after the underlying file problem has been corrected.
    bool autosave_blocked = false;
    std::uint32_t serialized_document_state_word_count = 0;
    std::vector<std::uint32_t> serialized_document_state_words;
    DocumentAdapter* document_adapter = nullptr;
};

// Viewport state and the world-renderer object are owned by the UI/platform
// layer. The document supplies only the recovered favourite-place policy.
class DocumentViewportHost {
public:
    virtual ~DocumentViewportHost() = default;
    virtual void set_viewport_navigation_mode(std::uint32_t mode) = 0;
    virtual void request_viewport_origin(std::uint32_t origin_x,
                                         std::uint32_t origin_y) = 0;
};

void go_to_first_favourite_place(DocumentViewportHost& host,
                                 const FavouritePlace& place);
void go_to_second_favourite_place(DocumentViewportHost& host,
                                  const FavouritePlace& place);
void go_to_third_favourite_place(DocumentViewportHost& host,
                                 const FavouritePlace& place);
void go_to_fourth_favourite_place(DocumentViewportHost& host,
                                  const FavouritePlace& place);
void go_to_fifth_favourite_place(DocumentViewportHost& host,
                                 const FavouritePlace& place);
void go_to_sixth_favourite_place(DocumentViewportHost& host,
                                 const FavouritePlace& place);

// The document owns timer-service ordering. Registry storage, sound-manager
// state, embedded-kit dispatch, the main window timer, and toolbar invalidation
// are supplied by the application/platform owner.
class DocumentTimerHost {
public:
    virtual ~DocumentTimerHost() = default;

    virtual bool world_update_timer_is_running() const = 0;
    virtual void broadcast_embedded_control_state(std::uint32_t state) = 0;
    virtual void kill_world_update_timer() = 0;
    virtual std::size_t non_scenery_object_count() const = 0;
    // The sweep reads each object's continuous-sound channel and releases it.
    // These are index-addressed rather than handing back a struct reference:
    // Object owns those fields directly, so a by-value record would be a copy
    // and the release would land nowhere.
    virtual std::int32_t object_sound_channel(std::size_t index) const = 0;
    virtual void release_object_sound_channel(std::size_t index) = 0;
    virtual bool sound_mixer_is_suspended() const = 0;
    virtual void clear_sound_channel_active(std::size_t channel) = 0;
    virtual void stop_sound_channel(std::size_t channel) = 0;
    virtual bool debug_logging_available() const = 0;
    virtual void log_continuous_sound_stop(std::int32_t channel) = 0;
    virtual void stop_all_sounds() = 0;
    virtual void mark_world_update_timer_paused() = 0;
    virtual std::uint32_t privilege_level() const = 0;
    virtual void update_world() = 0;
    virtual void invalidate_main_toolbar() = 0;

    // Resume needs the interval the pause kept, and the timer install.
    virtual std::uint32_t world_update_timer_interval_ms() const = 0;
    virtual void set_world_update_timer_interval_ms(
        std::uint32_t interval_ms) = 0;
    virtual void arm_world_update_timer(std::uint32_t interval_ms) = 0;
    virtual void mark_world_update_timer_running() = 0;
};

void service_world_update_timer(DocumentTimerHost& host);
void arm_world_update_timer(DocumentTimerHost& host);

} // namespace creatures1::application
