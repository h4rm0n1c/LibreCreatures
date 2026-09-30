#pragma once

// Kit sound: WAV files from the game's Sounds folder, played through
// DirectSound, several at once, looped or once, with the 1996 kits' volumes
// (hundredths of a decibel, 0 full, -10000 silent) and pans (-10000 left ..
// 10000 right).  Mute and the skin's volume apply to every KitSound in the
// kit at once (Options > Mute sounds).

#include <afxwin.h>

#include <map>
#include <string>
#include <vector>

struct IDirectSound;
struct IDirectSoundBuffer;

namespace c1kitshell {

class KitSound {
public:
    KitSound() = default;
    ~KitSound();
    KitSound(const KitSound&) = delete;
    KitSound& operator=(const KitSound&) = delete;

    // False (and every play does nothing) without a sound device.
    bool open(HWND window);
    // Reads `path` (PCM WAV) as `name`.
    bool load(const std::string& name, const std::string& path);
    // A channel number, or -1.
    int play(const std::string& name, bool loop, int volume = 0, int pan = 0);
    void stop(int channel);
    void stop_all();

    // Options > Mute sounds: while set, play does nothing, and setting it
    // stops whatever every KitSound is playing.
    static void set_muted(bool muted);
    static bool muted();
    // Added to every play's volume: 0 for the 1996 look, kLibreVolumeOffset
    // for the kits' own, which plays the same sounds more quietly.
    static void set_volume_offset(int hundredths_of_db);
    static constexpr int kLibreVolumeOffset = -600;  // about half as loud

private:
    struct Clip {
        std::vector<unsigned char> format;  // WAVEFORMATEX and any extra
        std::vector<unsigned char> data;
    };
    void release_finished();

    IDirectSound* device_ = nullptr;
    std::map<std::string, Clip> clips_;
    std::vector<IDirectSoundBuffer*> channels_;  // null: free
};

} // namespace c1kitshell
