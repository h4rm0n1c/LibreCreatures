#include "bacterium.hpp"

#include <cstdlib>

namespace creatures1::creatures {
namespace {

std::int32_t normalized_masked_random(std::uint32_t value,
                                      std::uint32_t mask) {
    std::uint32_t normalized = value & mask;
    if (static_cast<std::int32_t>(normalized) < 0) {
        normalized = (normalized - 1u | ~mask) + 1u;
    }
    return static_cast<std::int32_t>(normalized);
}

std::int8_t random_input_chemical_id(BacteriumRandomSource& random_source) {
    return static_cast<std::int8_t>(
        normalized_masked_random(random_source.next(), 0x80000007u) - 8);
}

std::int8_t random_output_chemical_id(BacteriumRandomSource& random_source) {
    return static_cast<std::int8_t>(
        normalized_masked_random(random_source.next(), 0x80000003u) - 0x18);
}

} // namespace

Bacterium::Bacterium()
    : activity_state_(BacteriumActivityState::inactive),
      kill_threshold_(0xb4),
      activation_threshold_(0x50),
      input_chemical_id_(static_cast<std::int8_t>(
          normalized_masked_random(static_cast<std::uint32_t>(std::rand()),
                                   0x80000007u) -
          8)) {
    for (std::int8_t& chemical_id : output_chemical_ids_) {
        chemical_id = static_cast<std::int8_t>(
            normalized_masked_random(static_cast<std::uint32_t>(std::rand()),
                                     0x80000003u) -
            0x18);
    }
}

Bacterium::Bacterium(BacteriumRandomSource& random_source)
    : activity_state_(BacteriumActivityState::inactive),
      kill_threshold_(0xb4),
      activation_threshold_(0x50),
      input_chemical_id_(random_input_chemical_id(random_source)) {
    for (std::int8_t& chemical_id : output_chemical_ids_) {
        chemical_id = random_output_chemical_id(random_source);
    }
}

void Bacterium::serialize(BacteriumArchive& archive) {
    if (archive.is_loading()) {
        activity_state_ = static_cast<BacteriumActivityState>(
            archive.read_byte());
        input_chemical_id_ = static_cast<std::int8_t>(archive.read_byte());
        kill_threshold_ = archive.read_byte();
        activation_threshold_ = archive.read_byte();
        for (std::int8_t& chemical_id : output_chemical_ids_) {
            chemical_id = static_cast<std::int8_t>(archive.read_byte());
        }
        return;
    }

    archive.write_byte(static_cast<std::uint8_t>(activity_state_));
    archive.write_byte(static_cast<std::uint8_t>(input_chemical_id_));
    archive.write_byte(kill_threshold_);
    archive.write_byte(activation_threshold_);
    for (std::int8_t chemical_id : output_chemical_ids_) {
        archive.write_byte(static_cast<std::uint8_t>(chemical_id));
    }
}

void Bacterium::update(Creature& owner, BacteriumUpdateHost& host) {
    if (activity_state_ == BacteriumActivityState::inactive) {
        return;
    }

    std::size_t registry_size_snapshot = host.creature_count();
    for (std::size_t index = 0; index < registry_size_snapshot; ++index) {
        Creature* const target = host.creature_at(index);
        if (target == nullptr || target == &owner ||
            !host.can_perceive(owner, *target)) {
            continue;
        }

        if (host.random_source().next() % 5u != 0u) {
            return;
        }

        Bacterium& target_bacterium = host.bacterium_of(*target);
        if (static_cast<std::uint8_t>(target_bacterium.activity_state()) > 1u) {
            return;
        }

        replicate_and_mutate(target_bacterium, host.random_source());
        return;
    }

    if (host.random_source().next() % 0x0bu != 0u) {
        return;
    }

    // The executable shifts the four output bytes of each environmental
    // bacterium, not the complete twelve-byte native records.
    for (std::size_t index = 0; index < 99; ++index) {
        host.environment_bacterium(index).output_chemical_ids() =
            host.environment_bacterium(index + 1).output_chemical_ids();
    }
    replicate_and_mutate(host.environment_bacterium(99),
                         host.random_source());
}

void Bacterium::replicate_and_mutate(
    Bacterium& offspring,
    BacteriumRandomSource& random_source) const {
    offspring.activity_state_ = activity_state_;
    offspring.kill_threshold_ = kill_threshold_;
    offspring.activation_threshold_ = activation_threshold_;
    offspring.input_chemical_id_ = input_chemical_id_;
    offspring.output_chemical_ids_ = output_chemical_ids_;

    const std::uint32_t mutation_selector = random_source.next() % 0x65u;
    if (mutation_selector < 0x51u) {
        if (mutation_selector < 0x47u) {
            if (mutation_selector < 0x3du) {
                if (mutation_selector > 0x32u) {
                    offspring.input_chemical_id_ =
                        random_input_chemical_id(random_source);
                }
            } else {
                offspring.kill_threshold_ = static_cast<std::uint8_t>(
                    static_cast<std::int32_t>(offspring.kill_threshold_) +
                    static_cast<std::int32_t>(random_source.next() % 0x15u) -
                    10);
                if (offspring.kill_threshold_ < 0x82u) {
                    offspring.kill_threshold_ = 0x82u;
                }
            }
        } else {
            offspring.activation_threshold_ = static_cast<std::uint8_t>(
                static_cast<std::int32_t>(offspring.activation_threshold_) +
                static_cast<std::int32_t>(random_source.next() % 0x15u) - 10);
            if (offspring.activation_threshold_ > 0x82u) {
                offspring.activation_threshold_ = 0x82u;
            }
        }
    } else {
        const std::int32_t chemical_id = normalized_masked_random(
            random_source.next(), 0x80000003u);
        const std::uint32_t output_slot = static_cast<std::uint32_t>(
            normalized_masked_random(random_source.next(), 0x80000003u));
        offspring.output_chemical_ids_[output_slot] =
            static_cast<std::int8_t>(chemical_id);
    }

    offspring.activity_state_ = BacteriumActivityState::dormant;
    if (random_source.debug_logging_enabled()) {
        random_source.log_replication(offspring);
    }
}

} // namespace creatures1::creatures
