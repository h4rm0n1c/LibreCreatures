#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "../creatures/genome.hpp"

namespace creatures1::brain {

class Brain;

enum class GenomeLocusKind : std::uint32_t {
    receptor = 0,
    emitter = 1,
};

struct LobeNeuron;

// The brain grid.  C1 lays every lobe out on a 64 x 64 grid: a genome's
// offsets fold into 0..63 and a lobe that runs off the edge is pulled back.
// LibreCreatures deviation, off unless the "ExtendedBrainGrid" setting is on:
// the extended grid lets offsets use the whole byte, placing lobes anywhere
// on a 208 x 208 grid.  Lobe sizes keep their caps (63 a side, 1024 neurons),
// so no brain grows; lobes only get room.  208 because the brain report sends
// a neuron's grid x and y as '0' + coordinate, which runs out of byte at 207.
// Positions are behaviour (CAOS `fire x y` addresses neurons by grid
// position), and a brain keeps the layout it was built with, so the setting
// affects brains built after it changes.
constexpr std::uint32_t kStandardBrainGridExtent = 0x40;
constexpr std::uint32_t kExtendedBrainGridExtent = 208;
void set_extended_brain_grid(bool enabled);
bool extended_brain_grid();

struct LobeConnection {
    LobeNeuron* target_neuron = nullptr;
    std::uint8_t target_grid_x = 0;
    std::uint8_t target_grid_y = 0;
    std::uint8_t current_weight = 0;
    std::uint8_t target_weight = 0;
    std::uint8_t baseline_weight = 0;
    std::uint8_t dendrite_state = 0;
    std::array<std::uint8_t, 2> unserialized_tail{};
};

struct LobeNeuron {
    std::uint8_t grid_x = 0;
    std::uint8_t grid_y = 0;
    std::uint8_t firing_strength = 0;
    std::uint8_t activation = 0;
    LobeConnection* rule0_connections_begin = nullptr;
    LobeConnection* rule1_connections_begin = nullptr;
    std::uint8_t rule0_connection_count = 0;
    std::uint8_t rule1_connection_count = 0;
    std::uint8_t winner_take_all_excluded = 0;
    std::uint8_t source_lobe_index = 0;
};

struct LobeRuleExpression {
    std::array<std::uint8_t, 10> tokens{};
    void load_from_genome(creatures1::creatures::Genome& genome);
};

enum class StandardLobeIndex : std::uint8_t {
    perception = 0,
    drive = 1,
    stimulus_source = 2,
    verb = 3,
    noun = 4,
    general_sensory = 5,
    decision = 6,
    attention = 7,
    concept_lobe = 8,
};

// These packed values are consumed by the lobe allocation/update paths. The
// names and values are established by the live count-distribution switch and
// connection-mode branches, not inferred from the decoder's temporaries.
enum class ConnectionCountDistribution : std::uint8_t {
    uniform = 0,
    two_sample_average = 1,
    two_sample_absolute_difference = 2,
    two_sample_complement_difference = 3,
};

enum class ConnectionMode : std::uint8_t {
    ordinary = 0,
    attach_loose_dendrites = 1,
    migrate_connections = 2,
};

struct LobeConnectionRule {
    std::uint32_t target_lobe_index = 0;
    std::uint8_t connection_count_min = 0;
    std::uint8_t connection_count_max = 0;
    ConnectionCountDistribution count_distribution =
        ConnectionCountDistribution::uniform;
    std::uint8_t target_cell_spread_radius = 0;
    std::uint8_t baseline_weight_min = 0;
    std::uint8_t baseline_weight_max = 0;
    std::uint8_t dendrite_state_min = 0;
    std::uint8_t dendrite_state_max = 0;
    ConnectionMode connection_mode = ConnectionMode::ordinary;
    std::uint8_t current_weight_decay_selector = 0;
    std::uint8_t target_weight_convergence_selector = 0;
    std::uint8_t baseline_weight_step_interval = 0;
    std::uint8_t dendrite_growth_interval = 0;
    LobeRuleExpression dendrite_growth_expression{};
    std::uint8_t dendrite_decay_interval = 0;
    LobeRuleExpression dendrite_decay_expression{};
    LobeRuleExpression current_weight_expression{};
    LobeRuleExpression target_weight_expression{};
    std::array<std::uint8_t, 2> not_archived{};

    void load_from_genome(creatures1::creatures::Genome& genome);
};

struct LobeLatePhaseRuntimeState {
    std::array<std::uint8_t, 2> per_rule_loose_dendrite_count{};
    std::array<std::uint8_t, 4> token_evaluation_state{};
    std::uint8_t padding = 0;
};

enum class PerceptionCopyState : std::uint32_t {
    not_copied = 0,
    copy_to_perception = 1,
    copy_to_perception_mutually_exclusive = 2,
};

enum class WinnerTakeAllFlag : std::uint8_t {
    enabled = 1,
};

class Lobe {
public:
    Lobe();
    ~Lobe();

    void load_genome(creatures1::creatures::Genome& genome,
                     StandardLobeIndex lobe_index);
    void allocate_runtime_state();
    void update_early_phase(const Brain* brain, std::uint32_t tick);
    void update_late_phase(Brain* brain, std::uint32_t tick);
    bool copies_to_perception() const {
        return percept_state != PerceptionCopyState::not_copied;
    }
    PerceptionCopyState perception_copy_state() const { return percept_state; }

    const LobeConnectionRule& connection_rule(std::size_t index) const;
    LobeConnectionRule& connection_rule(std::size_t index);
    LobeLatePhaseRuntimeState& late_phase_state();
    std::uint32_t active_neuron_count() const;
    LobeNeuron* active_neuron(std::uint32_t index) const;
    void swap_active_neurons(std::uint32_t first, std::uint32_t second);
    std::uint32_t neuron_count_value() const;
    std::uint32_t grid_width_value() const;
    std::uint32_t grid_height_value() const;
    LobeNeuron& neuron(std::uint32_t index);
    const LobeNeuron& neuron(std::uint32_t index) const;
    std::uint8_t winner_take_all_flags_value() const {
        return winner_take_all_flags;
    }
    std::uint32_t grid_x_offset_value() const;
    std::uint32_t grid_y_offset_value() const;
    std::uint8_t* resolve_genome_locus(GenomeLocusKind kind,
                                       std::uint8_t locus_index);
    void add_neuron_activation(std::uint32_t index,
                               std::uint8_t activation_delta);
    // CAOS `trig` replaces both byte lanes used by the native neuron record
    // with one parsed activation value; it does not add to the old value.
    void set_neuron_activation(std::uint32_t index,
                               std::uint8_t activation);
    void reset_runtime_activity();

private:
    friend class Brain;

    std::uint32_t grid_x_offset = 0;
    std::uint32_t grid_y_offset = 0;
    std::uint32_t grid_width = 0;
    std::uint32_t grid_height = 0;
    std::uint8_t activation_threshold = 0;
    std::uint8_t activation_relaxation_selector = 0;
    std::uint8_t activation_baseline = 0;
    std::uint8_t input_gain = 0;
    LobeRuleExpression lobe_expression{};
    std::uint8_t winner_take_all_flags = 0;
    std::uint8_t padding = 0;
    std::array<LobeConnectionRule, 2> connection_rules{};
    std::unique_ptr<LobeNeuron[]> neurons;
    std::unique_ptr<LobeConnection[]> connections;
    std::uint32_t neuron_count = 0;
    std::uint32_t total_connection_count = 0;
    std::unique_ptr<LobeNeuron*[]> active_neurons;
    std::uint32_t active_neuron_count_ = 0;
    PerceptionCopyState percept_state = PerceptionCopyState::not_copied;
    std::uint32_t rule0_migration_candidate_neuron_index = 99999;
    std::uint32_t rule1_migration_candidate_neuron_index = 99999;
    std::uint8_t active_fraction = 0;
    LobeLatePhaseRuntimeState late_phase_runtime_state{};
};

} // namespace creatures1::brain
