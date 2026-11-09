// A creature's voice: the original's Creature::SpeakTextWithVoice @0040b560
// (with Voice::PrepareSpeechText @004452b0 and Voice::GetNextSoundPair
// @004453c0) against creatures::Voice, reached through Creature::voice().
//
//   voice_equivalence.exe <Creatures.exe> <install-dir> [cases]
//
// The original's creature is a byte block whose Voice (at +0x29c0) is built
// by the original's own constructor (00445040); its CString work runs in
// the real MFC of the install (mfc140.dll, only read).  LC's side is the
// loop of WindowsCreatureSpeechPhraseHost::speak_text_with_voice (platform
// code, which this test cannot link), copied here, over LC's real Voice.
// The original's sound queue (0042ce20) and bubble (00426580) are
// recorders.
//
// Each case: random voice tables (81 trigram masks, 32 sound ids, 32
// durations) on both sides, then 4 random phrases (letters, capitals,
// spaces, punctuation and digits, 0 to 30 characters).  For each, the
// sounds queued (id, delay), the delay returned and the bubble's lifetime
// must match.  The same results prove the offsets the load test's table
// uses for the voice: masks +0x29c4, sound ids +0x2b08, durations +0x2b88
// and the accumulated delay +0x2c10.
#include "native_image.hpp"

#include "creatures/creature.hpp"
#include "creatures/voice.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace c1 = creatures1;
using c1::creatures::Creature;

namespace {

constexpr std::uint32_t kSpeakTextWithVoice = 0x0040b560, kVoiceConstruct = 0x00445040;
constexpr std::uint32_t kQueueSound = 0x0042ce20, kCreateBubble = 0x00426580, kApp = 0x00468d80;
constexpr std::size_t kAppBurble = 0xcc;
// The fields under test.
constexpr std::size_t kVoice = 0x29c0, kMasks = 0x29c4, kSoundIds = 0x2b08, kDurations = 0x2b88,
                      kDelay = 0x2c10;
constexpr std::size_t kBlockSize = 0x2d00;

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint32_t below(std::uint32_t n) { return next() % n; }
};

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

std::vector<std::string> g_native, g_lc;

void __attribute__((thiscall)) native_queue_sound(void*, std::uint32_t id, std::int32_t delay, int) {
    g_native.push_back("sound " + std::to_string(id) + " +" + std::to_string(delay));
}
void* __attribute__((thiscall)) native_create_bubble(void*, const char*, int lifetime, int) {
    g_native.push_back("bubble " + std::to_string(static_cast<std::uint8_t>(lifetime)));
    return nullptr;
}

std::string random_phrase(Random& random) {
    static const char kChars[] = "abcdefghijklmnopqrstuvwxyz      ABCZ!?.,'09";
    std::string p;
    const std::uint32_t n = random.below(31);
    for (std::uint32_t i = 0; i < n; ++i) p += kChars[random.below(sizeof kChars - 1)];
    return p;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: voice_equivalence <Creatures.exe> <install-dir> [cases]\n");
        return 2;
    }
    const long cases = argc > 3 ? std::strtol(argv[3], nullptr, 10) : 2000;
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    std::vector<std::string> missing;
    image.bind_real_imports(argv[2], missing);  // what is missing stays a trap
    image.redirect(kQueueSound, reinterpret_cast<const void*>(&native_queue_sound));
    image.redirect(kCreateBubble, reinterpret_cast<const void*>(&native_create_bubble));
    std::vector<std::uint8_t> app(0x200, 0);
    field<std::uint32_t>(app.data(), kAppBurble) = 1;
    *image.at<void**>(kApp) = app.data();

    using Construct = void(__attribute__((thiscall)) *)(void*);
    using Speak = void(__attribute__((thiscall)) *)(void*, const char*, std::int32_t*);
    Random random{0x6a09e667f3bcc909ull};
    long differing = 0, phrases = 0, sounds = 0;
    for (long index = 0; index < cases; ++index) {
        std::vector<std::uint8_t> native(kBlockSize, 0);
        reinterpret_cast<Construct>(image.at<void*>(kVoiceConstruct))(native.data() + kVoice);
        auto lc = std::make_unique<Creature>();
        auto& voice = lc->voice();
        for (std::size_t i = 0; i < c1::creatures::kVoiceTrigramMaskCount; ++i) {
            // Sparse masks, so some trigrams select nothing.
            const std::uint32_t m = random.next() & random.next() & random.next();
            voice.trigram_selection_masks()[i] = m;
            field<std::uint32_t>(native.data(), kMasks + 4 * i) = m;
        }
        for (std::size_t i = 0; i < c1::creatures::kVoiceSelectionCount; ++i) {
            const std::uint32_t id = random.below(4) == 0 ? 0 : random.next();
            const std::int32_t d = static_cast<std::int32_t>(random.below(12));
            voice.sound_ids()[i] = id;
            voice.durations()[i] = d;
            field<std::uint32_t>(native.data(), kSoundIds + 4 * i) = id;
            field<std::int32_t>(native.data(), kDurations + 4 * i) = d;
        }
        std::string mismatch;
        for (int p = 0; p < 4 && mismatch.empty(); ++p, ++phrases) {
            const std::string phrase = random_phrase(random);
            g_native.clear();
            g_lc.clear();
            std::int32_t native_delay = -1;
            reinterpret_cast<Speak>(image.at<void*>(kSpeakTextWithVoice))(native.data(), phrase.c_str(),
                                                                        &native_delay);
            // WindowsCreatureSpeechPhraseHost::speak_text_with_voice, burble on.
            if (voice.prepare_speech_text(phrase)) {
                c1::sound::SoundId id{};
                c1::creatures::VoiceDelayTicks delay{};
                while (voice.get_next_sound_pair(id, delay)) {
                    g_lc.push_back("sound " + std::to_string(id) + " +" + std::to_string(delay));
                }
            }
            const std::int32_t lc_delay = voice.accumulated_sound_delay_ticks();
            g_lc.push_back("bubble " + std::to_string(static_cast<std::uint8_t>(lc_delay + 5)));
            for (const auto& s : g_native) sounds += s[0] == 's';
            if (g_native != g_lc || native_delay != lc_delay ||
                field<std::int32_t>(native.data(), kDelay) != lc_delay) {
                char text[512];
                std::snprintf(text, sizeof text, "phrase [%s]: native delay %d (+0x2c10 %d), %zu calls%s%s; lc delay %d, "
                              "%zu calls%s%s",
                              phrase.c_str(), native_delay, field<std::int32_t>(native.data(), kDelay),
                              g_native.size(), g_native.empty() ? "" : ", first ",
                              g_native.empty() ? "" : g_native[0].c_str(), lc_delay, g_lc.size(),
                              g_lc.empty() ? "" : ", first ", g_lc.empty() ? "" : g_lc[0].c_str());
                mismatch = text;
            }
        }
        if (!mismatch.empty() && ++differing <= 10) std::printf("case %ld: %s\n", index, mismatch.c_str());
    }
    std::printf("covered: %ld phrases, %ld sounds\n", phrases, sounds);
    std::printf("%ld cases: %ld differ -> %s\n", cases, differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
