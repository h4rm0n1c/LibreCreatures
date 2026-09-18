#include "scenery.hpp"

namespace creatures1::objects {

Scenery::Scenery(display::Gallery* gallery, std::uint32_t image_index,
                 int render_plane, EntityRegistryHost& entities,
                 ObjectMovementBoundsHost& bounds,
                 ObjectRegistryHost& registry) {
    set_gallery(gallery);
    // Scenery's classifier is stamped literally: family 1, genus 0, species 0
    // and the deactivate event.
    set_classifier_components(kSceneryDeactivateEvent, 0, 0, kClassifierFamily);
    set_bounds_flags(kSceneryBoundsFlags);
    update_movement_bounds(bounds);
    disable_ticking();
    // The base Object constructor joins the non-scenery registry; scenery
    // belongs in its own list, so the native immediately leaves that one.
    unregister_from_non_scenery_object_registry(registry);

    auto entity = std::make_unique<Entity>(&entities);
    entity->set_render_plane(render_plane);
    entity->move_to(0, 0);
    entity->set_gallery(gallery);
    entity->set_image_index(static_cast<std::uint8_t>(image_index));
    entity->set_image_index_base(static_cast<std::uint8_t>(image_index));
    entity_ = std::move(entity);
}

void Scenery::serialize(ObjectArchive& archive) {
    Object::serialize(archive);

    if (archive.is_loading()) {
        entity_.reset(static_cast<Entity*>(
            archive.read_object_reference("Entity")));
        return;
    }

    archive.write_object_reference(entity_.get(), "Entity");
}

void Scenery::move_to(int world_x, int world_y) {
    if (entity_ == nullptr) {
        return;
    }
    if (world_x < 0) {
        world_x += world::kWorldWidth;
    } else if (world_x >= world::kWorldWidth) {
        world_x -= world::kWorldWidth;
    }
    entity_->move_to(world_x, world_y);
}

void Scenery::move_by(int delta_x, int delta_y) {
    if (entity_ != nullptr) {
        move_to(entity_->world_x() + delta_x, entity_->world_y() + delta_y);
    }
}

bool Scenery::get_bounds(world::WorldRect* out_bounds) const {
    if (out_bounds == nullptr) {
        return false;
    }
    *out_bounds = {};
    if (entity_ != nullptr && entity_->has_current_image()) {
        entity_->get_current_image_bounds(*out_bounds);
    }
    return true;
}

int Scenery::render_plane() const {
    return entity_ == nullptr ? 0 : entity_->render_plane();
}

int Scenery::sound_source_x() const {
    return entity_ == nullptr ? 0 : entity_->world_x();
}

int Scenery::sound_source_y() const {
    return entity_ == nullptr ? 0 : entity_->world_y();
}

int Scenery::current_visual_width() const {
    return entity_ == nullptr ? 0 : entity_->current_image_width();
}

int Scenery::current_visual_height() const {
    return entity_ == nullptr ? 0 : entity_->current_image_height();
}

void Scenery::move_to_and_redraw(int world_x, int world_y,
                                  SceneryMoveRedrawHost& renderer) {
    // The native method's ordering is renderer-visible: old bounds are
    // captured before the wrapped Entity coordinates change, and new bounds
    // are captured immediately afterwards.  The host performs that capture
    // and the resulting redraw/dirty-queue policy without exposing MFC.
    renderer.move_scenery_to_and_redraw(*this, *entity_, world_x, world_y);
}

} // namespace creatures1::objects
