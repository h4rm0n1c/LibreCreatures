#pragma once

#include "genome.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace creatures1::objects {
class ObjectArchive;
}

namespace creatures1::creatures {

// An egg saved to a file by File > Export Held Egg and read back by Import
// Egg (neither is in the 1996 game).  It holds what makes the egg that egg:
// its classifier (any egg, family 2 genus 5), the sex it hatches as (obv1)
// and the baby's genome, whose filename is the baby's moniker (obv0).
// Everything else -- its picture, its timer, where it is -- is a fresh
// hatchery egg's when it is imported.
//
// The file is one "CEgg" record written through the same MFC runtime-class
// archive as a world or an .exp: the version, the classifier, the sex, the
// genome's byte count and CRC-32, then the genome as a "CGenome" object
// reference carrying CGenome's own record.  Because each class appears once,
// the layout is fixed, and a file is checked byte for byte
// (is_well_formed_file) before any of it reaches the archive: people will
// rename a video to .egg and import it.
class Egg {
public:
    static constexpr std::uint32_t kVersion = 2;
    static constexpr std::uint32_t kClassifierMask = 0xffff0000u;
    static constexpr std::uint32_t kEggClassifier = 0x02050000u;  // 2 5 x

    // Far above any real genome (a norn's is about 8 KB).
    static constexpr std::size_t kMaxGenomeBytes = 1u << 20;
    // Everything before the genes: the CEgg record (30 bytes with its class
    // record) and the CGenome class record and header (26 bytes).
    static constexpr std::size_t kHeaderBytes = 56;

    static bool is_egg_classifier(std::uint32_t classifier) {
        return (classifier & kClassifierMask) == kEggClassifier;
    }

    // The standard CRC-32 (IEEE 802.3, as zlib computes it).
    static std::uint32_t crc32(const std::uint8_t* bytes, std::size_t count);

    // Whether a whole file is an egg this version reads: right size, right
    // class records, version, an egg classifier, a sex, sizes that agree
    // with each other and with the file, a moniker that is a safe filename,
    // genes that start "gene", and a matching CRC.
    static bool is_well_formed_file(const std::uint8_t* bytes,
                                    std::size_t size);

    // Throws std::runtime_error on a version this build cannot read, or a
    // genome whose size or CRC does not match the header.
    void serialize(objects::ObjectArchive& archive);

    std::uint32_t classifier = kEggClassifier;
    std::uint32_t sex = 1;  // obv1: 1 male, 2 female
    std::unique_ptr<Genome> genome;
};

} // namespace creatures1::creatures
