#include "volume_dialog.hpp"

#include <algorithm>
#include <cmath>

namespace creatures1::ui {
namespace {

constexpr std::int32_t kMinimumAttenuation = -5000;
constexpr std::int32_t kMaximumAttenuation = 0;
constexpr std::int32_t kMinimumSliderPosition = 0;
constexpr std::int32_t kMaximumSliderPosition = 255;
constexpr std::int32_t kSliderLineIncrement = 16;
constexpr double kAttenuationScale = 2000.0;

} // namespace

std::int32_t effect_volume_to_slider_position(std::int32_t attenuation) {
    if (attenuation >= kMaximumAttenuation) {
        return kMaximumSliderPosition;
    }
    // Native: `if (effect_volume_setting < -4999)`, i.e. <= -5000 goes
    // straight to 0. A strict `< kMinimumAttenuation` here left exactly
    // -5000 -- the common clamped floor from normalize_effect_volume --
    // falling through to the pow() branch and computing 1, not 0.
    if (attenuation <= kMinimumAttenuation) {
        return kMinimumSliderPosition;
    }

    const double linear_factor = std::pow(
        10.0, static_cast<double>(attenuation) / kAttenuationScale);
    const double bounded_factor = std::clamp(linear_factor, 0.0, 1.0);
    return static_cast<std::int32_t>(
        bounded_factor * static_cast<double>(kMaximumSliderPosition) + 0.5);
}

std::int32_t slider_position_to_effect_volume(std::int32_t position) {
    const std::int32_t bounded_position =
        std::clamp(position, kMinimumSliderPosition, kMaximumSliderPosition);
    if (bounded_position == kMinimumSliderPosition) {
        return kMinimumAttenuation;
    }
    if (bounded_position == kMaximumSliderPosition) {
        return kMaximumAttenuation;
    }

    const double linear_factor =
        static_cast<double>(bounded_position) /
        static_cast<double>(kMaximumSliderPosition);
    const auto attenuation = static_cast<std::int32_t>(
        std::log10(linear_factor) * kAttenuationScale);
    return std::max(attenuation, kMinimumAttenuation);
}

void initialise_volume_dialog(VolumeSliderApi& slider,
                              const SoundVolumeState* sound_state) {
    slider.set_range(kMinimumSliderPosition, kMaximumSliderPosition);
    slider.set_line_increment(kSliderLineIncrement);
    const auto position = sound_state == nullptr
                              ? kMaximumSliderPosition
                              : effect_volume_to_slider_position(
                                    sound_state->effect_volume_setting);
    slider.set_position(position);
}

void apply_volume_slider_change(VolumeSliderApi& slider,
                                SoundVolumeState* sound_state,
                                VolumePersistenceApi& persistence) {
    if (sound_state == nullptr) {
        return;
    }

    const auto attenuation = slider_position_to_effect_volume(slider.position());
    sound_state->effect_volume_setting = attenuation;
    if (!sound_state->volume_target_update_inhibited) {
        sound_state->target_master_attenuation = attenuation;
    }
    persistence.persist_effect_volume(attenuation);
}

} // namespace creatures1::ui
