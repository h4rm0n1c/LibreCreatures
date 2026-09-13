#include "attention.hpp"

namespace creatures1::creatures {

std::uint32_t get_attention_record_index(const AttentionClassifier& classifier) {
    switch (classifier.family) {
    case AttentionObjectFamily::simple_object:
        return classifier.genus;
    case AttentionObjectFamily::compound_object:
        return static_cast<std::uint32_t>(classifier.genus) + 0x19;
    case AttentionObjectFamily::creature:
        return static_cast<std::uint32_t>(classifier.genus) + 0x23;
    case AttentionObjectFamily::unknown:
    case AttentionObjectFamily::scenery:
        return 0;
    }
    return 0;
}

} // namespace creatures1::creatures
