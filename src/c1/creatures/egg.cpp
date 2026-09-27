#include "egg.hpp"

#include "../objects/object.hpp"

#include <stdexcept>
#include <string>

namespace creatures1::creatures {

void Egg::serialize(objects::ObjectArchive& archive) {
    if (archive.is_loading()) {
        const std::uint32_t version = archive.read_uint32();
        if (version != kVersion) {
            throw std::runtime_error("unsupported egg file version " +
                                     std::to_string(version));
        }
        classifier = archive.read_uint32();
        sex = archive.read_uint32();
        // The archive's CGenome factory hands the new genome to the reader.
        genome.reset(
            static_cast<Genome*>(archive.read_object_reference("CGenome")));
        return;
    }
    archive.write_uint32(kVersion);
    archive.write_uint32(classifier);
    archive.write_uint32(sex);
    archive.write_object_reference(genome.get(), "CGenome");
}

} // namespace creatures1::creatures
