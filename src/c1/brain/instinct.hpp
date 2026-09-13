#pragma once

#include <array>
#include <cstdint>

namespace creatures1::biochemistry {
class Biochemistry;
}

namespace creatures1::brain {

class Brain;

class InstinctArchive {
public:
    virtual ~InstinctArchive() = default;
    virtual bool is_loading() const = 0;
    virtual std::uint32_t read_u32() = 0;
    virtual void write_u32(std::uint32_t value) = 0;
};

class Instinct {
public:
    void serialize(InstinctArchive& archive);
    void run_dream_step(Brain& brain,
                        biochemistry::Biochemistry& biochemistry,
                        std::uint32_t tick);

    std::array<std::uint32_t, 3> replay_lobe_indices{};
    std::array<std::uint32_t, 3> replay_neuron_indices{};
    std::uint32_t decision_lobe_neuron_index = 0;
    std::uint32_t dream_chemical_index = 0;
    std::uint32_t dream_chemical_concentration = 0;
    std::uint32_t dream_step_index = 0;
};

}  // namespace creatures1::brain
