#pragma once

#include "map.hpp"

#include "../creatures/creature.hpp"
#include "../display/gallery.hpp"
#include "../objects/entity.hpp"
#include "../objects/object.hpp"
#include "../objects/scenery.hpp"
#include "../creatures/selection.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace creatures1::world {

// Owns the C1 world registries and top-level world objects.  Registry entries
// remain non-owning views, while the owner vectors express the native
// DeleteContents destruction boundary. Nested Entity allocations remain owned
// by their concrete Object/Creature owners and unregister themselves.
class WorldRuntime final : public objects::EntityRegistryHost,
                           public objects::ObjectRegistryHost,
                           public objects::ObjectRenderableSetHost,
                           public display::GalleryRegistry,
                           public creatures1::creatures::CreatureRegistryMutation {
public:
    WorldRuntime() = default;
    ~WorldRuntime() override = default;

    WorldRuntime(const WorldRuntime&) = delete;
    WorldRuntime& operator=(const WorldRuntime&) = delete;

    MapData& map_data() { return map_data_; }
    const MapData& map_data() const { return map_data_; }

    void add_entity(objects::Entity& entity) override;
    std::size_t entity_count() const override { return entities_.size(); }
    objects::Entity* entity_at(std::size_t index) const override;
    void remove_entity_at(std::size_t index) override;
    void report_invalid_index() const override;

    void add_object(objects::Object* object) override;
    std::size_t object_count() const override { return objects_.size(); }
    objects::Object* object_at(std::size_t index) const override;
    void remove_object_at(std::size_t index) override;
    void report_invalid_object_index() const;

    // Adoption is the only path for a dynamically created top-level world
    // object to enter this runtime. The object is registered before ownership
    // is transferred, so archive/load and scheduler walks see the same object
    // immediately. A missing owner is a hard error at destruction time.
    objects::Object& adopt_non_scenery_object(
        std::unique_ptr<objects::Object> object);
    creatures1::creatures::Creature& adopt_creature(
        std::unique_ptr<creatures1::creatures::Creature> creature);
    objects::Scenery& adopt_scenery_object(
        std::unique_ptr<objects::Scenery> object);
    std::size_t scenery_count() const { return scenery_.size(); }
    objects::Scenery* scenery_at(std::size_t index) const;
    void destroy_first_non_scenery_object();
    void destroy_first_creature();
    void destroy_first_scenery_object();
    void destroy_owned_world_objects();
    void add_world_object(objects::Object& object);
    std::size_t world_object_count() const { return world_objects_.size(); }
    objects::Object* world_object_at(std::size_t index) const;
    void remove_world_object(objects::Object& object);
    // Destroys the owned object and removes every borrowed registry view that
    // names it, including a creature's embedded Skeleton identity.
    void destroy_world_object(objects::Object& object);
    bool owns_non_scenery_object(const objects::Object& object) const;

    // Membership test for a pointer of unknown provenance: a CAOS value cast
    // to an object.  Answered from the same registries the rest of the world
    // iterates, so it cannot drift out of step with them, and it never
    // dereferences the candidate.
    bool is_live_object(const objects::Object* object) const;
    void reset_map_data() { map_data_.reset_for_document_delete(); }

    bool contains(const objects::Object& object) const override;
    void insert(objects::Object& object) override;
    void erase(objects::Object& object) override;

    std::size_t gallery_count() const override { return galleries_.size(); }
    display::Gallery* gallery_at(std::size_t index) const override;
    display::Gallery* add_gallery(
        std::unique_ptr<display::Gallery> gallery) override;

    // Called only after object/entity destructors have removed their borrowed
    // entries.  The ordering mirrors SFCDoc::DeleteContents: registries are
    // cleared before gallery storage is finally released.
    void clear_entity_registry();
    void clear_object_registry();
    void clear_renderable_registry();

    // The viewport scroll path shifts every renderable object by the same
    // delta, so the registry needs ordered access as well as membership.
    std::size_t renderable_count() const {
        return renderable_objects_.size();
    }
    objects::Object* renderable_at(std::size_t index) const {
        return index < renderable_objects_.size()
                   ? renderable_objects_[index]
                   : nullptr;
    }
    void clear_creature_registry();
    void clear_borrowed_registries();

    void add_creature(creatures1::creatures::CreatureSelectionEntry& creature);
    std::size_t creature_count() const override { return creatures_.size(); }
    creatures1::creatures::CreatureSelectionEntry* creature_at(
        std::size_t index) const override;
    bool remove_at(std::size_t index) override;
    void release_gallery(display::Gallery& gallery);
    void clear_galleries();

private:
    void destroy_owned_creature(
        creatures1::creatures::Creature& creature,
        bool remove_world_object_slot = true);

    MapData map_data_{};
    std::vector<objects::Entity*> entities_;
    std::vector<objects::Object*> objects_;
    std::vector<objects::Object*> renderable_objects_;
    std::vector<creatures1::creatures::CreatureSelectionEntry*> creatures_;
    std::vector<std::unique_ptr<display::Gallery>> galleries_;
    std::vector<std::unique_ptr<objects::Object>> owned_objects_;
    std::vector<std::unique_ptr<creatures1::creatures::Creature>>
        owned_creatures_;
    std::vector<objects::Scenery*> scenery_;
    std::vector<std::unique_ptr<objects::Scenery>> owned_scenery_;
    // SFCDoc serializes the initialized world-object set, not every object
    // present in the broader non-scenery registry.
    std::vector<objects::Object*> world_objects_;
};

} // namespace creatures1::world
