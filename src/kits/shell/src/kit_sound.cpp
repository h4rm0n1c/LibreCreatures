// Classic-look sound through DirectSound.  See kit_sound.hpp.

#include "c1kitshell/kit_sound.hpp"

#include <mmreg.h>
#include <dsound.h>

#include <cstdio>
#include <cstring>

namespace c1kitshell {
namespace {

bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    out.clear();
    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "rb") != 0 || file == nullptr) {
        return false;
    }
    unsigned char buffer[8192];
    size_t got;
    while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        out.insert(out.end(), buffer, buffer + got);
    }
    fclose(file);
    return true;
}

unsigned read32(const std::vector<unsigned char>& b, size_t at) {
    return b[at] | (b[at + 1] << 8) | (b[at + 2] << 16) | (unsigned(b[at + 3]) << 24);
}

} // namespace

KitSound::~KitSound() {
    stop_all();
    if (device_ != nullptr) {
        device_->Release();
    }
}

bool KitSound::open(HWND window) {
    if (device_ != nullptr) {
        return true;
    }
    if (FAILED(DirectSoundCreate(nullptr, &device_, nullptr)) || device_ == nullptr) {
        device_ = nullptr;
        return false;
    }
    device_->SetCooperativeLevel(window, DSSCL_NORMAL);
    return true;
}

// RIFF WAVE: the "fmt " chunk as the format, the "data" chunk as the sound.
bool KitSound::load(const std::string& name, const std::string& path) {
    std::vector<unsigned char> bytes;
    if (!read_file(path, bytes) || bytes.size() < 12 || std::memcmp(&bytes[0], "RIFF", 4) != 0 ||
        std::memcmp(&bytes[8], "WAVE", 4) != 0) {
        return false;
    }
    Clip clip;
    for (size_t at = 12; at + 8 <= bytes.size();) {
        const unsigned size = read32(bytes, at + 4);
        if (at + 8 + size > bytes.size()) break;
        if (std::memcmp(&bytes[at], "fmt ", 4) == 0) {
            clip.format.assign(bytes.begin() + at + 8, bytes.begin() + at + 8 + size);
        } else if (std::memcmp(&bytes[at], "data", 4) == 0) {
            clip.data.assign(bytes.begin() + at + 8, bytes.begin() + at + 8 + size);
        }
        at += 8 + size + (size & 1);
    }
    if (clip.format.size() < 16 || clip.data.empty()) {
        return false;
    }
    if (clip.format.size() < sizeof(WAVEFORMATEX)) {
        clip.format.resize(sizeof(WAVEFORMATEX), 0);  // cbSize 0
    }
    clips_[name] = std::move(clip);
    return true;
}

void KitSound::release_finished() {
    for (IDirectSoundBuffer*& buffer : channels_) {
        DWORD status = 0;
        if (buffer != nullptr && SUCCEEDED(buffer->GetStatus(&status)) &&
            (status & DSBSTATUS_PLAYING) == 0) {
            buffer->Release();
            buffer = nullptr;
        }
    }
}

int KitSound::play(const std::string& name, bool loop, int volume, int pan) {
    const auto found = clips_.find(name);
    if (device_ == nullptr || found == clips_.end()) {
        return -1;
    }
    release_finished();
    const Clip& clip = found->second;
    DSBUFFERDESC description = {};
    description.dwSize = sizeof(description);
    description.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_GLOBALFOCUS;
    description.dwBufferBytes = static_cast<DWORD>(clip.data.size());
    description.lpwfxFormat =
        reinterpret_cast<WAVEFORMATEX*>(const_cast<unsigned char*>(clip.format.data()));
    IDirectSoundBuffer* buffer = nullptr;
    if (FAILED(device_->CreateSoundBuffer(&description, &buffer, nullptr)) || buffer == nullptr) {
        return -1;
    }
    void* first = nullptr;
    void* second = nullptr;
    DWORD first_bytes = 0, second_bytes = 0;
    if (SUCCEEDED(buffer->Lock(0, description.dwBufferBytes, &first, &first_bytes, &second,
                               &second_bytes, 0))) {
        std::memcpy(first, clip.data.data(), first_bytes);
        if (second != nullptr) std::memcpy(second, clip.data.data() + first_bytes, second_bytes);
        buffer->Unlock(first, first_bytes, second, second_bytes);
    }
    buffer->SetVolume(volume);
    buffer->SetPan(pan);
    buffer->Play(0, 0, loop ? DSBPLAY_LOOPING : 0);
    for (size_t i = 0; i < channels_.size(); ++i) {
        if (channels_[i] == nullptr) {
            channels_[i] = buffer;
            return static_cast<int>(i);
        }
    }
    channels_.push_back(buffer);
    return static_cast<int>(channels_.size() - 1);
}

void KitSound::stop(int channel) {
    if (channel < 0 || channel >= static_cast<int>(channels_.size()) || channels_[channel] == nullptr) {
        return;
    }
    channels_[channel]->Stop();
    channels_[channel]->Release();
    channels_[channel] = nullptr;
}

void KitSound::stop_all() {
    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
        stop(i);
    }
}

} // namespace c1kitshell
