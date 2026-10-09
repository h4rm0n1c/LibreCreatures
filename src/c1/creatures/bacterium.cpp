#include "bacterium.hpp"

#include <algorithm>
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

    // CBacterium::Update @ 0x00401d80 shifts bytes +4..+0xb of each 12-byte
    // record down a slot -- the activity state, input chemical, both
    // thresholds and the four outputs, everything but the vtable pointer.
    // (The decompiler shows them as output_chemical_ids[-4..3].)  An earlier
    // port moved the outputs alone, so a strain's toxins travelled through
    // the pool and its antigen and thresholds stayed behind.
    for (std::size_t index = 0; index < 99; ++index) {
        host.environment_bacterium(index) =
            host.environment_bacterium(index + 1);
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
                // LibreCreatures deviation (both thresholds).  The native
                // adds the step into the byte first (ADD byte [ESI+6], DL at
                // 00401d1d; [ESI+7] at 00401cfc) and clamps after, so a step
                // past 255 or below 0 wrapped: a kill threshold of 250 + 10
                // became 4 and was raised to 130, an activation threshold of
                // 0 - 1 became 255 and was capped at 130.  Clamp the sum.
                const std::int32_t kill =
                    static_cast<std::int32_t>(offspring.kill_threshold_) +
                    static_cast<std::int32_t>(random_source.next() % 0x15u) -
                    10;
                offspring.kill_threshold_ =
                    static_cast<std::uint8_t>(std::clamp(kill, 0x82, 0xff));
            }
        } else {
            const std::int32_t activation =
                static_cast<std::int32_t>(offspring.activation_threshold_) +
                static_cast<std::int32_t>(random_source.next() % 0x15u) - 10;
            offspring.activation_threshold_ =
                static_cast<std::uint8_t>(std::clamp(activation, 0, 0x82));
        }
    } else {
        // LibreCreatures deviation.  ReplicateAndMutate @00401c70 stores
        // `rand & 3` here (00401cc8..00401ce3) without the -0x18 that the
        // constructor and the input branch (SUB AL,8 at 00401d3f) apply, so
        // a mutated output became chemical 0-3 -- nothing, pain, need for
        // pleasure or hunger -- instead of one of the four disease
        // chemicals 232-235 (histamine A and B, sleep and fever toxin).
        // Mutate among the disease chemicals, as new bacteria choose.
        const std::int32_t chemical_id = normalized_masked_random(
            random_source.next(), 0x80000003u) - 0x18;
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
