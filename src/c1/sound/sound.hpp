#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace creatures1::sound {

using SoundId = std::uint32_t;
using SoundChannelHandle = std::int32_t;

constexpr int kSoundSuccess = 0;
constexpr int kDirectSoundCooperativeLevelAlreadyAllocated = -0x7787fff6;
constexpr int kDirectSoundInvalidParameter = -0x7ff8ffa9;
constexpr int kDirectSoundBufferLost = -0x7787ff6a;
constexpr int kDirectSoundWrongPriority = -0x7787ffba;
constexpr int kSoundCacheCapacityOk = 0;
constexpr int kSoundCacheCapacityUnavailable = 2;
constexpr int kSoundMixerSuspended = 1;
constexpr int kSoundNoAvailableChannel = 2;
constexpr int kSoundInvalidChannelHandle = 3;
constexpr int kSoundOk = 0;
constexpr std::uint32_t kSoundChannelCount = 32;
// The original cached at most 0x80000 bytes (512 KB) of sound.  Looping
// sounds pin their cache entries while they play, and one of them alone can
// be 370 KB (juke.wav), so new sounds found no room and were silently
// dropped.  The whole stock Sounds folder is about 9 MB.
constexpr std::uint32_t kSoundCacheCapacityBytes = 32 * 1024 * 1024;

struct SoundFormat {
    std::uint16_t format_tag = 0;
    std::uint16_t channel_count = 0;
    std::uint32_t samples_per_second = 0;
    std::uint32_t average_bytes_per_second = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits_per_sample = 0;
};

struct LockedAudioRegions {
    std::uint8_t* first = nullptr;
    std::uint32_t first_size = 0;
    std::uint8_t* second = nullptr;
    std::uint32_t second_size = 0;
};

struct SoundChannel {
    void* backend_voice = nullptr;
    int active_generation = 0;
    std::uint32_t continuous_active_flag = 0;
    int fade_attenuation_step = 0;
    int attenuation = 0;
    struct CachedSound* cached_sound = nullptr;
};

struct CachedSound {
    void* primary_vptr = nullptr;
    SoundId sound_descriptor = 0;
    int last_use_generation = 0;
    int byte_size = 0;
    int backend_reference_count = 0;
    void* directsound_buffer = nullptr;
};

struct ScheduledSoundEvent {
    SoundId sound_id = 0;
    int remaining_ticks = 0;
    int attenuation = 0;
    int pan = 0;
};

// MFC CFile/CObArray, the DirectSound COM interfaces, USER32's main window,
// registry access, and the debug dialog are platform-owned.  The recovered
// sound policy talks to them only through this boundary.
class SoundSystemHost {
public:
    virtual ~SoundSystemHost() = default;

    virtual int create_direct_sound(void*& device) = 0;
    virtual void* main_window() const = 0;
    virtual int set_cooperative_level(void* device,
                                      void* owner_window,
                                      std::uint32_t priority) = 0;
    virtual int create_primary_buffer(void* device,
                                      std::uint32_t flags,
                                      void*& buffer) = 0;
    virtual int read_primary_format(void* buffer, SoundFormat& format) = 0;
    virtual int set_primary_format(void* buffer, const SoundFormat& format) = 0;

    virtual bool read_sound_file(SoundId id,
                                 std::vector<std::uint8_t>& bytes) = 0;
    virtual std::uint32_t sound_file_length(SoundId id) const = 0;
    virtual int create_pcm_buffer(void* device,
                                  std::uint32_t flags,
                                  const SoundFormat& format,
                                  std::uint32_t byte_size,
                                  void*& buffer) = 0;
    virtual int lock_audio_buffer(void* buffer,
                                  std::uint32_t byte_size,
                                  LockedAudioRegions& regions) = 0;
    virtual bool restore_audio_buffer(void* buffer) = 0;
    virtual void unlock_audio_buffer(void* buffer,
                                     const LockedAudioRegions& regions) = 0;
    virtual int duplicate_audio_buffer(void* device,
                                       void* source_buffer,
                                       void*& duplicate_buffer) = 0;
    virtual int get_audio_buffer_status(void* buffer,
                                        std::uint32_t& status) = 0;
    virtual int set_audio_buffer_volume(void* buffer, int attenuation) = 0;
    virtual int set_audio_buffer_pan(void* buffer, int pan) = 0;
    virtual int play_audio_buffer(void* buffer, bool loop) = 0;
    virtual bool audio_buffer_is_playing(void* buffer) const = 0;
    virtual void stop_audio_buffer(void* buffer) = 0;
    virtual void release_audio_buffer(void* buffer) = 0;
    virtual void release_direct_sound(void* device) = 0;
    virtual int compact_direct_sound(void* device) = 0;

    virtual bool read_effect_volume(std::int32_t& value) = 0;
    virtual void write_default_effect_volume(std::int32_t value) = 0;
    virtual bool read_sonic_preference(std::string& value) = 0;
    virtual void write_default_sonic_preference(std::string_view value) = 0;

    virtual void log(std::uint32_t category, std::string_view message) = 0;
    virtual void log_channel_stop(std::uint32_t channel_index) = 0;
    virtual void log_cache_inventory(
        const std::vector<const CachedSound*>& entries,
        int total_bytes) = 0;

    // Opt-in diagnostics (not in the original): one line per sound played
    // or dropped, with the reason.  Off unless the platform enables it.
    virtual bool trace_enabled() const { return false; }
    virtual void trace(std::string_view) {}
};

class SoundManager {
public:
    explicit SoundManager(SoundSystemHost& host);
    ~SoundManager();

    CachedSound* load_cached_sound(SoundId sound_id);
    CachedSound* find_or_load_cache_entry(SoundId sound_id);
    int ensure_sound_cache_capacity(int required_bytes);
    SoundChannelHandle start_channel(CachedSound* cached_sound,
                                     int attenuation,
                                     int pan,
                                     bool loop);
    int play_or_queue(SoundId sound_id,
                      int queue_delay_ticks,
                      int attenuation,
                      int pan);
    int start_continuous_sound(SoundId sound_id,
                               SoundChannelHandle& out_channel_handle,
                               int attenuation,
                               int pan,
                               bool loop);

    void stop_all_sounds();
    void stop_channel(std::uint32_t channel_handle);
    // The world-pause sweep clears the continuous marker before it stops
    // the channel, so update() does not treat the channel as a live
    // continuous sound while the world is stopped.
    void clear_continuous_channel_marker(std::uint32_t channel_handle) {
        if (channel_handle < kSoundChannelCount) {
            channels_[channel_handle].continuous_active_flag = 0;
        }
    }
    void destroy_cached_sound(CachedSound* cached_sound);
    void clear_sound_cache();
    int stop_continuous_sound(std::uint32_t channel_handle, bool fade_out);
    bool mixer_suspended() const { return mixer_suspended_flag_; }
    bool continuous_channel_is_playing(SoundChannelHandle channel_handle);
    bool update_continuous_channel(SoundChannelHandle channel_handle,
                                   int attenuation, int pan);
    int suspend_mixer();
    void restore_mixer();
    void update();
    void set_cache_capacity(int capacity_bytes) {
        sound_cache_capacity_bytes_ = capacity_bytes;
    }

    // Writes a formatted line through SoundSystemHost::trace when enabled.
    void trace(const char* format, ...);
    bool trace_enabled() const { return host_.trace_enabled(); }
    // The four-character name of a sound (its file name), for traces.
    static std::string sound_name(SoundId sound_id);

    bool backend_ready() const { return backend_ready_flag_; }
    SoundId sound_descriptor_override() const {
        return sound_descriptor_override_;
    }
    void set_sound_descriptor_override(SoundId sound_id) {
        sound_descriptor_override_ = sound_id;
    }
    void set_effect_volume_setting(int value) {
        effect_volume_setting_ = value;
        if (!volume_target_update_inhibit_flag_) {
            target_master_attenuation_ = value;
        }
    }

    bool volume_target_update_inhibited() const {
        return volume_target_update_inhibit_flag_;
    }
    int effect_volume_setting() const { return effect_volume_setting_; }
    int target_master_attenuation() const { return target_master_attenuation_; }

private:
    SoundSystemHost& host_;
    void* direct_sound_ = nullptr;
    void* primary_sound_buffer_ = nullptr;
    int active_channel_count_ = 0;
    std::array<SoundChannel, kSoundChannelCount> channels_{};
    int playback_generation_ = 0;
    bool backend_ready_flag_ = false;
    std::vector<std::unique_ptr<ScheduledSoundEvent>> scheduled_sound_events_;
    SoundId sound_descriptor_override_ = 0;
    bool mixer_suspended_flag_ = false;
    int effect_volume_setting_ = 0;
    int target_master_attenuation_ = 0;
    int master_attenuation_ = 0;
    bool volume_target_update_inhibit_flag_ = false;
    int sound_cache_capacity_bytes_ = 0;
    int sound_cache_total_bytes_ = 0;
    int sound_cache_generation_ = 0;
    std::vector<std::unique_ptr<CachedSound>> sound_cache_;
};

// Recovered startup policy.  The returned owner represents the original
// process-global SoundManager; the platform adapter decides where to retain it.
std::unique_ptr<SoundManager> initialize_sound_system(SoundSystemHost& host);

} // namespace creatures1::sound
