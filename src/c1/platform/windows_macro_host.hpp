#pragma once

#include "../application/sfc_ole.hpp"
#include "../brain/blackboard.hpp"
#include "mfc_creature_archives.hpp"
#include "windows_creature_hosts.hpp"
#include "windows_shell.hpp"

#include <optional>

namespace creatures1::platform {

// Concrete MacroNewObjectHost: the `new:` object factory.
//
// This is a separate class from WindowsMacroHost on purpose.  The other
// command families translate a CAOS verb into a call the document already
// offers; `new:` instead CONSTRUCTS objects, and needs the gallery cache, the
// entity registry and world-runtime adoption as collaborators.  Carrying
// those on the 145-method verb translator would mean rebuilding them per
// call.  It implements the two construction interfaces it needs rather than
// holding adapters for them, so this adds one type and no new interface.
class WindowsNewObjectHost final
    : public creatures1::scripting::MacroNewObjectHost,
      public creatures1::objects::SimpleObjectConstructionHost,
      public creatures1::objects::CompoundObjectConstructionHost {
public:
    explicit WindowsNewObjectHost(C1WindowsDocument& document)
        : document_(document) {}

    // MacroNewObjectHost.
    creatures1::objects::Object* create_call_button(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewCallButtonRequest& request) override;
    std::uint32_t generate_offspring_genome_file(
        creatures1::scripting::Macro& macro, std::uint32_t first_parent,
        std::uint32_t second_parent) override;
    creatures1::objects::Object* create_scenery(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewSceneryRequest& request) override;
    creatures1::objects::Object* create_vehicle(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewVehicleRequest& request) override;
    creatures1::objects::Object* create_creature(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewCreatureRequest& request) override;
    creatures1::objects::Object* create_blackboard(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewBlackboardRequest& request) override;
    void create_part(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewPartRequest& request) override;
    creatures1::objects::Object* create_simple_object(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewSimpleObjectRequest& request) override;
    creatures1::objects::Object* create_compound_object(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewCompoundObjectRequest& request)
        override;
    creatures1::objects::Object* create_lift(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::NewLiftRequest& request) override;

    // MacroCommandHost, reached through MacroNewObjectHost's virtual base.
    void report_syntax_error(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::MacroSyntaxDiagnostic& diagnostic)
        override;

    // SimpleObjectConstructionHost and CompoundObjectConstructionHost share
    // acquire_gallery; the rest is the entity registry, the bounds recompute
    // and the four lifetime releases.
    creatures1::display::Gallery* acquire_gallery(
        std::uint32_t sprite_file_id, int header_record_index,
        std::uint32_t image_count, bool cache_protected) override;
    creatures1::objects::EntityRegistryHost& entity_registry() override;
    void update_movement_bounds(
        creatures1::objects::SimpleObject& object) override;
    void remove_from_renderable_set(
        creatures1::objects::CompoundObject& object) override;
    void stop_continuous_sound(int sound_handle) override;
    void release_gallery(creatures1::display::Gallery& gallery) override;
    void unregister_from_object_registry(
        creatures1::objects::CompoundObject& object) override;

private:
    // Every creation path ends the same way: the world runtime takes
    // ownership and the raw pointer becomes the macro's target.
    creatures1::objects::Object* adopt(
        std::unique_ptr<creatures1::objects::Object> object);

    C1WindowsDocument& document_;
};

// Concrete owner for the recovered macro execution chain.
//
// MacroHolder needs a MacroHolderHost; MacroHolderHost's start_macro and
// execute_macro_to_output_buffer need a MacroSchedulerHost, which
// MacroSchedulerHostAdapter builds from a MacroExecutionHost and a
// MacroInterpreterHost.  This class is all four, so the adapter can finally be
// constructed and the CREATEMACRO/EXECUTE path the pipe server and CAOS
// console already drive becomes reachable.
//
// MacroInterpreterBindings is deliberately a list of nullable pointers so that
// missing command families are visible at the dispatch boundary rather than
// hidden behind an opaque callback.  Only the families this host owns are
// published; the rest stay null and the interpreter rejects their commands.
class WindowsMacroHost : public creatures1::scripting::MacroRuntimeHost,
                               public virtual creatures1::scripting::MacroHolderHost,
                               public creatures1::scripting::MacroInterpreterHost,
                               public virtual creatures1::scripting::MacroCommandHost,
                               public creatures1::scripting::MacroExceptionHost,
                               public creatures1::scripting::MacroDdeHost,
                               public creatures1::scripting::MacroObjectMotionHost,
                               public creatures1::scripting::MacroMessageHost,
                               public creatures1::scripting::MacroStimulusHost,
                               public creatures1::scripting::MacroSystemHost,
                               public creatures1::scripting::MacroBlackboardHost,
                               public creatures1::scripting::MacroDebugHost,
                               public creatures1::scripting::MacroApplicationHost,
                               public creatures1::scripting::MacroSoundHost,
                               public creatures1::scripting::MacroSoundPolicyHost,
                               public creatures1::scripting::MacroSpeechHost {
public:
    explicit WindowsMacroHost(C1WindowsDocument& document)
        : document_(document) {}

    // MacroExecutionHost, reached through MacroHolderHost.
    creatures1::objects::Object* selected_creature() const override;
    creatures1::objects::Object* initial_auxiliary_object() const override;
    creatures1::objects::Object* resolve_it_object(
        creatures1::objects::Object* script_owner) const override;

    // MacroInterpreterHost.
    creatures1::scripting::MacroInterpreterBindings interpreter_bindings()
        override;
    void report_too_many_macros(creatures1::scripting::Macro& macro,
                                std::size_t maximum_macros) override;

    // MacroObjectMotionHost: the three Object movement virtuals the CAOS
    // motion commands reach (slots 22 and 23, plus `aim:`).  The document owns
    // the per-class dispatch, because the concrete override differs by class
    // exactly as the native vtable does.
    void move_to_and_redraw(creatures1::objects::Object& object, int world_x,
                            int world_y) override;
    void move_by_and_redraw(creatures1::objects::Object& object, int delta_x,
                            int delta_y) override;
    void set_motion_target_part_index(creatures1::objects::Object& object,
                                      std::uint32_t part_index) override;

    std::int32_t vehicle_movement_vector_x(
        const creatures1::objects::Object& object) const override;
    std::int32_t vehicle_movement_vector_y(
        const creatures1::objects::Object& object) const override;
    void set_vehicle_movement_vector_x(creatures1::objects::Object& object,
                                       std::int32_t value) override;
    void set_vehicle_movement_vector_y(creatures1::objects::Object& object,
                                       std::int32_t value) override;

    // MacroObjectEventHost: `gpas`/`spas` queue the same immediate event the
    // rest of the runtime does.
    void queue_immediate_object_event(
        creatures1::objects::Object& source,
        creatures1::objects::Object& target,
        creatures1::objects::ObjectEventId event_id) override;

    // MacroDebugHost: the three recovered debug log formats.
    void log_debug_message(creatures1::scripting::Macro& macro,
                           std::string_view text) override;
    void log_debug_value(creatures1::scripting::Macro& macro,
                         std::uint32_t value) override;
    void log_debug_command_value(creatures1::scripting::Macro& macro,
                                 std::uint32_t value) override;

    // MacroApplicationHost: `app:` and `tool` reach the embedded-kit table.
    void shutdown_embedded_kit_tool(std::uint32_t tool_index) override;
    void register_embedded_kit_tool(creatures1::scripting::Macro& macro,
                                    std::string_view registry_name,
                                    std::string_view display_name,
                                    std::string_view launch_command,
                                    std::uint8_t raw_type_code) override;

    // MacroSoundHost: Object owns its own sound state; the document is the
    // playback host it reports through.
    void play_sound_effect(creatures1::objects::Object& object,
                           creatures1::sound::SoundId sound_id,
                           int queue_delay_ticks) override;
    void set_continuous_sound(creatures1::objects::Object& object,
                              creatures1::sound::SoundId sound_id,
                              bool persist_when_out_of_range) override;
    void fade_continuous_sound(creatures1::objects::Object& object) override;
    void stop_continuous_sound(creatures1::objects::Object& object) override;
    void load_sound_cache_if_audible(
        creatures1::objects::Object& object,
        creatures1::sound::SoundId sound_id) override;

    // MacroSoundPolicyHost: the four `sndf` words are view-state policy.
    void set_sound_foreground_policy() override;
    void enable_sound_and_restore_if_ready() override;
    void disable_sound_and_suspend_if_ready() override;
    void set_sound_conservative_policy() override;

    // MacroBlackboardHost: the four `bbd:` subcommands.  Macro owns the
    // grammar and the index bound; Blackboard owns display, edit mode and the
    // word banks.
    bool has_blackboard_target(
        const creatures1::scripting::Macro& macro) const override;
    std::uint32_t current_word_index(
        const creatures1::scripting::Macro& macro) const override;
    std::string current_word_text(
        const creatures1::scripting::Macro& macro) const override;
    void announce_blackboard_word(creatures1::scripting::Macro& macro,
                                  bool spoken, std::uint32_t word_index,
                                  std::string_view word_text) override;
    void write_blackboard_word(creatures1::scripting::Macro& macro,
                               std::uint32_t word_index, std::uint32_t value,
                               std::string_view word_text) override;
    void set_blackboard_edit_mode(creatures1::scripting::Macro& macro,
                                  std::uint32_t enabled) override;
    void redraw_blackboard(creatures1::scripting::Macro& macro,
                           std::uint32_t mode) override;

    // MacroSystemHost: the `sys:` prefix plus the three standalone commands
    // `edit`, `vrsn` and `room`, and `scrx`.  These reach the main frame, the
    // renderer viewport, MapData and the script-definition table.
    void move_main_window(int left, int top, int right, int bottom) override;
    void send_main_frame_command(std::uint32_t command) override;
    void enable_viewport_navigation() override;
    void set_viewport_origin(int x, int y) override;
    void open_world_file(std::string_view path) override;
    void set_ground_height(std::uint32_t x_block,
                           std::uint32_t height) override;
    void set_room_definition(std::uint32_t room_index, int left, int top,
                             int right, int bottom,
                             std::uint32_t room_type) override;
    void bring_main_window_to_front() override;
    void report_unsupported_language_version(
        creatures1::scripting::Macro& macro, std::uint32_t supported_version,
        std::uint32_t requested_version) override;
    void follow_macro_target(creatures1::scripting::Macro& macro) override;
    void set_dirty_world_rect(int left, int top, int right,
                              int bottom) override;
    void remove_script_definition(
        creatures1::scripting::ScriptClassifier classifier) override;

    // MacroStimulusHost: `stm#` carries an index into the target creature's
    // built-in context table; `stim` carries a complete descriptor.  Both have
    // the same four delivery modes, and every one of the eight producers is
    // recovered policy in creatures/events.cpp.
    void queue_sign_stimulus(creatures1::objects::Object& source,
                             std::int32_t stimulus_index) override;
    void queue_tact_stimulus(creatures1::objects::Object& source,
                             std::int32_t stimulus_index) override;
    void queue_speech_range_stimulus(creatures1::objects::Object& source,
                                     std::int32_t stimulus_index) override;
    void queue_built_in_stimulus(creatures1::objects::Object& source,
                                 creatures1::objects::Object& target,
                                 std::int32_t stimulus_index) override;
    void queue_stimulus_for_perceiving(
        creatures1::objects::Object& source,
        creatures1::creatures::StimulusContext& stimulus) override;
    void queue_stimulus_for_overlapping(
        creatures1::objects::Object& source,
        creatures1::creatures::StimulusContext& stimulus) override;
    void queue_stimulus_in_speech_range(
        creatures1::objects::Object& source,
        creatures1::creatures::StimulusContext& stimulus) override;
    void queue_direct_stimulus(
        creatures1::objects::Object& source,
        creatures1::objects::Object& target,
        creatures1::creatures::StimulusContext& stimulus) override;
    void report_invalid_stimulus_index(
        std::int32_t stimulus_index) const override;

    // MacroMessageHost extends MacroObjectEventHost with the three fan-out
    // forms of `mesg`; the fourth (`mesg writ`) is the direct queue the base
    // already supplies.
    void queue_perception_message(
        creatures1::objects::Object& source,
        creatures1::objects::ObjectEventId event_id) override;
    void queue_tactile_message(
        creatures1::objects::Object& source,
        creatures1::objects::ObjectEventId event_id) override;
    void queue_speech_range_message(
        creatures1::objects::Object& source,
        creatures1::objects::ObjectEventId event_id) override;

    // MacroSpeechHost.  say$ reaches Creature::Speak directly; say# and sayn
    // go through the phrase host and the speech-range fan-out.
    void speak_learned_word(creatures1::objects::Object& creature,
                            std::uint32_t learned_word_index) override;
    void speak_text(creatures1::objects::Object& creature,
                    std::string_view text) override;
    void speak_dominant_drive_phrase(
        creatures1::objects::Object& creature) override;

    // MacroDdeHost.  The DDE command family is part of the shipping binary --
    // the kits drive the game through it -- so it is bound rather than left
    // null.  Queries whose rendering has not been recovered yet report the gap
    // and return the native failure value, as the brain-activity report does.
    void adjust_score(creatures1::scripting::DdeScoreCounter counter,
                      std::int32_t delta) override;
    void notify_score_changed() override;
    void pan_view_to_selected_creature() override;
    std::optional<std::string> query_getb(
        creatures1::scripting::Macro& macro,
        creatures1::scripting::DdeGetBQuery query) override;
    bool update_putb(creatures1::scripting::Macro& macro,
                     creatures1::scripting::DdePutBCommand command,
                     std::string_view text) override;
    std::string render_learned_words(
        creatures1::scripting::Macro& macro,
        std::uint32_t word_index) override;
    std::string render_brain_lobe(creatures1::scripting::Macro& macro) override;
    std::string render_gene_counts(
        creatures1::scripting::Macro& macro) override;
    std::string render_cell_values(creatures1::scripting::Macro& macro,
                                   std::uint32_t cell, std::uint32_t variable,
                                   std::uint32_t field) override;
    bool capture_picture(creatures1::scripting::Macro& macro,
                         std::uint8_t width, std::uint8_t height,
                         std::string& output_path) override;

    // MacroCommandHost / MacroExceptionHost.
    void report_syntax_error(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::MacroSyntaxDiagnostic& diagnostic)
        override;
    void report_execution_exception(
        creatures1::scripting::Macro& macro,
        const creatures1::scripting::MacroExecutionExceptionDiagnostic&
            diagnostic) override;

    // MacroHolderHost.
    bool dispatch_start_macro_execution(
        creatures1::scripting::MacroHolder& holder, void* context) override;
    bool dispatch_execute_to_output(creatures1::scripting::MacroHolder& holder,
                                    void* context) override;
    bool dispatch_default(creatures1::scripting::MacroHolder& holder,
                          void* context) override;
    bool start_macro(creatures1::scripting::Macro& macro) override;
    std::uint32_t execute_macro_to_output_buffer(
        creatures1::scripting::Macro& macro, char* output_buffer) override;
    void publish_macro_output(creatures1::application::OleScriptVariant* context,
                              const char* output,
                              std::size_t output_length) override;
    bool start_macro_execution(creatures1::scripting::MacroHolder& holder,
                               char* output_buffer) override;
    bool execute_macro_to_output(creatures1::scripting::MacroHolder& holder,
                                 char* output_buffer) override;
    bool default_result(creatures1::scripting::MacroHolder& holder,
                        char* output_buffer) override;
    bool image_sequence_is_empty(
        creatures1::scripting::MacroHolder& holder) override;
    char* format_brain_activity_report(
        creatures1::objects::Object* brain_object, char* output_buffer,
        std::uint32_t report_mode, std::uint32_t rule_index) override;

    // MacroRuntimeHost.  Each method either delegates to already-translated
    // policy or is held with the reason it cannot be reached yet; none is a
    // silent no-op.
    bool is_live_object(
        const creatures1::objects::Object* object) const override;
    void report_invalid_object_reference(std::uint32_t value) const override;
    bool is_creature_object(const creatures1::objects::Object& object) const override;
    bool is_simple_object(const creatures1::objects::Object& object) const override;
    bool is_compound_object(const creatures1::objects::Object& object) const override;
    bool is_vehicle_object(const creatures1::objects::Object& object) const override;
    std::uint32_t creature_value(const creatures1::objects::Object& object,
                                 creatures1::scripting::MacroCreatureValue value,
                                 std::uint32_t index) const override;
    std::uint32_t attention_record_index(
        const creatures1::objects::Object& object) const override;
    std::int32_t ground_height(std::uint32_t x_block) const override;
    std::uint32_t room_count() const override;
    std::int32_t room_value(std::uint32_t room_index,
                            std::uint32_t field) const override;
    std::int32_t ambient_temperature_at(
        const creatures1::objects::Object& object) const override;
    std::int8_t ambient_wind() const override;
    std::uint32_t score_value(std::uint32_t index) const override;
    std::uint32_t world_tick_count() const override;
    std::uint32_t language_version() const override;
    std::uint32_t sound_settings() const override;
    std::uint32_t viewport_value(
        creatures1::scripting::MacroViewportValue value) const override;
    creatures1::objects::Object* pointer_tool() const override;
    creatures1::objects::Object* edit_object() const override;
    creatures1::objects::Object* topmost_pointer_object() const override;
    std::uint32_t enabled_object_count_matching(
        std::uint32_t family, std::uint32_t genus,
        std::uint32_t species) const override;
    std::size_t non_scenery_object_count() const override;
    creatures1::objects::Object* non_scenery_object_at(
        std::size_t index) const override;
    creatures1::objects::Object* random_non_scenery_object(
        const creatures1::scripting::MacroClassifierPattern& pattern) const override;
    std::uint32_t select_creature_target_pose_for_motion(
        creatures1::objects::Object& object,
        bool force_interaction_pose) override;
    void clear_creature_selected_decision_neuron(
        creatures1::objects::Object& object) override;
    void select_creature_walk_gait(creatures1::objects::Object& object) override;
    bool creature_approach_is_ready(
        const creatures1::objects::Object& object) const override;
    void reset_creature_animation_sequence(
        creatures1::objects::Object& object) override;
    void set_selected_creature(creatures1::objects::Object* creature,
                               bool notify) override;
    void set_edit_object(creatures1::objects::Object* object) override;
    void set_viewport_value(creatures1::scripting::MacroViewportValue value,
                            std::uint32_t new_value) override;
    void set_creature_value(creatures1::objects::Object& object,
                            creatures1::scripting::MacroCreatureAssignment field,
                            std::uint32_t value) override;
    void add_creature_chemical_moles(creatures1::objects::Object& object,
                                     std::uint32_t chemical_index,
                                     std::uint32_t moles) override;
    void fire_creature_neuron(creatures1::objects::Object& object,
                              std::int32_t global_x, std::int32_t global_y,
                              std::int32_t activation) override;
    void set_creature_brain_neuron_activation(
        creatures1::objects::Object& object, std::uint32_t lobe_index,
        std::uint32_t neuron_index, std::uint8_t activation) override;
    void set_object_image_index(creatures1::objects::Object& object,
                                std::uint8_t image_index,
                                std::int32_t part_index) override;
    void configure_simple_object_behavior(
        creatures1::objects::Object& object, std::uint32_t behavior_index,
        std::uint32_t interaction_flags) override;
    bool set_object_relative_image_index(creatures1::objects::Object& object,
                                         std::uint32_t relative_index,
                                         std::int32_t part_index) override;
    char* preload_object_image_sequence(creatures1::objects::Object& object,
                                        char* sequence_text,
                                        std::int32_t part_index) override;
    void teleport_target_and_refresh_selection(
        creatures1::objects::Object* target, std::int32_t world_x,
        std::int32_t world_y) override;
    void drive_presentation_for_bound_creatures(
        creatures1::objects::Object& target) override;
    void process_creature_insemination(
        creatures1::objects::Object& object) override;
    void die_creature(creatures1::objects::Object& object) override;
    void initialize_object_runtime_state(
        creatures1::objects::Object& object) override;
    void set_creature_sleep_indicator(creatures1::objects::Object& object,
                                      std::uint32_t enabled) override;
    void set_creature_dream_countdown(creatures1::objects::Object& object,
                                      std::uint32_t countdown) override;
    void notify_creature_dependents_on_removal(
        creatures1::objects::Object& object) override;
    void remove_object_from_event_bar(creatures1::objects::Object& object,
                                      bool record_auxiliary_state) override;
    void add_object_to_event_bar(creatures1::objects::Object* object) override;
    void stop_creature_involuntary_action(
        creatures1::objects::Object& object) override;
    void set_creature_action_activation_boost(
        creatures1::objects::Object& object, std::uint32_t value) override;
    void set_creature_involuntary_action_cooldown(
        creatures1::objects::Object& object, std::uint32_t action_index,
        std::uint8_t cooldown_ticks) override;
    void update_creature_bacterium(creatures1::objects::Object& object) override;
    void initialize_creature_default_vocabulary(
        creatures1::objects::Object& object) override;
    void set_compound_part_bounds(creatures1::objects::Object& object,
                                  std::uint32_t part_bounds_index,
                                  std::int32_t min_x, std::int32_t min_y,
                                  std::int32_t max_x,
                                  std::int32_t max_y) override;
    void set_compound_knob_function(creatures1::objects::Object& object,
                                    std::uint32_t function_index,
                                    std::uint32_t hotspot_index) override;
    void set_vehicle_creature_event_bounds(
        creatures1::objects::Object& object, std::int32_t min_x,
        std::int32_t min_y, std::int32_t max_x, std::int32_t max_y) override;
    void grab_vehicle_passengers(creatures1::objects::Object& object) override;
    void set_object_bounds_mode(creatures1::objects::Object& object,
                                std::uint32_t mode) override;
    void update_object_movement_bounds(
        creatures1::objects::Object& object) override;
    void note_egg_state_change() override;

private:
    creatures1::creatures::Creature* creature_of(
        creatures1::objects::Object& object) const;

    C1WindowsDocument& document_;
private:
    // `new:` needs galleries, the entity registry and world-runtime adoption;
    // it is held here so the bindings pointer outlives one interpreter call.
    WindowsNewObjectHost new_object_{document_};
};

// Small concrete owners for the host contracts the recovered CAOS commands
// delegate through.  Each is a thin accessor onto already-translated policy;
// none introduces a new interface.
class WindowsStimulusSourceHost final
    : public creatures1::creatures::Creature::StimulusSourceHost {
public:
    explicit WindowsStimulusSourceHost(C1WindowsDocument& document)
        : document_(document) {}

    creatures1::creatures::AttentionClassifier classify(
        const creatures1::objects::Object& object) const override;
    int sound_source_x(
        const creatures1::objects::Object& object) const override;
    int sound_source_y(
        const creatures1::objects::Object& object) const override;
    bool is_this_creature(
        const creatures1::objects::Object& object,
        const creatures1::creatures::Creature& creature) const override;

private:
    C1WindowsDocument& document_;
};

// An entity's animation frame change dirties the rectangle it vacated and the
// one it now covers; the renderer's queue owns viewport clipping and merging.
class WindowsEntityImageSequenceRenderHost final
    : public creatures1::objects::EntityImageSequenceRenderHost {
public:
    explicit WindowsEntityImageSequenceRenderHost(C1WindowsDocument& document)
        : document_(document) {}

    void redraw_image_sequence_change(
        creatures1::objects::Entity& entity,
        const creatures1::world::WorldRect& old_bounds,
        const creatures1::world::WorldRect& new_bounds) override;

private:
    C1WindowsDocument& document_;
};

class WindowsVehicleEventHost final
    : public creatures1::objects::VehicleEventHost {
public:
    explicit WindowsVehicleEventHost(C1WindowsDocument& document)
        : document_(document) {}

    std::size_t creature_count() const override;
    creatures1::objects::Object* creature_at(
        std::size_t index) const override;
    void report_invalid_creature_index() const override;
    bool object_is_bound_to_vehicle(
        const creatures1::objects::Object& creature,
        const creatures1::objects::Vehicle& vehicle) const override;
    void queue_immediate_event(creatures1::objects::Object& source,
                               creatures1::objects::Object& target,
                               creatures1::objects::ObjectEventId event_id,
                               std::uint32_t argument) override;

private:
    C1WindowsDocument& document_;
};

class WindowsCreatureBacteriumEnvironmentHost final
    : public creatures1::creatures::CreatureBacteriumEnvironmentHost,
      public creatures1::creatures::CreatureGoalDirectionHost {
public:
    explicit WindowsCreatureBacteriumEnvironmentHost(
        C1WindowsDocument& document)
        : document_(document), environment_(document) {}

    creatures1::creatures::CreatureEnvironmentHost& environment_host()
        override;
    creatures1::creatures::CreatureGoalDirectionHost& goal_direction_host()
        override;
    bool is_selected_creature(
        const creatures1::creatures::Creature& creature) const override;

    std::int32_t room_index_at(int world_x, int world_y) const override;
    std::int32_t room_class_at(int world_x, int world_y) const override;
    std::uint32_t current_interaction_event_id(
        const creatures1::objects::Object& object) const override;
    std::int32_t attention_record_index(
        const creatures1::objects::Object& object) const override;

private:
    C1WindowsDocument& document_;
    WindowsCreatureEnvironmentHost environment_;
};

// The document owns the event scheduler; this presents its immediate ring
// through the interface the recovered Creature and Object policies expect.
class WindowsImmediateEventQueueHost final
    : public creatures1::objects::ObjectImmediateEventQueueHost {
public:
    explicit WindowsImmediateEventQueueHost(C1WindowsDocument& document)
        : document_(document) {}

    void queue_immediate_event(creatures1::objects::Object& source,
                               creatures1::objects::Object& target,
                               creatures1::objects::ObjectEventId event_id,
                               std::uint32_t argument) override;

private:
    C1WindowsDocument& document_;
};

// CreatureAttentionHost.  Every method here either delegates to already
// translated policy or is a documented boundary; the sleep-indicator
// construction comes straight from SetSleepIndicator @ 0040da80.
class WindowsCreatureAttentionHost final
    : public creatures1::creatures::CreatureAttentionHost {
public:
    explicit WindowsCreatureAttentionHost(C1WindowsDocument& document)
        : document_(document) {}

    // ObjectImmediateEventQueueHost and CreatureScriptDispatchHost are also
    // inherited by CreatureAttentionHost.
    void queue_immediate_event(creatures1::objects::Object& source,
                               creatures1::objects::Object& target,
                               creatures1::objects::ObjectEventId event_id,
                               std::uint32_t argument) override;
    std::uint32_t classifier_base(
        const creatures1::creatures::Creature& creature) const override;
    int execute_script_for_classifier(
        creatures1::creatures::Creature& creature,
        creatures1::objects::Object* source, std::uint32_t classifier,
        bool force_restart) override;

    // CreatureObjectIdentityHost, inherited by CreatureAttentionHost; the
    // document already owns this mapping.
    creatures1::objects::Object& object_for_creature(
        creatures1::creatures::Creature& creature) const override;

    creatures1::objects::Object* create_sleep_indicator(
        creatures1::creatures::Creature& creature,
        int render_plane) override;
    bool action_event_target_is_live(
        const creatures1::objects::Object& target) const override;
    std::uint32_t classifier_base(
        const creatures1::objects::Object& object) const override;
    creatures1::creatures::ActionTargetRequirement action_target_requirement(
        std::size_t action_index) const override;
    creatures1::creatures::AttentionClassifier classify_object(
        const creatures1::objects::Object& object) const override;
    void dispatch_sleep_indicator_event(
        creatures1::objects::Object& indicator,
        creatures1::objects::ObjectEventId event_id,
        creatures1::objects::Object* target,
        std::uint32_t argument) override;
    void initialize_sleep_indicator(
        creatures1::objects::Object& indicator) override;
    bool debug_console_visible() const override;
    bool is_selected_creature(
        const creatures1::creatures::Creature& creature) const override;
    void log_attention_shift(
        const creatures1::creatures::Creature& creature,
        const creatures1::objects::Object* target) override;
    void log_override_action_script_selected(
        const creatures1::creatures::Creature& creature,
        std::uint32_t classifier) override;
    void log_no_action_script(const creatures1::creatures::Creature& creature,
                              std::uint32_t classifier) override;
    void log_involuntary_action(
        const creatures1::creatures::Creature& creature,
        int action_index) override;
    void log_action_selection(
        const creatures1::creatures::Creature& creature,
        const creatures1::objects::Object* motion_link,
        std::uint32_t classifier) override;

private:
    C1WindowsDocument& document_;
};

// ScriptExecutionHost: the classifier resolver owns the lookup and state
// transitions; this supplies the macro storage and scheduling it needs, which
// the macro chain now provides.
class WindowsScriptExecutionHost final
    : public creatures1::scripting::ScriptExecutionHost {
public:
    WindowsScriptExecutionHost(C1WindowsDocument& document,
                               WindowsMacroHost& macro_host)
        : document_(document), macro_host_(macro_host) {}

    void report_script_counts_if_changed(std::size_t stored_count,
                                         std::size_t running_count) override;
    creatures1::scripting::Macro* find_running_macro_for_owner(
        creatures1::objects::Object* script_owner) const override;
    creatures1::scripting::Macro* create_initialized_macro() override;
    creatures1::objects::Object* selected_creature() const override;
    void start_macro_execution(creatures1::scripting::Macro& macro) override;
    void report_missing_script(
        creatures1::scripting::ScriptClassifier classifier) override;

private:
    C1WindowsDocument& document_;
    WindowsMacroHost& macro_host_;
};

// CreatureInseminationHost.  generate_offspring_genome_file is already
// translated as a free function in creatures/genome.cpp, and the document
// already constructs the GenomeFileStore it needs, so this supplies only the
// random source and the recipient/logging boundary.
class WindowsCreatureInseminationHost final
    : public creatures1::creatures::CreatureInseminationHost,
      public creatures1::creatures::GenomeRandomSource,
      public creatures1::creatures::GenomeFilenameRegistry {
public:
    explicit WindowsCreatureInseminationHost(C1WindowsDocument& document)
        : document_(document) {}

    creatures1::creatures::Creature* recipient_for_insemination(
        creatures1::creatures::Creature& source) const override;
    creatures1::creatures::GenomeFilenameId generate_offspring_genome_file(
        creatures1::creatures::GenomeFilenameId maternal_source_filename,
        creatures1::creatures::GenomeFilenameId paternal_source_filename)
        override;
    bool debug_console_visible() const override;
    bool is_selected_creature(
        const creatures1::creatures::Creature& creature) const override;
    void log_insemination(
        creatures1::creatures::InseminationLogEvent event,
        const creatures1::creatures::Creature& source,
        const creatures1::creatures::Creature* recipient,
        creatures1::creatures::GenomeFilenameId paternal_source_filename)
        override;

    // GenomeFilenameRegistry: the recovered generator rejects a candidate
    // already in use by a creature or a family-four object.
    bool creature_uses_filename(
        creatures1::creatures::GenomeFilenameId id) const override;
    bool family_four_object_uses_filename(
        creatures1::creatures::GenomeFilenameId id) const override;

    // GenomeRandomSource.
    std::uint32_t next() override;
    void log_mutation(std::uint8_t old_value, std::uint8_t new_value,
                      std::uint8_t bit_distance) override;
    bool debug_logging_enabled() const override;
    void log_conception(std::uint32_t crossovers, std::uint32_t duplications,
                        std::uint32_t omissions,
                        std::uint32_t mutations) override;

private:
    C1WindowsDocument& document_;
};

// ObjectScriptDispatchHost: the Object-side twin of the Creature dispatch
// already implemented on WindowsCreatureAttentionHost.  Both resolve a
// classifier through the translated scripting::execute_script_for_classifier.
class WindowsObjectScriptDispatchHost final
    : public creatures1::objects::ObjectScriptDispatchHost {
public:
    explicit WindowsObjectScriptDispatchHost(C1WindowsDocument& document)
        : document_(document) {}

    int execute_script_for_classifier(
        creatures1::objects::Object& object,
        creatures1::objects::Object* from_object, std::uint32_t classifier,
        bool force_restart) override;

private:
    C1WindowsDocument& document_;
};

// Blackboard's two hosts.  Every service is an existing document one: the
// document is already the EntityRasterHost and the CompoundObjectMoveRedrawHost,
// it owns the pointer tool, and it now owns the retargetable text editor.
class WindowsBlackboardHost final
    : public creatures1::brain::BlackboardRuntimeHost,
      public creatures1::brain::BlackboardDisplayHost {
public:
    explicit WindowsBlackboardHost(C1WindowsDocument& document)
        : document_(document) {}

    // ObjectSoundViewportHost.
    creatures1::world::ViewportBounds sound_viewport() const override;

    // BlackboardTextInputHost.
    creatures1::objects::Object* pointer_tool() const override;
    void clear_text_input() override;
    void configure_text_input(creatures1::objects::Object* target,
                              std::uint32_t maximum_length,
                              std::uint32_t allowed_characters) override;
    void update_text_input(creatures1::objects::Object& target,
                           std::string_view text) override;

    // BlackboardDisplayHost.
    creatures1::objects::EntityRasterHost& raster_host() override;
    creatures1::objects::CompoundObjectMoveRedrawHost& redraw_host() override;

private:
    C1WindowsDocument& document_;
};

// CreatureConstructionHost plus its nested GenomeInitializationHost.  Every
// service either already exists on the document or is one of the small hosts
// built earlier; this composes them for the recovered genome constructor at
// 0040d580 and the Testing menu's norn-creation commands.
class WindowsCreatureConstructionHost final
    : public creatures1::creatures::CreatureConstructionHost,
      public creatures1::creatures::Creature::GenomeInitializationHost {
public:
    explicit WindowsCreatureConstructionHost(C1WindowsDocument& document)
        : document_(document),
          render_plane_(document),
          environment_(document) {}

    // CreatureConstructionHost.
    creatures1::creatures::Creature::InitializationHost& initialization_host()
        override;
    creatures1::creatures::Creature::GenomeInitializationHost&
    genome_initialization_host() override;
    std::size_t living_creature_count() const override;
    const creatures1::creatures::Creature* living_creature_at(
        std::size_t index) const override;
    void report_invalid_living_creature_index() const override;
    creatures1::objects::ObjectRenderableSetHost& renderable_set() override;
    creatures1::objects::ObjectMovementBoundsHost& movement_bounds() override;
    creatures1::objects::ObjectSoundPlaybackHost& sound_playback() override;
    const creatures1::creatures::MultibyteTextApi& text_api() const override;
    creatures1::creatures::CreatureEnvironmentHost& environment() override;
    void append_to_creature_registry(
        creatures1::creatures::Creature& creature) override;
    void rebuild_creature_selection_menu() override;

    // Creature::GenomeInitializationHost.
    creatures1::creatures::GenomeFileStore& genome_files() override;
    const creatures1::creatures::SkeletonSpriteBuildServices&
    skeleton_services() const override;
    creatures1::creatures::SkeletonRenderPlaneHost& render_plane_host()
        override;
    creatures1::objects::ObjectSoundPlaybackHost& sound_host() override;
    creatures1::biochemistry::BiochemistryLocusHost& biochemistry_locus_host(
        creatures1::biochemistry::Biochemistry& value) override;
    creatures1::creatures::VoiceFileStore& voice_files() override;

private:
    C1WindowsDocument& document_;
    WindowsSkeletonRenderPlaneHost render_plane_;
    WindowsCreatureEnvironmentHost environment_;
    mutable std::optional<creatures1::creatures::SkeletonSpriteBuildServices>
        skeleton_services_;
    std::optional<creatures1::platform::MfcBiochemistryLocusHost> locus_host_;
};

// GeneratedCreatureCommandHost for Testing > Create a male/female norn.
class WindowsGeneratedCreatureHost final
    : public creatures1::application::GeneratedCreatureCommandHost {
public:
    explicit WindowsGeneratedCreatureHost(C1WindowsDocument& document)
        : document_(document) {}

    creatures1::creatures::GenomeFilenameId generate_test_offspring_genome()
        override;
    creatures1::creatures::Creature* create_generated_creature(
        creatures1::creatures::GenomeFilenameId genome_source_filename,
        creatures1::creatures::CreatureConstructionSex construction_sex)
        override;
    void set_selected_creature(
        creatures1::creatures::Creature* creature) override;
    void set_edit_object(creatures1::creatures::Creature* creature) override;

private:
    C1WindowsDocument& document_;
};

} // namespace creatures1::platform
