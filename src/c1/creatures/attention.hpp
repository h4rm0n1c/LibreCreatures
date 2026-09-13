#pragma once

#include <cstdint>

namespace creatures1::creatures {

// The classifier family values used by the attention-record table. Unknown
// and scenery families deliberately share the executable's slot-zero path.
enum class AttentionObjectFamily : std::uint8_t {
    unknown = 0,
    scenery = 1,
    simple_object = 2,
    compound_object = 3,
    creature = 4,
};

struct AttentionClassifier {
    AttentionObjectFamily family = AttentionObjectFamily::unknown;
    std::uint8_t genus = 0;
};

// Maps an object's classifier to the fixed C1 attention-record slot.
std::uint32_t get_attention_record_index(const AttentionClassifier& classifier);

} // namespace creatures1::creatures
