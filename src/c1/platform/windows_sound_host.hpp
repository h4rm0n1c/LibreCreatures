#pragma once

#include "../sound/sound.hpp"

#include <string>

struct HWND__;
using HWND = HWND__*;

namespace creatures1::platform {

// Concrete C1 sound boundary.  SoundManager owns the recovered policy; this
// adapter owns DirectSound, Win32 file I/O, and the persisted HKCU settings.
class WindowsSoundSystemHost final : public sound::SoundSystemHost {
public:
    WindowsSoundSystemHost(HWND owner_window, std::string sound_directory);
    ~WindowsSoundSystemHost() override = default;

    int create_direct_sound(void*& device) override;
    void* main_window() const override;
    int set_cooperative_level(void* device, void* owner_window,
                              std::uint32_t priority) override;
    int create_primary_buffer(void* device, std::uint32_t flags,
                              void*& buffer) override;
    int read_primary_format(void* buffer, sound::SoundFormat& format) override;
    int set_primary_format(void* buffer,
                           const sound::SoundFormat& format) override;

    bool read_sound_file(sound::SoundId id,
                         std::vector<std::uint8_t>& bytes) override;
    std::uint32_t sound_file_length(sound::SoundId id) const override;
    int create_pcm_buffer(void* device, std::uint32_t flags,
                          const sound::SoundFormat& format,
                          std::uint32_t byte_size, void*& buffer) override;
    int lock_audio_buffer(void* buffer, std::uint32_t byte_size,
                          sound::LockedAudioRegions& regions) override;
    bool restore_audio_buffer(void* buffer) override;
    void unlock_audio_buffer(void* buffer,
                            const sound::LockedAudioRegions& regions) override;
    int duplicate_audio_buffer(void* device, void* source_buffer,
                               void*& duplicate_buffer) override;
    int get_audio_buffer_status(void* buffer,
                                std::uint32_t& status) override;
    int set_audio_buffer_volume(void* buffer, int attenuation) override;
    int set_audio_buffer_pan(void* buffer, int pan) override;
    int play_audio_buffer(void* buffer, bool loop) override;
    bool audio_buffer_is_playing(void* buffer) const override;
    void stop_audio_buffer(void* buffer) override;
    void release_audio_buffer(void* buffer) override;
    void release_direct_sound(void* device) override;
    int compact_direct_sound(void* device) override;

    bool read_effect_volume(std::int32_t& value) override;
    void write_default_effect_volume(std::int32_t value) override;
    bool read_sonic_preference(std::string& value) override;
    void write_default_sonic_preference(std::string_view value) override;

    void log(std::uint32_t category, std::string_view message) override;
    void log_channel_stop(std::uint32_t channel_index) override;
    void log_cache_inventory(
        const std::vector<const sound::CachedSound*>& entries,
        int total_bytes) override;

private:
    std::string sound_path(sound::SoundId id) const;
    bool open_settings_key(unsigned access, void*& key) const;

    HWND owner_window_ = nullptr;
    std::string sound_directory_;
};

} // namespace creatures1::platform
