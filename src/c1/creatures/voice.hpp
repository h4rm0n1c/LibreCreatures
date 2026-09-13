#pragma once

#include "../sound/sound.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace creatures1::creatures {

using VoiceTrigramSelectionMask = std::uint32_t;
using VoiceSoundDuration = std::int32_t;
using VoiceDelayTicks = std::int32_t;

constexpr std::size_t kVoiceAlphabetSize = 27;
constexpr std::size_t kVoiceTrigramMaskCount = kVoiceAlphabetSize * 3;
constexpr std::size_t kVoiceSelectionCount = 32;

// The archive buffer, MFC mode/error policy, and debug logging are supplied
// by the platform integration. Voice owns the persisted field order.
class VoiceArchive {
public:
    virtual ~VoiceArchive() = default;
    virtual bool is_loading() const = 0;
    virtual std::uint32_t read_uint32() = 0;
    virtual void write_uint32(std::uint32_t value) = 0;
};

// Resource path construction, file opening, and read/error policy belong to
// the platform adapter. Voice owns only the proven VCE payload layout.
class VoiceFileStore {
public:
    virtual ~VoiceFileStore() = default;
    virtual std::vector<std::uint8_t> read_voice_file(
        std::string_view filename) = 0;
};

class Voice {
public:
    Voice();
    ~Voice();

    std::string_view runtime_class_name() const noexcept;

    bool prepare_speech_text(std::string_view phrase);

    void load_voice_file(std::string_view filename, VoiceFileStore& files);

    void lookup_sound_pair_for_trigram(
        char previous,
        char current,
        char next,
        sound::SoundId& out_sound_id,
        VoiceSoundDuration& out_duration_ticks) const;

    bool get_next_sound_pair(sound::SoundId& out_sound_id,
                             VoiceDelayTicks& out_delay_ticks);

    void serialize(VoiceArchive& archive);

    std::array<VoiceTrigramSelectionMask, kVoiceTrigramMaskCount>&
    trigram_selection_masks() {
        return trigram_selection_masks_;
    }

    std::array<sound::SoundId, kVoiceSelectionCount>& sound_ids() {
        return sound_id_by_selection_;
    }

    std::array<VoiceSoundDuration, kVoiceSelectionCount>& durations() {
        return sound_duration_by_selection_;
    }

    // SpeakTextWithVoice @ 0040b560 returns this running total; it is the
    // bubble lifetime and the speech-range event delay.
    VoiceDelayTicks accumulated_sound_delay_ticks() const {
        return accumulated_sound_delay_ticks_;
    }

private:
    std::array<VoiceTrigramSelectionMask, kVoiceTrigramMaskCount>
        trigram_selection_masks_{};
    std::array<sound::SoundId, kVoiceSelectionCount> sound_id_by_selection_{};
    std::array<VoiceSoundDuration, kVoiceSelectionCount>
        sound_duration_by_selection_{};
    std::string normalized_phrase_;
    std::uint32_t normalized_phrase_cursor_ = 1;
    VoiceDelayTicks accumulated_sound_delay_ticks_ = 0;
};

std::unique_ptr<Voice> create_voice();

} // namespace creatures1::creatures
