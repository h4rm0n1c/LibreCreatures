#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "../biochemistry/biochemistry.hpp"
#include "../brain/brain.hpp"
#include "../brain/instinct.hpp"
#include "learned_words.hpp"
#include "selection.hpp"
#include "../world/geometry.hpp"
#include "../world/map.hpp"
#include "attention.hpp"
#include "bacterium.hpp"
#include "genome.hpp"
#include "registry.hpp"
#include "skeleton.hpp"
#include "stimulus.hpp"
#include "voice.hpp"

namespace creatures1::objects {
class Object;
class ObjectRegistryHost;
class ObjectImmediateEventQueueHost;
struct QueuedObjectEvent;
enum class ObjectEventId : std::uint32_t;
}

namespace creatures1::creatures {

class Creature;
class CreatureConstructionHost;

// Learned-word slots 0x48..0x4b are reserved action words rather than
// ordinary attention vocabulary. Their native meanings are stable C1 policy.
enum class HeardWordActionSlot : std::int32_t {
    yes = 0x48,
    no = 0x49,
    look = 0x4a,
    what = 0x4b,
};

// Construction-only selector recovered from Creature::Creature.  This is
// deliberately distinct from CreatureGender: the native constructor uses
// zero for "choose from the live population" and one/two for male/female,
// while the clean runtime's stored gender abstraction is zero-based.
enum class CreatureConstructionSex : std::uint32_t {
    random = 0,
    male = 1,
    female = 2,
};

// UpdatePerception @ 0x0040e8b0 drives five general-sensory neurons from the
// motion link.  SkeletonRenderPoseState holds genome_source_filename,
// mother_moniker and father_moniker at offsets 0/4/8, which fixes each test:
// the link is my parent when its genome is one of my parent monikers, my
// child when one of its parent monikers is my genome, and my sibling when we
// name the same mother or the same father.  The earlier names here
// ("has_same_genome", "is_father") did not describe those comparisons.
struct CreatureMotionLinkFacts {
    bool is_creature = false;
    bool link_is_my_parent = false;
    bool link_is_my_child = false;
    bool shares_a_parent = false;
    bool is_opposite_sex = false;
};

// The sixteen Decision-lobe action slots are filtered by this one-byte table
// before UpdateAttention allows the lobe to arbitrate.  This is the source
// counterpart of the existing /Creatures1/C1ActionTargetRequirement enum.
enum class ActionTargetRequirement : std::uint8_t {
    not_selectable = 0,
    always_eligible = 1,
    requires_no_target = 2,
    requires_target = 3,
};

// A Creature is also represented by an Object in the world hierarchy, but
// this clean model does not fake that inheritance across incomplete layouts.
// The world supplies the identity relationship explicitly where an Object
// event or Object-owned stimulus target is required.
class CreatureObjectIdentityHost {
public:
    virtual ~CreatureObjectIdentityHost() = default;
    virtual objects::Object& object_for_creature(Creature& creature) const = 0;
};

struct UnboundedWorldPositionInput {
    int mouse_client_x = 0;
    int mouse_client_y = 0;
    int viewport_left = 0;
    int viewport_top = 0;
};

// SFCView owns the pending-input flag and viewport coordinates, while the
// pointer tool and renderer own their concrete storage and redraw operations.
// This is the narrow application boundary for Creature's unbounded-position
// policy; it intentionally does not expose SFCView, MFC, or global pointers.
class CreatureUnboundedWorldPositionHost {
public:
    virtual ~CreatureUnboundedWorldPositionHost() = default;
    virtual bool read_view_input_and_clear_pending_flag(
        UnboundedWorldPositionInput& out) = 0;
    virtual objects::Object& object_for_creature(Creature& creature) const = 0;
    virtual void move_to_and_redraw(objects::Object& object, int world_x,
                                    int world_y) = 0;
    virtual void update_pointer_tool_unbounded_position_and_redraw() = 0;
};

enum class InseminationLogEvent : std::uint8_t {
    gamete_available,
    sperm_accepted,
    fertilization_attempted,
};

// Object classifier/layout checks and offspring-file construction belong to
// the world/genome owners. Creature retains the mating decision and its
// fertility IDs, while this boundary carries only those concrete services.
class CreatureInseminationHost {
public:
    virtual ~CreatureInseminationHost() = default;
    virtual Creature* recipient_for_insemination(Creature& source) const = 0;
    virtual GenomeFilenameId generate_offspring_genome_file(
        GenomeFilenameId maternal_source_filename,
        GenomeFilenameId paternal_source_filename) = 0;
    virtual bool debug_console_visible() const = 0;
    virtual bool is_selected_creature(const Creature& creature) const = 0;
    virtual void log_insemination(InseminationLogEvent event,
                                  const Creature& source,
                                  const Creature* recipient,
                                  GenomeFilenameId paternal_source_filename) = 0;
};

struct PointerAttentionCandidate {
    world::WorldRect bounds{};
    AttentionClassifier classifier{};
    bool is_pointer_tool = false;
    bool is_this_creature = false;
    bool blocks_pointer_attention = false;
};

// Supplies the world and pointer-tool boundary used by the recovered C1
// pointer-attention policy.  Registry storage, virtual Object dispatch, and
// Brain's lobe-7 representation stay outside the clean source lane.
class PointerAttentionApi {
public:
    virtual ~PointerAttentionApi() = default;
    virtual int creature_sound_source_x() const = 0;
    virtual int pointer_tool_sound_source_x() const = 0;
    virtual world::WorldRect creature_movement_bounds() const = 0;
    virtual std::size_t non_scenery_object_count() const = 0;
    virtual bool read_non_scenery_object(std::size_t index,
                                         PointerAttentionCandidate& out) const = 0;
    virtual AttentionClassifier pointer_tool_classifier() const = 0;
    virtual void clear_pointer_tool_lobe_neuron(std::uint32_t neuron_index) = 0;
    virtual void report_registry_bounds_failure() = 0;
};

std::uint32_t resolve_pointer_attention_target(PointerAttentionApi& api);

enum class StatusTextKey {
    male_life_stage,
    not_pregnant,
    dead,
    healthy,
    sick,
};

class CreatureStatusTextApi {
public:
    virtual ~CreatureStatusTextApi() = default;
    virtual std::string_view text(StatusTextKey key) const = 0;
};

struct CreatureStatusRoom {
    world::WorldRect bounds{};
};

class CreatureStatusWorldApi {
public:
    virtual ~CreatureStatusWorldApi() = default;
    virtual std::size_t room_count() const = 0;
    virtual CreatureStatusRoom room(std::size_t index) const = 0;
};

// The queued-event slots are virtual Creature hooks in the executable.  The
// object/script scheduler owns dispatch and event numbering; Creature only
// applies its alive-state gate and forwards the source with the fixed
// argument used by these three slots.
class CreatureScriptEventHost {
public:
    virtual ~CreatureScriptEventHost() = default;
    virtual void dispatch_script_event(Creature& creature,
                                       objects::Object* source,
                                       std::uint32_t event_id,
                                       std::uint32_t argument) = 0;
};

// Creature's virtual script override combines sleep-indicator teardown with
// classifier dispatch. The CAOS runtime owns lookup/execution; this adapter
// receives the already-packed Creature classifier and returns the native
// executed/not-executed result.
class CreatureScriptDispatchHost {
public:
    virtual ~CreatureScriptDispatchHost() = default;
    virtual std::uint32_t classifier_base(const Creature& creature) const = 0;
    virtual int execute_script_for_classifier(
        Creature& creature, objects::Object* source,
        std::uint32_t classifier, bool force_restart) = 0;
};

// Action selection changes Creature-owned brain state. Object identity,
// immediate-event storage, timer liveness, and CAOS lookup/execution remain
// runtime services at this boundary; none of the action policy is delegated
// to a free function or an opaque vtable call.
class CreatureAttentionHost : public CreatureObjectIdentityHost,
                              public objects::ObjectImmediateEventQueueHost,
                              public CreatureScriptDispatchHost {
public:
    virtual ~CreatureAttentionHost() = default;
    virtual objects::Object* create_sleep_indicator(
        Creature& creature, int render_plane) = 0;
    virtual bool action_event_target_is_live(
        const objects::Object& target) const = 0;
    virtual std::uint32_t classifier_base(
        const objects::Object& object) const = 0;
    virtual ActionTargetRequirement action_target_requirement(
        std::size_t action_index) const = 0;
    virtual AttentionClassifier classify_object(
        const objects::Object& object) const = 0;
    virtual void dispatch_sleep_indicator_event(
        objects::Object& indicator, objects::ObjectEventId event_id,
        objects::Object* source, std::uint32_t argument) = 0;
    virtual void initialize_sleep_indicator(objects::Object& indicator) = 0;
    virtual bool debug_console_visible() const = 0;
    virtual bool is_selected_creature(const Creature& creature) const = 0;
    virtual void log_attention_shift(const Creature& creature,
                                     const objects::Object* target) = 0;
    virtual void log_override_action_script_selected(
        const Creature& creature, std::uint32_t classifier) = 0;
    virtual void log_no_action_script(const Creature& creature,
                                      std::uint32_t classifier) = 0;
    virtual void log_involuntary_action(const Creature& creature,
                                        int action_index) = 0;
    virtual void log_action_selection(
        const Creature& creature, const objects::Object* motion_link,
        std::uint32_t action_index) = 0;
};

// World geometry and object classification are runtime services.  This
// boundary keeps Creature's perception policy independent of the concrete
// Object registry, map-room implementation, and renderer storage.
class CreaturePerceptionHost {
public:
    virtual ~CreaturePerceptionHost() = default;
    virtual std::size_t non_scenery_object_count() const = 0;
    virtual objects::Object* non_scenery_object_at(
        std::size_t index) const = 0;
    virtual CreatureMotionLinkFacts motion_link_facts(
        const objects::Object& object, const Creature& creature) const = 0;
    virtual bool is_this_creature(const objects::Object& target,
                                  const Creature& creature) const = 0;
    virtual bool has_bounds_flag(const objects::Object& target,
                                 std::uint8_t flag) const = 0;
    virtual bool read_bounds(const objects::Object& target,
                             world::WorldRect& out_bounds) const = 0;
    virtual bool is_map_room_reference(
        const objects::Object& reference) const = 0;
    virtual void nearest_map_room_bounds(int world_x, int world_y,
                                         world::WorldRect& out_bounds) const = 0;
};

// Goal-direction delivery needs one Object fact from the world-owned motion
// link. The attention table and Brain remain Creature-owned state.
class CreatureGoalDirectionHost {
public:
    virtual ~CreatureGoalDirectionHost() = default;
    // MapData::FindRoomContainingPoint returns the first containing room;
    // -1 is the native null-room result.  The room index, rather than a
    // copied rectangle, preserves the executable's pointer-identity test
    // when comparing the creature's room with a remembered goal room.
    virtual std::int32_t room_index_at(int world_x, int world_y) const = 0;
    // Native room classification is 1 for no room or room_type==1 and 0 for
    // a containing room with another proven/unknown discriminator.
    virtual std::int32_t room_class_at(int world_x, int world_y) const = 0;
    virtual std::uint32_t current_interaction_event_id(
        const objects::Object& object) const = 0;
    virtual std::int32_t attention_record_index(
        const objects::Object& object) const = 0;
};

// Pose transition policy stays Creature-owned.  Render-plane dispatch and
// redraw queue storage remain world/renderer services at this narrow edge.
class CreaturePoseAnimationHost {
public:
    virtual ~CreaturePoseAnimationHost() = default;
    virtual int render_plane(const objects::Object& object) const = 0;
    virtual void queue_dirty_world_rect(
        const world::WorldRect& bounds) = 0;
};

// The per-tick Creature update crosses the view/input, immediate-event,
// renderer, and Object-runtime boundaries.  Keeping those operations on one
// host mirrors the native call sequence without manufacturing a second C1
// implementation of SFCView, MFC Object, or the renderer.
class CreatureUpdateHost : public CreatureUnboundedWorldPositionHost,
                           public objects::ObjectImmediateEventQueueHost,
                           public CreaturePoseAnimationHost {
public:
    virtual ~CreatureUpdateHost() = default;
    virtual void dispatch_sleep_indicator_event(
        objects::Object& indicator, objects::ObjectEventId event_id,
        objects::Object* source, std::uint32_t argument) = 0;
    virtual void initialize_sleep_indicator(objects::Object& indicator) = 0;
};

// Creature's archive is the composition boundary for the already recovered
// Object/Skeleton/Brain/Biochemistry/Voice/Register records.  The concrete
// adapter owns MFC buffering and dynamic-object identity; this interface
// keeps the C1 field order and typed sub-record boundaries in source.
class CreatureArchive : public SkeletonArchive {
public:
    virtual ~CreatureArchive() = default;

    // CInstinct is a dynamically archived object.  Creature stores its
    // semantic value slots, while the archive host resolves the MFC object
    // tag/class lifetime and transfers the recovered Instinct record.
    virtual void read_instinct_reference(brain::Instinct& destination) = 0;
    virtual void write_instinct_reference(
        const brain::Instinct& instinct) = 0;
};

// Creature::Deserialize materialises dynamic CGenome records and then
// reattaches the creature to the world/application.  Archive object identity,
// filename collision scans, sprite/body construction, selection/UI refresh,
// and score storage belong to those hosts; the load ordering and child-genome
// policy remain explicit in Creature.
class CreatureDeserializationHost {
public:
    virtual ~CreatureDeserializationHost() = default;

    virtual std::unique_ptr<Genome> read_genome_reference(
        CreatureArchive& archive) = 0;
    // The primary scan excludes the creature currently being restored;
    // this matches the native registry loop's `other != this` test.
    virtual void ensure_unique_primary_genome_filename(
        Genome& genome, const Creature& current_creature) = 0;
    // The child scan has a separate native loop and must not inherit the
    // primary scan's exclusion rule.
    virtual void ensure_unique_child_genome_filename(
        Genome& genome, const Creature& current_creature) = 0;
    virtual void save_generated_genome(const Genome& genome) = 0;
    // Materialises the full Skeleton/Creature genome state.  This is kept
    // distinct from Creature::load_genome(), which consumes already-loaded
    // creature gene records.
    virtual bool load_materialized_genome(Creature& creature,
                                          Genome& genome) = 0;
    // Same formatting Creature's plain constructor uses to keep the
    // register's cached moniker strings in step with the identity fields
    // they describe.
    virtual std::string format_moniker(std::uint32_t value) const = 0;

    virtual void set_unbounded_bounds_and_update(Creature& creature) = 0;
    virtual void move_to_and_redraw(Creature& creature, int world_x,
                                    int world_y) = 0;
    virtual void set_default_bounds_and_update(Creature& creature) = 0;

    // This one operation retains the native selected-creature notification
    // order: selection state, embedded control broadcast, window/eye-view
    // refresh, viewport return, event-bar refresh, and toolbar invalidation.
    virtual void select_loaded_creature(Creature& creature) = 0;
    virtual void rebuild_creature_selection_menu() = 0;
    virtual void increment_living_norns() = 0;
    // Optional OLE/embedded-kit notification after score/menu integration.
    virtual void notify_creature_loaded_to_embedded_kits() = 0;
};

struct CreatureEnvironmentSample {
    // The native map path supplies the signed profile delta. Creature scales
    // it by 0x55 and assigns the result to exactly one of the two signals.
    int temperature_delta_raw = 0;
    bool uses_ambient_temperature = true;
    std::uint8_t ambient_light_level = 0;
};

// UpdateEnvironmentAndLifeStage owns the signal thresholds and life-stage
// transitions. MapData, Object sound-source dispatch, genome reinitialisation,
// and death effects remain runtime services at this narrow boundary.
class CreatureEnvironmentHost {
public:
    virtual ~CreatureEnvironmentHost() = default;
    virtual CreatureEnvironmentSample sample_at(int world_x,
                                                int world_y) const = 0;
    virtual int sound_source_x(const objects::Object& object) const = 0;
    virtual bool is_selected_creature(const Creature& creature) const = 0;
    virtual void initialize_from_genome(Creature& creature) = 0;
    virtual void die(Creature& creature) = 0;
};

struct CreatureSelectionRemovalResult {
    bool was_selected = false;
    std::size_t remaining_selection_count = 0;
};

// RemoveFromWorld is a Creature policy operation. MFC arrays, object
// registries, the selected-creature UI, renderable bounds, the sleep
// indicator, event/stimulus delivery, score state, and embedded OLE records
// remain owned by their concrete runtime hosts at these explicit seams.
class CreatureRemovalHost {
public:
    virtual ~CreatureRemovalHost() = default;

    // Ends interactions for every non-scenery object currently following the
    // creature. The host rereads its registry size after each callback.
    virtual void end_interactions_with_creature(Creature& creature) = 0;

    // Removes the creature from the MFC selection array and reports the
    // post-removal size used by the native UI notification path.
    virtual CreatureSelectionRemovalResult remove_from_creature_selection(
        Creature& creature) = 0;

    // Retains the native order: embedded selection broadcast, title/eye-view
    // handling, viewport return, event-bar refresh, toolbar invalidation.
    virtual void refresh_after_selected_creature_removal(
        Creature& creature, std::size_t remaining_selection_count) = 0;

    virtual void clear_bounds_reference_and_set_default(Creature& creature) = 0;
    virtual void release_sleep_indicator(Creature& creature) = 0;
    virtual void notify_dependents_on_removal(Creature& creature) = 0;
    virtual void purge_destroy_when_finished_macros(Creature& creature) = 0;
    virtual void decrement_living_norns() = 0;

    // Sends the native post-removal OLE notification to embedded record 8
    // when that dispatch interface is present.
    virtual void notify_creature_removed_to_embedded_kits() = 0;
};

// Die keeps the native state/notification order while the application owns
// registry cleanup, macro cancellation, selection/UI, eye-view, and debug
// presentation. Those effects are runtime services, not anonymous globals in
// the recovered Creature source.
class CreatureDeathHost {
public:
    virtual ~CreatureDeathHost() = default;
    virtual void release_sleep_indicator(Creature& creature) = 0;
    virtual void notify_dependents_on_death(Creature& creature) = 0;
    virtual void purge_destroy_when_finished_macros(Creature& creature) = 0;
    virtual void log_death_message(const Creature& creature) = 0;
    virtual void dispatch_death_event(Creature& creature) = 0;
    virtual void rebuild_creature_selection_menu() = 0;
    virtual bool is_selected_creature(const Creature& creature) const = 0;
    virtual void persist_and_close_selected_eye_view() = 0;
    virtual void broadcast_embedded_control_state() = 0;
    virtual void log_goal_drive_table(const Creature& creature) = 0;
};

// UpdateBacteriumAndEnvironment combines the embedded bacterium policy with
// the already recovered environment and goal-direction policies. Bacterium
// storage and the world hosts remain explicit runtime seams.
class CreatureBacteriumEnvironmentHost {
public:
    virtual ~CreatureBacteriumEnvironmentHost() = default;
    virtual CreatureEnvironmentHost& environment_host() = 0;
    virtual CreatureGoalDirectionHost& goal_direction_host() = 0;
    virtual bool is_selected_creature(const Creature& creature) const = 0;
};

// Pickup is a Creature-owned event policy with a deliberately narrow runtime
// seam. Object bounds/renderable state, Vehicle attachment geometry, the
// pointer-tool input/privilege state, immediate-event storage, script
// dispatch, selection, and redraw remain owned by their subsystems.
class CreaturePickupHost : public CreatureUnboundedWorldPositionHost,
                           public objects::ObjectImmediateEventQueueHost,
                           public CreatureScriptEventHost {
public:
    virtual ~CreaturePickupHost() = default;
    virtual objects::ObjectRenderableSetHost& renderables() = 0;
    virtual objects::ObjectMovementBoundsHost& movement_bounds_host() = 0;
    virtual objects::ObjectSoundPlaybackHost& sound_host() = 0;
    virtual objects::Object* pointer_tool() const = 0;
    virtual bool pointer_pickup_is_privileged(
        const objects::Object& source) const = 0;
    virtual int vehicle_attachment_render_plane(
        const objects::Object& vehicle) const = 0;
    virtual void select_creature(Creature& creature) = 0;
    virtual void queue_dirty_world_rect(
        const world::WorldRect& bounds) = 0;
};

// Drop uses the same Object/event/rendering services as pickup, but has no
// pointer privilege or selection policy. Keeping this smaller host separate
// makes the release/reattachment behavior explicit.
class CreatureDropHost : public CreatureObjectIdentityHost,
                         public objects::ObjectImmediateEventQueueHost,
                         public CreatureScriptEventHost {
public:
    virtual ~CreatureDropHost() = default;
    virtual objects::ObjectRenderableSetHost& renderables() = 0;
    virtual objects::ObjectMovementBoundsHost& movement_bounds_host() = 0;
    virtual objects::ObjectSoundPlaybackHost& sound_host() = 0;
    virtual objects::Object* pointer_tool() const = 0;
    virtual objects::Object* find_topmost_vehicle_overlap(
        Creature& creature) const = 0;
    virtual void queue_dirty_world_rect(
        const world::WorldRect& bounds) = 0;
};

enum class DriveThresholdState : std::uint8_t {
    all_at_or_below_lower = 0,
    some_above_lower = 1,
    some_above_upper = 2,
};

struct CreatureStatusSnapshot {
    std::string_view display_name;
    std::string_view genome_moniker;
    CreatureGender gender = CreatureGender::male;
    std::uint32_t age_ticks = 0;
    bool pregnant = false;
    std::uint8_t gestation_chemical_level = 0;
    std::uint8_t life_force_chemical_level = 0;
    std::array<std::uint8_t, 11> sickness_chemical_levels{};
    bool dead = false;
    int down_foot_x = 0;
    int down_foot_y = 0;
};

// Builds the pipe-delimited status record consumed by the external Owners
// Kit/DDE query.  The original MFC CString, resource loading, and secure CRT
// append operations are represented by these narrow adapters; field policy
// remains C1-owned and readable.
std::string format_status_for_external_query(
    const CreatureStatusSnapshot& creature,
    const CreatureStatusTextApi& text,
    const CreatureStatusWorldApi& world);

// This is the recovered language slice of Creature.  The remaining Creature
// state is intentionally added by its owning batches; this class does not
// pretend that a partial field map is the complete C1 object layout.
class Creature : public CreatureSelectionEntry,
                 public SkeletonReferenceOwner {
public:
    static constexpr std::size_t kLearnedWordRecordCount = 0x50;
    static constexpr std::size_t kBuiltInStimulusContextCount = 36;
    static constexpr std::size_t kAttentionRecordCount = 40;
    static constexpr std::size_t kInstinctCapacity = 20;

    // The no-argument form is the framework/archive factory path.  The
    // genome constructor below is the native game-creation path recovered at
    // 0040d580 and keeps all non-Creature services behind one explicit host.
    Creature() { skeleton_.set_reference_owner(this); }
    Creature(const Creature&) = delete;
    Creature& operator=(const Creature&) = delete;

    bool owner_references_object(
        const objects::Object* candidate) const override {
        return references_object(candidate);
    }
    void owner_clear_references_to(
        const objects::Object* candidate) override {
        clear_references_to_object(candidate);
    }
    Creature(GenomeFilenameId genome_source_filename,
             CreatureConstructionSex construction_sex,
             CreatureConstructionHost& host);

    struct InvoluntaryActionState {
        std::uint8_t trigger_probability = 0;
        std::uint8_t cooldown_ticks = 0;
    };

    struct GoalDriveLevels {
        std::uint8_t pain = 0;
        std::uint8_t nfp = 0;
        std::uint8_t hunger = 0;
        std::uint8_t coldness = 0;
        std::uint8_t hotness = 0;
        std::uint8_t tiredness = 0;
        std::uint8_t sleepiness = 0;
        std::uint8_t loneliness = 0;
        std::uint8_t crowdedness = 0;
        std::uint8_t fear = 0;
        std::uint8_t boredom = 0;
        std::uint8_t anger = 0;
        std::uint8_t sex = 0;
        std::uint8_t not_allocated_drive_2 = 0;
        std::uint8_t not_allocated_drive_3 = 0;
        std::uint8_t not_allocated_drive_4 = 0;

        static constexpr std::size_t size() { return 16; }

        std::uint8_t& operator[](std::size_t index) {
            switch (index) {
            case 0: return pain;
            case 1: return nfp;
            case 2: return hunger;
            case 3: return coldness;
            case 4: return hotness;
            case 5: return tiredness;
            case 6: return sleepiness;
            case 7: return loneliness;
            case 8: return crowdedness;
            case 9: return fear;
            case 10: return boredom;
            case 11: return anger;
            case 12: return sex;
            case 13: return not_allocated_drive_2;
            case 14: return not_allocated_drive_3;
            default: return not_allocated_drive_4;
            }
        }

        const std::uint8_t& operator[](std::size_t index) const {
            return const_cast<GoalDriveLevels&>(*this)[index];
        }
    };

    struct ControlState {
        std::uint8_t always_on_signal = 0;
        std::uint8_t asleep_signal = 0;
        std::uint8_t air_coldness_signal = 0;
        std::uint8_t air_hotness_signal = 0;
        std::uint8_t ambient_light_signal = 0;
        std::uint8_t crowdedness_signal = 0;
        std::uint8_t fertility_signal = 0;
        std::uint8_t pregnancy_signal = 0;
        std::uint8_t ovulation_signal = 0;
        std::uint8_t conception_probability = 0;
        std::array<std::uint8_t, 7> life_stage_advance_signals{};
        std::array<InvoluntaryActionState, 8> involuntary_actions{};
        std::uint8_t death_signal = 0;
        std::array<std::uint8_t, 8> floating_loci{};
        std::array<std::uint8_t, 8> gait_locus_levels{};
        GoalDriveLevels goal_direction_drive_levels{};

        void reset_to_initial_state();
    };

    struct AttentionRecord {
        objects::Object* target = nullptr;
        std::uint8_t target_neuron_index = 0;
        std::uint8_t lobe_activation = 0;
        int world_x = 0;
        int world_y = 0;
        bool visible = false;
        bool stimulus_seen = false;
    };

    struct GoalDirectionState {
        std::int32_t attention_record_index = -1;
        std::int32_t action_lobe_neuron_index = 0;
        std::uint8_t commitment = 0;
        std::uint8_t delivery_countdown = 0;
    };

    struct InstinctRuntimeState {
        ControlState control_state{};
        std::array<brain::Instinct, kInstinctCapacity> instincts{};
        std::uint32_t instinct_count = 0;
        std::uint32_t dream_countdown = 0;
    };

    using GoalDirectionWeightMatrix =
        std::array<std::array<std::int32_t, 16>, 40>;

    class InitializationHost {
    public:
        virtual ~InitializationHost() = default;
        virtual const StimulusContext* default_stimulus_context(
            std::size_t index) const = 0;
        virtual std::string localized_birthplace() const = 0;
        virtual std::string format_moniker(std::uint32_t value) const = 0;
    };

    // Genome initialization composes the already recovered Brain,
    // Skeleton, Biochemistry, Creature, and Voice loaders. File/resource
    // opening, sprite/gallery services, render-plane selection, and
    // biochemical locus resolution remain application-owned services at this
    // boundary; the load order is Creature policy.
    class GenomeInitializationHost {
    public:
        virtual ~GenomeInitializationHost() = default;
        virtual GenomeFileStore& genome_files() = 0;
        virtual const SkeletonSpriteBuildServices& skeleton_services() const = 0;
        virtual SkeletonRenderPlaneHost& render_plane_host() = 0;
        virtual objects::ObjectSoundPlaybackHost& sound_host() = 0;
        // The locus resolver has to reach the Brain and Creature that own the
        // loci, which it does through the biochemistry's owner -- so the
        // biochemistry being loaded is named here rather than left for the
        // host to guess.
        virtual biochemistry::BiochemistryLocusHost& biochemistry_locus_host(
            biochemistry::Biochemistry& value) = 0;
        virtual VoiceFileStore& voice_files() = 0;
    };

    class StimulusSourceHost {
    public:
        virtual ~StimulusSourceHost() = default;
        virtual AttentionClassifier classify(
            const objects::Object& object) const = 0;
        virtual int sound_source_x(const objects::Object& object) const = 0;
        virtual int sound_source_y(const objects::Object& object) const = 0;
        virtual bool is_this_creature(const objects::Object& object,
                                      const Creature& creature) const = 0;
    };

    // Speech playback and bubble creation are application/audio boundaries.
    // Creature owns the learned-word decision; the host owns Voice, Object,
    // and the selected runtime's UI/audio implementation.
    class CreatureSpeechHost {
    public:
        virtual ~CreatureSpeechHost() = default;
        virtual void speak(Creature& creature, std::string_view phrase) = 0;
    };

    // Phrase assembly stays Creature-owned. Voice timing, Object identity,
    // attention lookup, and speech-range event scheduling are runtime
    // services at this boundary; the host does not expose their storage.
    class CreatureSpeechPhraseHost {
    public:
        virtual ~CreatureSpeechPhraseHost() = default;
        virtual objects::Object& object_for_creature(
            Creature& creature) const = 0;
        virtual std::int32_t attention_record_index(
            const objects::Object& object) const = 0;
        virtual void speak_text_with_voice(
            Creature& creature, std::string_view phrase,
            std::int32_t& out_delay_world_ticks) = 0;
        virtual void queue_speech_event(
            objects::Object& source, std::uint32_t event_argument,
            std::int32_t delay_world_ticks) = 0;
    };

    // Heard-word dispatch combines four runtime-owned concerns that the
    // executable reaches through Object/global pointers: pointer-tool
    // identity, pointer-attention selection, phrase speech, and goal
    // direction. The token matching and action policy remain Creature-owned.
    class CreatureHeardWordsHost {
    public:
        virtual ~CreatureHeardWordsHost() = default;
        // The event source may be the pointer tool or another Creature.  The
        // application runtime resolves that Object-owned text buffer and
        // exposes it as mutable storage because the native tokenizer edits
        // the buffer in place.
        virtual char* mutable_words_for_event(
            const objects::QueuedObjectEvent& event) = 0;
        virtual bool is_pointer_tool(const objects::Object& object) const = 0;
        virtual PointerAttentionApi& pointer_attention_api() = 0;
        virtual CreatureSpeechPhraseHost& speech_phrase_host() = 0;
        virtual CreatureGoalDirectionHost& goal_direction_host() = 0;
    };

    // Blackboard and Creature sources carry learned-word text at different
    // native offsets.  The runtime resolves that source-specific layout and
    // returns the selected record index; Creature owns the reinforcement and
    // response policy after this boundary.
    class CreatureWordLearningHost {
    public:
        virtual ~CreatureWordLearningHost() = default;
        virtual bool resolve_word_learning_event(
            const objects::QueuedObjectEvent& event,
            char*& heard_word, std::size_t& record_index) const = 0;
    };

    // The native dispatcher optionally reports the selected creature's
    // built-in stimulus through the debug console.  Console existence,
    // object-label formatting, and MFC output remain an application edge;
    // this adapter carries only the C1 event and its already-typed context.
    class BuiltInStimulusDebugHost {
    public:
        virtual ~BuiltInStimulusDebugHost() = default;
        virtual bool debug_console_visible() const = 0;
        virtual bool is_selected_creature(const Creature& creature) const = 0;
        virtual void log_built_in_stimulus(
            const Creature& creature,
            const objects::Object* target,
            std::uint32_t stimulus_id,
            std::uint32_t magnitude,
            const StimulusContext& context) = 0;
        virtual void log_heard_words(const Creature& creature,
                                     std::string_view words) {
            (void)creature;
            (void)words;
        }
    };

    LearnedWordRecord& learned_word_record(std::size_t index) {
        return learned_word_records_[index];
    }

    const LearnedWordRecord& learned_word_record(std::size_t index) const {
        return learned_word_records_[index];
    }

    // Rebuilds the infant response for one learned word.  The multibyte
    // traversal and secure string operations are Microsoft CRT boundaries.
    void regenerate_learned_word_response(std::size_t record_index,
                                           const MultibyteTextApi& text_api);

    // Applies the native +0x3c/-0x2d reinforcement update to one learned
    // word. CRT multibyte comparison/copy and the final Speak call remain
    // explicit runtime boundaries; the thresholds, reset, and creature-only
    // speech suppression are C1 policy.
    void update_learned_word_record(
        char* heard_word,
        std::size_t record_index,
        objects::Object* source_object,
        const StimulusSourceHost& source_host,
        const MultibyteTextApi& text_api,
        CreatureSpeechHost& speech_host);

    // Assembles the native action phrase from learned responses, optionally
    // adds the name and motion-link target, emits voice timing, and queues
    // event-7 speech notifications through the runtime boundary.
    void speak_action_phrase(
        const MultibyteTextApi& text_api,
        CreatureSpeechPhraseHost& speech_host,
        CreatureSpeechHost& speech_output);

    // Selects the strongest of the sixteen native drive slots, assembles the
    // corresponding learned-response phrase, and hands speech playback to
    // the runtime. Attention-record lookup is needed only for the
    // target-directed hunger/crowdedness responses and remains a host
    // service.
    void speak_dominant_drive_phrase(
        const MultibyteTextApi& text_api,
        CreatureSpeechPhraseHost& phrase_host,
        CreatureSpeechHost& speech_output);

    // Native Speak delegates Voice playback and speech-bubble presentation
    // to the runtime Object/audio boundary in this clean model.
    void speak(std::string_view phrase, CreatureSpeechHost& speech_host);

    // Native SpeakTextWithVoice additionally returns the Voice-derived delay
    // used by speech-range event scheduling; Voice/Object presentation stays
    // behind the phrase host.
    void speak_text_with_voice(
        std::string_view phrase, std::int32_t& out_delay_world_ticks,
        CreatureSpeechPhraseHost& speech_host);

    // Destructively tokenizes a heard multibyte speech buffer, updates the
    // learned-word bank, dispatches the recovered built-in stimulus cases,
    // and commits the resulting attention/action direction. CRT traversal,
    // Object identity, pointer attention, speech, and world delivery remain
    // explicit host boundaries.
    void process_heard_words(
        objects::Object* source_object,
        char* mutable_words,
        const StimulusSourceHost& source_host,
        const MultibyteTextApi& text_api,
        CreatureHeardWordsHost& heard_words_host,
        CreatureSpeechHost& speech_output,
        BuiltInStimulusDebugHost* debug_host = nullptr,
        common::DebugLogHost* log_host = nullptr);

    // Handles native event 6 after the runtime resolves the source text
    // buffer.  Creature owns token processing and response/stimulus policy;
    // source-object layout and mutable text storage remain host-owned.
    void handle_heard_words_event(
        const objects::QueuedObjectEvent& event,
        const StimulusSourceHost& source_host,
        const MultibyteTextApi& text_api,
        CreatureHeardWordsHost& heard_words_host,
        CreatureSpeechHost& speech_output,
        BuiltInStimulusDebugHost* debug_host = nullptr,
        common::DebugLogHost* log_host = nullptr);

    // Handles native event 7.  The host recognizes the supported Blackboard
    // and Creature source layouts and exposes their selected word record;
    // Creature applies the recovered learned-word update and speech policy.
    void handle_word_learning_event(
        const objects::QueuedObjectEvent& event,
        const StimulusSourceHost& source_host,
        const MultibyteTextApi& text_api,
        const CreatureWordLearningHost& learning_host,
        CreatureSpeechHost& speech_output);

    // Selects the strongest attention target, resets the affected brain
    // lobes, wakes a sleeping creature when the target changes, and updates
    // Decision-lobe target eligibility. Object dispatch, SetAction, and
    // debug formatting remain explicit runtime services on the host.
    void update_attention(CreatureAttentionHost& host);

    // Selects and dispatches the native action classifier. The fallback to
    // quiescent is intentionally a retry loop, matching the executable when
    // a requested action has no installed script.
    void set_action(std::uint32_t action_id, CreatureAttentionHost& host);

    // Applies involuntary-action arbitration, Decision-lobe selection,
    // sleep-indicator teardown, speech choice, and activation boost in the
    // native order. Voice and speech presentation remain host services.
    void update_action_selection(
        CreatureAttentionHost& host,
        const MultibyteTextApi& text_api,
        CreatureSpeechPhraseHost& phrase_host,
        CreatureSpeechHost& speech_host);

    // Enables or removes the native SimpleObject sleep indicator. The
    // concrete object construction remains an application/runtime service;
    // Creature owns the state transition and classifier event choice.
    void set_sleep_indicator(bool enabled, CreatureAttentionHost& host);

    // Implements CAOS DONE's motor transition. The active involuntary action
    // is cancelled, the selected decision neuron is cleared when valid, and
    // the action-activation boost is reset. No object or scheduler service is
    // needed for this Creature-owned state change.
    void stop_current_involuntary_action();
    // CAOS `impt` stores the low byte of its operand as the action-activation
    // boost; the byte itself stays Creature-owned state.
    void set_action_activation_boost(std::uint8_t value) {
        action_activation_boost_ = value;
    }

    // CreateSystemInfoData @ 0x00410270 reports these three for the selected
    // creature, so they are readable state, not merely internal.
    std::uint32_t selected_action_id() const { return selected_action_id_; }
    std::uint8_t action_activation_boost() const {
        return action_activation_boost_;
    }
    // Native ParseRValue `aslp` reads the sleep-indicator state byte. Keep the
    // state query on Creature so the scripting host never reaches through the
    // native-layout Skeleton field or mistakes it for sound metadata.
    bool is_asleep() const { return skeleton_.sleep_indicator_active; }
    const objects::Object* motion_link() const { return skeleton_.motion_link; }
    // Native TOUC's -1/no-motion path clears only the selected decision-lobe
    // neuron lanes. It deliberately does not cancel the active involuntary
    // action or reset the action-activation boost; those additional writes
    // belong to DONE.
    void clear_selected_decision_neuron();
    void die(CreatureDeathHost& host);

    // Updates local environment signals, fertility/pregnancy flags, and the
    // life-stage/death transitions recovered at native 0040c8d0. The host
    // supplies only world/runtime operations that are not Creature policy.
    void update_environment_and_life_stage(
        CreatureEnvironmentHost& environment,
        common::DebugLogHost* log_host = nullptr);

    // Implements the explicit application testing command that advances one
    // real genome stage. Genome reloading and terminal death remain the
    // already-typed environment services; the stage guard and transition
    // order are Creature policy.
    bool force_age_one_stage(CreatureEnvironmentHost& environment,
                             common::DebugLogHost* log_host = nullptr);

    // Implements the shipped Testing > Instant verb vocabulary command.
    // Creature owns the learned-word writes, stage-0 promotion, dream
    // countdown, and speech choice; the environment, Microsoft CRT text
    // calls, and speech presentation remain explicit runtime services.
    void apply_instant_verb_vocabulary(
        CreatureEnvironmentHost& environment,
        const MultibyteTextApi& text_api,
        CreatureSpeechHost& speech_host);

    void update_bacterium_and_environment(
        CreatureBacteriumEnvironmentHost& host,
        common::DebugLogHost* log_host = nullptr);

    void process_dreaming(bool selected_creature = false,
                          common::DebugLogHost* log_host = nullptr);

    // Removes this creature from world-owned registries and UI/runtime
    // services while keeping the cleanup order recovered at 0040e0d0.
    void remove_from_world(CreatureRemovalHost& host);

    // Installs the executable's stock vocabulary tables into the fixed
    // learned-word bank. The name slot (16) is owned by creature naming and
    // is deliberately left untouched here.
    void initialize_default_vocabulary();

    // Seeds all 80 learned-word records from the executable's randomized
    // five-class banks. classifier_base is the recovered high-word classifier
    // value used to select the bank partition; the secure copy operation is a
    // Microsoft CRT boundary supplied by text_api.
    int initialize_learned_word_records(
        std::uint32_t classifier_base,
        const MultibyteTextApi& text_api);

    // Notifies every non-scenery object whose bounds follow this creature.
    // If at least one dependent exists, event 5 is queued for each and the
    // removal stimulus is suppressed; otherwise built-in stimulus 0 is
    // delivered through the common, already-typed stimulus sink.
    void notify_dependents_on_removal(
        objects::ObjectRegistryHost& non_scenery_objects,
        objects::ObjectImmediateEventQueueHost& immediate_events,
        const CreatureObjectIdentityHost& object_identity,
        const StimulusSourceHost& source_host,
        BuiltInStimulusDebugHost* debug_host = nullptr,
        common::DebugLogHost* log_host = nullptr);

    // Applies the recovered unbounded-world mouse placement policy. The view
    // host clears/returns pending input, while the event queue, Object visual
    // height, and pointer-tool redraw remain typed subsystem boundaries.
    void update_unbounded_world_position(
        CreatureUnboundedWorldPositionHost& world,
        objects::ObjectImmediateEventQueueHost& immediate_events);

    // Processes one successful motion-link mating opportunity. Object
    // classifier validation and offspring genome construction are host
    // services; the conception-probability decision and fertility state are
    // Creature-owned policy.
    void process_insemination(CreatureInseminationHost& host);

    void apply_goal_direction(std::int32_t attention_record_index,
                              std::int32_t action_lobe_neuron_index,
                              std::uint8_t attention_activation,
                              std::uint8_t action_commitment,
                              const CreatureGoalDirectionHost& world);
    void update_goal_direction(const CreatureGoalDirectionHost& world);
    void advance_pose_animation(
        CreaturePoseAnimationHost& world,
        objects::ObjectSoundPlaybackHost& sound_host);
    void update(CreatureUpdateHost& world,
                objects::ObjectSoundPlaybackHost& sound_host);

    // Persists the complete recovered Creature record.  Sleep-indicator
    // identity is runtime-only: despite the decompiler-shaped apparent
    // ReadObject in the native LOAD path, nine independent .exp specimens
    // prove that Voice begins immediately after the 40x16 matrix prefix.
    void serialize(CreatureArchive& archive,
                   objects::ObjectSoundPlaybackHost* sound_host = nullptr,
                   common::DebugLogHost* log_host = nullptr);

    // Restores the dynamic genomes and runtime attachment state of an
    // imported creature.  The archive host resolves CGenome identity; the
    // deserialization host owns global registries, rendering, UI, and score
    // integration.  This is the source-level owner of native 0040dda0.
    void deserialize(CreatureArchive& archive,
                     CreatureDeserializationHost& host,
                     common::DebugLogHost* log_host = nullptr);

    // Vtable slot 13, run on the edit object when a right-click drops it:
    // the Object bounds update, then the Skeleton re-plants its feet, then
    // EVENT_8 is queued self-to-self (native 0040da20).
    void queue_event_8_after_bounds_update(
        objects::ObjectMovementBoundsHost& world_host,
        objects::ObjectSoundPlaybackHost& sound_host,
        objects::ObjectImmediateEventQueueHost& event_queue);

    // Applies the native event-8 pickup policy. Vehicle attachment geometry,
    // pointer input/privilege, Object rendering, and application selection
    // stay at the explicit runtime boundary supplied by the host.
    void handle_pickup_event(const objects::QueuedObjectEvent& event,
                             CreaturePickupHost& host);

    // Applies the native event-9 drop policy: release a vehicle attachment,
    // restore default bounds when no vehicle is under the creature, or queue
    // reattachment and pointer cleanup when one is found.
    void handle_drop_event(const objects::QueuedObjectEvent& event,
                           CreatureDropHost& host);

    // Creature's Object override releases an active sleep indicator for all
    // events except the indicator's own wake/activation classifier, then
    // dispatches this Creature's classifier with the requested event byte.
    int dispatch_script_event(
        std::uint32_t event_id, objects::Object* source,
        bool force_restart, CreatureAttentionHost& attention,
        CreatureScriptDispatchHost& scripts);

    void initialize_runtime_state(const InitializationHost& host);
    void initialize_from_genome(GenomeInitializationHost& host);

    // The narrow half of the above, for a creature whose generated sprite
    // file no longer matches its gallery: native's rebuild reconstructs the
    // genome and runs Skeleton::LoadGenome alone, leaving the brain,
    // biochemistry and voice it already loaded untouched.
    bool rebuild_body_sprites(GenomeInitializationHost& host);
    void load_genome(Genome& genome);
    void select_walk_gait();
    objects::ObjectEventId click_event_id_at_world_position(int world_x,
                                                            int world_y) const;
    void get_part_center(int* out_x, int* out_y, int part_index) const;
    bool can_perceive(const objects::Object& target,
                      const CreaturePerceptionHost& world) const;
    void update_perception(
        const CreaturePerceptionHost& world,
        const StimulusSourceHost& source_host,
        BuiltInStimulusDebugHost* debug_host = nullptr);
    void update_drive_threshold_state(
        const std::array<std::int32_t, 16>& lower_thresholds,
        const std::array<std::int32_t, 16>& upper_thresholds);
    void apply_stimulus(objects::Object* source_object,
                        Creature* source_creature,
                        StimulusDescriptor descriptor,
                        StimulusChemicalIds chemical_ids,
                        StimulusChemicalAmounts chemical_amounts,
                        std::uint32_t magnitude,
                        const StimulusSourceHost& source_host,
                        common::DebugLogHost* log_host = nullptr);

    // Selects one genome-loaded built-in stimulus context, binds its target
    // and source creature, then enters the common stimulus sink.  The caller
    // supplies the object/classifier boundary used by apply_stimulus.
    void trigger_built_in_stimulus(
        std::uint32_t stimulus_id,
        objects::Object* target,
        std::uint32_t magnitude,
        const StimulusSourceHost& source_host,
        BuiltInStimulusDebugHost* debug_host = nullptr,
        common::DebugLogHost* log_host = nullptr);

    // The five status strings Creature::FormatStatusForExternalQuery @
    // 0x0040e520 loads from the string table.  Passed as data because that is
    // what they are; a resource-loading interface would add a type without
    // adding a capability.
    struct StatusStrings {
        std::string sick;          // 0xef2b
        std::string healthy;       // 0xef2c
        std::string dead;          // 0xef2d
        std::string not_pregnant;  // 0xef2f
        std::string male;          // 0xef30
    };

    // Ten '|'-separated fields, no trailing separator.  `getb ovvd` joins one
    // of these per selected creature with '&'.  The room table is passed as
    // the plain pointer/count view MapData already exposes.
    std::string format_status_for_external_query(
        const world::MapRoomTable& rooms, const StatusStrings& strings) const;

    void append_default_response_prefix(
        char* phrase_buffer, const MultibyteTextApi& text_api);
    void handle_queued_event_slot5(
        const objects::QueuedObjectEvent& event,
        CreatureScriptEventHost& event_host);
    void handle_queued_event_slot6(
        const objects::QueuedObjectEvent& event,
        CreatureScriptEventHost& event_host);
    void handle_queued_event_slot7(
        const objects::QueuedObjectEvent& event,
        CreatureScriptEventHost& event_host);

    // Returns the C1 novelty magnitude for an object's classifier slot and
    // marks that slot as observed. Classifier decoding remains an object/world
    // boundary because the clean Creature model does not own Object layout.
    std::uint32_t first_seen_stimulus_magnitude(
        const objects::Object& object,
        const StimulusSourceHost& source_host);

    bool references_object(const objects::Object* candidate) const;
    void clear_references_to_object(const objects::Object* candidate);

    brain::Brain* brain() const { return brain_.get(); }
    void set_brain(brain::Brain* value) { brain_.reset(value); }
    biochemistry::Biochemistry* biochemistry() const {
        return biochemistry_.get();
    }
    void set_biochemistry(biochemistry::Biochemistry* value) {
        biochemistry_.reset(value);
    }

    Bacterium& bacterium() { return bacterium_; }
    const Bacterium& bacterium() const { return bacterium_; }
    Skeleton& skeleton() { return skeleton_; }
    const Skeleton& skeleton() const { return skeleton_; }
    ControlState& control_state() { return instinct_runtime_state_.control_state; }
    const ControlState& control_state() const {
        return instinct_runtime_state_.control_state;
    }
    std::array<StimulusContext, kBuiltInStimulusContextCount>&
    built_in_stimulus_contexts() { return built_in_stimulus_contexts_; }
    const std::array<StimulusContext, kBuiltInStimulusContextCount>&
    built_in_stimulus_contexts() const { return built_in_stimulus_contexts_; }
    std::array<AttentionRecord, kAttentionRecordCount>& attention_records() {
        return attention_records_;
    }
    const std::array<AttentionRecord, kAttentionRecordCount>&
    attention_records() const { return attention_records_; }
    InstinctRuntimeState& instinct_runtime_state() {
        return instinct_runtime_state_;
    }
    const InstinctRuntimeState& instinct_runtime_state() const {
        return instinct_runtime_state_;
    }
    GenomeFilenameId child_genome_source_filename() const {
        return child_genome_source_filename_;
    }
    GenomeFilenameId gamete_genome_source_filename() const {
        return gamete_genome_source_filename_;
    }
    void set_child_genome_source_filename(GenomeFilenameId value) {
        child_genome_source_filename_ = value;
    }
    void set_gamete_genome_source_filename(GenomeFilenameId value) {
        gamete_genome_source_filename_ = value;
    }
    Voice& voice() { return voice_; }
    const Voice& voice() const { return voice_; }
    CreatureRegister& register_state() { return register_state_; }
    const CreatureRegister& register_state() const { return register_state_; }
    // Resolve the C1 creature-side biochemical locus map.  The native
    // CreatureLocusResolver is a four-byte subobject over this state; the
    // clean class exposes the same destinations directly instead of
    // reproducing that ABI-only vptr overlay.
    std::uint8_t* resolve_genome_locus(GenomeLocusKind kind,
                                       std::uint8_t tissue_index,
                                       std::uint8_t locus_index);
    GenomeSex genome_sex() const { return genome_sex_; }
    GenomeLifeStage genome_life_stage() const {
        return static_cast<GenomeLifeStage>(genome_life_stage_);
    }
    std::uint8_t death_state() const { return death_state_; }

    // UpdateWorld increments this Creature-owned counter after the
    // biochemistry pass. The mutation is kept here so the application host
    // does not reach into the recovered Creature record.
    std::uint32_t biochemistry_tick() const { return biochemistry_tick_; }
    void increment_biochemistry_tick() { ++biochemistry_tick_; }

    // CreatureSelectionEntry is the narrow application/UI view of the real
    // creature.  These accessors keep menu and selection code on typed game
    // state instead of reaching through an MFC object pointer or a guessed
    // field offset.
    // Native UpdateWorld owns one tick gate: the Object subobject embedded in
    // the Skeleton.  Do not maintain a second Creature-side copy; scheduler,
    // brain, perception, dreaming, and interaction paths must agree.
    bool tick_enabled() const override { return skeleton_.tick_enabled(); }
    std::string display_name() const override {
        return register_state_.history().display_name;
    }
    // The only place "is this creature dead" is spelled out.  death_state_
    // is the one archived, one-byte truth (native's death_state field);
    // there used to be a second, independently-settable `dead_` bool that
    // Creature::die() never updated on the live-death path, so a creature
    // that died during play kept reading as alive to life_state(), update(),
    // and action-selection eligibility until the world was saved and
    // reloaded (deserialize was the only place that ever derived it
    // correctly: `dead_ = death_state_ != 0`).  Removing the second field
    // makes that class of divergence impossible rather than merely fixed at
    // one call site.
    bool is_dead() const { return death_state_ != 0; }

    CreatureLifeState life_state() const override {
        return is_dead() ? CreatureLifeState::dead : CreatureLifeState::alive;
    }
    CreatureGender gender() const override {
        return genome_sex_ == GenomeSex::female ? CreatureGender::female
                                                 : CreatureGender::male;
    }
    bool has_child_genome_source() const override {
        return child_genome_source_filename_ != 0;
    }
    std::uint32_t classifier_species_family() const override {
        return skeleton_.classifier_base() & 0xffff0000U;
    }
    std::uint32_t chemical_concentration(
        std::uint32_t chemical_index) const override {
        if (biochemistry_ == nullptr || chemical_index >=
                                            biochemistry_->chemical_states().size()) {
            return 0;
        }
        return biochemistry_->chemical_states()[chemical_index].concentration;
    }
    std::uint32_t age_in_ticks() const override {
        return register_state_.age_ticks();
    }
    void set_selection_menu_command_id(std::uint32_t command_id) override {
        selection_menu_command_id_ = command_id;
    }

    std::uint32_t selection_menu_command_id() const {
        return selection_menu_command_id_;
    }

private:
    std::array<LearnedWordRecord, kLearnedWordRecordCount>
        learned_word_records_{};
    Skeleton skeleton_{};
    std::unique_ptr<brain::Brain> brain_;
    std::unique_ptr<biochemistry::Biochemistry> biochemistry_;
    std::array<StimulusContext, kBuiltInStimulusContextCount>
        built_in_stimulus_contexts_{};
    std::array<AttentionRecord, kAttentionRecordCount> attention_records_{};
    GoalDirectionState goal_direction_state_{};
    GoalDirectionWeightMatrix goal_direction_weight_matrix_{};
    InstinctRuntimeState instinct_runtime_state_{};
    GenomeFilenameId child_genome_source_filename_ = 0;
    GenomeFilenameId gamete_genome_source_filename_ = 0;
    GenomeSex genome_sex_ = GenomeSex::male;
    std::uint8_t genome_life_stage_ = 0;
    std::uint32_t biochemistry_tick_ = 0;
    std::uint8_t death_state_ = 0;
    AttentionClassifier classifier_{};
    std::uint32_t selected_action_id_ = 0;
    std::uint8_t action_activation_boost_ = 0;
    std::uint8_t active_involuntary_action_index_ = 0xff;
    std::uint32_t selection_menu_command_id_ = 0;
    objects::Object* sleep_indicator_object_ = nullptr;
    objects::Object* caos_object_pointer_ = nullptr;
    CreatureRegister register_state_{};
    Voice voice_{};
    Bacterium bacterium_{};
};

// Runtime/application services touched only by the native genome-creation
// constructor.  Creature owns the order and state transitions; population
// storage, CRT/MFC-backed resource loading, rendering, environment sampling,
// registry mutation, and selection UI remain outside the clean game module.
class CreatureConstructionHost {
public:
    virtual ~CreatureConstructionHost() = default;

    virtual Creature::InitializationHost& initialization_host() = 0;
    virtual Creature::GenomeInitializationHost& genome_initialization_host() = 0;

    virtual std::size_t living_creature_count() const = 0;
    virtual const Creature* living_creature_at(std::size_t index) const = 0;
    virtual void report_invalid_living_creature_index() const = 0;

    virtual objects::ObjectRenderableSetHost& renderable_set() = 0;
    virtual objects::ObjectMovementBoundsHost& movement_bounds() = 0;
    virtual objects::ObjectSoundPlaybackHost& sound_playback() = 0;
    virtual const MultibyteTextApi& text_api() const = 0;
    virtual CreatureEnvironmentHost& environment() = 0;

    virtual void append_to_creature_registry(Creature& creature) = 0;
    virtual void rebuild_creature_selection_menu() = 0;
};

} // namespace creatures1::creatures
