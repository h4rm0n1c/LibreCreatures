#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace creatures1::creatures {

enum class CreatureGender : std::uint32_t {
    male = 0,
    female = 1,
};

enum class CreatureLifeState : std::uint32_t {
    alive = 0,
    dead = 1,
};

// The menu code only needs this narrow view of the real Creature object.  The
// concrete Creature implementation supplies the accessors; this interface
// keeps UI policy independent of its storage, archive members, and MFC base.
class CreatureSelectionEntry {
public:
    virtual ~CreatureSelectionEntry() = default;

    virtual bool tick_enabled() const = 0;
    virtual std::string display_name() const = 0;
    virtual CreatureLifeState life_state() const = 0;
    virtual CreatureGender gender() const = 0;
    virtual bool has_child_genome_source() const = 0;
    virtual std::uint32_t classifier_species_family() const = 0;
    virtual std::uint32_t chemical_concentration(
        std::uint32_t chemical_index) const = 0;
    virtual void set_selection_menu_command_id(std::uint32_t command_id) = 0;
};

class CreatureRegistryView {
public:
    virtual ~CreatureRegistryView() = default;

    virtual std::size_t creature_count() const = 0;
    virtual CreatureSelectionEntry* creature_at(std::size_t index) const = 0;
};

// The mutation surface is separate from the read-only view used by menu
// construction.  The original executable removes the object from its global
// CObArray and then asks the UI to rebuild; neither framework detail belongs in
// the registry's semantic contract.
class CreatureRegistryMutation : public CreatureRegistryView {
public:
    ~CreatureRegistryMutation() override = default;

    virtual bool remove_at(std::size_t index) = 0;
};

// This is the application-owned selection array. Entries are borrowed from
// the registry and remain owned by the creature system.
class CreatureSelectionState {
public:
    void clear() { selected_.clear(); }
    void add(CreatureSelectionEntry* creature) { selected_.push_back(creature); }

    std::size_t size() const { return selected_.size(); }
    CreatureSelectionEntry* at(std::size_t index) const { return selected_[index]; }

private:
    std::vector<CreatureSelectionEntry*> selected_;
};

} // namespace creatures1::creatures
