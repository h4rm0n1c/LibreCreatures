#include "tick.hpp"

namespace creatures1::world {

void update_creature_bacteria_and_environment(
    BacteriumUpdateTarget* const* creatures,
    std::size_t creature_count) {
    std::size_t index = 0;
    std::size_t registry_size_snapshot = creature_count;
    while (index < registry_size_snapshot) {
        BacteriumUpdateTarget& creature = *creatures[index];
        if (creature.tick_enabled()) {
            creature.update_bacterium_and_environment();
            registry_size_snapshot = creature_count;
        }
        ++index;
    }
}

void advance_bacterium_service_phase(BacteriumServiceHost& host) {
    const BacteriumServicePhase previous_phase = host.service_phase();
    const std::size_t initial_creature_count = host.creature_count();

    auto& phase = host.service_phase();
    const auto phase_value = static_cast<std::uint8_t>(phase);
    phase = static_cast<BacteriumServicePhase>((phase_value + 1u) & 7u);

    switch (previous_phase) {
    case BacteriumServicePhase::idle_0:
    case BacteriumServicePhase::idle_2:
    case BacteriumServicePhase::idle_3:
    case BacteriumServicePhase::idle_4:
    case BacteriumServicePhase::idle_5:
    case BacteriumServicePhase::idle_6:
        return;

    case BacteriumServicePhase::infection_attempt: {
        const std::size_t creature_count = host.creature_count();
        if (creature_count == 0) {
            return;
        }

        std::uint32_t roll_limit = 500;
        if (host.smoothed_idle_cycle_index() < 100 ||
            host.selected_creature_count() > 0x18) {
            roll_limit = 0xfa;
        }
        if (host.next_random() % (roll_limit + 1u) != 0) {
            return;
        }

        const std::size_t target_index =
            host.next_random() % creature_count;
        auto& target_bacterium = host.creature_bacterium(target_index);
        if (static_cast<std::uint8_t>(target_bacterium.activity_state()) < 2) {
            const std::size_t source_index = host.next_random() % 100;
            auto& source_bacterium = host.environment_bacterium(source_index);
            source_bacterium.replicate_and_mutate(
                target_bacterium, host.bacterium_random_source());
        }
        host.log_environment_infection();
        return;
    }

    case BacteriumServicePhase::creature_update:
        break;
    }

    std::size_t index = 0;
    std::size_t registry_size_snapshot = initial_creature_count;
    while (index < registry_size_snapshot) {
        if (host.creature_tick_enabled(index)) {
            host.update_creature_bacterium_and_environment(index);
            registry_size_snapshot = host.creature_count();
        }
        ++index;
    }
}

} // namespace creatures1::world
