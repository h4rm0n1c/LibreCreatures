#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace creatures1::archive {

// The binary stores these as 32-bit values.  Their internal encoding is not
// established by the recovered code, so the archive layer deliberately keeps
// them as values and does not assign a made-up identifier type.
using FuneralKitStateWord = std::uint32_t;

class FuneralKitGateway {
public:
    virtual ~FuneralKitGateway() = default;

    virtual bool is_connected() const = 0;
    virtual void submit_state_word(FuneralKitStateWord value) = 0;
};

// The document persists at most sixteen pending words.
constexpr std::size_t kMaximumPendingFuneralKitStateWords = 16;

void append_funeral_kit_document_state_word(
    std::vector<FuneralKitStateWord>& pending_state_words,
    FuneralKitStateWord value);

// FlushFuneralKitDocumentStateWords @ 00435c10: deliver each pending word to
// the Funeral Kit and clear them.  While the kit is not connected the words
// stay queued so the Graveyard still receives them once it connects.
void flush_funeral_kit_document_state_words(
    FuneralKitGateway& gateway,
    std::vector<FuneralKitStateWord>& pending_state_words);

} // namespace creatures1::archive
