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

void Scenery::move_to_and_redraw(int world_x, int world_y,
                                  SceneryMoveRedrawHost& renderer) {
    // The native method's ordering is renderer-visible: old bounds are
    // captured before the wrapped Entity coordinates change, and new bounds
    // are captured immediately afterwards.  The host performs that capture
    // and the resulting redraw/dirty-queue policy without exposing MFC.
    renderer.move_scenery_to_and_redraw(*this, *entity_, world_x, world_y);
}

} // namespace creatures1::objects
