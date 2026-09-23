#pragma once

#include <cstddef>
#include <cstdint>

#include "../creatures/bacterium.hpp"

namespace creatures1::world {

enum class BacteriumServicePhase : std::uint8_t {
    idle_0 = 0,
    infection_attempt = 1,
    idle_2 = 2,
    idle_3 = 3,
    idle_4 = 4,
    idle_5 = 5,
    idle_6 = 6,
    creature_update = 7,
};

// Owns the process-wide state touched by the recovered eight-phase service.
// Registry storage, the MFC random/debug implementations, and MapData's
// bacterium array remain outside the world policy.
class BacteriumServiceHost {
public:
    virtual ~BacteriumServiceHost() = default;
    virtual BacteriumServicePhase& service_phase() = 0;
    virtual std::size_t creature_count() const = 0;
    virtual std::size_t selected_creature_count() const = 0;
    virtual int smoothed_idle_cycle_index() const = 0;
    virtual std::uint32_t next_random() = 0;
    virtual creatures1::creatures::BacteriumRandomSource&
    bacterium_random_source() = 0;
    virtual bool creature_tick_enabled(std::size_t index) const = 0;
    virtual creatures1::creatures::Bacterium& creature_bacterium(
        std::size_t index) = 0;
    virtual creatures1::creatures::Bacterium& environment_bacterium(
        std::size_t index) = 0;
    virtual void update_creature_bacterium_and_environment(
        std::size_t index) = 0;
    virtual void log_environment_infection() = 0;
};

void advance_bacterium_service_phase(BacteriumServiceHost& host);

} // namespace creatures1::world
