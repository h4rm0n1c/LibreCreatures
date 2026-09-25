#include "windows_sound_host.hpp"

#include "windows_prelude.hpp"
#include <dsound.h>

#include <array>
#include <cstring>
#include <utility>

namespace creatures1::platform {
namespace {

constexpr char kRegistryPath[] =
    "Software\\Gameware Development\\Creatures 1\\1.0";

IDirectSound* as_device(void* value) {
    return static_cast<IDirectSound*>(value);
}

IDirectSoundBuffer* as_buffer(void* value) {
    return static_cast<IDirectSoundBuffer*>(value);
}

WAVEFORMATEX to_wave_format(const sound::SoundFormat& format) {
    WAVEFORMATEX result{};
    result.wFormatTag = format.format_tag;
    result.nChannels = format.channel_count;
    result.nSamplesPerSec = format.samples_per_second;
    result.nAvgBytesPerSec = format.average_bytes_per_second;
    result.nBlockAlign = format.block_align;
    result.wBitsPerSample = format.bits_per_sample;
    return result;
}

void from_wave_format(const WAVEFORMATEX& native,
                      sound::SoundFormat& format) {
    format.format_tag = native.wFormatTag;
    format.channel_count = native.nChannels;
    format.samples_per_second = native.nSamplesPerSec;
    format.average_bytes_per_second = native.nAvgBytesPerSec;
    format.block_align = native.nBlockAlign;
    format.bits_per_sample = native.wBitsPerSample;
}

} // namespace

WindowsSoundSystemHost::WindowsSoundSystemHost(
    HWND owner_window, std::string sound_directory)
    : owner_window_(owner_window), sound_directory_(std::move(sound_directory)) {
    if (!sound_directory_.empty() && sound_directory_.back() != '\\') {
        sound_directory_.push_back('\\');
    }
    char path[MAX_PATH] = {};
    if (GetEnvironmentVariableA("C1_SOUND_LOG", path, MAX_PATH) != 0) {
        trace_path_ = path;
    }
}

void WindowsSoundSystemHost::trace(std::string_view line) {
    if (trace_path_.empty()) {
        return;
    }
    char stamp[24];
    wsprintfA(stamp, "%10lu ", static_cast<unsigned long>(GetTickCount()));
    const std::string text = stamp + std::string(line) + "\n";
    const HANDLE file = CreateFileA(trace_path_.c_str(), FILE_APPEND_DATA,
                                    FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written,
              nullptr);
    CloseHandle(file);
}

int WindowsSoundSystemHost::create_direct_sound(void*& device) {
    IDirectSound* native_device = nullptr;
    const HRESULT result = DirectSoundCreate(nullptr, &native_device, nullptr);
    device = native_device;
    return static_cast<int>(result);
}

void* WindowsSoundSystemHost::main_window() const {
    return owner_window_;
}

int WindowsSoundSystemHost::set_cooperative_level(
    void* device, void* owner_window, std::uint32_t priority) {
    return static_cast<int>(as_device(device)->SetCooperativeLevel(
        static_cast<HWND>(owner_window),
        priority == 2 ? DSSCL_PRIORITY : DSSCL_NORMAL));
}

int WindowsSoundSystemHost::create_primary_buffer(
    void* device, std::uint32_t flags, void*& buffer) {
    DSBUFFERDESC description{};
    description.dwSize = sizeof(description);
    description.dwFlags = flags;
    IDirectSoundBuffer* native_buffer = nullptr;
    const HRESULT result = as_device(device)->CreateSoundBuffer(
        &description, &native_buffer, nullptr);
    buffer = native_buffer;
    return static_cast<int>(result);
}

int WindowsSoundSystemHost::read_primary_format(
    void* buffer, sound::SoundFormat& format) {
    WAVEFORMATEX native{};
    DWORD byte_count = sizeof(native);
    const HRESULT result = as_buffer(buffer)->GetFormat(
        &native, sizeof(native), &byte_count);
    if (SUCCEEDED(result)) {
        from_wave_format(native, format);
    }
    return static_cast<int>(result);
}

int WindowsSoundSystemHost::set_primary_format(
    void* buffer, const sound::SoundFormat& format) {
    const WAVEFORMATEX native = to_wave_format(format);
    return static_cast<int>(as_buffer(buffer)->SetFormat(&native));
}

std::string WindowsSoundSystemHost::sound_path(sound::SoundId id) const {
    std::array<char, 5> fourcc{};
    std::memcpy(fourcc.data(), &id, 4);
    return sound_directory_ + fourcc.data() + ".wav";
}

bool WindowsSoundSystemHost::read_sound_file(
    sound::SoundId id, std::vector<std::uint8_t>& bytes) {
    bytes.clear();
    const std::string path = sound_path(id);
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    const bool sized = GetFileSizeEx(file, &size) != FALSE &&
                       size.QuadPart >= 0 && size.QuadPart <= 0x7fffffff;
    if (!sized) {
        CloseHandle(file);
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size.QuadPart));
    DWORD total_read = 0;
    const bool read = bytes.empty() ||
                      (ReadFile(file, bytes.data(),
                                static_cast<DWORD>(bytes.size()), &total_read,
                                nullptr) != FALSE &&
                       total_read == bytes.size());
    CloseHandle(file);
    if (!read) {
        bytes.clear();
    }
    return read;
}

std::uint32_t WindowsSoundSystemHost::sound_file_length(
    sound::SoundId id) const {
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExA(sound_path(id).c_str(), GetFileExInfoStandard,
                              &attributes)) {
        return 0;
    }
    ULARGE_INTEGER size{};
    size.HighPart = attributes.nFileSizeHigh;
    size.LowPart = attributes.nFileSizeLow;
    return size.QuadPart > 0xffffffffu
               ? 0
               : static_cast<std::uint32_t>(size.QuadPart);
}

int WindowsSoundSystemHost::create_pcm_buffer(
    void* device, std::uint32_t flags, const sound::SoundFormat& format,
    std::uint32_t byte_size, void*& buffer) {
    const WAVEFORMATEX native_format = to_wave_format(format);
    DSBUFFERDESC description{};
    description.dwSize = sizeof(description);
    description.dwFlags = flags;
    description.dwBufferBytes = byte_size;
    description.lpwfxFormat = const_cast<WAVEFORMATEX*>(&native_format);
    IDirectSoundBuffer* native_buffer = nullptr;
    const HRESULT result = as_device(device)->CreateSoundBuffer(
        &description, &native_buffer, nullptr);
    buffer = native_buffer;
    return static_cast<int>(result);
}

int WindowsSoundSystemHost::lock_audio_buffer(
    void* buffer, std::uint32_t byte_size, sound::LockedAudioRegions& regions) {
    void* first = nullptr;
    void* second = nullptr;
    DWORD first_size = 0;
    DWORD second_size = 0;
    const HRESULT result = as_buffer(buffer)->Lock(
        0, byte_size, &first, &first_size, &second, &second_size,
        DSBLOCK_ENTIREBUFFER);
    regions.first = static_cast<std::uint8_t*>(first);
    regions.first_size = first_size;
    regions.second = static_cast<std::uint8_t*>(second);
    regions.second_size = second_size;
    return static_cast<int>(result);
}

bool WindowsSoundSystemHost::restore_audio_buffer(void* buffer) {
    return as_buffer(buffer)->Restore() == DS_OK;
}

void WindowsSoundSystemHost::unlock_audio_buffer(
    void* buffer, const sound::LockedAudioRegions& regions) {
    as_buffer(buffer)->Unlock(regions.first, regions.first_size,
                              regions.second, regions.second_size);
}

int WindowsSoundSystemHost::duplicate_audio_buffer(
    void* device, void* source_buffer, void*& duplicate_buffer) {
    IDirectSoundBuffer* native_duplicate = nullptr;
    const HRESULT result = as_device(device)->DuplicateSoundBuffer(
        as_buffer(source_buffer), &native_duplicate);
    duplicate_buffer = native_duplicate;
    return static_cast<int>(result);
}

int WindowsSoundSystemHost::get_audio_buffer_status(
    void* buffer, std::uint32_t& status) {
    DWORD native_status = 0;
    const HRESULT result = as_buffer(buffer)->GetStatus(&native_status);
    status = native_status;
    return static_cast<int>(result);
}

int WindowsSoundSystemHost::set_audio_buffer_volume(void* buffer,
                                                     int attenuation) {
    return static_cast<int>(as_buffer(buffer)->SetVolume(attenuation));
}

int WindowsSoundSystemHost::set_audio_buffer_pan(void* buffer, int pan) {
    return static_cast<int>(as_buffer(buffer)->SetPan(pan));
}

int WindowsSoundSystemHost::play_audio_buffer(void* buffer, bool loop) {
    return static_cast<int>(as_buffer(buffer)->Play(
        0, 0, loop ? DSBPLAY_LOOPING : 0));
}

bool WindowsSoundSystemHost::audio_buffer_is_playing(void* buffer) const {
    DWORD status = 0;
    return SUCCEEDED(as_buffer(buffer)->GetStatus(&status)) &&
           (status & DSBSTATUS_PLAYING) != 0;
}

void WindowsSoundSystemHost::stop_audio_buffer(void* buffer) {
    if (buffer != nullptr) {
        as_buffer(buffer)->Stop();
    }
}

void WindowsSoundSystemHost::release_audio_buffer(void* buffer) {
    if (buffer != nullptr) {
        as_buffer(buffer)->Release();
    }
}

void WindowsSoundSystemHost::release_direct_sound(void* device) {
    if (device != nullptr) {
        as_device(device)->Release();
    }
}

int WindowsSoundSystemHost::compact_direct_sound(void* device) {
    return static_cast<int>(as_device(device)->Compact());
}

bool WindowsSoundSystemHost::open_settings_key(unsigned access,
                                                void*& key) const {
    HKEY native_key = nullptr;
    const LSTATUS result = RegCreateKeyExA(
        HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0, access, nullptr,
        &native_key, nullptr);
    key = native_key;
    return result == ERROR_SUCCESS;
}

bool WindowsSoundSystemHost::read_effect_volume(std::int32_t& value) {
    void* opaque_key = nullptr;
    if (!open_settings_key(KEY_READ, opaque_key)) {
        return false;
    }
    HKEY key = static_cast<HKEY>(opaque_key);
    DWORD type = 0;
    DWORD byte_count = sizeof(value);
    const bool present = RegQueryValueExA(
        key, "EffectVolume", nullptr, &type,
        reinterpret_cast<LPBYTE>(&value), &byte_count) == ERROR_SUCCESS &&
        type == REG_DWORD && byte_count == sizeof(value);
    RegCloseKey(key);
    return present;
}

void WindowsSoundSystemHost::write_default_effect_volume(std::int32_t value) {
    void* opaque_key = nullptr;
    if (!open_settings_key(KEY_SET_VALUE, opaque_key)) {
        return;
    }
    HKEY key = static_cast<HKEY>(opaque_key);
    RegSetValueExA(key, "EffectVolume", 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
}

bool WindowsSoundSystemHost::read_sonic_preference(std::string& value) {
    void* opaque_key = nullptr;
    if (!open_settings_key(KEY_READ, opaque_key)) {
        return false;
    }
    HKEY key = static_cast<HKEY>(opaque_key);
    DWORD type = 0;
    DWORD byte_count = 0;
    bool present = RegQueryValueExA(key, "Sonic", nullptr, &type, nullptr,
                                    &byte_count) == ERROR_SUCCESS &&
                   (type == REG_SZ || type == REG_EXPAND_SZ);
    std::vector<char> buffer(byte_count + 1, '\0');
    if (present) {
        present = RegQueryValueExA(
                      key, "Sonic", nullptr, &type,
                      reinterpret_cast<LPBYTE>(buffer.data()), &byte_count) ==
                  ERROR_SUCCESS;
    }
    if (present) {
        value.assign(buffer.data());
    }
    RegCloseKey(key);
    return present;
}

void WindowsSoundSystemHost::write_default_sonic_preference(
    std::string_view value) {
    void* opaque_key = nullptr;
    if (!open_settings_key(KEY_SET_VALUE, opaque_key)) {
        return;
    }
    HKEY key = static_cast<HKEY>(opaque_key);
    const std::string text(value);
    RegSetValueExA(key, "Sonic", 0, REG_SZ,
                   reinterpret_cast<const BYTE*>(text.c_str()),
                   static_cast<DWORD>(text.size() + 1));
    RegCloseKey(key);
}

void WindowsSoundSystemHost::log(std::uint32_t /*category*/,
                                  std::string_view message) {
    const std::string text(message);
    OutputDebugStringA(text.c_str());
}

void WindowsSoundSystemHost::log_channel_stop(std::uint32_t channel_index) {
    char buffer[64]{};
    wsprintfA(buffer, "Sound channel %lu stopped\n",
              static_cast<unsigned long>(channel_index));
    OutputDebugStringA(buffer);
}

void WindowsSoundSystemHost::log_cache_inventory(
    const std::vector<const sound::CachedSound*>& entries,
    int total_bytes) {
    char buffer[160]{};
    wsprintfA(buffer, "Sound Cache: %2d elements, %7d bytes\n",
              static_cast<int>(entries.size()), total_bytes);
    OutputDebugStringA(buffer);

    for (std::size_t index = 0; index < entries.size(); ++index) {
        const sound::CachedSound* entry = entries[index];
        if (entry == nullptr) {
            continue;
        }
        // The native line names the entry by its four-character sound
        // descriptor with the .wav extension appended.
        const std::uint32_t descriptor =
            static_cast<std::uint32_t>(entry->sound_descriptor);
        char name[9]{};
        name[0] = static_cast<char>(descriptor & 0xffu);
        name[1] = static_cast<char>((descriptor >> 8) & 0xffu);
        name[2] = static_cast<char>((descriptor >> 16) & 0xffu);
        name[3] = static_cast<char>((descriptor >> 24) & 0xffu);
        name[4] = '.';
        name[5] = 'w';
        name[6] = 'a';
        name[7] = 'v';

        wsprintfA(buffer,
                  " Element #%2d, %8s, %7d bytes, used %6d, copies %d\n",
                  static_cast<int>(index), name, entry->byte_size,
                  entry->last_use_generation, entry->backend_reference_count);
        OutputDebugStringA(buffer);
    }
}

} // namespace creatures1::platform
