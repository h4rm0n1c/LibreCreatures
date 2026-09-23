#include "attention.hpp"

namespace creatures1::creatures {

namespace {

// Creature::attention_records has 40 slots.
constexpr std::uint32_t kAttentionRecordCount = 40;

std::uint32_t native_attention_record_index(
    const AttentionClassifier& classifier) {
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

} // namespace

std::uint32_t get_attention_record_index(const AttentionClassifier& classifier) {
    // GetAttentionRecordIndex @ 0x00426430 (and its inlined copies in
    // Creature::UpdatePerception @ 0x0040bf10) map simple genus g to g,
    // compound to g + 25 and creature to g + 35 without a bound, so a
    // simple genus >= 40, compound >= 15 or creature >= 5 indexed past the
    // 40 attention records into learned_word_records and beyond.
    // LibreCreatures sends an out-of-range index to slot 0, the slot native
    // already uses for unknown and scenery objects; in-range slots are
    // unchanged.
    const std::uint32_t index = native_attention_record_index(classifier);
    return index < kAttentionRecordCount ? index : 0;
}

} // namespace creatures1::creatures
