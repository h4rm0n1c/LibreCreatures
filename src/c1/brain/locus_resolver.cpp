#include "brain.hpp"

namespace creatures1::brain {

std::uint8_t* Brain::resolve_genome_locus(GenomeLocusKind kind,
                                          std::uint8_t tissue_index,
                                          std::uint8_t locus_index) {
    if (lobe_count_ == 0) {
        return nullptr;
    }

    const std::uint32_t normalized_tissue_index = tissue_index % lobe_count_;
    if (normalized_tissue_index >= lobes_.size()) {
        return nullptr;
    }
    return lobes_[normalized_tissue_index].resolve_genome_locus(kind,
                                                                 locus_index);
}

} // namespace creatures1::brain
