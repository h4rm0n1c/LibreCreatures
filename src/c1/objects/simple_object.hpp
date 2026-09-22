#pragma once

#include "entity.hpp"
#include "object.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace creatures1::objects {

struct QueuedObjectEvent;

class SimpleObject;
class CompoundObject;
class Bubble;
class BubbleConstructionHost;
enum class BubblePlacementMode : std::uint8_t;

class SimpleObjectBubbleHost {
public:
    virtual ~SimpleObjectBubbleHost() = default;
    virtual int viewport_left() const = 0;
    virtual int viewport_right() const = 0;
    virtual BubbleConstructionHost& bubble_construction() = 0;
};

// Tick policy is owned by SimpleObject. The world owns only the input and
// presentation effects needed by the two recovered specialised paths; their
// movement, animation, centering, wrapping, and bounds policy live below.
class SimpleObjectTickHost : public virtual ObjectSoundPlaybackHost,
                             public virtual ObjectRenderableSetHost,
                             public virtual ObjectScriptDispatchHost,
                             public virtual EntityImageSequenceRenderHost {
public:
    ~SimpleObjectTickHost() override = default;

    virtual bool pointer_input_pending() const = 0;
    virtual void process_pending_pointer_input(SimpleObject& pointer_tool) = 0;
    virtual SimpleObject* pointer_tool() const = 0;
    virtual int pointer_mouse_world_x() const = 0;
    virtual int pointer_mouse_world_y() const = 0;

    // The renderer decides whether a dirty rectangle is immediately
    // presented or merged into its deferred queue. SimpleObject supplies the
    // exact native rectangle(s), including wrapped-world coordinates.
    virtual void present_or_queue_dirty_world_rect(
        const world::WorldRect& dirty_rect) = 0;

    // A carried object is anchored and planed from three raw offsets on its
    // bounds reference, which the native reads without regard to the
    // reference's actual type:
    //
    //   [ref + 0xdc]        anchor x
    //   [ref + 0xf4]        anchor y
    //   [ref + 0x78] byte   index into {-1, 1, 1, -1} for the plane offset
    //
    // On a CompoundObject those are part_bounds[1].min_x, part_bounds[2].max_x
    // and the low byte of parts[3].entity.  On a Skeleton -- a creature
    // holding something -- the same offsets are limb_chain_end_x[4],
    // limb_chain_end_y[4] and facing_direction: the right-arm chain end, i.e.
    // the hand, and a 0..3 facing that indexes the table exactly.  The host
    // resolves the reference's real type; this object only consumes the
    // result.
    virtual bool carried_object_anchor(const Object& reference, int& out_x,
                                       int& out_y) const = 0;
    virtual int carried_object_render_plane_offset(
        const Object& reference) const = 0;
};

// The native byte is a bit mask.  The event numbers are intentionally not
// used as bit positions: C1 stores event 1, event 2, and event 0 in bits
// 0, 1, and 2 respectively.
enum class InteractionEventFlag : std::uint8_t {
    event_1_enabled = 0x01,
    event_2_enabled = 0x02,
    event_0_enabled = 0x04,
};

// Loading a native unbounded SimpleObject performs a placement pass after
// the archive tail is restored.  The renderer/world owner supplies that pass;
// SimpleObject retains only the recovered state transition and ordering.
// Gallery acquisition, Entity registration, and movement-bound recompute are
// owner services. The constructor keeps the native object-assembly order
// while leaving those registries and file/cache details outside SimpleObject.
class SimpleObjectConstructionHost {
public:
    virtual ~SimpleObjectConstructionHost() = default;
    virtual display::Gallery* acquire_gallery(
        std::uint32_t sprite_file_id, int header_record_index,
        std::uint32_t image_count, bool cache_protected) = 0;
    virtual EntityRegistryHost& entity_registry() = 0;
    virtual void update_movement_bounds(SimpleObject& object) = 0;
};

// The world renderer owns viewport clipping, wrapping, immediate presentation,
// and the deferred dirty-rectangle queue. SimpleObject supplies old/new bounds.
class SimpleObjectMoveRedrawHost {
public:
    virtual ~SimpleObjectMoveRedrawHost() = default;
    virtual void redraw_after_simple_object_move(
        SimpleObject& object, const world::WorldRect& old_bounds,
        const world::WorldRect& new_bounds) = 0;
};

// Interaction transitions are SimpleObject policy over world-owned queues,
// registries, overlap geometry, pointer input, and redraw. Keeping those
// services together makes the recovered EVENT_4/EVENT_5 state machine
// readable without importing the native globals or MFC container layout.
class SimpleObjectInteractionHost : public virtual ObjectRenderableSetHost,
                                    public virtual ObjectMovementBoundsHost,
                                    public virtual ObjectOverlapHost,
                                    public virtual ObjectImmediateEventQueueHost,
                                    public virtual ObjectScriptDispatchHost,
                                    public virtual SimpleObjectMoveRedrawHost {
public:
    ~SimpleObjectInteractionHost() override = default;
    virtual Object* pointer_tool() const = 0;
    virtual int pointer_world_x() const = 0;
    virtual int pointer_world_y() const = 0;
    virtual std::size_t non_scenery_object_count() const = 0;
    virtual Object* non_scenery_object_at(std::size_t index) const = 0;
    virtual void report_invalid_non_scenery_index() const = 0;
};

// Placement during edit/load uses the same redraw path as ordinary movement.
// The world supplies the live mouse position and renderable-set transition;
// SimpleObject retains the recovered bounds, plane, wrapping, and ordering.
class SimpleObjectPlacementHost : public virtual SimpleObjectMoveRedrawHost {
public:
    ~SimpleObjectPlacementHost() override = default;
    virtual ObjectRenderableSetHost& renderables() = 0;
    virtual int mouse_world_x() const = 0;
    virtual int mouse_world_y() const = 0;
};

// Finalizing an editor placement crosses the same narrow world services as
// Object's bounds update, plus the immediate event queue and redraw path.
// SimpleObject retains the recovered ordering and privilege policy; the
// editor singleton, renderable index, and queue storage remain host-owned.
class SimpleObjectEditHost : public virtual ObjectMovementBoundsHost,
                             public virtual ObjectRenderableSetHost,
                             public virtual ObjectImmediateEventQueueHost,
                             public virtual SimpleObjectMoveRedrawHost {
public:
    ~SimpleObjectEditHost() override = default;
    virtual int privilege_level() const = 0;
};

// A SimpleObject owns one drawable Entity.  The three signed selectors are
// indexed by Object's current interaction state; the byte-sized storage and
// signed result are properties confirmed by the native record and method.
class SimpleObject : public Object {
public:
    static constexpr std::size_t kClickEventCount = 3;

    SimpleObject();
    ~SimpleObject() override;

    std::unique_ptr<Bubble> create_bubble(
        std::string_view bubble_text, std::uint8_t lifetime_ticks,
        BubblePlacementMode placement_mode, SimpleObjectBubbleHost& host);
    void tick(SimpleObjectTickHost& host);
    void update_unbounded_position_and_redraw(SimpleObjectTickHost& host);
    void update_entity_for_explicit_rect_bounds_and_redraw(
        SimpleObjectTickHost& host);
    void handle_queued_event_4(
        const QueuedObjectEvent& event,
        creatures1::creatures::CreatureEventFanoutHost& fanout,
        SimpleObjectInteractionHost& host);
    void handle_queued_event_5(
        const QueuedObjectEvent& event, SimpleObjectInteractionHost& host);

    SimpleObject(std::uint32_t sprite_file_id, int header_record_index,
                 std::uint32_t image_count, bool cache_protected,
                 int initial_world_x, int initial_world_y, int render_plane,
                 std::uint8_t bounds_flags,
                 std::uint8_t classifier_event,
                 std::uint8_t classifier_species,
                 std::uint8_t classifier_genus,
                 std::uint8_t classifier_family,
                 std::uint8_t click_event_selector_0,
                 std::uint32_t reserved_word_0,
                 std::uint32_t reserved_word_1,
                 std::uint8_t interaction_event_flags,
                 SimpleObjectConstructionHost& construction);

    Entity* entity() { return entity_.get(); }
    const Entity* entity() const { return entity_.get(); }
    void set_entity(std::unique_ptr<Entity> entity) {
        entity_ = std::move(entity);
    }

    void serialize(ObjectArchive& archive);
    // Applies one of the native five BHVR selector records and the low-byte
    // interaction mask to this object's interaction state.
    void configure_interaction_behavior(std::uint32_t behavior_index,
                                        std::uint32_t interaction_flags);
    void initialize_unbounded_object_placement(
        SimpleObjectPlacementHost& placement);
    void finalize_object_edit(SimpleObjectEditHost& edit_host);

    // Interaction handlers retain the native three-state rotation:
    // queued event 0 selects script event 1, event 1 selects event 2, and
    // event 2 selects event 0.  Creature stimulus storage/queueing and CAOS
    // script lookup remain runtime-owned services.
    void handle_queued_event_0(
        const QueuedObjectEvent& event,
        creatures1::creatures::CreatureEventFanoutHost& fanout,
        ObjectScriptDispatchHost& scripts);
    void handle_queued_event_1(
        const QueuedObjectEvent& event,
        creatures1::creatures::CreatureEventFanoutHost& fanout,
        ObjectScriptDispatchHost& scripts);
    void handle_queued_event_2(
        const QueuedObjectEvent& event,
        creatures1::creatures::CreatureEventFanoutHost& fanout,
        ObjectScriptDispatchHost& scripts);

    int sound_source_x() const override;
    int sound_source_y() const override;
    int current_visual_width() const override;
    int current_visual_height() const override;

    void move_to(int world_x, int world_y);
    void move_by(int delta_x, int delta_y) override;
    void move_to_and_redraw(int world_x, int world_y,
                            SimpleObjectMoveRedrawHost& renderer);
    void move_by_and_redraw(int delta_x, int delta_y,
                            SimpleObjectMoveRedrawHost& renderer);
    bool get_bounds(world::WorldRect* out_bounds) const override;
    int render_plane() const override;

    ObjectEventId click_event_id_at_world_position(
        int world_x, int world_y) const override;
    void get_part_center(int* out_x, int* out_y,
                         std::int32_t part_index) const override;

    char* parse_image_sequence(char* sequence_text, const char* sequence_end,
                               int part_index) override;
    bool image_sequence_is_empty(int part_index) const override;
    int relative_image_index(int part_index) const override;

    // The native virtual takes only the text and part index; image-cache
    // ownership is supplied by this clean-room overload so the recovered
    // preload walk has an explicit source-level boundary.
    char* preload_image_sequence(char* sequence_text, const char* sequence_end,
                                 int part_index,
                                 ImagePreloadHost& preload_host) const;

    // Preserve the native Entity::*AndRedraw side effects without importing
    // renderer/window ownership into SimpleObject.
    bool set_relative_image_index(CaosValue relative_index,
                                  int part_index,
                                  EntityImageSequenceRenderHost& redraw_host);
    void set_image_index(std::uint8_t image_index,
                         EntityImageSequenceRenderHost& redraw_host);

    std::array<std::int8_t, kClickEventCount>
        click_event_for_interaction_state{};
    std::uint8_t interaction_event_flags = 0;
    int saved_entity_render_plane = 0;

    // Also run by Creature::RemoveFromWorld on everything the creature is
    // carrying or riding, which is why it is not private.
    void end_interaction_with_source(
        Object* source_object, SimpleObjectInteractionHost& host);

private:
    void handle_queued_event(
        const QueuedObjectEvent& event,
        std::uint32_t expected_interaction_state,
        std::uint8_t required_event_flag, ObjectEventId script_event,
        creatures1::creatures::CreatureEventFanoutHost& fanout,
        ObjectScriptDispatchHost& scripts);
    static int wrap_world_x_once(int x);

    std::unique_ptr<Entity> entity_;
};

} // namespace creatures1::objects
