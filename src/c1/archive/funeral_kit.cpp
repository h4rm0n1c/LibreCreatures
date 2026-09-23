#include "funeral_kit.hpp"

namespace creatures1::archive {

void append_funeral_kit_document_state_word(
    std::vector<FuneralKitStateWord>& pending_state_words,
    FuneralKitStateWord value) {
    if (pending_state_words.size() < kMaximumPendingFuneralKitStateWords) {
        pending_state_words.push_back(value);
    }
}

void flush_funeral_kit_document_state_words(
    FuneralKitGateway& gateway,
    std::vector<FuneralKitStateWord>& pending_state_words) {
    if (!gateway.is_connected()) {
        return;
    }

    for (const FuneralKitStateWord value : pending_state_words) {
        gateway.submit_state_word(value);
    }
    pending_state_words.clear();
}

} // namespace creatures1::archive
