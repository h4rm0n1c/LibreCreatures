#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "../archive/string_archive.hpp"
#include "selection.hpp"

namespace creatures1::creatures {

void remove_creature_from_registry(
    CreatureRegistryMutation& registry,
    CreatureSelectionEntry* creature,
    const std::function<void()>& refresh_selection_menu);

struct CreatureHistory {
    std::string genome_moniker;
    std::string display_name;
    std::string father_moniker;
    std::string mother_moniker;
    std::string birthday;
    std::string birthplace;
    std::string history_entry_06;
    std::string history_entry_07;
    std::string history_entry_08;
    std::string history_entry_09;
};

// Owns the persisted identity/history record associated with a creature.
// Framework runtime-class registration and its ABI factory/destructor
// wrappers are supplied by the MFC/toolchain boundary; only this state and
// its archive record belong in the C1 source lane.
class CreatureRegister {
public:
    CreatureRegister();
    ~CreatureRegister();

    CreatureRegister(const CreatureRegister&) = delete;
    CreatureRegister& operator=(const CreatureRegister&) = delete;

    CreatureHistory& history() { return history_; }
    const CreatureHistory& history() const { return history_; }

    void serialize(archive::StringArchive& archive);

    // Native keeps exactly one age counter and it lives here:
    // (this->register_state).age_ticks is what Creature::Serialize restores
    // @ 0040dda0, what the per-tick update increments @ 0040e1d0, what
    // Reset zeroes, and what FormatStatusForExternalQuery @ 0040e520 prints.
    // The port briefly had a second copy on Creature itself, which was the
    // one being serialised and incremented while this one stayed zero -- so
    // every creature reported an age of 0:00 to the kits.
    std::uint32_t age_ticks() const { return age_ticks_; }
    void set_age_ticks(std::uint32_t ticks) { age_ticks_ = ticks; }
    void advance_age_tick() { ++age_ticks_; }

    // The ten history fields in their recorded order.  The DDE `getb data` /
    // `putb data` pair addresses them positionally, so the order is part of
    // the contract rather than an implementation detail.
    std::array<std::string*, 10> history_entries();
    std::array<const std::string*, 10> history_entries() const;

private:

    CreatureHistory history_;
    std::uint32_t age_ticks_ = 0;
    std::uint32_t selection_menu_command_id_ = 0;
    std::uint32_t action_selection_state_ = 0;
};

} // namespace creatures1::creatures
