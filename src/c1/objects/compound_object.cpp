#include "compound_object.hpp"

#include <algorithm>
#include <new>

namespace creatures1::objects {

CompoundObject::CompoundObject(CompoundObjectLifetimeHost* lifetime_host)
    : lifetime_host_(lifetime_host) {
    initialize_part_storage();
}

CompoundObject::CompoundObject(
    std::uint32_t sprite_file_id, int header_record_index,
    std::uint32_t image_count, bool cache_protected,
    CompoundObjectConstructionHost& construction)
    : lifetime_host_(&construction) {
    // Native order is Object construction, gallery acquisition, then the
    // compound-specific part tables. The host owns the cache/registry policy;
    // CompoundObject owns the acquired reference from this point onward.
    set_gallery(construction.acquire_gallery(
        sprite_file_id, header_record_index, image_count, cache_protected));
    initialize_part_storage();
}

CompoundObject::~CompoundObject() {
    // Entity ownership is intrinsic to CompoundObject. Entity destructors
    // perform their own registry removal before the outer object leaves the
    // renderable set, matching the native destruction order.
    for (CompoundPart& part : parts_) {
        part.entity.reset();
    }

    if (lifetime_host_ == nullptr) {
        return;
    }

    lifetime_host_->remove_from_renderable_set(*this);
    if (continuous_sound_handle() >= 0) {
        lifetime_host_->stop_continuous_sound(continuous_sound_handle());
        clear_continuous_sound_state();
    }
    if (gallery() != nullptr) {
        display::Gallery* owned_gallery = gallery();
        lifetime_host_->release_gallery(*owned_gallery);
        set_gallery(nullptr);
    }
    lifetime_host_->unregister_from_object_registry(*this);
}

void CompoundObject::clear_owned_parts_and_gallery(
    CompoundObjectLifetimeHost& host) {
    for (CompoundPart& part : parts_) {
        part.entity.reset();
    }
    if (gallery() != nullptr) {
        display::Gallery* owned_gallery = gallery();
        host.release_gallery(*owned_gallery);
        set_gallery(nullptr);
    }
}

std::unique_ptr<CompoundObject> create_compound_object() {
    return std::unique_ptr<CompoundObject>(new (std::nothrow) CompoundObject());
}

void CompoundObject::initialize_part_storage() {
    set_classifier_family(kClassifierFamily);
    for (CompoundPart& part : parts_) {
        part.entity.reset();
    }
    part_count_ = 0;
    for (world::WorldRect& bounds : part_bounds_) {
        bounds.min_x = -1;
    }
    creature_event_config.event_config_value = {{-1, -1, -1}};
    click_event_bounds_index.part_bounds_index = {{-1, -1, -1}};
}

int CompoundObject::sound_source_x() const {
    return parts_[0].entity->world_x();
}

int CompoundObject::sound_source_y() const {
    return parts_[0].entity->world_y();
}

int CompoundObject::current_visual_width() const {
    return parts_[0].entity->current_image_width();
}

int CompoundObject::current_visual_height() const {
    return parts_[0].entity->current_image_height();
}

int CompoundObject::wrap_world_x_once(int x) {
    if (x < 0) {
        return x + world::kWorldWidth;
    }
    if (x >= world::kWorldWidth) {
        return x - world::kWorldWidth;
    }
    return x;
}

void CompoundObject::move_by(int delta_x, int delta_y) {
    for (int index = 0; index < part_count_; ++index) {
        Entity* entity = parts_[index].entity.get();
        if (entity != nullptr) {
            entity->set_world_x(wrap_world_x_once(entity->world_x() + delta_x));
            entity->set_world_y(entity->world_y() + delta_y);
        }
    }
}

void CompoundObject::move_to(int world_x, int world_y) {
    for (int index = 0; index < part_count_; ++index) {
        Entity* entity = parts_[index].entity.get();
        if (entity != nullptr) {
            entity->set_world_x(wrap_world_x_once(
                parts_[index].local_x_offset + world_x));
            entity->set_world_y(parts_[index].local_y_offset + world_y);
        }
    }
}

void CompoundObject::move_to_and_redraw(
    int world_x, int world_y, CompoundObjectMoveRedrawHost& renderer) {
    world::WorldRect old_bounds{};
    if (parts_[0].entity != nullptr) {
        parts_[0].entity->get_current_image_bounds(old_bounds);
    }

    move_to(world_x, world_y);

    world::WorldRect new_bounds{};
    if (parts_[0].entity != nullptr) {
        parts_[0].entity->get_current_image_bounds(new_bounds);
    }
    renderer.redraw_after_compound_object_move(*this, old_bounds, new_bounds);
}

void CompoundObject::tick(CompoundObjectTickHost& host) {
    update_sound(host);

    if (timer_period() > 0) {
        const std::int32_t next_countdown = timer_countdown() - 1;
        set_timer_countdown(next_countdown);
        if (next_countdown < 1) {
            host.dispatch_timer_event(*this);
            set_timer_countdown(timer_period());
        }
    }

    for (int index = 0; index < part_count_; ++index) {
        Entity* entity = parts_[index].entity.get();
        if (entity != nullptr) {
            host.advance_image_sequence(*entity);
        }
    }
}

void CompoundObject::move_by_and_redraw(
    int delta_x, int delta_y, CompoundObjectMoveRedrawHost& renderer) {
    world::WorldRect old_bounds{};
    if (parts_[0].entity != nullptr) {
        parts_[0].entity->get_current_image_bounds(old_bounds);
    }

    move_by(delta_x, delta_y);

    world::WorldRect new_bounds{};
    if (parts_[0].entity != nullptr) {
        parts_[0].entity->get_current_image_bounds(new_bounds);
    }
    renderer.redraw_after_compound_object_move(*this, old_bounds, new_bounds);
}

void CompoundObject::queue_primary_part_dirty_rect(
    CompoundObjectMoveRedrawHost& renderer) {
    world::WorldRect primary_bounds{};
    if (parts_[0].entity != nullptr) {
        parts_[0].entity->get_current_image_bounds(primary_bounds);
    }
    renderer.queue_compound_object_dirty_rect(*this, primary_bounds);
}

bool CompoundObject::get_bounds(world::WorldRect* out_bounds) const {
    if (parts_[0].entity == nullptr) {
        *out_bounds = {};
        return true;
    }
    parts_[0].entity->get_current_image_bounds(*out_bounds);
    return true;
}

int CompoundObject::render_plane() const {
    return parts_[0].entity->render_plane();
}

void CompoundObject::set_part_bounds(
    std::size_t index, const world::WorldRect& bounds) {
    if (index >= part_bounds_.size()) {
        return;
    }
    part_bounds_[index] = bounds;
}

void CompoundObject::serialize(ObjectArchive& archive) {
    // CArchive's ReadObject/WriteObject operation owns the dynamic Entity
    // framing and invokes Entity::Serialize at that boundary. This method
    // therefore emits only CompoundObject's own fields after Object::Serialize;
    // calling Entity::serialize here would duplicate every nested Entity
    // record in the stream.
    Object::serialize(archive);

    if (archive.is_loading()) {
        part_count_ = archive.read_int32();
        for (int index = 0; index < part_count_; ++index) {
            parts_[index].entity.reset(static_cast<Entity*>(
                archive.read_object_reference("Entity")));
            parts_[index].local_x_offset = archive.read_int32();
            parts_[index].local_y_offset = archive.read_int32();
        }
        for (world::WorldRect& bounds : part_bounds_) {
            archive.read_bytes(&bounds, sizeof(bounds));
        }
        for (int& value : creature_event_config.event_config_value) {
            value = archive.read_int32();
        }
        return;
    }

    archive.write_int32(part_count_);
    for (int index = 0; index < part_count_; ++index) {
        archive.write_object_reference(parts_[index].entity.get(), "Entity");
        archive.write_int32(parts_[index].local_x_offset);
        archive.write_int32(parts_[index].local_y_offset);
    }
    for (const world::WorldRect& bounds : part_bounds_) {
        archive.write_bytes(&bounds, sizeof(bounds));
    }
    for (int value : creature_event_config.event_config_value) {
        archive.write_int32(value);
    }
}

void CompoundObject::handle_queued_event(
    const QueuedObjectEvent& event, std::size_t config_index,
    ObjectEventId interaction_event, ObjectEventId script_event,
    CompoundObjectEventHost& host) {
    Object* source = event.source;
    if (source == nullptr) {
        return;
    }

    const bool source_is_creature = host.source_is_creature(*source);
    const std::uint32_t previous_event = current_interaction_event_id();
    const std::uint32_t interaction_value =
        static_cast<std::uint32_t>(interaction_event);

    if (previous_event == interaction_value) {
        if (!source_is_creature) {
            return;
        }
    } else if (!source_is_creature ||
               creature_event_config.event_config_value[config_index] != -1) {
        set_current_interaction_event_id(interaction_value);
        if (host.dispatch_script_event(*this, source, script_event) != 0) {
            return;
        }
        set_current_interaction_event_id(previous_event);
        if (!host.source_is_creature(*source)) {
            return;
        }
    }

    QueuedCreatureStimulus stimulus{};
    if (host.copy_built_in_stimulus(*source, *this, 0, stimulus)) {
        host.queue_creature_stimulus(stimulus);
    }
}

void CompoundObject::handle_queued_event_0(
    const QueuedObjectEvent& event, CompoundObjectEventHost& host) {
    handle_queued_event(event, 0, ObjectEventId::event_1,
                        ObjectEventId::event_1, host);
}

void CompoundObject::handle_queued_event_1(
    const QueuedObjectEvent& event, CompoundObjectEventHost& host) {
    handle_queued_event(event, 1, ObjectEventId::event_2,
                        ObjectEventId::event_2, host);
}

void CompoundObject::handle_queued_event_2(
    const QueuedObjectEvent& event, CompoundObjectEventHost& host) {
    handle_queued_event(event, 2, ObjectEventId::event_0,
                        ObjectEventId::event_0, host);
}

ObjectEventId CompoundObject::click_event_id_at_world_position(
    int world_x, int world_y) const {
    const Entity& primary_entity = *parts_[0].entity;
    int relative_world_x = world_x - primary_entity.world_x();
    if (relative_world_x < 0) {
        relative_world_x += world::kWorldWidth;
    } else if (relative_world_x >= world::kWorldWidth) {
        relative_world_x -= world::kWorldWidth;
    }
    const int relative_world_y = world_y - primary_entity.world_y();

    for (std::size_t index = 0; index < click_event_bounds_index.part_bounds_index.size();
         ++index) {
        const int bounds_index = click_event_bounds_index.part_bounds_index[index];
        if (bounds_index == -1) {
            continue;
        }
        const world::WorldRect& bounds = part_bounds_[bounds_index];
        const bool inside_y = bounds.min_y <= relative_world_y &&
                              relative_world_y < bounds.max_y;
        const bool inside_x = bounds.max_x < world::kWorldWidth
                                  ? bounds.min_x <= relative_world_x &&
                                        relative_world_x < bounds.max_x
                                  : relative_world_x <
                                            bounds.max_x - world::kWorldWidth ||
                                        bounds.min_x <= relative_world_x;
        if (inside_x && inside_y) {
            return static_cast<ObjectEventId>(index);
        }
    }
    return static_cast<ObjectEventId>(~std::uint32_t{0});
}

void CompoundObject::get_part_center(
    int* out_world_x, int* out_world_y,
    std::int32_t creature_event_index) const {
    const int configured_bounds_index = creature_event_config.event_config_value[
        static_cast<std::size_t>(creature_event_index)];
    int min_x = 0;
    int min_y = 0;
    int max_x = parts_[0].entity->current_image_width();
    int max_y = parts_[0].entity->current_image_height();
    if (configured_bounds_index != -1) {
        const world::WorldRect& bounds = part_bounds_[configured_bounds_index];
        min_x = bounds.min_x;
        min_y = bounds.min_y;
        max_x = bounds.max_x;
        max_y = bounds.max_y;
    }

    const int raw_world_x = (max_x - min_x) / 2 +
                            parts_[0].entity->world_x() + min_x;
    *out_world_x = wrap_world_x_once(raw_world_x);
    *out_world_y = (max_y - min_y) / 2 +
                   parts_[0].entity->world_y() + min_y;
}

bool CompoundObject::set_relative_image_index(
    CaosValue relative_index, int part_index,
    EntityImageSequenceRenderHost& redraw_host) {
    Entity* entity = parts_[part_index].entity.get();
    if (entity == nullptr) {
        return false;
    }
    entity->set_relative_image_index_and_redraw(
        static_cast<std::int32_t>(relative_index), redraw_host);
    return true;
}

void CompoundObject::set_image_index(
    std::uint8_t image_index, int part_index,
    EntityImageSequenceRenderHost& redraw_host) {
    Entity* entity = parts_[part_index].entity.get();
    if (entity != nullptr) {
        entity->set_image_index_and_redraw(image_index, redraw_host);
    }
}

char* CompoundObject::parse_image_sequence(char* sequence_text,
                                            int part_index) {
    return parts_[part_index].entity->parse_image_sequence(sequence_text);
}

bool CompoundObject::image_sequence_is_empty(int part_index) const {
    return parts_[part_index].entity->image_sequence_is_empty();
}

int CompoundObject::relative_image_index(int part_index) const {
    return parts_[part_index].entity->relative_image_index();
}

char* CompoundObject::preload_image_sequence(
    char* sequence_text, int part_index, ImagePreloadHost& preload_host) const {
    return parts_[part_index].entity->preload_image_sequence(sequence_text,
                                                              preload_host);
}

} // namespace creatures1::objects
