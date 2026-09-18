#pragma once

#include "entity.hpp"
#include "object.hpp"

#include <memory>

namespace creatures1::objects {

class Scenery;

// Scenery::MoveToAndRedraw is a world-renderer operation in the native
// binary.  The renderer owns bounds capture, wrapped viewport intersection,
// immediate presentation, and the deferred dirty-rectangle queue; Scenery
// only supplies the object and its Entity coordinate transition.
class SceneryMoveRedrawHost {
public:
    virtual ~SceneryMoveRedrawHost() = default;
    virtual void move_scenery_to_and_redraw(Scenery& scenery, Entity& entity,
                                            int world_x, int world_y) = 0;
};

class Scenery : public Object {
public:
    static constexpr std::uint8_t kClassifierFamily = 1;
    // C1_SCRIPT_DEACTIVATE is event 0 in the recovered classifier enum.
    static constexpr std::uint8_t kSceneryDeactivateEvent = 0;
    static constexpr std::uint8_t kSceneryBoundsFlags = 0x10;

    Scenery() = default;

    // `new: scen`, from Macro::ExecuteNewCommand @ 0x0041d130.  The recovered
    // sequence is: acquire the gallery, stamp the scenery classifier
    // (family 1, genus 0, species 0, event deactivate), set bounds flag 0x10,
    // recompute movement bounds, clear the tick gate, leave the non-scenery
    // registry, then build one Entity at the origin carrying the image index
    // and render plane.
    //
    // The gallery arrives already acquired and the two collaborators are the
    // concrete ones this body calls.  Bundling them into a construction
    // interface would add a type without adding a capability.
    Scenery(display::Gallery* gallery, std::uint32_t image_index,
            int render_plane, EntityRegistryHost& entities,
            ObjectMovementBoundsHost& bounds, ObjectRegistryHost& registry);

    Entity* entity() { return entity_.get(); }
    const Entity* entity() const { return entity_.get(); }
    void set_entity(std::unique_ptr<Entity> entity) {
        entity_ = std::move(entity);
    }

    void serialize(ObjectArchive& archive);

    // Native Scenery table 00457a70 shares these Entity-backed bodies with
    // SimpleObject, despite inheriting directly from Object.
    void move_by(int delta_x, int delta_y) override;
    void move_to(int world_x, int world_y);
    bool get_bounds(world::WorldRect* out_bounds) const override;
    int render_plane() const override;
    int sound_source_x() const override;
    int sound_source_y() const override;
    int current_visual_width() const override;
    int current_visual_height() const override;

    void move_to_and_redraw(int world_x, int world_y,
                            SceneryMoveRedrawHost& renderer);

private:
    std::unique_ptr<Entity> entity_;
};

} // namespace creatures1::objects
