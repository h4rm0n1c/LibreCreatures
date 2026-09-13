#pragma once

#include <cstdint>

namespace creatures1::ui {

class VolumeSliderApi {
public:
    virtual ~VolumeSliderApi() = default;

    virtual void set_range(std::int32_t minimum, std::int32_t maximum) = 0;
    virtual void set_line_increment(std::int32_t increment) = 0;
    virtual std::int32_t position() const = 0;
    virtual void set_position(std::int32_t position) = 0;
};

struct SoundVolumeState {
    std::int32_t effect_volume_setting;
    std::int32_t target_master_attenuation;
    bool volume_target_update_inhibited;
};

class VolumePersistenceApi {
public:
    virtual ~VolumePersistenceApi() = default;

    virtual void persist_effect_volume(std::int32_t attenuation) = 0;
};

std::int32_t effect_volume_to_slider_position(std::int32_t attenuation);
std::int32_t slider_position_to_effect_volume(std::int32_t position);

void initialise_volume_dialog(VolumeSliderApi& slider,
                              const SoundVolumeState* sound_state);

void apply_volume_slider_change(VolumeSliderApi& slider,
                                SoundVolumeState* sound_state,
                                VolumePersistenceApi& persistence);

} // namespace creatures1::ui
