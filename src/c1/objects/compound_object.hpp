#pragma once

#include "entity.hpp"
#include "object.hpp"
#include "events.hpp"
#include "../creatures/events.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace creatures1::objects {

class CompoundObject;

class CompoundObjectTickHost : public virtual ObjectSoundPlaybackHost {
public:
    virtual ~CompoundObjectTickHost() = default;
    virtual void dispatch_timer_event(CompoundObject& object) = 0;
    virtual void advance_image_sequence(Entity& entity) = 0;
};

class CompoundObjectMoveRedrawHost {
public:
    virtual ~CompoundObjectMoveRedrawHost() = default;
    virtual void redraw_after_compound_object_move(
        CompoundObject& object, const world::WorldRect& old_bounds,
        const world::WorldRect& new_bounds) = 0;
    virtual void queue_compound_object_dirty_rect(
        CompoundObject& object, const world::WorldRect& primary_bounds) = 0;
};

class CompoundObjectLifetimeHost {
public:
    virtual ~CompoundObjectLifetimeHost() = default;
    virtual void remove_from_renderable_set(CompoundObject& object) = 0;
    virtual void stop_continuous_sound(int sound_handle) = 0;
    virtual void release_gallery(display::Gallery& gallery) = 0;
    virtual void unregister_from_object_registry(CompoundObject& object) = 0;
};

// Gallery acquisition is part of the native gallery-owning constructor. The
// same owner also supplies the release operations used when the compound
// object is destroyed, so the clean constructor has one explicit lifetime
// boundary rather than a hidden global gallery lookup.
class CompoundObjectConstructionHost : public CompoundObjectLifetimeHost {
public:
    virtual ~CompoundObjectConstructionHost() = default;
    virtual display::Gallery* acquire_gallery(
        std::uint32_t sprite_file_id, int header_record_index,
        std::uint32_t image_count, bool cache_protected) = 0;
};

// Compound interaction dispatch crosses three owners in the native binary:
// Object's current-event state, the Creature/script runtime, and the global
// stimulus queue. The host supplies those owners without making CompoundObject
// depend on Creature's storage layout.
class CompoundObjectEventHost {
public:
    virtual ~CompoundObjectEventHost() = default;
    virtual bool source_is_creature(const Object& source) const = 0;
    virtual int dispatch_script_event(CompoundObject& target,
                                      Object* source,
                                      ObjectEventId event_id) = 0;
    virtual bool copy_built_in_stimulus(
        const Object& source, Object& target, std::uint32_t stimulus_index,
        QueuedCreatureStimulus& out) const = 0;
    virtual void queue_creature_stimulus(
        const QueuedCreatureStimulus& stimulus) = 0;
};

struct CompoundPart {
    std::unique_ptr<Entity> entity;
    int local_x_offset = 0;
    int local_y_offset = 0;
};

struct CompoundObjectCreatureEventConfig {
    // -1 means that the interaction event has no configured part-bound
    // override. These values are also checked by the queued-event path.
    //
    // Six, not three.  CompoundObject::Serialize @ 00424b30 runs its config
    // loop until `5 < index` after the increment, which is six iterations in
    // both the read and write branches, and re_work/tools/parse_sfc.py reads
    // six here while parsing a real World.sfc to remaining=0.  The Ghidra
    // plate said three and the port inherited that; three under-reads twelve
    // bytes per CompoundObject and desynchronises the whole document stream.
    std::array<int, 6> event_config_value{{-1, -1, -1, -1, -1, -1}};
};

struct CompoundObjectClickEventBoundsIndex {
    std::array<int, 3> part_bounds_index{{-1, -1, -1}};
};

class CompoundObject : public Object {
public:
    static constexpr std::size_t kPartCapacity = 10;
    static constexpr std::size_t kPartBoundsCount = 6;
    static constexpr std::uint8_t kClassifierFamily = 3;

    explicit CompoundObject(
        CompoundObjectLifetimeHost* lifetime_host = nullptr);
    CompoundObject(std::uint32_t sprite_file_id, int header_record_index,
                   std::uint32_t image_count, bool cache_protected,
                   CompoundObjectConstructionHost& construction);
    ~CompoundObject() override;

    CompoundObject(const CompoundObject&) = delete;
    CompoundObject& operator=(const CompoundObject&) = delete;

    void set_lifetime_host(CompoundObjectLifetimeHost* lifetime_host) {
        lifetime_host_ = lifetime_host;
    }
    void clear_owned_parts_and_gallery(CompoundObjectLifetimeHost& host);

    void initialize_part_storage();

    int sound_source_x() const override;
    int sound_source_y() const override;
    int current_visual_width() const override;
    int current_visual_height() const override;
    void move_by(int delta_x, int delta_y) override;
    void move_to(int world_x, int world_y);
    void move_by_and_redraw(int delta_x, int delta_y,
                            CompoundObjectMoveRedrawHost& renderer);
    void tick(CompoundObjectTickHost& host);
    void queue_primary_part_dirty_rect(CompoundObjectMoveRedrawHost& renderer);
    void move_to_and_redraw(int world_x, int world_y,
                            CompoundObjectMoveRedrawHost& renderer);
    bool get_bounds(world::WorldRect* out_bounds) const override;
    int render_plane() const override;

    void serialize(ObjectArchive& archive);

    ObjectEventId click_event_id_at_world_position(int world_x,
                                                   int world_y) const override;
    void get_part_center(int* out_world_x, int* out_world_y,
                         std::int32_t creature_event_index) const override;

    void handle_queued_event_0(
        const QueuedObjectEvent& event,
        CompoundObjectEventHost& host);
    void handle_queued_event_1(
        const QueuedObjectEvent& event,
        CompoundObjectEventHost& host);
    void handle_queued_event_2(
        const QueuedObjectEvent& event,
        CompoundObjectEventHost& host);

    bool set_relative_image_index(CaosValue relative_index, int part_index,
                                  EntityImageSequenceRenderHost& redraw_host);
    void set_image_index(std::uint8_t image_index, int part_index,
                         EntityImageSequenceRenderHost& redraw_host);

    char* parse_image_sequence(char* sequence_text, int part_index) override;
    bool image_sequence_is_empty(int part_index) const override;
    int relative_image_index(int part_index) const override;
    char* preload_image_sequence(char* sequence_text, int part_index,
                                 ImagePreloadHost& preload_host) const;

    int part_count() const { return part_count_; }
    CompoundPart& part(std::size_t index) { return parts_[index]; }
    const CompoundPart& part(std::size_t index) const { return parts_[index]; }
    world::WorldRect& part_bounds(std::size_t index) {
        return part_bounds_[index];
    }
    const world::WorldRect& part_bounds(std::size_t index) const {
        return part_bounds_[index];
    }
    void set_part_bounds(std::size_t index, const world::WorldRect& bounds);

    // Native `knob` stores six words at one contiguous boundary: entries
    // 0..2 select creature-event bounds and entries 3..5 select the three
    // clickable hand hotspots. Keep the two typed views synchronized here.
    void set_knob_function(std::size_t function_index, int hotspot_index);

    CompoundObjectCreatureEventConfig creature_event_config{};
    CompoundObjectClickEventBoundsIndex click_event_bounds_index{};

private:
    static int wrap_world_x_once(int x);
    void handle_queued_event(
        const QueuedObjectEvent& event, std::size_t config_index,
        ObjectEventId interaction_event, ObjectEventId script_event,
        CompoundObjectEventHost& host);

    int part_count_ = 0;
    std::array<CompoundPart, kPartCapacity> parts_{};
    std::array<world::WorldRect, kPartBoundsCount> part_bounds_{};
    CompoundObjectLifetimeHost* lifetime_host_ = nullptr;
};

std::unique_ptr<CompoundObject> create_compound_object();

} // namespace creatures1::objects
