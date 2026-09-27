#pragma once

#include "genome.hpp"

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
// archive as a world or an .exp: the version, the classifier, the sex, then
// the genome as a "CGenome" object reference carrying CGenome's own record.
class Egg {
public:
    static constexpr std::uint32_t kVersion = 1;
    static constexpr std::uint32_t kClassifierMask = 0xffff0000u;
    static constexpr std::uint32_t kEggClassifier = 0x02050000u;  // 2 5 x

    static bool is_egg_classifier(std::uint32_t classifier) {
        return (classifier & kClassifierMask) == kEggClassifier;
    }

    // Throws std::runtime_error on a version this build cannot read.
    void serialize(objects::ObjectArchive& archive);

    std::uint32_t classifier = kEggClassifier;
    std::uint32_t sex = 1;  // obv1: 1 male, 2 female
    std::unique_ptr<Genome> genome;
};

} // namespace creatures1::creatures
