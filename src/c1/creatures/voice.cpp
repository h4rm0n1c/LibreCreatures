#include "voice.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace creatures1::creatures {

namespace {

char lowercase_ascii(char character) {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character - 'A' + 'a');
    }
    return character;
}

} // namespace

Voice::Voice() {
    durations().fill(1);
}

Voice::~Voice() = default;

std::string_view Voice::runtime_class_name() const noexcept {
    return "Voice";
}

void Voice::load_voice_file(std::string_view filename, VoiceFileStore& files) {
    constexpr std::size_t kVoicePairBytes = kVoiceSelectionCount * 8;
    constexpr std::size_t kMaskBytes = kVoiceTrigramMaskCount * 4;
    constexpr std::size_t kMinimumVoiceFileBytes =
        kVoicePairBytes + kMaskBytes;

    const std::vector<std::uint8_t> payload = files.read_voice_file(filename);
    if (payload.size() < kMinimumVoiceFileBytes) {
        throw std::runtime_error("C1 voice file is shorter than its VCE payload");
    }

    const auto read_u32 = [&payload](std::size_t& cursor) {
        const std::uint32_t value =
            static_cast<std::uint32_t>(payload[cursor]) |
            (static_cast<std::uint32_t>(payload[cursor + 1]) << 8) |
            (static_cast<std::uint32_t>(payload[cursor + 2]) << 16) |
            (static_cast<std::uint32_t>(payload[cursor + 3]) << 24);
        cursor += 4;
        return value;
    };

    std::size_t cursor = 0;
    for (std::size_t index = 0; index < kVoiceSelectionCount; ++index) {
        sound_id_by_selection_[index] = read_u32(cursor);
        sound_duration_by_selection_[index] =
            static_cast<VoiceSoundDuration>(read_u32(cursor));
    }
    for (VoiceTrigramSelectionMask& mask : trigram_selection_masks_) {
        mask = read_u32(cursor);
    }
}

bool Voice::prepare_speech_text(std::string_view phrase) {
    std::string normalized;
    normalized.reserve(phrase.size() + 2);
    normalized.push_back('.');

    char last_appended_character = '\0';
    for (char character : phrase) {
        const char lowercase_character = lowercase_ascii(character);
        if (lowercase_character == ' ') {
            if (last_appended_character != '\0' &&
                last_appended_character != ' ') {
                normalized.push_back(lowercase_character);
                last_appended_character = lowercase_character;
            }
        } else if (lowercase_character >= 'a' &&
                   lowercase_character <= 'z') {
            normalized.push_back(lowercase_character);
            last_appended_character = lowercase_character;
        }
    }

    normalized.push_back('.');
    normalized_phrase_ = std::move(normalized);
    normalized_phrase_cursor_ = 1;
    accumulated_sound_delay_ticks_ = 0;
    return normalized_phrase_.size() > 2;
}

void Voice::lookup_sound_pair_for_trigram(
    char previous,
    char current,
    char next,
    sound::SoundId& out_sound_id,
    VoiceSoundDuration& out_duration_ticks) const {
    constexpr std::uint32_t kSpaceOrPeriod = 26;
    constexpr std::uint32_t kCurrentCharacterMask = 0x0000000fu;
    constexpr std::uint32_t kPreviousCharacterMask = 0x000007f0u;
    constexpr std::uint32_t kNextCharacterMask = 0x0003f800u;
    constexpr std::uint32_t kAllLettersMask = 0xfffc0000u;

    std::array<std::uint32_t, 3> character_indices{};
    const std::array<char, 3> input = {previous, current, next};
    for (std::size_t index = 0; index < input.size(); ++index) {
        character_indices[index] =
            (input[index] == ' ' || input[index] == '.')
                ? kSpaceOrPeriod
                : static_cast<std::uint32_t>(
                      static_cast<std::int32_t>(input[index]) - 'a');
    }

    const std::uint32_t previous_index = character_indices[0];
    const std::uint32_t current_index = character_indices[1];
    const std::uint32_t next_index = character_indices[2];
    const std::uint32_t intersection =
        trigram_selection_masks_[next_index + 2 * kVoiceAlphabetSize] &
        trigram_selection_masks_[current_index + kVoiceAlphabetSize] &
        trigram_selection_masks_[previous_index];

    std::uint32_t candidates =
        (current_index == kSpaceOrPeriod) ? intersection & kCurrentCharacterMask
                                          : intersection;
    candidates = (previous_index == kSpaceOrPeriod)
                     ? candidates & kPreviousCharacterMask
                     : candidates;
    candidates = (next_index == kSpaceOrPeriod)
                     ? candidates & kNextCharacterMask
                     : candidates;
    if (previous_index < kSpaceOrPeriod &&
        current_index < kSpaceOrPeriod && next_index < kSpaceOrPeriod) {
        candidates &= kAllLettersMask;
    }

    std::uint32_t selection = 0;
    if (candidates != 0) {
        std::array<std::uint32_t, kVoiceSelectionCount> available{};
        std::size_t available_count = 0;
        for (std::uint32_t bit = 0; bit < kVoiceSelectionCount; ++bit) {
            if ((candidates & (1u << bit)) != 0) {
                available[available_count++] = bit;
            }
        }
        const std::uint32_t character_sum =
            previous_index + current_index + next_index;
        selection = available[character_sum % available_count];
    } else {
        const std::uint32_t character_sum =
            previous_index + current_index + next_index;
        if (current_index == kSpaceOrPeriod) {
            // The executable's signed-mask normalization reduces this
            // non-negative C1 character sum to the low two bits here.
            selection = character_sum & 0x80000003u;
            if (static_cast<std::int32_t>(selection) < 0) {
                selection = ((selection - 1u) | 0xfffffffcu) + 1u;
            }
        } else if (previous_index == kSpaceOrPeriod) {
            selection = character_sum % 7u + 4u;
        } else if (next_index == kSpaceOrPeriod) {
            selection = character_sum % 7u + 10u;
        } else {
            selection = character_sum % 14u + 18u;
        }
    }

    out_sound_id = sound_id_by_selection_[selection];
    out_duration_ticks = sound_duration_by_selection_[selection];
}

bool Voice::get_next_sound_pair(sound::SoundId& out_sound_id,
                                VoiceDelayTicks& out_delay_ticks) {
    out_sound_id = 0;
    out_delay_ticks = 0;

    if (normalized_phrase_.size() <= 1) {
        return false;
    }

    while (normalized_phrase_cursor_ < normalized_phrase_.size() - 1) {
        const std::size_t cursor = normalized_phrase_cursor_;
        VoiceSoundDuration duration = 0;
        lookup_sound_pair_for_trigram(
            normalized_phrase_[cursor - 1], normalized_phrase_[cursor],
            normalized_phrase_[cursor + 1], out_sound_id, duration);

        out_delay_ticks = accumulated_sound_delay_ticks_;
        ++normalized_phrase_cursor_;
        accumulated_sound_delay_ticks_ += duration;

        if (normalized_phrase_cursor_ < normalized_phrase_.size() - 1) {
            const char first = normalized_phrase_[cursor];
            const char second = normalized_phrase_[normalized_phrase_cursor_];
            const char third =
                normalized_phrase_[normalized_phrase_cursor_ + 1];
            const bool all_letters =
                first >= 'a' && first <= 'z' && second >= 'a' &&
                second <= 'z' && third >= 'a' && third <= 'z';
            if (all_letters) {
                ++normalized_phrase_cursor_;
            }
        }

        if (out_sound_id != 0) {
            return true;
        }
    }
    return false;
}

void Voice::serialize(VoiceArchive& archive) {
    if (archive.is_loading()) {
        for (VoiceTrigramSelectionMask& mask : trigram_selection_masks_) {
            mask = archive.read_uint32();
        }
        for (std::size_t index = 0; index < kVoiceSelectionCount; ++index) {
            sound_id_by_selection_[index] = archive.read_uint32();
            sound_duration_by_selection_[index] =
                static_cast<VoiceSoundDuration>(archive.read_uint32());
        }
        return;
    }

    for (VoiceTrigramSelectionMask mask : trigram_selection_masks_) {
        archive.write_uint32(mask);
    }
    for (std::size_t index = 0; index < kVoiceSelectionCount; ++index) {
        archive.write_uint32(sound_id_by_selection_[index]);
        archive.write_uint32(static_cast<std::uint32_t>(
            sound_duration_by_selection_[index]));
    }
}

std::unique_ptr<Voice> create_voice() {
    return std::make_unique<Voice>();
}

} // namespace creatures1::creatures
