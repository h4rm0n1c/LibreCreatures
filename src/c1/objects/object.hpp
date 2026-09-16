#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "../world/geometry.hpp"
#include "../world/viewport.hpp"
#include "../sound/sound.hpp"
#include "../scripting/classifier_scripts.hpp"

namespace creatures1::scripting {
struct MacroObjectContext;
}

namespace creatures1::display {
struct Gallery;
}

namespace creatures1::creatures {
class CreatureEventFanoutHost;
}

namespace creatures1::objects {

using CaosValue = std::uint32_t;

class Object;
struct QueuedObjectEvent;
struct QueuedCreatureStimulus;

// Object owns the persisted field order.  Buffer management, MFC dynamic
// object identity, and archive exceptions remain in this adapter boundary.
// The interface is deliberately expressed in source-level terms so Object
// does not need to know about CArchive's internal cursor or vtable.
class ObjectArchive : public scripting::ScriptArchiveWriter {
public:
    virtual ~ObjectArchive() = default;
    virtual bool is_loading() const = 0;
    virtual std::uint32_t read_uint32() = 0;
    virtual std::uint8_t read_byte() = 0;
    virtual std::int32_t read_int32() = 0;
    virtual void write_uint32(std::uint32_t value) = 0;
    virtual void write_byte(std::uint8_t value) = 0;
    virtual void write_int32(std::int32_t value) = 0;
    virtual void read_bytes(void* destination, std::size_t count) = 0;
    virtual void write_bytes(const void* source, std::size_t count) = 0;
    virtual void* read_object_reference(
        std::string_view runtime_class_name) = 0;
    virtual void write_object_reference(
        const void* object, std::string_view runtime_class_name) = 0;

    // These four operations are the exact record shape emitted by
    // SerializeScriptsForClassifier/DeserializeScriptsForClassifier.
    virtual std::int32_t read_script_count() = 0;
    virtual std::uint32_t read_script_classifier() = 0;
    virtual std::string read_script_text() = 0;
};

class ObjectRegistryHost {
public:
    virtual ~ObjectRegistryHost() = default;
    virtual void add_object(Object* object) = 0;
    virtual std::size_t object_count() const = 0;
    virtual Object* object_at(std::size_t index) const = 0;
    virtual void remove_object_at(std::size_t index) = 0;
    virtual void report_invalid_index() const = 0;
};

// Destruction is a real Object state transition.  The renderable set, sound
// mixer, gallery registry, and object registry remain owners of their storage
// and platform handles; this host supplies only those release operations.
class ObjectCleanupHost {
public:
    virtual ~ObjectCleanupHost() = default;
    virtual void remove_from_renderable_set(Object& object) = 0;
    virtual void stop_continuous_sound(int sound_handle) = 0;
    virtual void release_gallery(display::Gallery& gallery) = 0;
    virtual void unregister_from_object_registry(Object& object) = 0;
};

class ObjectEventDispatchHost {
public:
    virtual ~ObjectEventDispatchHost() = default;
    virtual void dispatch_event_8(Object& object) = 0;
};

enum class ObjectEventId : std::uint32_t;

class ObjectImmediateEventQueueHost {
public:
    virtual ~ObjectImmediateEventQueueHost() = default;
    virtual void queue_immediate_event(Object& source, Object& target,
                                       ObjectEventId event_id,
                                       std::uint32_t argument) = 0;
};

// Runtime reset is an Object-owned state transition.  Event-bar membership,
// macro cancellation, creature selection, redraw, and registry insertion
// remain owned by their application subsystems.
class ObjectInitializationHost {
public:
    virtual ~ObjectInitializationHost() = default;
    virtual bool is_edit_object(const Object& object) const = 0;
    virtual void clear_edit_object() = 0;
    virtual void remove_from_event_bar(Object& object,
                                       bool remove_all_entries) = 0;
    virtual void purge_destroy_when_finished_macros(Object& object) = 0;
    virtual void move_to_and_redraw(Object& object, int world_x,
                                    int world_y) = 0;
    virtual bool is_selected_creature(const Object& object) const = 0;
    virtual void clear_selected_creature(bool notify) = 0;
    virtual void report_creature_base_function_misuse() = 0;
    virtual void rebuild_creature_selection_menu() = 0;
    virtual void add_to_world_object_registry(Object& object) = 0;
};

// Sound visibility uses only the renderer's current world viewport. Audio
// mixing remains a separate subsystem boundary; this host deliberately does
// not expose a mixer or any platform sound handles.
class ObjectSoundViewportHost {
public:
    virtual ~ObjectSoundViewportHost() = default;
    virtual world::ViewportBounds sound_viewport() const = 0;
};

// Object owns the sound visibility and descriptor transitions. The document's
// mute flag, SoundManager, and debug console are application boundaries.
class ObjectSoundPlaybackHost : public ObjectSoundViewportHost {
public:
    enum class LogEvent : std::uint32_t {
        play_immediate = 0,
        queue_sound = 1,
        sound_error = 2,
        continuous_offscreen_replace = 3,
        continuous_replace = 4,
        continuous_reentered_range = 5,
        continuous_gone_out_of_range = 6,
        continuous_finished_channel = 7,
        continuous_finished_sound = 8,
    };

    virtual ~ObjectSoundPlaybackHost() = default;
    virtual bool sounds_muted() const = 0;
    virtual sound::SoundManager& sound_manager() = 0;
    virtual bool debug_console_available() const = 0;
    virtual void log_sound_event(LogEvent event, sound::SoundId sound_id,
                                 int value) = 0;
};

// Renderable-set ownership belongs to the world renderer. Object only owns
// the mode transition that decides whether this object is indexed there.
// The set's hash buckets and allocator are deliberately not part of Object's
// source-level model.
class ObjectRenderableSetHost {
public:
    virtual ~ObjectRenderableSetHost() = default;
    virtual bool contains(const Object& object) const = 0;
    virtual void insert(Object& object) = 0;
    virtual void erase(Object& object) = 0;
};

// Map-room lookup and vehicle geometry are world/Vehicle responsibilities.
// These methods expose the values consumed by Object::update_movement_bounds
// without importing MapData or the Vehicle storage layout into object.hpp.
class ObjectMovementBoundsHost {
public:
    virtual ~ObjectMovementBoundsHost() = default;
    virtual void clear_edit_object() = 0;
    virtual void find_nearest_room_bounds_at_point(
        int world_x, int world_y, world::WorldRect& out_bounds) const = 0;
    virtual world::WorldRect vehicle_local_bounds(
        const Object& vehicle) const = 0;
    virtual int vehicle_primary_entity_x(const Object& vehicle) const = 0;
    virtual int vehicle_primary_entity_y(const Object& vehicle) const = 0;
    virtual Object* edit_object() const = 0;
};

// Overlap selection is Object policy over world-owned object storage.  The
// host supplies the pointer-tool point bounds and the special vehicle
// footprint; normal objects use their virtual GetBounds implementation.
class ObjectOverlapHost {
public:
    virtual ~ObjectOverlapHost() = default;
    virtual std::size_t object_count() const = 0;
    virtual Object* object_at(std::size_t index) const = 0;
    virtual void report_invalid_index() const = 0;
    virtual bool is_pointer_tool(const Object& object) const = 0;
    virtual world::WorldRect pointer_tool_bounds(
        const Object& pointer_tool) const = 0;
    virtual world::WorldRect vehicle_interaction_bounds(
        const Object& vehicle) const = 0;
};

// Script lookup/execution is owned by the CAOS runtime. Object supplies the
// packed classifier base and event number; the runtime supplies script
// resolution, restart policy, and execution state.
class ObjectScriptDispatchHost {
public:
    virtual ~ObjectScriptDispatchHost() = default;
    virtual int execute_script_for_classifier(
        Object& object, Object* from_object, std::uint32_t classifier,
        bool force_restart) = 0;
};

// The registry, macro scheduler, event queues, and Creature type are owned by
// their respective subsystems.  This interface exposes only the facts used by
// Object::can_be_destroyed; it keeps those owners out of the Object header and
// makes the recovered safety policy readable without decompiler placeholders.
class ObjectLifetimeHost {
public:
    virtual ~ObjectLifetimeHost() = default;

    virtual std::size_t object_count() const = 0;
    virtual Object* object_at(std::size_t index) const = 0;
    virtual void report_invalid_index() const = 0;

    virtual std::size_t running_macro_count() const = 0;
    virtual const scripting::MacroObjectContext* running_macro_at(
        std::size_t index) const = 0;

    virtual std::size_t immediate_event_count() const = 0;
    virtual const QueuedObjectEvent* immediate_event_at(
        std::size_t index) const = 0;
    virtual std::size_t delayed_event_count() const = 0;
    virtual const QueuedObjectEvent* delayed_event_at(
        std::size_t index) const = 0;
    virtual bool delayed_event_is_active(
        const QueuedObjectEvent& event) const = 0;

    virtual std::size_t queued_stimulus_count() const = 0;
    virtual const QueuedCreatureStimulus* queued_stimulus_at(
        std::size_t index) const = 0;
    virtual bool stimulus_targets_object(
        const QueuedCreatureStimulus& stimulus,
        const Object& object) const = 0;
};

enum class ObjectEventId : std::uint32_t {
    no_event = 0xffffffffU,
    event_0 = 0,
    event_1 = 1,
    event_2 = 2,
    event_3 = 3,
    event_4 = 4,
    event_5 = 5,
    event_6 = 6,
    event_7 = 7,
    event_8 = 8,
    event_9 = 9,
};

enum class SoundAudibilityState : std::uint32_t {
    out_of_range = 0,
    active_viewport = 1,
    extended_range = 2,
};

class Object {
public:
    // Native storage is one byte; callers pass the promoted 32-bit value.
    // Values 0..4 are the modes recovered from the live SetBoundsMode
    // comparisons. Unknown byte values remain representable for fidelity.
    enum class BoundsMode : std::uint8_t {
        default_world = 0,
        unbounded_1 = 1,
        unbounded_2 = 2,
        vehicle_local = 3,
        explicit_rectangle = 4,
    };

    static std::unique_ptr<Object> create_object(
        ObjectRegistryHost* registry = nullptr);

    explicit Object(ObjectRegistryHost* registry = nullptr,
                    ObjectCleanupHost* cleanup = nullptr);
    virtual ~Object();

    // Base-object virtual defaults. Derived object types override these when
    // they own images, movement, bounds, or part-center information.
    virtual int relative_image_index(int part_index) const;
    virtual bool image_sequence_is_empty(int part_index) const;
    virtual void move_by(int delta_x, int delta_y);
    virtual bool get_bounds(world::WorldRect* out_bounds) const;
    virtual void get_part_center(int* out_x, int* out_y,
                                 std::int32_t part_index) const;
    virtual int render_plane() const;
    virtual int sound_source_x() const;
    virtual int sound_source_y() const;
    virtual int current_visual_width() const;
    virtual int current_visual_height() const;
    virtual char* parse_image_sequence(char* sequence_text, int part_index);
    virtual bool set_relative_image_index(CaosValue relative_index,
                                          int part_index);
    virtual char* preload_image_sequence(char* sequence_text, int part_index);

    virtual ObjectEventId click_event_id_at_world_position(int world_x,
                                                           int world_y) const;
    virtual bool references_object(Object* candidate) const;

    // Archives the recovered Object base record. Derived serializers call
    // this first, matching the native inheritance order.
    void serialize(ObjectArchive& archive);

    using BoundsFlags = std::uint8_t;
    // SimpleObject::HandleQueuedEvent4 @00427cc0 tests the low two bits of
    // the bounds-flag byte at Object+0x9; UpdateMovementBounds @00426470
    // tests 0x40 there before consulting the map room.
    static constexpr BoundsFlags kAllowCreatureExplicitRectBounds = 0x01u;
    static constexpr BoundsFlags kAllowPointerToolUnboundedPlacement = 0x02u;
    static constexpr BoundsFlags kActivatable = 0x04u;
    static constexpr BoundsFlags kIsVehicle = 0x08u;
    // "Wallbound" in the CAOS Attributes table: limits movement to the
    // current room.  UpdateMovementBounds @00426470 tests it before doing
    // the map-room lookup, so it is what gives a creature a real floor.
    static constexpr BoundsFlags kUseCurrentMapRoom = 0x40u;

    bool has_bounds_flag(BoundsFlags mask) const {
        return (bounds_flags_ & mask) == mask;
    }

    BoundsMode bounds_mode() const { return bounds_mode_; }

    // The world scheduler reads the native per-object tick gate before
    // dispatching the concrete object tick. Keep that byte-owned state
    // visible through a typed accessor; the scheduler must not infer it from
    // timer fields or a decompiler-shaped offset.
    bool tick_enabled() const { return tick_enabled_; }

    // The packed classifier is an Object-owned value.  Presentation code may
    // inspect it without reaching into the recovered storage layout.
    std::uint32_t classifier_base() const;

    // CAOS addresses these fields by name.  Keep the recovered storage
    // private while exposing the same source-level operations to Macro.
    void set_classifier_base(std::uint32_t packed_classifier) {
        set_classifier_components(
            static_cast<std::uint8_t>(packed_classifier),
            static_cast<std::uint8_t>(packed_classifier >> 8),
            static_cast<std::uint8_t>(packed_classifier >> 16),
            static_cast<std::uint8_t>(packed_classifier >> 24));
    }

    std::uint32_t object_variable(std::size_t index) const {
        switch (index) {
        case 0: return caos_object_variable_0_;
        case 1: return caos_object_variable_1_;
        case 2: return caos_object_variable_2_;
        default: return 0;
        }
    }

    void set_object_variable(std::size_t index, std::uint32_t value) {
        switch (index) {
        case 0: caos_object_variable_0_ = value; break;
        case 1: caos_object_variable_1_ = value; break;
        case 2: caos_object_variable_2_ = value; break;
        default: break;
        }
    }

    Object* caos_object_pointer() const { return caos_object_pointer_; }
    void set_caos_object_pointer(Object* object) { caos_object_pointer_ = object; }

    Object* find_topmost_overlapping_object(
        BoundsFlags bounds_flag_mask, BoundsFlags bounds_flag_value,
        const ObjectOverlapHost& world) const;

    void unregister_from_non_scenery_object_registry(
        ObjectRegistryHost& registry);
    void dispatch_event_8(ObjectEventDispatchHost& dispatcher);
    void queue_event_8_after_bounds_update(
        ObjectMovementBoundsHost& world_host,
        ObjectImmediateEventQueueHost& event_queue);
    void dispatch_event_7_after_bounds_update(
        ObjectMovementBoundsHost& world_host,
        ObjectScriptDispatchHost& scripts);
    void initialize_runtime_state(ObjectInitializationHost& runtime);
    void set_deletion_movement_bounds();
    // Creature is represented in the non-scenery registry by its embedded
    // Skeleton/Object.  Its construction and archive paths need the same
    // explicit enable transition as the native Object state.
    void enable_ticking();
    void disable_ticking();
    void compute_sound_attenuation_and_pan(
        const ObjectSoundViewportHost& renderer, int& out_attenuation,
        int& out_pan) const;
    SoundAudibilityState sound_audibility_state(
        const ObjectSoundViewportHost& renderer) const;
    void play_sound_effect(sound::SoundId sound_id, int queue_delay_ticks,
                           bool force_during_archive,
                           ObjectSoundPlaybackHost& playback);
    void set_continuous_sound(sound::SoundId sound_id,
                              bool persist_when_out_of_range,
                              ObjectSoundPlaybackHost& playback);
    void fade_continuous_sound(ObjectSoundPlaybackHost& playback);
    void stop_continuous_sound(ObjectSoundPlaybackHost& playback);
    void update_sound(ObjectSoundPlaybackHost& playback);
    void set_bounds_mode(std::uint32_t requested_mode,
                         ObjectRenderableSetHost& renderables);
    void update_movement_bounds(ObjectMovementBoundsHost& world_host);
    int dispatch_script_event(ObjectEventId event_id,
                              Object* from_object,
                              bool force_restart,
                              ObjectScriptDispatchHost& scripts);
    void clear_reference_and_set_default_bounds(
        ObjectRenderableSetHost& renderables);
    void handle_queued_creature_event(
        const QueuedObjectEvent& event,
        creatures1::creatures::CreatureEventFanoutHost& fanout);
    void handle_queued_event_3(
        const QueuedObjectEvent& event,
        creatures1::creatures::CreatureEventFanoutHost& fanout);
    bool can_be_destroyed(const ObjectLifetimeHost& runtime) const;

    bool is_sound_source_below_world_y() const;

    // CAOS `baby` reads this rectangle's lower edge and writes it back; the
    // egg movement limit is the only field that command touches.
    void set_movement_bounds_max_y(int value) {
        movement_bounds_.max_y = value;
    }
    const world::WorldRect& movement_bounds() const {
        return movement_bounds_;
    }

    // Interactive drops must not retain the map lookup's no-room sentinel.
    // The native object stores the resulting rectangle directly before the
    // final MoveToAndRedraw call; keep that recovery operation object-owned.
    void set_world_movement_bounds_for_drop() {
        movement_bounds_ = {0, 0, world::kWorldWidth, world::kWorldHeight};
    }

    std::uint32_t continuous_sound_descriptor() const {
        return continuous_sound_descriptor_;
    }

    std::int32_t timer_countdown_for_script() const {
        return timer_countdown_;
    }

    // CAOS `tick` writes the same parsed 32-bit value to both timer fields.
    // Keep that command-facing mutation on Object; the interpreter only owns
    // operand parsing and target selection.
    void set_timer_for_script(std::int32_t value) {
        timer_countdown_ = value;
        timer_period_ = value;
    }

    std::uint32_t script_bounds_flags() const {
        return bounds_flags_;
    }

    // CE VY is a packed view of the four native bytes beginning at the
    // bounds-mode field.  The final two bytes are not part of the archived
    // Object record, but the native CAOS interpreter reads and writes them;
    // retain them explicitly instead of collapsing them into an opaque word.
    std::uint32_t packed_bounds_state_for_script() const {
        return static_cast<std::uint32_t>(bounds_mode_) |
               (static_cast<std::uint32_t>(bounds_flags_) << 8) |
               (static_cast<std::uint32_t>(bounds_state_reserved_0_) << 16) |
               (static_cast<std::uint32_t>(bounds_state_reserved_1_) << 24);
    }

    void set_packed_bounds_state_for_script(std::uint32_t value) {
        bounds_mode_ = static_cast<BoundsMode>(value);
        bounds_flags_ = static_cast<std::uint8_t>(value >> 8);
        bounds_state_reserved_0_ = static_cast<std::uint8_t>(value >> 16);
        bounds_state_reserved_1_ = static_cast<std::uint8_t>(value >> 24);
    }

    void set_bounds_flags_for_script(std::uint32_t value) {
        bounds_flags_ = static_cast<std::uint8_t>(value);
    }

    // The recovered field is one byte at offset 0x24.  UpdateAllCreatureBrainInputs
    // @ 00432ee0 tests that byte alone, so the read is public while the packed
    // four-byte script view below stays a separate accessor.
    std::uint32_t current_interaction_event_id() const {
        return current_interaction_event_id_;
    }

    std::uint32_t current_interaction_event_id_for_script() const {
        return static_cast<std::uint32_t>(current_interaction_event_id_) |
               (static_cast<std::uint32_t>(current_interaction_event_reserved_0_) << 8) |
               (static_cast<std::uint32_t>(current_interaction_event_reserved_1_) << 16) |
               (static_cast<std::uint32_t>(current_interaction_event_reserved_2_) << 24);
    }

    void set_current_interaction_event_id_for_script(std::uint32_t value) {
        current_interaction_event_id_ = static_cast<std::uint8_t>(value);
        current_interaction_event_reserved_0_ = static_cast<std::uint8_t>(value >> 8);
        current_interaction_event_reserved_1_ = static_cast<std::uint8_t>(value >> 16);
        current_interaction_event_reserved_2_ = static_cast<std::uint8_t>(value >> 24);
    }

    bool uses_unbounded_world_position() const {
        return bounds_mode_ == BoundsMode::unbounded_1;
    }

    // Both unbounded modes hold a position that is relative rather than
    // absolute, so both need a renderable-set entry.  Callers that mean
    // "mid-placement, following the mouse" want the stricter predicate above.
    bool uses_relative_world_position() const {
        return bounds_mode_ == BoundsMode::unbounded_1 ||
               bounds_mode_ == BoundsMode::unbounded_2;
    }

    // This relationship is Object-owned state. Registry traversal and
    // removal notification remain world/application responsibilities.
    Object* bounds_reference_object() const {
        return bounds_reference_object_;
    }

    void set_bounds_reference_object(Object* reference) {
        bounds_reference_object_ = reference;
    }

    // Native Creature::InitializeRuntimeState @00408520 ORs a bit into this
    // byte directly, because a Creature *is* its Object there.  The port
    // composes the Object into Skeleton, so the owner needs a way to reach
    // the byte without shadowing it on the derived class.
    void merge_bounds_flags(BoundsFlags mask) { bounds_flags_ |= mask; }

    // The native Object record owns these references; the registries and
    // platform handles remain application-owned boundaries.
    display::Gallery* gallery() const { return gallery_; }
    void set_gallery(display::Gallery* gallery) { gallery_ = gallery; }
    std::int32_t continuous_sound_handle() const { return sound_handle_; }
    // CAOS variable slot zero is object-owned runtime state.  Most object
    // types treat it as a general-purpose variable; Blackboard uses it as
    // the selected word index while editing or displaying its label.
    std::uint32_t object_variable_0() const {
        return caos_object_variable_0_;
    }
    void clear_continuous_sound_state() {
        sound_handle_ = -1;
        continuous_sound_descriptor_ = 0;
        continuous_sound_persist_flag_ = false;
    }
    bool continuous_sound_persists() const {
        return continuous_sound_persist_flag_;
    }
    // The world-pause sweep in SFCDoc::ServiceWorldUpdateTimer @ 0x004336d0
    // releases the channel but keeps the persist flag, and keeps the
    // descriptor when the sound is marked to persist -- so the sound can be
    // restarted on resume.  clear_continuous_sound_state drops all three and
    // is the wrong operation here.
    void release_continuous_sound_channel() {
        sound_handle_ = -1;
        if (!continuous_sound_persist_flag_) {
            continuous_sound_descriptor_ = 0;
        }
    }

    virtual void clear_references_to(Object* candidate);

protected:
    // Derived constructors copy the classifier and bounds flags after the
    // base Object state has been initialized.
    void set_classifier_components(std::uint8_t event,
                                   std::uint8_t species,
                                   std::uint8_t genus,
                                   std::uint8_t family);
    void set_bounds_flags(std::uint8_t flags) { bounds_flags_ = flags; }
    void set_classifier_family(std::uint8_t family);
    BoundsFlags bounds_flags() const { return bounds_flags_; }
    void set_current_interaction_event_id(std::uint32_t value) {
        current_interaction_event_id_ = value;
    }
    std::int32_t timer_period() const { return timer_period_; }
    std::int32_t timer_countdown() const { return timer_countdown_; }
    void set_timer_countdown(std::int32_t value) {
        timer_countdown_ = value;
    }

private:
    struct Classifier {
        std::uint8_t event = 0;
        std::uint8_t species = 0;
        std::uint8_t genus = 0;
        std::uint8_t family = 0;
    };

    Classifier classifier_{};
    BoundsMode bounds_mode_ = BoundsMode::default_world;
    std::uint8_t bounds_flags_ = 0;
    std::uint8_t bounds_state_reserved_0_ = 0;
    std::uint8_t bounds_state_reserved_1_ = 0;
    world::WorldRect movement_bounds_{};
    Object* bounds_reference_object_ = nullptr;
    std::uint8_t current_interaction_event_id_ = 0;
    std::uint8_t current_interaction_event_reserved_0_ = 0;
    std::uint8_t current_interaction_event_reserved_1_ = 0;
    std::uint8_t current_interaction_event_reserved_2_ = 0;
    std::uint32_t caos_object_variable_0_ = 0;
    std::uint32_t caos_object_variable_1_ = 0;
    std::uint32_t caos_object_variable_2_ = 0;
    std::int32_t timer_period_ = 0;
    std::int32_t timer_countdown_ = 0;
    bool tick_enabled_ = true;
    std::int32_t sound_handle_ = -1;
    std::uint32_t continuous_sound_descriptor_ = 0;
    bool continuous_sound_persist_flag_ = false;
    Object* caos_object_pointer_ = nullptr;
    display::Gallery* gallery_ = nullptr;
    ObjectRegistryHost* registry_ = nullptr;
    ObjectCleanupHost* cleanup_ = nullptr;
};

} // namespace creatures1::objects
