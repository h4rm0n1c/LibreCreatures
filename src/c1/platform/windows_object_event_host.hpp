#pragma once

#include "windows_prelude.hpp"

#include "../creatures/creature.hpp"
#include "../objects/call_button.hpp"
#include "../objects/compound_object.hpp"
#include "../objects/events.hpp"
#include "../objects/lift.hpp"
#include "../objects/simple_object.hpp"
#include "../ui/tools.hpp"
#include "../brain/blackboard.hpp"
#include "windows_creature_hosts.hpp"
#include "windows_macro_host.hpp"
#include "windows_pointer_tool_host.hpp"

namespace creatures1::platform {

class C1WindowsDocument;

// Concrete ObjectEventRuntime.  World tick phases 0/4/8/12 run
// ProcessQueuedObjectEventsAndStimuli @ 00432d20, which drives every queued
// object event through the target's vtable and then drains the creature
// stimulus ring.  The pump itself is already translated in
// objects/events.cpp; the ten handlers it dispatches are already translated
// as ordinary members on the port's Object hierarchy.  What was missing is
// this boundary, which reproduces the native vtable's slot assignments.
//
// The recovered mapping, read out of the tables at 0x00457a70 (Object and
// Scenery), 0x004579b8 (SimpleObject), 0x00457834 (Bubble), 0x004578ec
// (PointerTool), 0x00457be0 (CallButton), 0x00457554 (CompoundObject) and
// 0x004547fc (Creature):
//
//   queued id  slot   Object/Scenery      SimpleObject        CompoundObject      Creature
//   EVENT_0    5      creature event      handle_queued_0     compound 0          slot5
//   EVENT_1    6      creature event      handle_queued_1     compound 1          slot6
//   EVENT_2    7      creature event      handle_queued_2     compound 2          slot7
//   EVENT_3    10     creature event      handle_queued_3     handle_queued_3     (no-op)
//   EVENT_4    8      creature event      handle_queued_4     creature event      pickup
//   EVENT_5    9      (no-op)             handle_queued_5     (no-op)             drop
//   EVENT_6    11     (no-op)             (no-op)             (no-op)             heard words
//   EVENT_7    12     (no-op)             (no-op)             (no-op)             word learning
//   EVENT_8    14     dispatch_event_7_after_bounds_update  (same for every class)
//   EVENT_9    15     dispatch_event_8                      (same for every class)
//
// CallButton overrides slots 5 and 6; its slot-6 body is a tail jump back to
// slot 5, so both queued events run request_lift_call.  PointerTool overrides
// slots 8 and 9. Lift overrides slots 5 and 6 with its up/down movement
// requests; its slot 7 is the inherited generic event-3 body. Blackboard
// keeps CompoundObject's event 0/1/2 handlers.
// CompoundObjectEventHost.  Four facts: whether the event source is a
// creature, the classifier script dispatch, and the built-in stimulus copy
// and queue the compound interaction path shares with the fan-out producers.
class WindowsCompoundObjectEventHost final
    : public creatures1::objects::CompoundObjectEventHost {
public:
    explicit WindowsCompoundObjectEventHost(C1WindowsDocument& document)
        : document_(document) {}

    bool source_is_creature(
        const creatures1::objects::Object& source) const override;
    int dispatch_script_event(creatures1::objects::CompoundObject& target,
                              creatures1::objects::Object* source,
                              creatures1::objects::ObjectEventId event_id)
        override;
    bool copy_built_in_stimulus(
        const creatures1::objects::Object& source,
        creatures1::objects::Object& target, std::uint32_t stimulus_index,
        creatures1::objects::QueuedCreatureStimulus& out) const override;
    void queue_creature_stimulus(
        const creatures1::objects::QueuedCreatureStimulus& stimulus) override;

private:
    C1WindowsDocument& document_;
};

// CreatureScriptEventHost: the one call Creature's queued interaction events
// make back into the script runtime.
class WindowsCreatureScriptEventHost final
    : public creatures1::creatures::CreatureScriptEventHost {
public:
    explicit WindowsCreatureScriptEventHost(C1WindowsDocument& document)
        : document_(document) {}

    void dispatch_script_event(creatures1::creatures::Creature& creature,
                               creatures1::objects::Object* source,
                               std::uint32_t event_id,
                               std::uint32_t argument) override;

private:
    C1WindowsDocument& document_;
};

// CallButtonRuntimeHost.  CallButton overrides queued events 0 and 1; the
// slot-6 body is a tail jump to slot 5, so both run request_lift_call.  The
// three own methods are the map-room queries that pick the called floor.
// Also the LiftRuntimeHost: a Lift and its call buttons share the map-room
// queries, the compound event host and the immediate event queue, so this
// carries both contracts rather than a second class restating them.
// It is also the CompoundObjectTickHost, and it was already the VehicleTickHost
// -- LiftRuntimeHost derives from that, so vehicles and lifts both tick through
// this one class.  Every base those contracts compose is already implemented
// here, so the two ticks cost two new methods rather than a new class.
class WindowsCallButtonRuntimeHost final
    : public creatures1::objects::CallButtonRuntimeHost,
      public creatures1::objects::LiftRuntimeHost,
      public creatures1::objects::CompoundObjectTickHost {
public:
    explicit WindowsCallButtonRuntimeHost(C1WindowsDocument& document)
        : document_(document), compound_(document) {}

    // CallButtonRuntimeHost's own queries.
    std::size_t room_count() const override;
    bool room_contains_point(std::size_t room_index, int world_x,
                             int world_y) const override;
    int room_bottom(std::size_t room_index) const override;

    // ObjectMovementBoundsHost.
    void clear_edit_object() override;
    void find_nearest_room_bounds_at_point(
        int world_x, int world_y,
        creatures1::world::WorldRect& out_bounds) const override;
    creatures1::world::WorldRect vehicle_local_bounds(
        const creatures1::objects::Object& vehicle) const override;
    int vehicle_primary_entity_x(
        const creatures1::objects::Object& vehicle) const override;
    int vehicle_primary_entity_y(
        const creatures1::objects::Object& vehicle) const override;
    creatures1::objects::Object* edit_object() const override;

    // ObjectImmediateEventQueueHost.
    void queue_immediate_event(creatures1::objects::Object& source,
                               creatures1::objects::Object& target,
                               creatures1::objects::ObjectEventId event_id,
                               std::uint32_t argument) override;

    // ObjectScriptDispatchHost.
    int execute_script_for_classifier(creatures1::objects::Object& object,
                                      creatures1::objects::Object* from_object,
                                      std::uint32_t classifier,
                                      bool force_restart) override;

    // CompoundObjectEventHost.
    bool source_is_creature(
        const creatures1::objects::Object& source) const override;
    int dispatch_script_event(creatures1::objects::CompoundObject& target,
                              creatures1::objects::Object* source,
                              creatures1::objects::ObjectEventId event_id)
        override;
    bool copy_built_in_stimulus(
        const creatures1::objects::Object& source,
        creatures1::objects::Object& target, std::uint32_t stimulus_index,
        creatures1::objects::QueuedCreatureStimulus& out) const override;
    void queue_creature_stimulus(
        const creatures1::objects::QueuedCreatureStimulus& stimulus) override;

    // LiftRuntimeHost's own three, plus VehicleTickHost's one.
    void publish_active_lift(creatures1::objects::Lift& lift) override;
    int call_button_floor(
        const creatures1::objects::CallButton& button) const override;
    void deactivate_call_button(
        creatures1::objects::CallButton& button) override;
    int ground_height_at_x_block(std::size_t block_index) const override;

    // ObjectSoundViewportHost, reached through ObjectSoundPlaybackHost.
    creatures1::world::ViewportBounds sound_viewport() const override;

    // ObjectSoundPlaybackHost.
    bool sounds_muted() const override;
    creatures1::sound::SoundManager& sound_manager() override;
    bool debug_console_available() const override;
    void log_sound_event(creatures1::objects::ObjectSoundPlaybackHost::LogEvent event,
                         creatures1::sound::SoundId sound_id,
                         int value) override;

    // ObjectRegistryHost.
    void add_object(creatures1::objects::Object* object) override;
    std::size_t object_count() const override;
    creatures1::objects::Object* object_at(std::size_t index) const override;
    void remove_object_at(std::size_t index) override;
    void report_invalid_index() const override;

    // EntityImageSequenceRenderHost.
    void redraw_image_sequence_change(
        creatures1::objects::Entity& entity,
        const creatures1::world::WorldRect& old_bounds,
        const creatures1::world::WorldRect& new_bounds) override;

    // CompoundObjectTickHost.
    void dispatch_timer_event(
        creatures1::objects::CompoundObject& object) override;
    void advance_image_sequence(creatures1::objects::Entity& entity) override;

    // CompoundObjectMoveRedrawHost.
    void redraw_after_compound_object_move(
        creatures1::objects::CompoundObject& object,
        const creatures1::world::WorldRect& old_bounds,
        const creatures1::world::WorldRect& new_bounds) override;
    void queue_compound_object_dirty_rect(
        creatures1::objects::CompoundObject& object,
        const creatures1::world::WorldRect& primary_bounds) override;

private:
    C1WindowsDocument& document_;
    WindowsCompoundObjectEventHost compound_;
};

// ObjectEventDispatchHost: queued EVENT_9 reaches Object::dispatch_event_8,
// which asks the runtime to run the object's own event-8 classifier script.
class WindowsObjectEventDispatchHost final
    : public creatures1::objects::ObjectEventDispatchHost {
public:
    explicit WindowsObjectEventDispatchHost(C1WindowsDocument& document)
        : document_(document) {}

    void dispatch_event_8(creatures1::objects::Object& object) override;

private:
    C1WindowsDocument& document_;
};

// SimpleObjectInteractionHost serves queued events 4 and 5 for every
// SimpleObject that is not the pointer tool.  Every method is an existing
// document, renderer or pointer service.
// It is also the SimpleObjectTickHost and the BubbleTickHost.  It already
// carries the renderable set, the script dispatch and the pointer queries
// those ticks need, and a Bubble is a SimpleObject, so both tick contracts
// live here rather than in classes that would restate the same collaborators.
class WindowsSimpleObjectInteractionHost final
    : public creatures1::objects::SimpleObjectInteractionHost,
      public creatures1::objects::SimpleObjectTickHost,
      public creatures1::objects::BubbleTickHost,
      public creatures1::objects::SimpleObjectEditHost {
public:
    // WindowsPointerToolRuntimeHost already implements every ObjectOverlapHost
    // query and the three pointer queries; this composes it rather than
    // restating them.
    explicit WindowsSimpleObjectInteractionHost(C1WindowsDocument& document);

    // SimpleObjectInteractionHost's own queries.  The return is narrowed to
    // SimpleObject* so this one override also satisfies SimpleObjectTickHost,
    // whose pointer_tool() is the same query at that type.
    creatures1::objects::SimpleObject* pointer_tool() const override;
    int pointer_world_x() const override;
    int pointer_world_y() const override;
    std::size_t non_scenery_object_count() const override;
    creatures1::objects::Object* non_scenery_object_at(
        std::size_t index) const override;
    void report_invalid_non_scenery_index() const override;

    // ObjectRenderableSetHost.
    bool contains(const creatures1::objects::Object& object) const override;
    void insert(creatures1::objects::Object& object) override;
    void erase(creatures1::objects::Object& object) override;

    // ObjectMovementBoundsHost.
    void clear_edit_object() override;
    void find_nearest_room_bounds_at_point(
        int world_x, int world_y,
        creatures1::world::WorldRect& out_bounds) const override;
    creatures1::world::WorldRect vehicle_local_bounds(
        const creatures1::objects::Object& vehicle) const override;
    int vehicle_primary_entity_x(
        const creatures1::objects::Object& vehicle) const override;
    int vehicle_primary_entity_y(
        const creatures1::objects::Object& vehicle) const override;
    creatures1::objects::Object* edit_object() const override;

    // ObjectOverlapHost.
    std::size_t object_count() const override;
    creatures1::objects::Object* object_at(std::size_t index) const override;
    void report_invalid_index() const override;
    bool is_pointer_tool(
        const creatures1::objects::Object& object) const override;
    creatures1::world::WorldRect pointer_tool_bounds(
        const creatures1::objects::Object& pointer_tool) const override;
    creatures1::world::WorldRect vehicle_interaction_bounds(
        const creatures1::objects::Object& vehicle) const override;

    // ObjectImmediateEventQueueHost.
    void queue_immediate_event(creatures1::objects::Object& source,
                               creatures1::objects::Object& target,
                               creatures1::objects::ObjectEventId event_id,
                               std::uint32_t argument) override;

    // ObjectScriptDispatchHost.
    int execute_script_for_classifier(creatures1::objects::Object& object,
                                      creatures1::objects::Object* from_object,
                                      std::uint32_t classifier,
                                      bool force_restart) override;

    // SimpleObjectTickHost's own operations.
    bool pointer_input_pending() const override;
    void process_pending_pointer_input(
        creatures1::objects::SimpleObject& pointer_tool) override;
    int pointer_mouse_world_x() const override;
    int pointer_mouse_world_y() const override;
    void present_or_queue_dirty_world_rect(
        const creatures1::world::WorldRect& dirty_rect) override;
    int native_part3_entity_byte_render_plane_offset(
        const creatures1::objects::CompoundObject& reference) const override;

    // SimpleObjectEditHost adds only this to the four bases already here.
    int privilege_level() const override;

    // BubbleTickHost.
    void move_bubble_and_redraw(creatures1::objects::Bubble& bubble,
                                int world_x, int world_y) override;
    void expire_bubble(creatures1::objects::Bubble& bubble) override;

    // EntityImageSequenceRenderHost.
    void redraw_image_sequence_change(
        creatures1::objects::Entity& entity,
        const creatures1::world::WorldRect& old_bounds,
        const creatures1::world::WorldRect& new_bounds) override;

    // ObjectSoundViewportHost / ObjectSoundPlaybackHost.
    creatures1::world::ViewportBounds sound_viewport() const override;
    bool sounds_muted() const override;
    creatures1::sound::SoundManager& sound_manager() override;
    bool debug_console_available() const override;
    void log_sound_event(
        creatures1::objects::ObjectSoundPlaybackHost::LogEvent event,
        creatures1::sound::SoundId sound_id, int value) override;

    // SimpleObjectMoveRedrawHost.
    void redraw_after_simple_object_move(
        creatures1::objects::SimpleObject& object,
        const creatures1::world::WorldRect& old_bounds,
        const creatures1::world::WorldRect& new_bounds) override;

private:
    C1WindowsDocument& document_;
    WindowsPointerToolRuntimeHost pointer_;
};

// CreaturePickupHost and CreatureDropHost serve queued events 4 and 5 on a
// Creature.  Both compose the same renderer/bounds/sound trio the Skeleton
// needs, so they share one implementation surface.
class WindowsCreaturePickupDropHost final
    : public creatures1::creatures::CreaturePickupHost,
      public creatures1::creatures::CreatureDropHost {
public:
    explicit WindowsCreaturePickupDropHost(C1WindowsDocument& document)
        : document_(document), interaction_(document), events_(document) {}

    // Shared by both hosts.
    creatures1::objects::ObjectRenderableSetHost& renderables() override;
    creatures1::objects::ObjectMovementBoundsHost& movement_bounds_host()
        override;
    creatures1::objects::ObjectSoundPlaybackHost& sound_host() override;
    creatures1::objects::Object* pointer_tool() const override;
    void queue_dirty_world_rect(
        const creatures1::world::WorldRect& bounds) override;

    // CreaturePickupHost.
    bool pointer_pickup_is_privileged(
        const creatures1::objects::Object& source) const override;
    int vehicle_attachment_render_plane(
        const creatures1::objects::Object& vehicle) const override;
    void select_creature(creatures1::creatures::Creature& creature) override;

    // CreatureDropHost.
    creatures1::objects::Object* find_topmost_vehicle_overlap(
        creatures1::creatures::Creature& creature) const override;

    // CreatureUnboundedWorldPositionHost (via CreaturePickupHost) and
    // CreatureObjectIdentityHost (via CreatureDropHost).
    bool read_view_input_and_clear_pending_flag(
        creatures1::creatures::UnboundedWorldPositionInput& out) override;
    creatures1::objects::Object& object_for_creature(
        creatures1::creatures::Creature& creature) const override;
    void move_to_and_redraw(creatures1::objects::Object& object, int world_x,
                            int world_y) override;
    void update_pointer_tool_unbounded_position_and_redraw() override;

    // ObjectImmediateEventQueueHost.
    void queue_immediate_event(creatures1::objects::Object& source,
                               creatures1::objects::Object& target,
                               creatures1::objects::ObjectEventId event_id,
                               std::uint32_t argument) override;

    // CreatureScriptEventHost.
    void dispatch_script_event(creatures1::creatures::Creature& creature,
                               creatures1::objects::Object* source,
                               std::uint32_t event_id,
                               std::uint32_t argument) override;

private:
    C1WindowsDocument& document_;
    WindowsSimpleObjectInteractionHost interaction_;
    WindowsCreatureScriptEventHost events_;
};

// The per-tick Creature update (Object vtable slot 41, native's real
// CCreature::Tick body) -- SFCDoc::UpdateWorld's generic non-scenery Tick
// walk (tick_non_scenery_object below) had no case for a Creature at all,
// so this was never called: gait/pose animation, eye-blink randomization,
// motion-link tracking, age-tick advancement, unbounded-position handling,
// and the sleep-indicator bubble's positioning/events all silently never
// ran, for every creature, every tick.  Also drives Object::update_sound
// (Creature::update's first line), so this is the same reason creature
// sound never played.
class WindowsCreatureUpdateHost final
    : public creatures1::creatures::CreatureUpdateHost {
public:
    explicit WindowsCreatureUpdateHost(C1WindowsDocument& document)
        : document_(document) {}

    // CreatureUnboundedWorldPositionHost.
    bool read_view_input_and_clear_pending_flag(
        creatures1::creatures::UnboundedWorldPositionInput& out) override;
    creatures1::objects::Object& object_for_creature(
        creatures1::creatures::Creature& creature) const override;
    void move_to_and_redraw(creatures1::objects::Object& object, int world_x,
                            int world_y) override;
    void update_pointer_tool_unbounded_position_and_redraw() override;

    // ObjectImmediateEventQueueHost.
    void queue_immediate_event(creatures1::objects::Object& source,
                               creatures1::objects::Object& target,
                               creatures1::objects::ObjectEventId event_id,
                               std::uint32_t argument) override;

    // CreaturePoseAnimationHost.
    int render_plane(const creatures1::objects::Object& object) const override;
    void queue_dirty_world_rect(
        const creatures1::world::WorldRect& bounds) override;

    // CreatureUpdateHost's own.
    void dispatch_sleep_indicator_event(
        creatures1::objects::Object& indicator,
        creatures1::objects::ObjectEventId event_id,
        creatures1::objects::Object* source, std::uint32_t argument) override;
    void initialize_sleep_indicator(
        creatures1::objects::Object& indicator) override;

private:
    C1WindowsDocument& document_;
};

// CreatureHeardWordsHost and CreatureWordLearningHost serve queued events 6
// and 7.  Both only have to resolve the source object's word storage; the
// tokenizer, learned-word update and speech policy are Creature-owned.
class WindowsCreatureWordsHost final
    : public creatures1::creatures::Creature::CreatureHeardWordsHost,
      public creatures1::creatures::Creature::CreatureWordLearningHost,
      public creatures1::creatures::PointerAttentionApi {
public:
    WindowsCreatureWordsHost(C1WindowsDocument& document,
                             creatures1::creatures::Creature& creature)
        : document_(document), creature_(creature), phrase_(document),
          goal_direction_(document) {}

    // CreatureHeardWordsHost.
    char* mutable_words_for_event(
        const creatures1::objects::QueuedObjectEvent& event) override;
    bool is_pointer_tool(
        const creatures1::objects::Object& object) const override;
    creatures1::creatures::PointerAttentionApi& pointer_attention_api()
        override;
    creatures1::creatures::Creature::CreatureSpeechPhraseHost&
    speech_phrase_host() override;
    creatures1::creatures::CreatureGoalDirectionHost& goal_direction_host()
        override;

    // CreatureWordLearningHost.
    bool resolve_word_learning_event(
        const creatures1::objects::QueuedObjectEvent& event, char*& heard_word,
        std::size_t& record_index) const override;

    // PointerAttentionApi.
    int creature_sound_source_x() const override;
    int pointer_tool_sound_source_x() const override;
    creatures1::world::WorldRect creature_movement_bounds() const override;
    std::size_t non_scenery_object_count() const override;
    bool read_non_scenery_object(
        std::size_t index,
        creatures1::creatures::PointerAttentionCandidate& out) const override;
    creatures1::creatures::AttentionClassifier pointer_tool_classifier()
        const override;
    void clear_pointer_tool_lobe_neuron(
        std::uint32_t neuron_index) override;
    void report_registry_bounds_failure() override;

private:
    C1WindowsDocument& document_;
    creatures1::creatures::Creature& creature_;
    WindowsCreatureSpeechPhraseHost phrase_;
    WindowsCreatureBacteriumEnvironmentHost goal_direction_;
};

class WindowsObjectEventRuntime final
    : public creatures1::objects::ObjectEventRuntime {
public:
    explicit WindowsObjectEventRuntime(C1WindowsDocument& document)
        : document_(document) {}

    bool object_is_tick_enabled(
        const creatures1::objects::Object& object) const override;

    void handle_event_0(
        creatures1::objects::Object& target,
        const creatures1::objects::QueuedObjectEvent& event) override;
    void handle_event_1(
        creatures1::objects::Object& target,
        const creatures1::objects::QueuedObjectEvent& event) override;
    void handle_event_2(
        creatures1::objects::Object& target,
        const creatures1::objects::QueuedObjectEvent& event) override;
    void handle_event_3(
        creatures1::objects::Object& target,
        const creatures1::objects::QueuedObjectEvent& event) override;
    void handle_event_4(
        creatures1::objects::Object& target,
        const creatures1::objects::QueuedObjectEvent& event) override;
    void handle_event_5(
        creatures1::objects::Object& target,
        const creatures1::objects::QueuedObjectEvent& event) override;
    void handle_event_6(
        creatures1::objects::Object& target,
        const creatures1::objects::QueuedObjectEvent& event) override;
    void handle_event_7(
        creatures1::objects::Object& target,
        const creatures1::objects::QueuedObjectEvent& event) override;
    void handle_event_8(creatures1::objects::Object& target) override;
    void handle_event_9(creatures1::objects::Object& target) override;

    bool creature_is_tick_enabled(
        const creatures1::creatures::Creature& creature) const override;
    void apply_stimulus(
        creatures1::creatures::Creature& target,
        creatures1::objects::Object* source_object,
        creatures1::creatures::Creature& source_creature,
        const creatures1::creatures::StimulusDescriptor& descriptor,
        const creatures1::creatures::StimulusChemicalIds& chemical_ids,
        const creatures1::creatures::StimulusChemicalAmounts& chemical_amounts,
        std::uint32_t magnitude) override;

private:
    // Native dispatch is a vtable slot, so the most-derived override wins.
    // These resolve the target the same way, in the same order.
    creatures1::creatures::Creature* as_creature(
        creatures1::objects::Object& target) const;

    C1WindowsDocument& document_;
};

} // namespace creatures1::platform
