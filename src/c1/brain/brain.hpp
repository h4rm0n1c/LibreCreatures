#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "lobe.hpp"

namespace creatures1::brain {

enum class ActivityReportMode : std::uint32_t {
    firing_strength = 0,
    activation = 1,
    maximum_current_weight = 2,
    average_target_weight = 3,
    average_dendrite_state = 4,
};

class BrainUpdateHost {
public:
    virtual ~BrainUpdateHost() = default;
    virtual void report_perception_overrun() = 0;
};

// CBrain's archive is a fixed little-endian field stream.  MFC framing and
// buffer management stay in the concrete archive adapter; this interface
// exposes only the primitive transfers used by the recovered serializer.
class BrainArchive {
public:
    virtual ~BrainArchive() = default;
    virtual bool is_loading() const = 0;
    virtual std::uint32_t read_u32() = 0;
    virtual std::uint8_t read_byte() = 0;
    virtual void write_u32(std::uint32_t value) = 0;
    virtual void write_byte(std::uint8_t value) = 0;
};

// Semantic owner for the fixed C1 brain lobe array.  The binary layout is
// recorded in Ghidra; this class is the clean-source ownership boundary and
// is intentionally independent of compiler ABI details.
class Brain {
public:
    Lobe& lobe(std::uint32_t index) { return lobes_[index]; }
    const Lobe& lobe(std::uint32_t index) const { return lobes_[index]; }

    std::uint32_t lobe_count() const { return lobe_count_; }
    void set_lobe_count(std::uint32_t count) { lobe_count_ = count; }

    void load_genome(creatures1::creatures::Genome& genome);

    void serialize(BrainArchive& archive);

    void update(std::uint32_t tick, BrainUpdateHost* host = nullptr);

    // Resolve the byte addressed by a genome receptor or emitter locus.
    // The binary stores this resolver as a four-byte subobject immediately
    // before the lobe array; the clean model exposes the same ownership
    // through Brain and Lobe rather than reproducing that ABI bias.
    std::uint8_t* resolve_genome_locus(GenomeLocusKind kind,
                                       std::uint8_t tissue_index,
                                       std::uint8_t locus_index);

    void add_lobe_neuron_activation(std::uint32_t lobe_index,
                                    std::uint32_t neuron_index,
                                    std::uint8_t activation_delta);
    void set_lobe_neuron_activation(std::uint32_t lobe_index,
                                    std::uint32_t neuron_index,
                                    std::uint8_t activation);
    // Implements CAOS `fire`: scan lobes and neurons in native order for the
    // first neuron at the requested global coordinate, clamp the signed
    // activation to a byte, and replace that neuron's activation.
    void fire_neuron_at_global_position(std::int32_t global_x,
                                        std::int32_t global_y,
                                        std::int32_t activation);
    void initialize_connections();
    void reset_runtime_activity();
    // Dream completion raises each connection's baseline weight to its target
    // weight when the target has advanced beyond the baseline.
    void normalize_dream_connection_weights();

    // Writes three characters for each nonzero activity entry, terminates the
    // buffer, and returns the resulting byte count including that terminator.
    std::size_t format_activity_report(char* output,
                                       ActivityReportMode mode,
                                       int rule_index) const;

private:
    friend class Lobe;

    std::array<Lobe, 32> lobes_{};
    std::uint32_t lobe_count_ = 0;
};

} // namespace creatures1::brain
