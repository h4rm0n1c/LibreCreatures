#pragma once

#include <cstddef>
#include <cstdint>

#include "object.hpp"

namespace creatures1::creatures {
class Creature;
}

namespace creatures1::objects {

class Object;
class ObjectEventScheduler;
class ObjectRegistryHost;

// Deleting one object is a game-owned ordering policy that touches several
// runtime owners.  The host contains only the application/UI/registry calls
// that cannot be represented by the object and scheduler modules themselves.
class ObjectDeletionHost : public ObjectInitializationHost {
public:
    ~ObjectDeletionHost() override = default;

    // The Object that is a creature's identity resolves to its Creature, so
    // the purge can drop stimuli queued for the Creature itself.
    virtual const creatures1::creatures::Creature* creature_for_object(
        const Object& object) const = 0;
    virtual bool selected_creature_exists() const = 0;
    virtual void clear_references_from_other_object(Object& object,
                                                     Object& deleted_object) = 0;
    virtual void remove_from_creature_selection(Object& object) = 0;
    virtual bool remove_from_creature_registry(Object& object) = 0;
    virtual void delete_object(Object& object) = 0;
};

class ViewAnchorObject {
public:
    virtual ~ViewAnchorObject() = default;

    virtual bool is_view_unbounded() const = 0;
    virtual bool is_creature() const = 0;
    virtual int current_visual_height() const = 0;
    virtual void move_to(int world_x, int world_y) = 0;
};

struct RenderableObjectRange {
    ViewAnchorObject* const* values = nullptr;
    std::size_t count = 0;
};

struct ViewAnchorInput {
    int mouse_client_x = 0;
    int mouse_client_y = 0;
    int viewport_left = 0;
    int viewport_top = 0;
};

void update_view_anchored_objects(
    const ViewAnchorInput* view,
    RenderableObjectRange renderable_objects,
    ViewAnchorObject* pointer_tool,
    ViewAnchorObject* edit_object);

// Removes an object from all C1 runtime references, then invokes its virtual
// deleting hook exactly once. Queue storage and macro slots are concrete
// subsystem state; UI, creature arrays, and the final deleting dispatch are
// explicit host boundaries.
void delete_object_and_purge_runtime_references(
    Object& object,
    ObjectDeletionHost& host,
    ObjectEventScheduler& event_scheduler,
    ObjectRegistryHost& non_scenery_registry);

} // namespace creatures1::objects
