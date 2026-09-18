#include "lifecycle.hpp"

#include "events.hpp"
#include "object.hpp"
#include "../scripting/macro.hpp"

namespace creatures1::objects {

void update_view_anchored_objects(
    const ViewAnchorInput* view,
    RenderableObjectRange renderable_objects,
    ViewAnchorObject* pointer_tool,
    ViewAnchorObject* edit_object) {
    if (view == nullptr) {
        return;
    }

    constexpr int kWorldWidth = 0x20a0;
    const int raw_world_x = view->mouse_client_x + view->viewport_left;
    const int anchored_x = raw_world_x < kWorldWidth
                               ? raw_world_x
                               : raw_world_x - kWorldWidth;
    const int mouse_world_y = view->mouse_client_y + view->viewport_top;
    int pointer_tool_y = mouse_world_y;

    for (std::size_t index = 0; index < renderable_objects.count; ++index) {
        ViewAnchorObject* object = renderable_objects.values[index];
        if (object == nullptr || object == pointer_tool ||
            !object->is_view_unbounded()) {
            continue;
        }

        if (object->is_creature()) {
            object->move_to(anchored_x,
                            mouse_world_y + object->current_visual_height());
        } else {
            object->move_to(anchored_x, mouse_world_y);
            pointer_tool_y = mouse_world_y - 0x10;
        }
        break;
    }

    if (pointer_tool != nullptr) {
        pointer_tool->move_to(anchored_x, pointer_tool_y);
    }
    if (edit_object != nullptr) {
        edit_object->move_to(anchored_x, mouse_world_y);
    }
}

void delete_object_and_purge_runtime_references(
    Object& object,
    ObjectDeletionHost& host,
    ObjectEventScheduler& event_scheduler,
    ObjectRegistryHost& non_scenery_registry) {
    if (host.is_edit_object(object)) {
        host.clear_edit_object();
    }

    host.remove_from_event_bar(object, true);
    host.purge_destroy_when_finished_macros(object);

    object.set_deletion_movement_bounds();
    host.move_to_and_redraw(object, 1000, 4000);
    object.disable_ticking();

    // Native clears whatever creature is selected, not only the one being
    // deleted, then runs the selection-changed refresh sequence.
    if (host.selected_creature_exists()) {
        host.clear_selected_creature(true);
    }

    event_scheduler.purge_object_references(object,
                                            host.creature_for_object(object));
    scripting::clear_object_references_from_running_macros(&object);

    for (std::size_t index = 0; index < non_scenery_registry.object_count();
         ++index) {
        Object* other = non_scenery_registry.object_at(index);
        if (other == nullptr) {
            non_scenery_registry.report_invalid_index();
            return;
        }
        if (other != &object) {
            host.clear_references_from_other_object(*other, object);
        }
    }

    host.remove_from_creature_selection(object);
    if (host.remove_from_creature_registry(object)) {
        host.rebuild_creature_selection_menu();
    }

    // The native path attempts the registry removal and calls the object's
    // deleting virtual whether or not the object was still indexed there.
    for (std::size_t index = 0; index < non_scenery_registry.object_count();
         ++index) {
        Object* candidate = non_scenery_registry.object_at(index);
        if (candidate == nullptr) {
            non_scenery_registry.report_invalid_index();
            return;
        }
        if (candidate == &object) {
            non_scenery_registry.remove_object_at(index);
            break;
        }
    }
    host.delete_object(object);
}

} // namespace creatures1::objects
