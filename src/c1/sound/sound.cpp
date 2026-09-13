#include "sound.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>

namespace creatures1::sound {
namespace {

class WaveReader {
public:
    explicit WaveReader(const std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}

    bool read_tag(std::array<char, 4>& tag) {
        if (!read_bytes(tag.data(), tag.size())) {
            return false;
        }
        return true;
    }

    bool read_u16(std::uint16_t& value) {
        std::array<std::uint8_t, 2> bytes{};
        if (!read_bytes(bytes.data(), bytes.size())) {
            return false;
        }
        value = static_cast<std::uint16_t>(bytes[0]) |
                (static_cast<std::uint16_t>(bytes[1]) << 8);
        return true;
    }

    bool read_u32(std::uint32_t& value) {
        std::array<std::uint8_t, 4> bytes{};
        if (!read_bytes(bytes.data(), bytes.size())) {
            return false;
        }
        value = static_cast<std::uint32_t>(bytes[0]) |
                (static_cast<std::uint32_t>(bytes[1]) << 8) |
                (static_cast<std::uint32_t>(bytes[2]) << 16) |
                (static_cast<std::uint32_t>(bytes[3]) << 24);
        return true;
    }

    bool skip(std::size_t count) {
        if (count > bytes_.size() - position_) {
            return false;
        }
        position_ += count;
        return true;
    }

    std::size_t position() const { return position_; }
    std::size_t remaining() const { return bytes_.size() - position_; }

private:
    bool read_bytes(void* destination, std::size_t count) {
        if (count > remaining()) {
            return false;
        }
        std::memcpy(destination, bytes_.data() + position_, count);
        position_ += count;
        return true;
    }

    const std::vector<std::uint8_t>& bytes_;
    std::size_t position_ = 0;
};

bool is_tag(const std::array<char, 4>& actual, const char (&expected)[5]) {
    return std::memcmp(actual.data(), expected, 4) == 0;
}

bool parse_wave_file(const std::vector<std::uint8_t>& bytes,
                     SoundFormat& format,
                     std::size_t& pcm_offset,
                     std::uint32_t& pcm_size) {
    WaveReader reader(bytes);
    std::array<char, 4> tag{};
    std::uint32_t ignored_chunk_size = 0;
    std::uint32_t fmt_chunk_size = 0;

    if (!reader.read_tag(tag) || !is_tag(tag, "RIFF") ||
        !reader.read_u32(ignored_chunk_size) ||
        !reader.read_tag(tag) || !is_tag(tag, "WAVE") ||
        !reader.read_tag(tag) || !is_tag(tag, "fmt ") ||
        !reader.read_u32(fmt_chunk_size) || fmt_chunk_size < 0x10) {
        return false;
    }
    if (!reader.read_u16(format.format_tag) ||
        !reader.read_u16(format.channel_count) ||
        !reader.read_u32(format.samples_per_second) ||
        !reader.read_u32(format.average_bytes_per_second) ||
        !reader.read_u16(format.block_align) ||
        !reader.read_u16(format.bits_per_sample) ||
        !reader.skip(fmt_chunk_size - 0x10) ||
        !reader.read_tag(tag) || !reader.read_u32(ignored_chunk_size)) {
        return false;
    }
    if (is_tag(tag, "fact")) {
        if (!reader.skip(ignored_chunk_size) ||
            !reader.read_tag(tag) || !reader.read_u32(ignored_chunk_size)) {
            return false;
        }
    }
    // `ignored_chunk_size` already holds this chunk's size, read generically
    // above (or after skipping "fact").  Re-reading a u32 here consumed the
    // first four bytes of the PCM data itself as a second "size", which is
    // why every real sound file -- none of which carry a "fact" chunk --
    // failed to parse: the misread value (0x80808080 for centered 8-bit
    // PCM) blew past reader.remaining() and the whole file was rejected.
    if (!is_tag(tag, "data")) {
        return false;
    }
    pcm_size = ignored_chunk_size;
    if (pcm_size > reader.remaining()) {
        return false;
    }
    pcm_offset = reader.position();
    return true;
}

std::string decode_sound_preference(const std::array<std::uint8_t, 13>& encoded) {
    // Exact bytes are persisted in Ghidra at 0045af20/30/40.  The original
    // loop emits source positions 10, 8, 6, 4, 2, 0 and then a NUL.
    constexpr std::array<std::size_t, 6> kDecodeOrder = {10, 8, 6, 4, 2, 0};
    std::string decoded;
    decoded.reserve(kDecodeOrder.size());
    for (const std::size_t index : kDecodeOrder) {
        decoded.push_back(static_cast<char>(encoded[index]));
    }
    return decoded;
}

bool equals_ignore_case(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const char a = left[index] >= 'A' && left[index] <= 'Z'
                           ? static_cast<char>(left[index] - 'A' + 'a')
                           : left[index];
        const char b = right[index] >= 'A' && right[index] <= 'Z'
                           ? static_cast<char>(right[index] - 'A' + 'a')
                           : right[index];
        if (a != b) {
            return false;
        }
    }
    return true;
}

int normalize_effect_volume(std::int32_t raw_value) {
    // The registry value is a signed attenuation in the original code.  The
    // compiler's CMOV sequence makes non-negative values become zero and
    // clamps the negative range to -5000.
    int value = raw_value < 1 ? raw_value : 0;
    return std::max(value, -5000);
}

constexpr std::array<std::uint8_t, 13> kEncodedBarney = {
    0x59, 0x6a, 0x45, 0x49, 0x4e, 0x78, 0x52, 0x34, 0x41, 0x73, 0x62, 0x61, 0x00};
constexpr std::array<std::uint8_t, 13> kEncodedBibble = {
    0x45, 0x6e, 0x4c, 0x68, 0x42, 0x52, 0x42, 0x64, 0x49, 0x53, 0x62, 0x65, 0x00};
constexpr std::array<std::uint8_t, 13> kEncodedNilnil = {
    0x4c, 0x73, 0x49, 0x62, 0x4e, 0x65, 0x4c, 0x64, 0x49, 0x43, 0x6e, 0x79, 0x00};

} // namespace

SoundManager::SoundManager(SoundSystemHost& host) : host_(host) {
    for (SoundChannel& channel : channels_) {
        channel = {};
    }

    const int create_status = host_.create_direct_sound(direct_sound_);
    if (create_status != kSoundSuccess) {
        host_.log(1, "Failed to create DS Object");
        return;
    }
    const int cooperative_status = host_.set_cooperative_level(
        direct_sound_, host_.main_window(), 2);
    if (cooperative_status != kSoundSuccess) {
        if (cooperative_status == kDirectSoundCooperativeLevelAlreadyAllocated) {
            host_.log(1, "Failed to set cooperative level\nResources already allocated");
        } else if (cooperative_status == kDirectSoundInvalidParameter) {
            host_.log(1, "Failed to set cooperative level\nInvalid parameter");
        }
        return;
    }

    const int primary_status = host_.create_primary_buffer(
        direct_sound_, 1, primary_sound_buffer_);
    if (primary_status != kSoundSuccess) {
        host_.log(1, "Primary buffer not created");
        return;
    }

    SoundFormat primary_format{};
    host_.read_primary_format(primary_sound_buffer_, primary_format);
    primary_format.samples_per_second = 0x5622;
    primary_format.channel_count = 2;
    primary_format.average_bytes_per_second = 0x15888;
    primary_format.block_align = 4;
    primary_format.bits_per_sample = 0x10;
    if (host_.set_primary_format(primary_sound_buffer_, primary_format) !=
        kSoundSuccess) {
        host_.log(1, "Primary buffer could not be set to new format");
        host_.read_primary_format(primary_sound_buffer_, primary_format);
        return;
    }

    active_channel_count_ = 0;
    playback_generation_ = 1;
    for (SoundChannel& channel : channels_) {
        channel.active_generation = 0;
    }
    backend_ready_flag_ = true;
    mixer_suspended_flag_ = false;
    effect_volume_setting_ = 0;
    target_master_attenuation_ = 0;
    master_attenuation_ = 0;
    volume_target_update_inhibit_flag_ = false;
}

SoundManager::~SoundManager() {
    if (backend_ready_flag_) {
        // The original destructor calls StopAllSounds explicitly and then
        // enters ClearSoundCache, whose own policy repeats that guard.
        stop_all_sounds();
        clear_sound_cache();
        host_.release_direct_sound(direct_sound_);
    }
}

void SoundManager::stop_channel(std::uint32_t channel_handle) {
    host_.log_channel_stop(channel_handle);

    SoundChannel& channel = channels_[channel_handle];
    if (active_channel_count_ == 0 || !backend_ready_flag_ ||
        channel.active_generation == 0) {
        return;
    }

    void* backend_voice = channel.backend_voice;
    channel.active_generation = 0;
    host_.stop_audio_buffer(backend_voice);
    if (channel.cached_sound != nullptr) {
        host_.release_audio_buffer(backend_voice);
        --channel.cached_sound->backend_reference_count;
    }
    --active_channel_count_;
}

void SoundManager::stop_all_sounds() {
    if (!backend_ready_flag_) {
        return;
    }

    scheduled_sound_events_.clear();
    if (active_channel_count_ == 0) {
        return;
    }

    for (std::uint32_t channel_index = 0;
         channel_index < kSoundChannelCount;
         ++channel_index) {
        SoundChannel& channel = channels_[channel_index];
        host_.log_channel_stop(channel_index);
        if (active_channel_count_ == 0 || !backend_ready_flag_ ||
            channel.active_generation == 0) {
            continue;
        }

        void* backend_voice = channel.backend_voice;
        CachedSound* cached_sound = channel.cached_sound;
        channel.active_generation = 0;
        host_.stop_audio_buffer(backend_voice);
        if (cached_sound != nullptr) {
            host_.release_audio_buffer(backend_voice);
            --cached_sound->backend_reference_count;
        }
        --active_channel_count_;
    }
}

void SoundManager::destroy_cached_sound(CachedSound* cached_sound) {
    if (cached_sound == nullptr) {
        return;
    }

    if (cached_sound->backend_reference_count > 0) {
        for (std::uint32_t channel_index = 0;
             channel_index < kSoundChannelCount;
             ++channel_index) {
            SoundChannel& channel = channels_[channel_index];
            if (channel.cached_sound != nullptr &&
                channel.cached_sound == cached_sound) {
                stop_channel(channel_index);
            }
        }
    }

    if (cached_sound->directsound_buffer != nullptr) {
        for (std::uint32_t channel_index = 0;
             channel_index < kSoundChannelCount;
             ++channel_index) {
            SoundChannel& channel = channels_[channel_index];
            if (channel.active_generation != 0 &&
                channel.backend_voice == cached_sound->directsound_buffer) {
                stop_channel(channel_index);
            }
        }
        host_.release_audio_buffer(cached_sound->directsound_buffer);
        cached_sound->directsound_buffer = nullptr;
    }
}

void SoundManager::clear_sound_cache() {
    if (backend_ready_flag_) {
        stop_all_sounds();
        if (sound_cache_capacity_bytes_ > 0) {
            for (const auto& entry : sound_cache_) {
                destroy_cached_sound(entry.get());
            }
            sound_cache_.clear();
        }
    }
    sound_cache_total_bytes_ = 0;
}

int SoundManager::ensure_sound_cache_capacity(int required_bytes) {
    if (sound_cache_capacity_bytes_ < required_bytes) {
        return kSoundCacheCapacityUnavailable;
    }

    while (sound_cache_capacity_bytes_ - sound_cache_total_bytes_ <
           required_bytes) {
        std::size_t eviction_index = sound_cache_.size();
        int oldest_generation = std::numeric_limits<int>::max();
        for (std::size_t index = 0; index < sound_cache_.size(); ++index) {
            CachedSound* candidate = sound_cache_[index].get();
            if (candidate->last_use_generation >= oldest_generation ||
                candidate->backend_reference_count != 0 ||
                candidate->directsound_buffer == nullptr ||
                host_.audio_buffer_is_playing(candidate->directsound_buffer)) {
                continue;
            }
            oldest_generation = candidate->last_use_generation;
            eviction_index = index;
        }

        if (eviction_index == sound_cache_.size()) {
            return kSoundCacheCapacityUnavailable;
        }

        CachedSound* candidate = sound_cache_[eviction_index].get();
        sound_cache_total_bytes_ -= candidate->byte_size;
        destroy_cached_sound(candidate);
        sound_cache_.erase(sound_cache_.begin() +
                           static_cast<std::ptrdiff_t>(eviction_index));
    }

    const int compact_result = host_.compact_direct_sound(direct_sound_);
    if (compact_result == kDirectSoundInvalidParameter) {
        host_.log(1, "DS Compact- Invalid Parameter");
    } else if (compact_result == kDirectSoundWrongPriority) {
        host_.log(1, "DS Compact- Wrong priority");
    }
    return kSoundCacheCapacityOk;
}

int SoundManager::stop_continuous_sound(std::uint32_t channel_handle,
                                        bool fade_out) {
    host_.log(1, "Stop Cnt Sound %d\n");
    if (mixer_suspended_flag_) {
        return 6;
    }
    if (channel_handle >= kSoundChannelCount) {
        return kSoundInvalidChannelHandle;
    }

    SoundChannel& channel = channels_[channel_handle];
    channel.continuous_active_flag = 0;
    if (fade_out) {
        channel.fade_attenuation_step =
            (-10000 - channel.attenuation) / 0xf;
    } else {
        stop_channel(channel_handle);
    }
    return kSoundOk;
}

bool SoundManager::continuous_channel_is_playing(
    SoundChannelHandle channel_handle) {
    if (channel_handle < 0 ||
        channel_handle >= static_cast<SoundChannelHandle>(kSoundChannelCount)) {
        return false;
    }

    SoundChannel& channel =
        channels_[static_cast<std::size_t>(channel_handle)];
    if (channel.active_generation == 0 || channel.backend_voice == nullptr) {
        return false;
    }

    std::uint32_t status = 0;
    (void)host_.get_audio_buffer_status(channel.backend_voice, status);
    return (status & 1U) != 0;
}

bool SoundManager::update_continuous_channel(
    SoundChannelHandle channel_handle, int attenuation, int pan) {
    if (mixer_suspended_flag_ || channel_handle < 0 ||
        channel_handle >= static_cast<SoundChannelHandle>(kSoundChannelCount)) {
        return false;
    }

    SoundChannel& channel =
        channels_[static_cast<std::size_t>(channel_handle)];
    channel.attenuation = attenuation;
    int effective_attenuation = master_attenuation_ + attenuation;
    if (effective_attenuation < -5000) {
        effective_attenuation = -5000;
    }
    if (channel.backend_voice == nullptr) {
        return false;
    }
    (void)host_.set_audio_buffer_volume(channel.backend_voice,
                                         effective_attenuation);
    (void)host_.set_audio_buffer_pan(channel.backend_voice, pan);
    return true;
}

int SoundManager::suspend_mixer() {
    if (mixer_suspended_flag_ || !backend_ready_flag_) {
        return 6;
    }

    host_.log(1, "Suspending mixer\n");
    stop_all_sounds();
    mixer_suspended_flag_ = true;
    return 0;
}

void SoundManager::restore_mixer() {
    if (!mixer_suspended_flag_ || !backend_ready_flag_) {
        return;
    }

    host_.log(1, "Restore mixer\n");
    mixer_suspended_flag_ = false;
}

void SoundManager::update() {
    if (!backend_ready_flag_) {
        return;
    }

    for (std::size_t index = scheduled_sound_events_.size(); index-- > 0;) {
        ScheduledSoundEvent& event = *scheduled_sound_events_[index];
        if (event.remaining_ticks < 1) {
            const SoundId sound_id = sound_descriptor_override_ != 0
                                         ? sound_descriptor_override_
                                         : event.sound_id;
            if (!mixer_suspended_flag_ && backend_ready_flag_) {
                CachedSound* cached_sound = find_or_load_cache_entry(sound_id);
                if (cached_sound != nullptr) {
                    (void)start_channel(cached_sound, event.attenuation,
                                         event.pan, false);
                }
            }
            scheduled_sound_events_.erase(
                scheduled_sound_events_.begin() +
                static_cast<std::ptrdiff_t>(index));
        } else {
            --event.remaining_ticks;
        }
    }

    if (master_attenuation_ != target_master_attenuation_) {
        if (master_attenuation_ < target_master_attenuation_) {
            master_attenuation_ += 200;
            if (master_attenuation_ > target_master_attenuation_) {
                master_attenuation_ = target_master_attenuation_;
            }
        } else {
            master_attenuation_ -= 200;
            if (master_attenuation_ < target_master_attenuation_) {
                master_attenuation_ = target_master_attenuation_;
            }
        }
    }

    for (std::uint32_t channel_index = 0;
         channel_index < kSoundChannelCount; ++channel_index) {
        SoundChannel& channel = channels_[channel_index];
        if (channel.active_generation == 0) {
            continue;
        }

        std::uint32_t status = 0;
        (void)host_.get_audio_buffer_status(channel.backend_voice, status);
        if ((status & 1U) == 0) {
            if (channel.continuous_active_flag == 0) {
                stop_channel(channel_index);
            } else {
                host_.log(1, "Sound %d locked\n");
            }
            continue;
        }

        if (channel.fade_attenuation_step != 0) {
            channel.attenuation += channel.fade_attenuation_step;
            if (channel.attenuation < -9999) {
                stop_channel(channel_index);
                continue;
            }
        }

        int combined_attenuation =
            channel.attenuation + master_attenuation_;
        if (combined_attenuation < -5000) {
            combined_attenuation = -5000;
        }
        (void)host_.set_audio_buffer_volume(channel.backend_voice,
                                             combined_attenuation);
    }
}

CachedSound* SoundManager::load_cached_sound(SoundId sound_id) {
    if (!backend_ready_flag_) {
        return nullptr;
    }

    std::vector<std::uint8_t> file_bytes;
    if (!host_.read_sound_file(sound_id, file_bytes)) {
        return nullptr;
    }

    SoundFormat format{};
    std::size_t pcm_offset = 0;
    std::uint32_t pcm_size = 0;
    if (!parse_wave_file(file_bytes, format, pcm_offset, pcm_size)) {
        return nullptr;
    }

    void* audio_buffer = nullptr;
    if (host_.create_pcm_buffer(direct_sound_, 0xe2, format, pcm_size,
                                audio_buffer) != kSoundSuccess) {
        return nullptr;
    }

    LockedAudioRegions regions{};
    int lock_status = host_.lock_audio_buffer(audio_buffer, pcm_size, regions);
    if (lock_status == kDirectSoundBufferLost &&
        host_.restore_audio_buffer(audio_buffer)) {
        lock_status = host_.lock_audio_buffer(audio_buffer, pcm_size, regions);
    }
    if (lock_status != kSoundSuccess) {
        host_.release_audio_buffer(audio_buffer);
        return nullptr;
    }

    const std::uint8_t* source = file_bytes.data() + pcm_offset;
    const std::uint32_t first_copy =
        std::min(regions.first_size, pcm_size);
    std::memcpy(regions.first, source, first_copy);
    if (regions.second != nullptr && regions.second_size != 0 &&
        first_copy < pcm_size) {
        std::memcpy(regions.second, source + first_copy,
                    std::min(regions.second_size, pcm_size - first_copy));
    }
    host_.unlock_audio_buffer(audio_buffer, regions);

    auto cached_sound = std::make_unique<CachedSound>();
    cached_sound->sound_descriptor = sound_id;
    cached_sound->last_use_generation = sound_cache_generation_++;
    cached_sound->byte_size = static_cast<int>(pcm_size);
    cached_sound->backend_reference_count = 0;
    cached_sound->directsound_buffer = audio_buffer;
    CachedSound* result = cached_sound.get();
    sound_cache_.push_back(std::move(cached_sound));
    return result;
}

CachedSound* SoundManager::find_or_load_cache_entry(SoundId sound_id) {
    for (const auto& entry : sound_cache_) {
        if (entry->sound_descriptor == sound_id) {
            entry->last_use_generation = sound_cache_generation_++;
            return entry.get();
        }
    }

    const std::uint32_t file_length = host_.sound_file_length(sound_id);
    if (ensure_sound_cache_capacity(static_cast<int>(file_length)) !=
        kSoundCacheCapacityOk) {
        return nullptr;
    }

    CachedSound* loaded_sound = load_cached_sound(sound_id);
    if (loaded_sound != nullptr) {
        sound_cache_total_bytes_ += loaded_sound->byte_size;
    }

    std::vector<const CachedSound*> inventory;
    inventory.reserve(sound_cache_.size());
    for (const auto& entry : sound_cache_) {
        inventory.push_back(entry.get());
    }
    host_.log_cache_inventory(inventory, sound_cache_total_bytes_);
    return loaded_sound;
}

SoundChannelHandle SoundManager::start_channel(CachedSound* cached_sound,
                                                int attenuation,
                                                int pan,
                                                bool loop) {
    if (cached_sound == nullptr || !backend_ready_flag_ ||
        active_channel_count_ >= static_cast<int>(kSoundChannelCount)) {
        return -1;
    }

    std::size_t channel_index = 0;
    while (channel_index < channels_.size() &&
           channels_[channel_index].active_generation != 0) {
        ++channel_index;
    }
    if (channel_index == channels_.size()) {
        return -1;
    }

    SoundChannel& channel = channels_[channel_index];
    void* source_buffer = cached_sound->directsound_buffer;

    // DirectSound permits the cached buffer to be shared only while it is
    // stopped.  A playing cached buffer is duplicated and the cache entry is
    // retained by the channel until that copy is released.
    std::uint32_t status = 0;
    (void)host_.get_audio_buffer_status(source_buffer, status);

    if ((status & 1U) == 0) {
        channel.backend_voice = source_buffer;
        channel.cached_sound = nullptr;
    } else {
        void* duplicate_buffer = nullptr;
        const int duplicate_status = host_.duplicate_audio_buffer(
            direct_sound_, source_buffer, duplicate_buffer);
        channel.backend_voice = duplicate_buffer;
        channel.cached_sound = cached_sound;
        ++cached_sound->backend_reference_count;
        if (duplicate_status != kSoundSuccess) {
            channel.continuous_active_flag = 0;
            channel.fade_attenuation_step = 0;
            return static_cast<SoundChannelHandle>(channel_index);
        }
    }

    channel.attenuation = attenuation;
    int effective_attenuation = master_attenuation_ + attenuation;
    if (effective_attenuation < -5000) {
        effective_attenuation = -5000;
    }
    host_.set_audio_buffer_volume(channel.backend_voice,
                                  effective_attenuation);
    host_.set_audio_buffer_pan(channel.backend_voice, pan);

    if (host_.play_audio_buffer(channel.backend_voice, loop) ==
        kSoundSuccess) {
        channel.active_generation = ++playback_generation_;
        ++active_channel_count_;
    }

    channel.continuous_active_flag = 0;
    channel.fade_attenuation_step = 0;
    return static_cast<SoundChannelHandle>(channel_index);
}

int SoundManager::play_or_queue(SoundId sound_id,
                                int queue_delay_ticks,
                                int attenuation,
                                int pan) {
    if (sound_descriptor_override_ != 0) {
        sound_id = sound_descriptor_override_;
    }
    if (mixer_suspended_flag_ || !backend_ready_flag_) {
        return kSoundMixerSuspended;
    }

    CachedSound* cached_sound = find_or_load_cache_entry(sound_id);
    if (queue_delay_ticks == 0) {
        if (cached_sound != nullptr &&
            start_channel(cached_sound, attenuation, pan, false) != -1) {
            return kSoundOk;
        }
        return kSoundNoAvailableChannel;
    }
    if (cached_sound == nullptr) {
        return kSoundNoAvailableChannel;
    }

    auto queued_sound = std::make_unique<ScheduledSoundEvent>();
    queued_sound->sound_id = sound_id;
    queued_sound->remaining_ticks = queue_delay_ticks;
    queued_sound->attenuation = attenuation;
    queued_sound->pan = pan;
    scheduled_sound_events_.push_back(std::move(queued_sound));
    return kSoundOk;
}

int SoundManager::start_continuous_sound(
    SoundId sound_id,
    SoundChannelHandle& out_channel_handle,
    int attenuation,
    int pan,
    bool loop) {
    if (sound_descriptor_override_ != 0) {
        sound_id = sound_descriptor_override_;
    }
    if (mixer_suspended_flag_ || !backend_ready_flag_) {
        return kSoundMixerSuspended;
    }

    CachedSound* cached_sound = find_or_load_cache_entry(sound_id);
    if (cached_sound == nullptr) {
        return kSoundNoAvailableChannel;
    }

    out_channel_handle =
        start_channel(cached_sound, attenuation, pan, loop);
    if (out_channel_handle == -1) {
        return kSoundNoAvailableChannel;
    }

    channels_[static_cast<std::size_t>(out_channel_handle)]
        .continuous_active_flag = 1;
    return kSoundOk;
}

std::unique_ptr<SoundManager> initialize_sound_system(SoundSystemHost& host) {
    // These arrays are the three exact 13-byte data objects used by the
    // original startup routine.  Their decoded spellings are mixed-case in
    // the image, but the original comparisons are _stricmp comparisons.
    const std::string barney = decode_sound_preference(kEncodedBarney);
    const std::string bibble = decode_sound_preference(kEncodedBibble);
    const std::string nilnil = decode_sound_preference(kEncodedNilnil);

    auto manager = std::make_unique<SoundManager>(host);
    manager->clear_sound_cache();
    manager->set_cache_capacity(kSoundCacheCapacityBytes);

    std::int32_t raw_effect_volume = 0;
    if (host.read_effect_volume(raw_effect_volume)) {
        if (raw_effect_volume != 0) {
            const int effect_volume = normalize_effect_volume(raw_effect_volume);
            manager->set_effect_volume_setting(effect_volume);
        }
    } else {
        host.write_default_effect_volume(0);
    }

    std::string sonic_preference;
    if (!host.read_sonic_preference(sonic_preference)) {
        sonic_preference = "None";
        host.write_default_sonic_preference(sonic_preference);
    }
    if (equals_ignore_case(sonic_preference, barney)) {
        manager->set_sound_descriptor_override(0x796e7262);
    }
    if (equals_ignore_case(sonic_preference, nilnil)) {
        manager->set_sound_descriptor_override(0x6c6e6c6e);
    }
    if (equals_ignore_case(sonic_preference, bibble)) {
        manager->set_sound_descriptor_override(0x62696c62);
    }
    return manager;
}

} // namespace creatures1::sound
