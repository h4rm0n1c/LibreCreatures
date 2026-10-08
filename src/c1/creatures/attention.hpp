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

// Whether the classifier has an attention slot of its own.  An unknown or
// scenery object has slot 0, as in C1; a simple genus of 40 or more, a
// compound genus of 15 or more or a creature genus of 5 or more has none --
// C1 indexed past the 40 records for those, and get_attention_record_index
// sends them to slot 0 only so that no lookup overruns.
bool has_attention_record(const AttentionClassifier& classifier);

} // namespace creatures1::creatures
