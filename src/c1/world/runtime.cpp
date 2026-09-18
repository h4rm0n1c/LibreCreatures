#include "runtime.hpp"

#include <algorithm>
#include <stdexcept>

namespace creatures1::world {

void WorldRuntime::add_entity(objects::Entity& entity) {
    entities_.push_back(&entity);
}

objects::Entity* WorldRuntime::entity_at(std::size_t index) const {
    if (index >= entities_.size()) {
        report_invalid_index();
    }
    return entities_[index];
}

void WorldRuntime::remove_entity_at(std::size_t index) {
    if (index >= entities_.size()) {
        report_invalid_index();
    }
    entities_.erase(entities_.begin() + static_cast<std::ptrdiff_t>(index));
}

void WorldRuntime::report_invalid_index() const {
    throw std::out_of_range("C1 entity registry index");
}

void WorldRuntime::add_object(objects::Object* object) {
    if (object != nullptr) {
        objects_.push_back(object);
    }
}

objects::Object* WorldRuntime::object_at(std::size_t index) const {
    if (index >= objects_.size()) {
        report_invalid_object_index();
    }
    return objects_[index];
}

void WorldRuntime::remove_object_at(std::size_t index) {
    if (index >= objects_.size()) {
        report_invalid_object_index();
    }
    objects_.erase(objects_.begin() + static_cast<std::ptrdiff_t>(index));
}

void WorldRuntime::report_invalid_object_index() const {
    throw std::out_of_range("C1 object registry index");
}

objects::Object& WorldRuntime::adopt_non_scenery_object(
    std::unique_ptr<objects::Object> object) {
    if (object == nullptr) {
        throw std::invalid_argument("C1 world runtime received null object");
    }

    objects::Object* const raw = object.get();
    if (std::find(objects_.begin(), objects_.end(), raw) == objects_.end()) {
        objects_.push_back(raw);
    }
    owned_objects_.push_back(std::move(object));
    return *raw;
}

creatures1::creatures::Creature& WorldRuntime::adopt_creature(
    std::unique_ptr<creatures1::creatures::Creature> creature) {
    if (creature == nullptr) {
        throw std::invalid_argument("C1 world runtime received null creature");
    }

    creatures1::creatures::Creature* const raw = creature.get();
    objects::Object* const object_identity = &raw->skeleton();
    owned_creatures_.push_back(std::move(creature));
    try {
        add_creature(*raw);
        if (std::find(objects_.begin(), objects_.end(), object_identity) ==
            objects_.end()) {
            objects_.push_back(object_identity);
        }
    } catch (...) {
        creatures_.erase(
            std::remove(creatures_.begin(), creatures_.end(),
                        static_cast<creatures1::creatures::CreatureSelectionEntry*>(raw)),
            creatures_.end());
        objects_.erase(
            std::remove(objects_.begin(), objects_.end(), object_identity),
            objects_.end());
        owned_creatures_.pop_back();
        throw;
    }
    return *raw;
}

objects::Scenery& WorldRuntime::adopt_scenery_object(
    std::unique_ptr<objects::Scenery> object) {
    if (object == nullptr) {
        throw std::invalid_argument("C1 world runtime received null scenery");
    }

    objects::Scenery* const raw = object.get();
    scenery_.push_back(raw);
    owned_scenery_.push_back(std::move(object));
    return *raw;
}

objects::Scenery* WorldRuntime::scenery_at(std::size_t index) const {
    if (index >= scenery_.size()) {
        throw std::out_of_range("C1 scenery registry index");
    }
    return scenery_[index];
}

void WorldRuntime::destroy_first_non_scenery_object() {
    if (objects_.empty()) {
        throw std::out_of_range("C1 non-scenery object registry is empty");
    }

    objects::Object* const target = objects_.front();
    const auto owner = std::find_if(
        owned_objects_.begin(), owned_objects_.end(),
        [target](const std::unique_ptr<objects::Object>& candidate) {
            return candidate.get() == target;
        });
    if (owner == owned_objects_.end()) {
        const auto creature_owner = std::find_if(
            owned_creatures_.begin(), owned_creatures_.end(),
            [target](const std::unique_ptr<creatures1::creatures::Creature>&
                         candidate) {
                return candidate != nullptr &&
                       &candidate->skeleton() == target;
            });
        if (creature_owner == owned_creatures_.end()) {
            throw std::logic_error(
                "C1 non-scenery registry contains an unowned object");
        }
        destroy_owned_creature(**creature_owner);
        return;
    }

    objects_.erase(objects_.begin());
    remove_world_object(*target);
    erase(*target);
    owned_objects_.erase(owner);
}

void WorldRuntime::destroy_first_scenery_object() {
    if (scenery_.empty()) {
        throw std::out_of_range("C1 scenery registry is empty");
    }

    objects::Scenery* const target = scenery_.front();
    const auto owner = std::find_if(
        owned_scenery_.begin(), owned_scenery_.end(),
        [target](const std::unique_ptr<objects::Scenery>& candidate) {
            return candidate.get() == target;
        });
    if (owner == owned_scenery_.end()) {
        throw std::logic_error("C1 scenery registry contains an unowned object");
    }

    scenery_.erase(scenery_.begin());
    remove_world_object(*target);
    owned_scenery_.erase(owner);
}

bool WorldRuntime::contains(const objects::Object& object) const {
    return std::find(renderable_objects_.begin(), renderable_objects_.end(),
                     &object) != renderable_objects_.end();
}

void WorldRuntime::insert(objects::Object& object) {
    if (!contains(object)) {
        renderable_objects_.push_back(&object);
    }
}

void WorldRuntime::erase(objects::Object& object) {
    const auto found = std::find(renderable_objects_.begin(),
                                 renderable_objects_.end(), &object);
    if (found != renderable_objects_.end()) {
        renderable_objects_.erase(found);
    }
}

display::Gallery* WorldRuntime::gallery_at(std::size_t index) const {
    if (index >= galleries_.size()) {
        throw std::out_of_range("C1 gallery registry index");
    }
    return galleries_[index].get();
}

display::Gallery* WorldRuntime::add_gallery(
    std::unique_ptr<display::Gallery> gallery) {
    if (gallery == nullptr) {
        throw std::invalid_argument("C1 gallery registry received null");
    }
    display::Gallery* const result = gallery.get();
    galleries_.push_back(std::move(gallery));
    return result;
}

void WorldRuntime::clear_entity_registry() {
    entities_.clear();
}

void WorldRuntime::clear_object_registry() {
    if (!owned_objects_.empty() || !owned_scenery_.empty() ||
        !owned_creatures_.empty() ||
        !world_objects_.empty()) {
        throw std::logic_error(
            "C1 borrowed registries cleared before owned world objects");
    }
    objects_.clear();
}

void WorldRuntime::clear_renderable_registry() {
    renderable_objects_.clear();
}

void WorldRuntime::clear_creature_registry() {
    if (!owned_creatures_.empty()) {
        throw std::logic_error(
            "C1 creature registry cleared before owned creatures");
    }
    creatures_.clear();
}

void WorldRuntime::add_world_object(objects::Object& object) {
    if (std::find(world_objects_.begin(), world_objects_.end(), &object) ==
        world_objects_.end()) {
        world_objects_.push_back(&object);
    }
}

objects::Object* WorldRuntime::world_object_at(std::size_t index) const {
    if (index >= world_objects_.size()) {
        throw std::out_of_range("C1 world-object registry index");
    }
    return world_objects_[index];
}

void WorldRuntime::remove_world_object(objects::Object& object) {
    world_objects_.erase(
        std::remove(world_objects_.begin(), world_objects_.end(), &object),
        world_objects_.end());
}

bool WorldRuntime::is_live_object(const objects::Object* object) const {
    if (object == nullptr) {
        return false;
    }
    if (std::find(objects_.begin(), objects_.end(), object) != objects_.end()) {
        return true;
    }
    return std::any_of(scenery_.begin(), scenery_.end(),
                       [object](const objects::Scenery* candidate) {
                           return static_cast<const objects::Object*>(
                                      candidate) == object;
                       });
}

bool WorldRuntime::owns_non_scenery_object(const objects::Object& object) const {
    return std::any_of(owned_objects_.begin(), owned_objects_.end(),
                       [&object](const std::unique_ptr<objects::Object>& candidate) {
                           return candidate.get() == &object;
                       });
}

void WorldRuntime::destroy_world_object(objects::Object& object) {
    const auto object_owner = std::find_if(
        owned_objects_.begin(), owned_objects_.end(),
        [&object](const std::unique_ptr<objects::Object>& candidate) {
            return candidate.get() == &object;
        });
    if (object_owner != owned_objects_.end()) {
        objects_.erase(std::remove(objects_.begin(), objects_.end(), &object),
                       objects_.end());
        erase(object);
        owned_objects_.erase(object_owner);
        return;
    }

    const auto scenery_owner = std::find_if(
        owned_scenery_.begin(), owned_scenery_.end(),
        [&object](const std::unique_ptr<objects::Scenery>& candidate) {
            return candidate.get() == &object;
        });
    if (scenery_owner != owned_scenery_.end()) {
        scenery_.erase(std::remove(scenery_.begin(), scenery_.end(),
                                   static_cast<objects::Scenery*>(&object)),
                       scenery_.end());
        erase(object);
        owned_scenery_.erase(scenery_owner);
        return;
    }

    const auto creature_owner = std::find_if(
        owned_creatures_.begin(), owned_creatures_.end(),
        [&object](const std::unique_ptr<creatures1::creatures::Creature>&
                      candidate) {
            return candidate != nullptr && &candidate->skeleton() == &object;
        });
    if (creature_owner != owned_creatures_.end()) {
        // Document::save removes the world-object slot by its original index
        // immediately after this call, so leave that borrowed slot present
        // until the caller completes the native two-step deletion.
        destroy_owned_creature(**creature_owner, false);
        return;
    }

    throw std::logic_error(
        "C1 world-object registry contains an unowned object");
}

void WorldRuntime::add_creature(
    creatures1::creatures::CreatureSelectionEntry& creature) {
    if (std::find(creatures_.begin(), creatures_.end(), &creature) ==
        creatures_.end()) {
        creatures_.push_back(&creature);
    }
}

creatures1::creatures::CreatureSelectionEntry* WorldRuntime::creature_at(
    std::size_t index) const {
    if (index >= creatures_.size()) {
        throw std::out_of_range("C1 creature registry index");
    }
    return creatures_[index];
}

bool WorldRuntime::remove_at(std::size_t index) {
    if (index >= creatures_.size()) {
        throw std::out_of_range("C1 creature registry index");
    }
    creatures_.erase(creatures_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void WorldRuntime::destroy_owned_creature(
    creatures1::creatures::Creature& creature,
    bool remove_world_object_slot) {
    const auto owner = std::find_if(
        owned_creatures_.begin(), owned_creatures_.end(),
        [&creature](const std::unique_ptr<creatures1::creatures::Creature>&
                        candidate) {
            return candidate.get() == &creature;
        });
    if (owner == owned_creatures_.end()) {
        throw std::logic_error("C1 creature registry contains an unowned creature");
    }

    creatures_.erase(
        std::remove(creatures_.begin(), creatures_.end(), &creature),
        creatures_.end());
    objects::Object& object_identity = creature.skeleton();
    objects_.erase(
        std::remove(objects_.begin(), objects_.end(), &object_identity),
        objects_.end());
    if (remove_world_object_slot) {
        remove_world_object(object_identity);
    }
    erase(object_identity);
    owned_creatures_.erase(owner);
}

void WorldRuntime::release_gallery(display::Gallery& gallery) {
    auto found = std::find_if(
        galleries_.begin(), galleries_.end(),
        [&gallery](const std::unique_ptr<display::Gallery>& candidate) {
            return candidate.get() == &gallery;
        });
    if (found == galleries_.end()) {
        throw std::out_of_range("C1 gallery release for unknown gallery");
    }

    if ((*found)->reference_count > 0) {
        --(*found)->reference_count;
    }
    if ((*found)->reference_count == 0) {
        galleries_.erase(found);
    }
}

void WorldRuntime::clear_galleries() {
    galleries_.clear();
}

} // namespace creatures1::world
