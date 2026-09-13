#pragma once

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

void flush_funeral_kit_document_state_words(
    FuneralKitGateway& gateway,
    std::vector<FuneralKitStateWord>& pending_state_words);

} // namespace creatures1::archive
