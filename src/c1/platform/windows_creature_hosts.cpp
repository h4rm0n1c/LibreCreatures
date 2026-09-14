#include "windows_creature_hosts.hpp"

#include "../objects/object.hpp"
#include "../objects/debug.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include <cstdlib>

#include "../world/map.hpp"
#include "mfc_creature_archives.hpp"
#include "../world/runtime.hpp"
#include "windows_macro_host.hpp"
#include "windows_shell.hpp"

namespace creatures1::platform {

void WindowsCreatureDeathHost::release_sleep_indicator(
    creatures1::creatures::Creature& creature) {
    // Creature::Die @ 0040dc30 releases the indicator with the same three
    // steps as the disable branch of SetSleepIndicator: dispatch event 0,
    // reinitialise the object, clear the stored identity.  Creature already
    // owns that sequence, so this drives it rather than repeating it.
    //
    // The earlier note here said the port never constructs a sleep indicator.
    // That stopped being true once create_sleep_indicator was bound.
    WindowsCreatureAttentionHost attention(document_);
    creature.set_sleep_indicator(false, attention);
}

void WindowsCreatureDeathHost::notify_dependents_on_death(
    creatures1::creatures::Creature& creature) {
    // Every other world object drops its references to the dying creature's
    // object identity, matching the native deletion sweep.
    // NotifyDependentsOnRemoval @ 0x0040... walks
    // g_non_scenery_object_registry, which is the collection
    // non_scenery_object_at indexes; world_object_count counts the larger
    // all-objects list and would truncate or overrun the sweep.
    creatures1::objects::Object& dying = document_.object_for_creature(creature);
    const std::size_t count = document_.non_scenery_object_count();
    for (std::size_t index = 0; index < count; ++index) {
        creatures1::objects::Object* other =
            document_.non_scenery_object_at(index);
        if (other != nullptr && other != &dying) {
            other->clear_references_to(&dying);
        }
    }
}

void WindowsCreatureDeathHost::purge_destroy_when_finished_macros(
    creatures1::creatures::Creature& creature) {
    document_.purge_destroy_when_finished_macros(
        document_.object_for_creature(creature));
}

void WindowsCreatureDeathHost::log_death_message(
    const creatures1::creatures::Creature& /*creature*/) {
    OutputDebugStringA("Creature died\n");
}

void WindowsCreatureDeathHost::dispatch_death_event(
    creatures1::creatures::Creature& creature) {
    // Creature::Die @ 0040dc30 dispatches script event 0x48 on the creature's
    // own object, with itself as the from-object.  The id has no name in
    // events.hpp, but the value is recovered evidence, not a guess.
    constexpr std::uint32_t kCreatureDeathEvent = 0x48;
    creatures1::objects::Object& object =
        document_.object_for_creature(creature);
    WindowsObjectScriptDispatchHost scripts(document_);
    object.dispatch_script_event(
        static_cast<creatures1::objects::ObjectEventId>(kCreatureDeathEvent),
        &object, false, scripts);
}

void WindowsCreatureDeathHost::rebuild_creature_selection_menu() {
    document_.rebuild_creature_selection_menu();
}

bool WindowsCreatureDeathHost::is_selected_creature(
    const creatures1::creatures::Creature& creature) const {
    return document_.selected_creature() == &creature;
}

void WindowsCreatureDeathHost::persist_and_close_selected_eye_view() {
    document_.persist_and_close_eye_view();
}

void WindowsCreatureDeathHost::broadcast_embedded_control_state() {
    // The recovered death path broadcasts state 8, the same code the object
    // lifecycle sweep uses.
    document_.broadcast_embedded_control_state(8);
}

void WindowsCreatureDeathHost::log_goal_drive_table(
    const creatures1::creatures::Creature& /*creature*/) {
    OutputDebugStringA("Goal drive table\n");
}

std::uint32_t WindowsSkeletonRenderPlaneHost::next_random_value() {
    return static_cast<std::uint32_t>(rand());
}

std::size_t WindowsSkeletonRenderPlaneHost::creature_count() const {
    return document_.creature_count();
}

creatures1::objects::Object* WindowsSkeletonRenderPlaneHost::creature_at(
    std::size_t index) const {
    // CreatureSelectionEntry is the narrow view of the real Creature, so the
    // registry entry is the creature itself.
    auto* creature = static_cast<creatures1::creatures::Creature*>(
        document_.creature_at(index));
    return creature == nullptr ? nullptr
                               : &document_.object_for_creature(*creature);
}

void WindowsSkeletonLifetimeHost::destroy_limb(
    creatures1::creatures::LimbPart& limb) {
    // Limbs are built as unique_ptr and released into the raw chain pointers,
    // so the lifetime host owns the matching delete.
    delete &limb;
}

void WindowsSkeletonLifetimeHost::stop_continuous_sound(int sound_handle) {
    if (document_.sound_manager_available()) {
        document_.sound_manager().stop_continuous_sound(
            static_cast<std::uint32_t>(sound_handle), false);
    }
}

void WindowsSkeletonLifetimeHost::remove_from_renderable_set(
    creatures1::creatures::Skeleton& skeleton) {
    document_.renderables().erase(skeleton);
}

void WindowsSkeletonLifetimeHost::unregister_from_object_registry(
    creatures1::creatures::Skeleton& skeleton) {
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return;
    }
    // This was searching world_objects_ (world_object_at/world_object_count)
    // but then deleting by that same index from an entirely unrelated
    // container (WorldRuntime::remove_at erases from creatures_, not
    // world_objects_) -- so a Creature's Skeleton was never actually
    // removed from EITHER the object registry that totl/enum iterate
    // (objects_) or world_objects_. Every Creature that died left a
    // permanent dangling Object* in objects_, so totl/enum family=4
    // (creature) counts read freed memory ever after. Mirror
    // Object::unregister_from_non_scenery_object_registry's own, already-
    // correct pattern for objects_, then separately clean up
    // world_objects_ via its real removal API.
    const std::size_t count = runtime->object_count();
    for (std::size_t index = 0; index < count; ++index) {
        if (runtime->object_at(index) == &skeleton) {
            runtime->remove_object_at(index);
            break;
        }
    }
    runtime->remove_world_object(skeleton);
}

void WindowsSkeletonLifetimeHost::release_gallery(
    creatures1::display::Gallery& gallery) {
    if (creatures1::world::WorldRuntime* runtime = document_.world_runtime()) {
        runtime->release_gallery(gallery);
    }
}

creatures1::creatures::CreatureEnvironmentSample
WindowsCreatureEnvironmentHost::sample_at(int world_x, int world_y) const {
    creatures1::creatures::CreatureEnvironmentSample sample{};
    creatures1::world::WorldRuntime* runtime = document_.world_runtime();
    if (runtime == nullptr) {
        return sample;
    }
    creatures1::world::MapData& map = runtime->map_data();
    const creatures1::world::MapRoomTable rooms{
        &map.room_at(0), static_cast<std::size_t>(map.room_count())};
    const creatures1::world::AmbientEnvironmentTable ambient{
        creatures1::world::kAmbientEnvironmentRecords.data(),
        creatures1::world::kAmbientEnvironmentRecords.size()};
    sample.temperature_delta_raw =
        creatures1::world::get_ambient_temperature_at_point(
            rooms, map.ambient_environment_index_value(), ambient, world_x,
            world_y, document_);
    return sample;
}

int WindowsCreatureEnvironmentHost::sound_source_x(
    const creatures1::objects::Object& object) const {
    return object.sound_source_x();
}

bool WindowsCreatureEnvironmentHost::is_selected_creature(
    const creatures1::creatures::Creature& creature) const {
    return document_.selected_creature() == &creature;
}

void WindowsCreatureEnvironmentHost::initialize_from_genome(
    creatures1::creatures::Creature& creature) {
    // Every member of GenomeInitializationHost now has an owner: the document
    // supplies the genome and voice stores, the sound host and the entity
    // registry; the skeleton services aggregate is assembled from the resource
    // host it already builds; and the render-plane and biochemistry locus
    // hosts are constructed here for the call.  Skeleton lifetime operations
    // go through the document, whose lifetime covers the creature.
    class Host final
        : public creatures1::creatures::Creature::GenomeInitializationHost {
    public:
        Host(C1WindowsDocument& document,
             creatures1::biochemistry::Biochemistry& biochemistry)
            : document_(document),
              render_plane_(document),
              locus_(biochemistry),
              services_(document.skeleton_services(document)) {}

        creatures1::creatures::GenomeFileStore& genome_files() override {
            return document_.genome_files();
        }
        const creatures1::creatures::SkeletonSpriteBuildServices&
        skeleton_services() const override {
            return services_;
        }
        creatures1::creatures::SkeletonRenderPlaneHost& render_plane_host()
            override {
            return render_plane_;
        }
        creatures1::objects::ObjectSoundPlaybackHost& sound_host() override {
            return document_;
        }
        creatures1::biochemistry::BiochemistryLocusHost&
        biochemistry_locus_host(
            creatures1::biochemistry::Biochemistry& /*value*/) override {
            // Already bound to this creature's biochemistry at construction.
            return locus_;
        }
        creatures1::creatures::VoiceFileStore& voice_files() override {
            return document_.voice_files();
        }

    private:
        C1WindowsDocument& document_;
        WindowsSkeletonRenderPlaneHost render_plane_;
        MfcBiochemistryLocusHost locus_;
        creatures1::creatures::SkeletonSpriteBuildServices services_;
    } ;

    creatures1::biochemistry::Biochemistry* biochemistry =
        creature.biochemistry();
    if (biochemistry == nullptr) {
        return;
    }
    Host host(document_, *biochemistry);

    creature.initialize_from_genome(host);
}

void WindowsCreatureEnvironmentHost::die(
    creatures1::creatures::Creature& creature) {
    creature.die(death_host_);
}

void WindowsCreatureSpeechHost::speak(
    creatures1::creatures::Creature& creature, std::string_view phrase) {
    creatures1::objects::Object& speaker =
        document_.object_for_creature(creature);

    // Voice is conditional; the bubble is not.
    if (document_.burble_is_enabled() &&
        creature.voice().prepare_speech_text(phrase)) {
        creatures1::sound::SoundId sound_id{};
        creatures1::creatures::VoiceDelayTicks delay_ticks{};
        while (creature.voice().get_next_sound_pair(sound_id, delay_ticks)) {
            speaker.play_sound_effect(sound_id,
                                      static_cast<int>(delay_ticks), true,
                                      document_);
        }
    }

    // The port models a Creature's object identity as its Skeleton, which is
    // an Object rather than a SimpleObject, so the bubble is constructed
    // directly with the speaker as its anchor instead of through
    // SimpleObject::create_bubble.  The placement decision is the same one
    // that member makes.
    create_speech_bubble(document_, speaker, phrase, 0x14);
}

void create_speech_bubble(C1WindowsDocument& document,
                          creatures1::objects::Object& speaker,
                          std::string_view text,
                          std::uint8_t lifetime_ticks) {
    const int viewport_center =
        document.viewport_left() +
        (document.viewport_right() - document.viewport_left()) / 2;
    const bool place_on_right = speaker.sound_source_x() <= viewport_center;

    // Creature::Speak @ 0040b5f0 and SpeakTextWithVoice @ 0040b560 both pass
    // placement mode 0 to SimpleObject::CreateBubble, not the centred mode.
    auto bubble = std::unique_ptr<creatures1::objects::Bubble>(
        new (std::nothrow) creatures1::objects::Bubble(
            &speaker, lifetime_ticks, text,
            creatures1::objects::BubblePlacementMode::speech,
            place_on_right, document.bubble_construction()));
    if (bubble != nullptr) {
        document.adopt_speech_bubble(std::move(bubble));
    }
}

creatures1::creatures::Creature*
WindowsAgeCommandPlatform::selected_creature() {
    return document_.selected_creature();
}

creatures1::creatures::CreatureEnvironmentHost&
WindowsAgeCommandPlatform::creature_environment() {
    return environment_;
}

const creatures1::creatures::MultibyteTextApi&
WindowsAgeCommandPlatform::text_api() const {
    return document_;
}

creatures1::creatures::Creature::CreatureSpeechHost&
WindowsAgeCommandPlatform::speech_host() {
    return speech_;
}

std::uint32_t WindowsBacteriumRandomSource::next() {
    return static_cast<std::uint32_t>(rand());
}

bool WindowsBacteriumRandomSource::debug_logging_enabled() const {
    // The native tests g_debug_console_dialog before formatting anything.
    return active_debug_console() != nullptr;
}

void WindowsBacteriumRandomSource::log_replication(
    const creatures1::creatures::Bacterium& bacterium) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console == nullptr) {
        return;
    }
    // ReplicateAndMutate @ 00401c70 emits this under category 0x400, gated on
    // the console AND the global logging flag.  Ghidra did not recover the
    // three argument expressions; Antigen/Virulence/Resistance map onto the
    // only three scalar fields the routine mutates, in declaration order.
    creatures1::common::debug_log(
        *console, 0x400,
        "Bacterium replicated: Antigen=%d Virulence=%d Resistance=%d\n",
        static_cast<int>(bacterium.input_chemical_id()),
        static_cast<int>(bacterium.kill_threshold()),
        static_cast<int>(bacterium.activation_threshold()));
}

creatures1::creatures::Creature*
WindowsInfectCreatureHost::selected_creature() const {
    return document_.selected_creature();
}

creatures1::creatures::BacteriumRandomSource&
WindowsInfectCreatureHost::random_source() {
    return random_;
}

creatures1::creatures::Bacterium& WindowsInfectCreatureHost::world_bacterium_at(
    std::size_t index) {
    // MapData owns the hundred world bacterium records the native indexes.
    return document_.world_runtime()->map_data().bacterium_at(index);
}

bool WindowsInfectCreatureHost::debug_console_visible() const {
    return active_debug_console() != nullptr;
}

void WindowsInfectCreatureHost::log_infection() {
    // InfectSelectedCreatureWithRandomBacterium @ 004348c0 emits exactly this
    // line under category 0x1000 once the console exists.
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x1000,
            "Subject has been infected by menu command.\n");
    }
}

creatures1::creatures::Creature* WindowsForceAgeHost::selected_creature() const {
    return document_.selected_creature();
}

creatures1::creatures::CreatureEnvironmentHost&
WindowsForceAgeHost::environment() {
    return environment_;
}

creatures1::common::DebugLogHost* WindowsForceAgeHost::debug_log_host() {
    // The port has no debug console yet, so the ageing path logs nothing.
    return nullptr;
}

bool WindowsEuthanasiaHost::confirm_euthanasia() {
    return AfxMessageBox("Are you sure you want to put this creature to sleep?",
                         MB_YESNO | MB_ICONQUESTION, 0) == IDYES;
}

bool WindowsEuthanasiaHost::selected_creature_exists() const {
    return document_.selected_creature() != nullptr;
}

void WindowsEuthanasiaHost::kill_selected_creature() {
    if (auto* creature = document_.selected_creature()) {
        creature->die(death_host_);
    }
}


// --- WindowsCreatureFanoutHost ---------------------------------------------

std::size_t WindowsCreatureFanoutHost::creature_count() const {
    return document_.creature_count();
}

creatures1::creatures::Creature* WindowsCreatureFanoutHost::creature_at(
    std::size_t index) const {
    return static_cast<creatures1::creatures::Creature*>(
        document_.creature_at(index));
}

void WindowsCreatureFanoutHost::report_invalid_creature_index() const {
    // The native registry walk answers a bad index with
    // AfxThrowInvalidArgException.  The recovered producers instead skip the
    // slot, so this keeps the condition visible rather than silent.
    OutputDebugStringA("Creature registry index out of range in event fan-out\n");
}

int WindowsCreatureFanoutHost::source_sound_source_x(
    const creatures1::objects::Object& source) const {
    return const_cast<creatures1::objects::Object&>(source).sound_source_x();
}

int WindowsCreatureFanoutHost::source_sound_source_y(
    const creatures1::objects::Object& source) const {
    return const_cast<creatures1::objects::Object&>(source).sound_source_y();
}

int WindowsCreatureFanoutHost::down_foot_x(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().down_foot_x;
}

int WindowsCreatureFanoutHost::down_foot_y(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().down_foot_y;
}

bool WindowsCreatureFanoutHost::is_same_object(
    const creatures1::creatures::Creature& creature,
    const creatures1::objects::Object& object) const {
    return &document_.object_for_creature(
               const_cast<creatures1::creatures::Creature&>(creature)) ==
           &object;
}

bool WindowsCreatureFanoutHost::can_perceive(
    const creatures1::creatures::Creature& creature,
    const creatures1::objects::Object& source) const {
    return creature.can_perceive(source, *this);
}

bool WindowsCreatureFanoutHost::source_is_vehicle(
    const creatures1::objects::Object& source) const {
    return source.has_bounds_flag(creatures1::objects::Object::kIsVehicle);
}

bool WindowsCreatureFanoutHost::creature_is_bound_to_source(
    const creatures1::creatures::Creature& creature,
    const creatures1::objects::Object& source) const {
    return creature.skeleton().bounds_reference_object() == &source;
}

bool WindowsCreatureFanoutHost::is_creature_classifier(
    const creatures1::creatures::Creature& creature) const {
    // Native tests the packed classifier's family byte, not the registry
    // membership, so a creature-family object that is not a Creature record
    // would still pass.  Keep that exact test.
    return ((creature.skeleton().classifier_base() >> 24) & 0xffu) == 4u;
}

creatures1::objects::Object& WindowsCreatureFanoutHost::object_for_creature(
    creatures1::creatures::Creature& creature) const {
    return document_.object_for_creature(creature);
}

void WindowsCreatureFanoutHost::queue_object_event(
    creatures1::objects::Object& source, creatures1::objects::Object& target,
    creatures1::objects::ObjectEventId event_id, std::uint32_t argument,
    std::int32_t delay_world_ticks) {
    document_.queue_object_event_with_delay(source, target, event_id, argument,
                                            delay_world_ticks);
}

bool WindowsCreatureFanoutHost::copy_built_in_stimulus(
    const creatures1::creatures::Creature& creature,
    std::int32_t stimulus_index, creatures1::objects::Object& source,
    creatures1::objects::QueuedCreatureStimulus& out) const {
    // QueueSignStimulusForPerceivingCreatures @ 00423390 writes source and
    // target into the creature's own built-in context and then copies that
    // record into the queue.  Every reader of the stored contexts overwrites
    // both fields before use (Creature::trigger_built_in_stimulus does the
    // same two writes), so the residual mutation is not load-bearing and the
    // copy is assembled here instead.
    const auto& contexts = creature.built_in_stimulus_contexts();
    if (stimulus_index < 0 ||
        static_cast<std::size_t>(stimulus_index) >= contexts.size()) {
        return false;
    }
    const creatures1::creatures::StimulusContext& context =
        contexts[static_cast<std::size_t>(stimulus_index)];
    out.source_object = &source;
    out.target_creature = const_cast<creatures1::creatures::Creature*>(&creature);
    out.descriptor = context.descriptor;
    out.chemical_ids = context.chemical_ids;
    out.chemical_amounts = context.chemical_amounts;
    return true;
}

void WindowsCreatureFanoutHost::queue_creature_stimulus(
    const creatures1::objects::QueuedCreatureStimulus& stimulus) {
    document_.queue_creature_stimulus(stimulus);
}

void WindowsCreatureFanoutHost::report_invalid_stimulus_index(
    std::int32_t stimulus_index) const {
    char message[96];
    std::snprintf(message, sizeof(message),
                  "A 'STM# SHOU' MACRO HAS SENT AN INVALID STIMULUS (%d)\n",
                  static_cast<int>(stimulus_index));
    OutputDebugStringA(message);
}

std::size_t WindowsCreatureFanoutHost::non_scenery_object_count() const {
    return document_.non_scenery_object_count();
}

creatures1::objects::Object* WindowsCreatureFanoutHost::non_scenery_object_at(
    std::size_t index) const {
    return document_.non_scenery_object_at(index);
}

creatures1::creatures::CreatureMotionLinkFacts
WindowsCreatureFanoutHost::motion_link_facts(
    const creatures1::objects::Object& object,
    const creatures1::creatures::Creature& creature) const {
    // UpdatePerception @ 0040e8b0 reads the link's SkeletonRenderPoseState
    // monikers at offsets 0/4/8 -- genome, mother, father -- which is only
    // meaningful when the link is a creature-family object.
    creatures1::creatures::CreatureMotionLinkFacts facts{};
    facts.is_creature = ((object.classifier_base() >> 24) & 0xffu) == 4u;
    if (!facts.is_creature) {
        return facts;
    }

    const creatures1::creatures::Creature* link =
        document_.creature_for_object(object);
    if (link == nullptr) {
        return facts;
    }

    const creatures1::creatures::Skeleton& link_skeleton = link->skeleton();
    const creatures1::creatures::Skeleton& self = creature.skeleton();

    facts.link_is_my_parent =
        link_skeleton.genome_source_filename == self.mother_moniker ||
        link_skeleton.genome_source_filename == self.father_moniker;
    facts.link_is_my_child =
        link_skeleton.mother_moniker == self.genome_source_filename ||
        link_skeleton.father_moniker == self.genome_source_filename;
    facts.shares_a_parent =
        link_skeleton.mother_moniker == self.mother_moniker ||
        link_skeleton.father_moniker == self.father_moniker;
    // The species test masks off the event and species bytes, leaving family
    // and genus; the sex comparison is the genome sex, not the display gender.
    facts.is_opposite_sex =
        ((object.classifier_base() ^ self.classifier_base()) & 0xffff0000u) ==
            0 &&
        link->genome_sex() != creature.genome_sex();
    return facts;
}

bool WindowsCreatureFanoutHost::is_this_creature(
    const creatures1::objects::Object& target,
    const creatures1::creatures::Creature& creature) const {
    return is_same_object(creature, target);
}

bool WindowsCreatureFanoutHost::has_bounds_flag(
    const creatures1::objects::Object& target, std::uint8_t flag) const {
    return target.has_bounds_flag(flag);
}

bool WindowsCreatureFanoutHost::read_bounds(
    const creatures1::objects::Object& target,
    creatures1::world::WorldRect& out_bounds) const {
    const_cast<creatures1::objects::Object&>(target).get_bounds(&out_bounds);
    return true;
}

bool WindowsCreatureFanoutHost::is_map_room_reference(
    const creatures1::objects::Object& reference) const {
    // CanPerceiveObject @ 00409750 tests the reference's packed classifier
    // family and genus, not a bounds flag: family 3 genus 2 is the map-room
    // marker object, and a creature anchored to it perceives nothing.
    return (reference.classifier_base() & 0xffff0000u) == 0x03020000u;
}

void WindowsCreatureFanoutHost::nearest_map_room_bounds(
    int world_x, int world_y,
    creatures1::world::WorldRect& out_bounds) const {
    document_.find_nearest_room_bounds_at_point(world_x, world_y, out_bounds);
}

// --- WindowsCreatureSpeechPhraseHost ---------------------------------------

creatures1::objects::Object&
WindowsCreatureSpeechPhraseHost::object_for_creature(
    creatures1::creatures::Creature& creature) const {
    return document_.object_for_creature(creature);
}

std::int32_t WindowsCreatureSpeechPhraseHost::attention_record_index(
    const creatures1::objects::Object& object) const {
    // GetAttentionRecordIndex @ 00426430 is a pure classifier mapping; the
    // recovered free function already carries it.
    WindowsStimulusSourceHost classifier(document_);
    return static_cast<std::int32_t>(
        creatures1::creatures::get_attention_record_index(
            classifier.classify(object)));
}

void WindowsCreatureSpeechPhraseHost::speak_text_with_voice(
    creatures1::creatures::Creature& creature, std::string_view phrase,
    std::int32_t& out_delay_world_ticks) {
    // SpeakTextWithVoice @ 0040b560 differs from Speak only in reporting the
    // Voice's accumulated delay and in deriving the bubble lifetime from it.
    creatures1::objects::Object& speaker =
        document_.object_for_creature(creature);
    out_delay_world_ticks = 0;

    if (document_.burble_is_enabled() &&
        creature.voice().prepare_speech_text(phrase)) {
        creatures1::sound::SoundId sound_id{};
        creatures1::creatures::VoiceDelayTicks delay_ticks{};
        while (creature.voice().get_next_sound_pair(sound_id, delay_ticks)) {
            speaker.play_sound_effect(sound_id, static_cast<int>(delay_ticks),
                                      true, document_);
        }
    }

    const creatures1::creatures::VoiceDelayTicks accumulated =
        creature.voice().accumulated_sound_delay_ticks();
    out_delay_world_ticks = static_cast<std::int32_t>(accumulated);
    // The native lifetime is a byte: `(char)accumulated_delay + 5`.
    create_speech_bubble(
        document_, speaker, phrase,
        static_cast<std::uint8_t>(static_cast<std::uint8_t>(accumulated) + 5));
}

void WindowsCreatureSpeechPhraseHost::queue_speech_event(
    creatures1::objects::Object& source, std::uint32_t event_argument,
    std::int32_t delay_world_ticks) {
    // All three Creature phrase call sites queue EVENT_7; only the Macro
    // `say#` and PointerTool bubbles use EVENT_6, and neither goes through
    // the phrase host.
    creatures1::creatures::queue_events_in_speech_range(
        source, creatures1::objects::ObjectEventId::event_7, event_argument,
        delay_world_ticks, fanout_);
}


// --- WindowsBacteriumServiceHost -------------------------------------------

creatures1::world::BacteriumServicePhase&
WindowsBacteriumServiceHost::service_phase() {
    return document_.bacterium_service_phase();
}

std::size_t WindowsBacteriumServiceHost::creature_count() const {
    return document_.creature_count();
}

std::size_t WindowsBacteriumServiceHost::selected_creature_count() const {
    return document_.selected_creature_count();
}

int WindowsBacteriumServiceHost::smoothed_idle_cycle_index() const {
    // SFCApp::OnIdle keeps this average; the infection roll is 1-in-501 once
    // the application has been idle-cycling freely and 1-in-251 while it is
    // busy or the world is crowded, so a constant here would pick the wrong
    // rate forever.
    return g_active_app_state == nullptr
               ? 0
               : g_active_app_state->idle_cadence.smoothed_idle_cycle;
}

std::uint32_t WindowsBacteriumServiceHost::next_random() {
    return static_cast<std::uint32_t>(std::rand());
}

creatures1::creatures::BacteriumRandomSource&
WindowsBacteriumServiceHost::bacterium_random_source() {
    return random_;
}

bool WindowsBacteriumServiceHost::creature_tick_enabled(
    std::size_t index) const {
    auto* creature = static_cast<creatures1::creatures::Creature*>(
        document_.creature_at(index));
    return creature != nullptr && creature->skeleton().tick_enabled();
}

creatures1::creatures::Bacterium&
WindowsBacteriumServiceHost::creature_bacterium(std::size_t index) {
    auto* creature = static_cast<creatures1::creatures::Creature*>(
        document_.creature_at(index));
    return creature->bacterium();
}

creatures1::creatures::Bacterium&
WindowsBacteriumServiceHost::environment_bacterium(std::size_t index) {
    return document_.world_runtime()->map_data().bacterium_at(index);
}

void WindowsBacteriumServiceHost::update_creature_bacterium_and_environment(
    std::size_t index) {
    auto* creature = static_cast<creatures1::creatures::Creature*>(
        document_.creature_at(index));
    if (creature == nullptr) {
        return;
    }
    WindowsCreatureBacteriumEnvironmentHost environment(document_);
    creature->update_bacterium_and_environment(environment);
}

void WindowsBacteriumServiceHost::log_environment_infection() {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console != nullptr) {
        creatures1::common::debug_log(
            *console, 0x800,
            "A creature has been infected from environment\n");
    }
}

// --- WindowsCreatureWorldUpdateHost ----------------------------------------

std::size_t WindowsCreatureWorldUpdateHost::creature_count() const {
    return document_.creature_count();
}

creatures1::creatures::Creature*
WindowsCreatureWorldUpdateHost::creature_at(std::size_t index) const {
    return static_cast<creatures1::creatures::Creature*>(
        document_.creature_at(index));
}

bool WindowsCreatureWorldUpdateHost::creature_is_tick_enabled(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().tick_enabled();
}

bool WindowsCreatureWorldUpdateHost::creature_is_alive(
    const creatures1::creatures::Creature& creature) const {
    return creature.life_state() ==
           creatures1::creatures::CreatureLifeState::alive;
}

creatures1::objects::Object* WindowsCreatureWorldUpdateHost::motion_link(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().motion_link;
}

int WindowsCreatureWorldUpdateHost::motion_target_x(
    const creatures1::creatures::Creature& creature) const {
    // The native reads the stored skeleton field, not the link's current
    // position, so a moving link does not retarget the distance neuron until
    // the motion pass writes it.
    return creature.skeleton().motion_target_x;
}

int WindowsCreatureWorldUpdateHost::sound_source_x(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().sound_source_x();
}

bool WindowsCreatureWorldUpdateHost::object_has_interaction_event(
    const creatures1::objects::Object& object) const {
    return object.current_interaction_event_id() != 0;
}

bool WindowsCreatureWorldUpdateHost::boundary_correction_pending(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().boundary_correction_pending;
}

void WindowsCreatureWorldUpdateHost::clear_boundary_correction(
    creatures1::creatures::Creature& creature) {
    creature.skeleton().boundary_correction_pending = false;
}

creatures1::objects::Object* WindowsCreatureWorldUpdateHost::bounds_reference(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().bounds_reference_object();
}

void WindowsCreatureWorldUpdateHost::trigger_built_in_stimulus(
    creatures1::creatures::Creature& creature, std::uint32_t stimulus_id,
    std::uint32_t argument) {
    // UpdateAllCreatureBrainInputs passes the creature's own object as the
    // stimulus target, not the bounds reference it just proved to be null.
    WindowsStimulusSourceHost source(document_);
    creature.trigger_built_in_stimulus(
        stimulus_id, &document_.object_for_creature(creature), argument,
        source);
}

void WindowsCreatureWorldUpdateHost::update_perception(
    creatures1::creatures::Creature& creature) {
    WindowsStimulusSourceHost source(document_);
    creature.update_perception(perception_, source);
}

void WindowsCreatureWorldUpdateHost::update_attention(
    creatures1::creatures::Creature& creature) {
    WindowsCreatureAttentionHost attention(document_);
    creature.update_attention(attention);
}

creatures1::world::WorldRect
WindowsCreatureWorldUpdateHost::movement_bounds(
    const creatures1::creatures::Creature& creature) const {
    return creature.skeleton().movement_bounds();
}

creatures1::world::WorldRect
WindowsCreatureWorldUpdateHost::movement_bounds(
    const creatures1::objects::Object& object) const {
    return object.movement_bounds();
}

bool WindowsCreatureWorldUpdateHost::reference_uses_own_attention_bounds(
    const creatures1::objects::Object& object) const {
    // The attention pass adopts the reference's own rectangle for every
    // reference except the family 3 genus 2 map-room marker, which is the
    // same discriminator CanPerceiveObject uses.
    return (object.classifier_base() & 0xffff0000u) != 0x03020000u;
}

void WindowsCreatureWorldUpdateHost::object_part_center(
    const creatures1::objects::Object& object, int part_index, int& out_x,
    int& out_y) const {
    const_cast<creatures1::objects::Object&>(object).get_part_center(
        &out_x, &out_y, part_index);
}

bool WindowsCreatureWorldUpdateHost::should_log_attention_loss(
    const creatures1::creatures::Creature& creature) const {
    return active_debug_console() != nullptr &&
           document_.is_selected_creature(creature.skeleton());
}

namespace {

std::string attention_target_label(
    const creatures1::objects::Object& target) {
    const std::uint32_t classifier = target.classifier_base();
    creatures1::objects::ObjectDebugLabelInput input{};
    input.classifier.event = static_cast<std::uint8_t>(classifier & 0xff);
    input.classifier.species =
        static_cast<std::uint8_t>((classifier >> 8) & 0xff);
    input.classifier.genus =
        static_cast<std::uint8_t>((classifier >> 16) & 0xff);
    input.classifier.family =
        static_cast<creatures1::objects::ClassifierFamily>(
            (classifier >> 24) & 0xff);
    return creatures1::objects::format_object_debug_label(&input);
}

} // namespace

void WindowsCreatureWorldUpdateHost::log_attention_target_out_of_reach(
    const creatures1::creatures::Creature& /*creature*/,
    const creatures1::objects::Object& target) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console == nullptr) {
        return;
    }
    creatures1::common::debug_log(*console, 0x80, "%s gone out of reach.\n",
                                  attention_target_label(target).c_str());
}

void WindowsCreatureWorldUpdateHost::log_attention_target_inaccessible(
    const creatures1::creatures::Creature& /*creature*/,
    const creatures1::objects::Object& target) {
    C1DebugConsoleDialog* console = active_debug_console();
    if (console == nullptr) {
        return;
    }
    creatures1::common::debug_log(*console, 0x20,
                                  "Object %s has become inaccessible\n",
                                  attention_target_label(target).c_str());
}

// --- WindowsDriveThresholdObject -------------------------------------------

bool WindowsDriveThresholdObject::tick_enabled() const {
    return object_ != nullptr && object_->tick_enabled();
}

void WindowsDriveThresholdObject::update_drive_threshold_state() {
    if (object_ == nullptr) {
        return;
    }
    creatures1::creatures::Creature* creature =
        document_.mutable_creature_for_object(*object_);
    if (creature == nullptr) {
        // Object's own vtable slot 42 does nothing; only Creature overrides.
        return;
    }
    creature->update_drive_threshold_state(
        creatures1::creatures::kDriveLowerThresholds,
        creatures1::creatures::kDriveUpperThresholds);
}

void run_creature_drive_threshold_phase(C1WindowsDocument& document) {
    const std::size_t count = document.non_scenery_object_count();
    std::vector<WindowsDriveThresholdObject> adapters;
    std::vector<creatures1::creatures::DriveThresholdObject*> pointers;
    adapters.reserve(count);
    pointers.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        adapters.emplace_back(document,
                              document.non_scenery_object_at(index));
    }
    for (WindowsDriveThresholdObject& adapter : adapters) {
        pointers.push_back(&adapter);
    }

    creatures1::creatures::NonSceneryObjectRegistryView view{};
    view.objects = pointers.data();
    view.object_count = pointers.size();
    creatures1::creatures::update_all_creature_drive_threshold_states(view);
}

} // namespace creatures1::platform
