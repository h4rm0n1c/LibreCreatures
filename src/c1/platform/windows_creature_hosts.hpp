#pragma once

#include "windows_prelude.hpp"

#include "../application/application.hpp"
#include "../application/main_frame.hpp"
#include "../creatures/creature.hpp"
#include "../creatures/events.hpp"
#include "../creatures/update.hpp"
#include "../world/tick.hpp"
#include "../creatures/skeleton.hpp"

namespace creatures1::platform {

class C1WindowsDocument;

// Concrete CreatureDeathHost.  Creature::die owns the alive/dead guard and the
// whole death sequence; every operation here is an existing document service,
// so this is the composition seam rather than new policy.
class WindowsCreatureDeathHost final
    : public creatures1::creatures::CreatureDeathHost {
public:
    explicit WindowsCreatureDeathHost(C1WindowsDocument& document)
        : document_(document) {}

    void release_sleep_indicator(
        creatures1::creatures::Creature& creature) override;
    void notify_dependents_on_death(
        creatures1::creatures::Creature& creature) override;
    void purge_destroy_when_finished_macros(
        creatures1::creatures::Creature& creature) override;
    void log_death_message(
        const creatures1::creatures::Creature& creature) override;
    void dispatch_death_event(
        creatures1::creatures::Creature& creature) override;
    void rebuild_creature_selection_menu() override;
    bool is_selected_creature(
        const creatures1::creatures::Creature& creature) const override;
    void persist_and_close_selected_eye_view() override;
    void broadcast_embedded_control_state() override;
    void log_goal_drive_table(
        const creatures1::creatures::Creature& creature) override;

private:
    C1WindowsDocument& document_;
};

// Concrete SkeletonRenderPlaneHost: the creature registry plus the process
// random source the native plane selection uses.
class WindowsSkeletonRenderPlaneHost final
    : public creatures1::creatures::SkeletonRenderPlaneHost {
public:
    explicit WindowsSkeletonRenderPlaneHost(C1WindowsDocument& document)
        : document_(document) {}

    std::uint32_t next_random_value() override;
    std::size_t creature_count() const override;
    creatures1::objects::Object* creature_at(std::size_t index) const override;

private:
    C1WindowsDocument& document_;
};

// Concrete SkeletonLifetimeHost.  Every operation is an existing service:
// the renderable set, sound mixer, gallery registry and world object registry
// all own their storage already; this only supplies the release calls.
class WindowsSkeletonLifetimeHost final
    : public creatures1::creatures::SkeletonLifetimeHost {
public:
    explicit WindowsSkeletonLifetimeHost(C1WindowsDocument& document)
        : document_(document) {}

    void destroy_limb(creatures1::creatures::LimbPart& limb) override;
    void stop_continuous_sound(int sound_handle) override;
    void remove_from_renderable_set(
        creatures1::creatures::Skeleton& skeleton) override;
    void unregister_from_object_registry(
        creatures1::creatures::Skeleton& skeleton) override;
    void release_gallery(creatures1::display::Gallery& gallery) override;

private:
    C1WindowsDocument& document_;
};

// Concrete CreatureEnvironmentHost.  The world sample comes from MapData's
// recovered room/ambient policy; death delegates to the death host above.
class WindowsCreatureEnvironmentHost final
    : public creatures1::creatures::CreatureEnvironmentHost {
public:
    explicit WindowsCreatureEnvironmentHost(C1WindowsDocument& document)
        : document_(document), death_host_(document) {}

    creatures1::creatures::CreatureEnvironmentSample sample_at(
        int world_x, int world_y) const override;
    int sound_source_x(
        const creatures1::objects::Object& object) const override;
    bool is_selected_creature(
        const creatures1::creatures::Creature& creature) const override;
    void initialize_from_genome(
        creatures1::creatures::Creature& creature) override;
    void die(creatures1::creatures::Creature& creature) override;

private:
    C1WindowsDocument& document_;
    WindowsCreatureDeathHost death_host_;
};

// Concrete CreatureSpeechHost.  Recovered Creature::Speak @ 0040b5f0: when
// burble is enabled and the Voice prepares the phrase, drain every generated
// sound pair through the creature's object; then create the speech bubble
// with the native lifetime of 0x14 regardless of whether voice ran.
class WindowsCreatureSpeechHost final
    : public creatures1::creatures::Creature::CreatureSpeechHost {
public:
    explicit WindowsCreatureSpeechHost(C1WindowsDocument& document)
        : document_(document) {}

    void speak(creatures1::creatures::Creature& creature,
               std::string_view phrase) override;

private:
    C1WindowsDocument& document_;
};

// Concrete MainFrameAgeCommandPlatform for Testing > Instant verb vocabulary.
class WindowsAgeCommandPlatform final
    : public creatures1::application::MainFrameAgeCommandPlatform {
public:
    explicit WindowsAgeCommandPlatform(C1WindowsDocument& document)
        : document_(document), environment_(document), speech_(document) {}

    creatures1::creatures::Creature* selected_creature() override;
    creatures1::creatures::CreatureEnvironmentHost& creature_environment()
        override;
    const creatures1::creatures::MultibyteTextApi& text_api() const override;
    creatures1::creatures::Creature::CreatureSpeechHost& speech_host()
        override;

private:
    C1WindowsDocument& document_;
    WindowsCreatureEnvironmentHost environment_;
    WindowsCreatureSpeechHost speech_;
};

// Concrete BacteriumRandomSource: the process CRT random source the native
// infection path uses.
class WindowsBacteriumRandomSource final
    : public creatures1::creatures::BacteriumRandomSource {
public:
    std::uint32_t next() override;
    bool debug_logging_enabled() const override;
    void log_replication(
        const creatures1::creatures::Bacterium& bacterium) override;
};

// Concrete InfectSelectedCreatureHost for the Testing menu command.
class WindowsInfectCreatureHost final
    : public creatures1::application::InfectSelectedCreatureHost {
public:
    explicit WindowsInfectCreatureHost(C1WindowsDocument& document)
        : document_(document) {}

    creatures1::creatures::Creature* selected_creature() const override;
    creatures1::creatures::BacteriumRandomSource& random_source() override;
    creatures1::creatures::Bacterium& world_bacterium_at(
        std::size_t index) override;
    bool debug_console_visible() const override;
    void log_infection() override;

private:
    C1WindowsDocument& document_;
    WindowsBacteriumRandomSource random_;
};

// Concrete ForceAgeSelectedCreatureHost for the Testing menu command.
class WindowsForceAgeHost final
    : public creatures1::application::ForceAgeSelectedCreatureHost {
public:
    explicit WindowsForceAgeHost(C1WindowsDocument& document)
        : document_(document), environment_(document) {}

    creatures1::creatures::Creature* selected_creature() const override;
    creatures1::creatures::CreatureEnvironmentHost& environment() override;
    creatures1::common::DebugLogHost* debug_log_host() override;

private:
    C1WindowsDocument& document_;
    WindowsCreatureEnvironmentHost environment_;
};

// Concrete EuthanasiaHost.  The confirmation is a Windows message box; the
// kill itself is the recovered Creature::die policy through the death host.
class WindowsEuthanasiaHost final
    : public creatures1::application::EuthanasiaHost {
public:
    explicit WindowsEuthanasiaHost(C1WindowsDocument& document)
        : document_(document), death_host_(document) {}

    bool confirm_euthanasia() override;
    bool selected_creature_exists() const override;
    void kill_selected_creature() override;

private:
    C1WindowsDocument& document_;
    WindowsCreatureDeathHost death_host_;
};

// Shared speech-bubble construction.  Creature::Speak @ 0040b5f0 and
// SpeakTextWithVoice @ 0040b560 both reach SimpleObject::CreateBubble with
// placement mode 0; only the lifetime differs.
void create_speech_bubble(C1WindowsDocument& document,
                          creatures1::objects::Object& speaker,
                          std::string_view text,
                          std::uint8_t lifetime_ticks);

// Concrete CreatureEventFanoutHost and CreaturePerceptionHost.  The five
// fan-out free functions in creatures/events.cpp and Creature's own
// perception scan are the recovered policy; everything this host supplies is
// an existing document/registry service, so the two interfaces are carried
// together rather than split into two adapters that would each need the same
// registry and object-identity lookups.
class WindowsCreatureFanoutHost final
    : public creatures1::creatures::CreatureEventFanoutHost,
      public creatures1::creatures::CreaturePerceptionHost {
public:
    explicit WindowsCreatureFanoutHost(C1WindowsDocument& document)
        : document_(document) {}

    // --- CreatureEventFanoutHost ---
    std::size_t creature_count() const override;
    creatures1::creatures::Creature* creature_at(
        std::size_t index) const override;
    void report_invalid_creature_index() const override;

    int source_sound_source_x(
        const creatures1::objects::Object& source) const override;
    int source_sound_source_y(
        const creatures1::objects::Object& source) const override;
    int down_foot_x(
        const creatures1::creatures::Creature& creature) const override;
    int down_foot_y(
        const creatures1::creatures::Creature& creature) const override;
    bool is_same_object(
        const creatures1::creatures::Creature& creature,
        const creatures1::objects::Object& object) const override;
    bool can_perceive(
        const creatures1::creatures::Creature& creature,
        const creatures1::objects::Object& source) const override;
    bool source_is_vehicle(
        const creatures1::objects::Object& source) const override;
    bool creature_is_bound_to_source(
        const creatures1::creatures::Creature& creature,
        const creatures1::objects::Object& source) const override;
    bool is_creature_classifier(
        const creatures1::creatures::Creature& creature) const override;
    creatures1::objects::Object& object_for_creature(
        creatures1::creatures::Creature& creature) const override;

    void queue_object_event(creatures1::objects::Object& source,
                            creatures1::objects::Object& target,
                            creatures1::objects::ObjectEventId event_id,
                            std::uint32_t argument,
                            std::int32_t delay_world_ticks) override;

    bool copy_built_in_stimulus(
        const creatures1::creatures::Creature& creature,
        std::int32_t stimulus_index,
        creatures1::objects::Object& source,
        creatures1::objects::QueuedCreatureStimulus& out) const override;
    void queue_creature_stimulus(
        const creatures1::objects::QueuedCreatureStimulus& stimulus) override;
    void report_invalid_stimulus_index(
        std::int32_t stimulus_index) const override;

    // --- CreaturePerceptionHost ---
    std::size_t non_scenery_object_count() const override;
    creatures1::objects::Object* non_scenery_object_at(
        std::size_t index) const override;
    creatures1::creatures::CreatureMotionLinkFacts motion_link_facts(
        const creatures1::objects::Object& object,
        const creatures1::creatures::Creature& creature) const override;
    bool is_this_creature(
        const creatures1::objects::Object& target,
        const creatures1::creatures::Creature& creature) const override;
    bool has_bounds_flag(const creatures1::objects::Object& target,
                         std::uint8_t flag) const override;
    bool read_bounds(const creatures1::objects::Object& target,
                     creatures1::world::WorldRect& out_bounds) const override;
    bool is_map_room_reference(
        const creatures1::objects::Object& reference) const override;
    void nearest_map_room_bounds(
        int world_x, int world_y,
        creatures1::world::WorldRect& out_bounds) const override;

private:
    C1WindowsDocument& document_;
};

// Concrete CreatureSpeechPhraseHost.  SpeakTextWithVoice returns the Voice's
// accumulated delay, which both the bubble lifetime and the speech-range
// event scheduling read; the fan-out host supplies the latter.
class WindowsCreatureSpeechPhraseHost final
    : public creatures1::creatures::Creature::CreatureSpeechPhraseHost {
public:
    explicit WindowsCreatureSpeechPhraseHost(C1WindowsDocument& document)
        : document_(document), fanout_(document) {}

    creatures1::objects::Object& object_for_creature(
        creatures1::creatures::Creature& creature) const override;
    std::int32_t attention_record_index(
        const creatures1::objects::Object& object) const override;
    void speak_text_with_voice(
        creatures1::creatures::Creature& creature, std::string_view phrase,
        std::int32_t& out_delay_world_ticks) override;
    void queue_speech_event(creatures1::objects::Object& source,
                            std::uint32_t event_argument,
                            std::int32_t delay_world_ticks) override;

private:
    C1WindowsDocument& document_;
    WindowsCreatureFanoutHost fanout_;
};

// Concrete BacteriumServiceHost: world tick phase 15 runs
// AdvanceBacteriumServicePhase @ 0x00433310, an eight-phase sub-schedule whose
// phase 1 probabilistically infects one creature from the map's bacteria and
// whose phase 7 updates every tick-enabled creature's bacterium.  Every
// service below is an existing document or map one.
class WindowsBacteriumServiceHost final
    : public creatures1::world::BacteriumServiceHost {
public:
    explicit WindowsBacteriumServiceHost(C1WindowsDocument& document)
        : document_(document) {}

    creatures1::world::BacteriumServicePhase& service_phase() override;
    std::size_t creature_count() const override;
    std::size_t selected_creature_count() const override;
    int smoothed_idle_cycle_index() const override;
    std::uint32_t next_random() override;
    creatures1::creatures::BacteriumRandomSource& bacterium_random_source()
        override;
    bool creature_tick_enabled(std::size_t index) const override;
    creatures1::creatures::Bacterium& creature_bacterium(
        std::size_t index) override;
    creatures1::creatures::Bacterium& environment_bacterium(
        std::size_t index) override;
    void update_creature_bacterium_and_environment(
        std::size_t index) override;
    void log_environment_infection() override;

private:
    C1WindowsDocument& document_;
    WindowsBacteriumRandomSource random_;
};

// Concrete CreatureWorldUpdateHost.  World tick phases 1/5/9/13 run
// UpdateAllCreatureBrainInputs @ 00432ee0 and phases 2/10 run
// UpdateAllCreaturePerceptionAndAttention @ 00433020; both recovered passes
// live in creatures/update.cpp and needed only this boundary.  The perception
// pass reaches Creature::update_perception, which is why this host owns a
// fan-out host (the CreaturePerceptionHost half) and an attention host.
class WindowsCreatureWorldUpdateHost final
    : public creatures1::creatures::CreatureWorldUpdateHost {
public:
    explicit WindowsCreatureWorldUpdateHost(C1WindowsDocument& document)
        : document_(document), perception_(document) {}

    std::size_t creature_count() const override;
    creatures1::creatures::Creature* creature_at(
        std::size_t index) const override;
    bool creature_is_tick_enabled(
        const creatures1::creatures::Creature& creature) const override;
    bool creature_is_alive(
        const creatures1::creatures::Creature& creature) const override;

    creatures1::objects::Object* motion_link(
        const creatures1::creatures::Creature& creature) const override;
    int motion_target_x(
        const creatures1::creatures::Creature& creature) const override;
    int sound_source_x(
        const creatures1::creatures::Creature& creature) const override;
    bool object_has_interaction_event(
        const creatures1::objects::Object& object) const override;

    bool boundary_correction_pending(
        const creatures1::creatures::Creature& creature) const override;
    void clear_boundary_correction(
        creatures1::creatures::Creature& creature) override;
    creatures1::objects::Object* bounds_reference(
        const creatures1::creatures::Creature& creature) const override;
    void trigger_built_in_stimulus(creatures1::creatures::Creature& creature,
                                   std::uint32_t stimulus_id,
                                   std::uint32_t argument) override;

    void update_perception(
        creatures1::creatures::Creature& creature) override;
    void update_attention(creatures1::creatures::Creature& creature) override;

    creatures1::world::WorldRect movement_bounds(
        const creatures1::creatures::Creature& creature) const override;
    creatures1::world::WorldRect movement_bounds(
        const creatures1::objects::Object& object) const override;
    bool reference_uses_own_attention_bounds(
        const creatures1::objects::Object& object) const override;
    void object_part_center(const creatures1::objects::Object& object,
                            int part_index, int& out_x,
                            int& out_y) const override;

    bool should_log_attention_loss(
        const creatures1::creatures::Creature& creature) const override;
    void log_attention_target_out_of_reach(
        const creatures1::creatures::Creature& creature,
        const creatures1::objects::Object& target) override;
    void log_attention_target_inaccessible(
        const creatures1::creatures::Creature& creature,
        const creatures1::objects::Object& target) override;

private:
    C1WindowsDocument& document_;
    WindowsCreatureFanoutHost perception_;
};

// Concrete DriveThresholdObject.  World tick phase 7 walks the non-scenery
// registry and calls vtable slot 42 on every tick-enabled entry.  Only
// Creature overrides that slot, so this adapter resolves the entry back to
// its Creature and is inert for every other object -- which is exactly what
// the base Object implementation does.
class WindowsDriveThresholdObject final
    : public creatures1::creatures::DriveThresholdObject {
public:
    WindowsDriveThresholdObject(C1WindowsDocument& document,
                                creatures1::objects::Object* object)
        : document_(document), object_(object) {}

    bool tick_enabled() const override;
    void update_drive_threshold_state() override;

private:
    C1WindowsDocument& document_;
    creatures1::objects::Object* object_ = nullptr;
};

// Runs world tick phase 7 over the current non-scenery registry.
void run_creature_drive_threshold_phase(C1WindowsDocument& document);

} // namespace creatures1::platform
