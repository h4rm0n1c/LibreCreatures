#include "instinct.hpp"

#include "brain.hpp"
#include "../biochemistry/biochemistry.hpp"

namespace creatures1::brain {

void Instinct::run_dream_step(Brain& brain,
                              biochemistry::Biochemistry& biochemistry,
                              std::uint32_t tick) {
    biochemistry.update(tick);

    const std::uint32_t lobe_count = brain.lobe_count();
    const std::uint32_t reset_count =
        lobe_count < 8 ? lobe_count : 8;
    for (std::uint32_t lobe_index = 0; lobe_index < reset_count;
         ++lobe_index) {
        Lobe& lobe = brain.lobe(lobe_index);
        for (std::uint32_t neuron_index = 0;
             neuron_index < lobe.neuron_count_value();
             ++neuron_index) {
            LobeNeuron& neuron = lobe.neuron(neuron_index);
            neuron.firing_strength = 0;
            neuron.activation = 0;
        }
    }

    for (std::size_t replay_index = 0;
         replay_index < replay_lobe_indices.size();
         ++replay_index) {
        const std::uint32_t lobe_index = replay_lobe_indices[replay_index];
        if (lobe_index == 0 || lobe_index >= lobe_count) {
            continue;
        }
        Lobe& lobe = brain.lobe(lobe_index);
        const std::uint32_t neuron_index = replay_neuron_indices[replay_index];
        if (neuron_index < lobe.neuron_count_value()) {
            lobe.neuron(neuron_index).activation = 0xff;
        }
    }

    if (lobe_count > 6) {
        Lobe& decision_lobe = brain.lobe(6);
        if (decision_lobe_neuron_index < decision_lobe.neuron_count_value()) {
            LobeNeuron& decision_neuron =
                decision_lobe.neuron(decision_lobe_neuron_index);
            decision_neuron.firing_strength = 0xff;
            decision_neuron.activation = 0xff;
        }
    }

    brain.update(tick);
}

void Instinct::serialize(InstinctArchive& archive) {
    if (archive.is_loading()) {
        for (std::uint32_t& index : replay_lobe_indices) {
            index = archive.read_u32();
        }
        for (std::uint32_t& index : replay_neuron_indices) {
            index = archive.read_u32();
        }
        decision_lobe_neuron_index = archive.read_u32();
        dream_chemical_index = archive.read_u32();
        dream_chemical_concentration = archive.read_u32();
        dream_step_index = archive.read_u32();
        return;
    }

    for (const std::uint32_t index : replay_lobe_indices) {
        archive.write_u32(index);
    }
    for (const std::uint32_t index : replay_neuron_indices) {
        archive.write_u32(index);
    }
    archive.write_u32(decision_lobe_neuron_index);
    archive.write_u32(dream_chemical_index);
    archive.write_u32(dream_chemical_concentration);
    archive.write_u32(dream_step_index);
}

}  // namespace creatures1::brain
