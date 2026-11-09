// The Object base and SimpleObject's click selectors: the original's
// Object::Object @00425750, Object::UpdateSound @00426190,
// Object::~Object @00425a70 and SimpleObject's click event (vftable slot
// 28, @00427fc0) against LC's Object and SimpleObject.
//
//   object_equivalence.exe <Creatures.exe> [cases]
//
// Each case runs one of four operations on an original object (a byte
// block) and an LC object holding the same state:
//
//   constructor  the original's constructor on a block filled with 0xaa,
//                against a new LC Object: every field LC's Object keeps
//                must be where the constructor writes its first value.
//   update sound the continuous sound's per-tick update, with a sound id,
//                a persist flag and a channel handle.  Its two paths are
//                both run: no channel (start the sound again: the id and the
//                persist flag go to the mixer, the new handle comes back)
//                and an idle channel (stop it, by its handle).  The mixer's
//                calls and the three fields after the update are compared.
//   destructor   with a channel handle and a gallery: the channel stopped
//                and the gallery released must be the same.
//   click event  SimpleObject's click event for each interaction state:
//                the selector byte the original reads at +0x58 + state.
//
// The same results in every case prove the offsets the load test's table
// uses for these fields (src/c1/creatures/load_dump_objects.inc).  The
// original's helpers for the world (the object registry, the viewport, the
// mixer) are replaced by recording stubs here.
#include "native_image.hpp"

#include "objects/object.hpp"
#include "objects/object_field_dump.hpp"
#include "objects/simple_object.hpp"
#include "sound/sound.hpp"
#include "world/viewport.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace c1 = creatures1;
using c1::objects::Object;
using c1::objects::ObjectFieldDump;

namespace {

constexpr std::uint32_t kConstruct = 0x00425750, kUpdateSound = 0x00426190, kDestruct = 0x00425a70,
                        kSimpleClickEvent = 0x00427fc0;
constexpr std::uint32_t kAudibility = 0x00425f80, kAttenuationPan = 0x00425e40, kStartContinuous = 0x00441850,
                        kStopContinuous = 0x004419d0, kRegistryRemove = 0x00425b60, kDestructHelper = 0x0042d0c0;
constexpr std::uint32_t kRegistrySetAtGrow = 0x00450c44;  // the IAT slot the constructor calls
constexpr std::uint32_t kDebugConsole = 0x00467d28, kDocument = 0x00468d7c, kMixer = 0x00468d94;
constexpr std::size_t kDocumentMuted = 0x180, kMixerSuspended = 0x330, kChannels = 0x10, kChannelStride = 24,
                      kChannelActive = 0x14;

// The fields under test (the offsets this test proves).
constexpr std::size_t kClassifier = 0x4, kBoundsMode = 0x8, kBoundsFlags = 0x9, kMovementBounds = 0xc,
                      kBoundsReference = 0x1c, kCaosObjectPointer = 0x20, kInteractionEvent = 0x24,
                      kSoundHandle = 0x28, kSoundId = 0x2c, kSoundPersist = 0x30, kObjectVariables = 0x34,
                      kGallery = 0x40, kTimerPeriod = 0x44, kTimerCountdown = 0x48, kTickEnabled = 0x4c,
                      kClickSelectors = 0x58;
constexpr std::size_t kBlockSize = 0x200;

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

// ---- the original's side: recording stubs ----

struct MixerCall {
    char kind;  // 's' start, 'x' stop
    std::uint32_t a, b;
    bool operator==(const MixerCall& o) const { return kind == o.kind && a == o.a && b == o.b; }
};
std::vector<MixerCall> g_native_calls, g_lc_calls;
int g_native_new_handle = 0;
std::vector<const void*> g_native_released_galleries;

int __attribute__((thiscall)) native_audible(void*) { return 1; }
void __attribute__((thiscall)) native_attenuation_pan(void*, int* attenuation, int* pan) {
    *attenuation = 0;
    *pan = 0;
}
int __attribute__((stdcall)) native_start(std::uint32_t id, int* handle, int, int, int persist) {
    g_native_calls.push_back({'s', id, static_cast<std::uint32_t>(persist != 0)});
    *handle = g_native_new_handle;
    return 0;
}
int __attribute__((stdcall)) native_stop(std::uint32_t handle, int) {
    g_native_calls.push_back({'x', handle, 0});
    return 0;
}
void __attribute__((thiscall)) native_set_at_grow(void*, int, void*) {}
void __attribute__((thiscall)) native_registry_remove(void*) {}
void __attribute__((thiscall)) native_destruct_helper(void*, void*) {}
void __attribute__((thiscall)) native_gallery_delete(void* gallery, int) {
    g_native_released_galleries.push_back(gallery);
}

// ---- LC's side ----

// A sound system with no device: every call succeeds; a sound "file" is a
// minimal WAV; the only calls recorded are a play (the id it loaded and its
// loop flag) and a channel stop.
class FakeSoundSystem final : public c1::sound::SoundSystemHost {
public:
    c1::sound::SoundId last_loaded = 0;
    int create_direct_sound(void*& device) override { device = this; return 0; }
    void* main_window() const override { return nullptr; }
    int set_cooperative_level(void*, void*, std::uint32_t) override { return 0; }
    int create_primary_buffer(void*, std::uint32_t, void*& buffer) override { buffer = this; return 0; }
    int read_primary_format(void*, c1::sound::SoundFormat&) override { return 0; }
    int set_primary_format(void*, const c1::sound::SoundFormat&) override { return 0; }
    bool read_sound_file(c1::sound::SoundId id, std::vector<std::uint8_t>& bytes) override {
        static const std::uint8_t wav[] = {'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
                                           16, 0, 0, 0, 1, 0, 1, 0, 0x22, 0x56, 0, 0, 0x22, 0x56, 0, 0, 1, 0, 8, 0,
                                           'd', 'a', 't', 'a', 4, 0, 0, 0, 0x80, 0x80, 0x80, 0x80};
        bytes.assign(wav, wav + sizeof wav);
        last_loaded = id;
        return true;
    }
    std::uint32_t sound_file_length(c1::sound::SoundId) const override { return 48; }
    int create_pcm_buffer(void*, std::uint32_t, const c1::sound::SoundFormat&, std::uint32_t,
                          void*& buffer) override {
        buffers.push_back(std::make_unique<std::uint8_t[]>(16));
        buffer = buffers.back().get();
        return 0;
    }
    int lock_audio_buffer(void* buffer, std::uint32_t size, c1::sound::LockedAudioRegions& regions) override {
        regions.first = static_cast<std::uint8_t*>(buffer);
        regions.first_size = size;
        return 0;
    }
    bool restore_audio_buffer(void*) override { return true; }
    void unlock_audio_buffer(void*, const c1::sound::LockedAudioRegions&) override {}
    int duplicate_audio_buffer(void*, void* source, void*& duplicate) override { duplicate = source; return 0; }
    int get_audio_buffer_status(void*, std::uint32_t& status) override { status = 0; return 0; }
    int set_audio_buffer_volume(void*, int) override { return 0; }
    int set_audio_buffer_pan(void*, int) override { return 0; }
    int play_audio_buffer(void* buffer, bool loop) override {
        for (const auto& [b, id] : loaded) {
            if (b == buffer && recording) g_lc_calls.push_back({'s', id, static_cast<std::uint32_t>(loop)});
        }
        return 0;
    }
    bool audio_buffer_is_playing(void*) const override { return false; }
    void stop_audio_buffer(void*) override {}
    void release_audio_buffer(void*) override {}
    void release_direct_sound(void*) override {}
    int compact_direct_sound(void*) override { return 0; }
    bool read_effect_volume(std::int32_t&) override { return false; }
    void write_default_effect_volume(std::int32_t) override {}
    bool read_sonic_preference(std::string&) override { return false; }
    void write_default_sonic_preference(std::string_view) override {}
    void log(std::uint32_t, std::string_view) override {}
    void log_channel_stop(std::uint32_t channel) override {
        if (recording) g_lc_calls.push_back({'x', channel, 0});
    }
    void log_cache_inventory(const std::vector<const c1::sound::CachedSound*>& entries, int) override {
        // Pair each new buffer with the sound loaded into it, so a play
        // can report its sound id.
        for (const auto* entry : entries) {
            bool known = false;
            for (const auto& [b, id] : loaded) known = known || b == entry->directsound_buffer;
            if (!known) loaded.push_back({entry->directsound_buffer, entry->sound_descriptor});
        }
    }

    bool recording = false;
    std::vector<std::pair<void*, c1::sound::SoundId>> loaded;
    std::vector<std::unique_ptr<std::uint8_t[]>> buffers;
};

class FakePlayback final : public c1::objects::ObjectSoundPlaybackHost {
public:
    explicit FakePlayback(c1::sound::SoundManager& manager) : manager_(manager) {}
    bool muted = false;
    c1::world::ViewportBounds sound_viewport() const override { return {0, 0, 0x2000, 0x1000}; }
    bool sounds_muted() const override { return muted; }
    c1::sound::SoundManager& sound_manager() override { return manager_; }
    bool debug_console_available() const override { return false; }
    void log_sound_event(LogEvent, c1::sound::SoundId, int) override {}

private:
    c1::sound::SoundManager& manager_;
};

// An object inside the sound viewport (the original's audibility test is a
// stub that says so).
class AudibleObject final : public Object {
public:
    using Object::Object;
    bool get_bounds(c1::world::WorldRect* out) const override {
        *out = {100, 100, 120, 120};
        return true;
    }
};

class RecordingCleanup final : public c1::objects::ObjectCleanupHost {
public:
    std::vector<int> stopped;
    std::vector<const void*> released;
    void remove_from_renderable_set(Object&) override {}
    void stop_continuous_sound(int handle) override { stopped.push_back(handle); }
    void release_gallery(c1::display::Gallery& gallery) override { released.push_back(&gallery); }
    void unregister_from_object_registry(Object&) override {}
};

std::string calls_text(const std::vector<MixerCall>& calls) {
    std::string out;
    for (const MixerCall& c : calls) {
        char t[48];
        std::snprintf(t, sizeof t, " %c(%08x,%u)", c.kind, static_cast<unsigned>(c.a), static_cast<unsigned>(c.b));
        out += t;
    }
    return out.empty() ? " none" : out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: object_equivalence <Creatures.exe> [cases]\n");
        return 2;
    }
    const long cases = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 100000;
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    image.redirect(kAudibility, reinterpret_cast<const void*>(&native_audible));
    image.redirect(kAttenuationPan, reinterpret_cast<const void*>(&native_attenuation_pan));
    image.redirect(kStartContinuous, reinterpret_cast<const void*>(&native_start));
    image.redirect(kStopContinuous, reinterpret_cast<const void*>(&native_stop));
    image.redirect(kRegistryRemove, reinterpret_cast<const void*>(&native_registry_remove));
    image.redirect(kDestructHelper, reinterpret_cast<const void*>(&native_destruct_helper));
    *image.at<void**>(kRegistrySetAtGrow) = reinterpret_cast<void*>(&native_set_at_grow);
    *image.at<std::uint32_t*>(kDebugConsole) = 0;
    std::vector<std::uint8_t> document(0x400, 0), mixer(0x400, 0);
    *image.at<void**>(kDocument) = document.data();
    *image.at<void**>(kMixer) = mixer.data();
    void* gallery_vtable[4] = {nullptr, reinterpret_cast<void*>(&native_gallery_delete), nullptr, nullptr};

    using Call0 = void(__attribute__((thiscall)) *)(void*);
    using ClickEvent = int(__attribute__((thiscall)) *)(void*, int, int);
    const char* names[] = {"constructor", "update sound", "destructor", "click event"};
    long counts[4] = {}, differing_by[4] = {};
    Random random{0x2545f4914f6cdd1dull};
    long differing = 0;
    for (long index = 0; index < cases; ++index) {
        const int which = static_cast<int>(random.below(4));
        ++counts[which];
        std::vector<std::uint8_t> native(kBlockSize, 0xaa);
        std::string mismatch;
        char text[512];
        switch (which) {
        case 0: {
            Object lc;
            reinterpret_cast<Call0>(image.at<void*>(kConstruct))(native.data());
            const auto& b = field<c1::world::WorldRect>(native.data(), kMovementBounds);
            const auto& m = lc.movement_bounds();
            const bool same =
                field<std::uint32_t>(native.data(), kClassifier) == lc.classifier_base() &&
                native[kBoundsMode] == static_cast<std::uint8_t>(lc.bounds_mode()) &&
                native[kBoundsFlags] == lc.script_bounds_flags() && b.min_x == m.min_x && b.min_y == m.min_y &&
                b.max_x == m.max_x && b.max_y == m.max_y &&
                (field<void*>(native.data(), kBoundsReference) == nullptr) == (lc.bounds_reference_object() == nullptr) &&
                (field<void*>(native.data(), kCaosObjectPointer) == nullptr) == (lc.caos_object_pointer() == nullptr) &&
                native[kInteractionEvent] == lc.current_interaction_event_id() &&
                field<std::int32_t>(native.data(), kSoundHandle) == lc.continuous_sound_handle() &&
                field<std::uint32_t>(native.data(), kSoundId) == lc.continuous_sound_descriptor() &&
                (field<std::uint32_t>(native.data(), kSoundPersist) != 0) == lc.continuous_sound_persists() &&
                field<std::uint32_t>(native.data(), kObjectVariables) == lc.object_variable(0) &&
                field<std::uint32_t>(native.data(), kObjectVariables + 4) == lc.object_variable(1) &&
                field<std::uint32_t>(native.data(), kObjectVariables + 8) == lc.object_variable(2) &&
                (field<void*>(native.data(), kGallery) == nullptr) == (lc.gallery() == nullptr) &&
                field<std::int32_t>(native.data(), kTimerPeriod) == ObjectFieldDump::timer_period(lc) &&
                field<std::int32_t>(native.data(), kTimerCountdown) == lc.timer_countdown_for_script() &&
                (native[kTickEnabled] != 0) == lc.tick_enabled() && native[kTickEnabled] <= 1;
            if (!same) {
                std::snprintf(text, sizeof text,
                              "native tick %02x handle %d id %08x persist %08x gallery %p; lc tick %d handle %d id "
                              "%08x persist %d gallery %p",
                              native[kTickEnabled], field<std::int32_t>(native.data(), kSoundHandle),
                              field<std::uint32_t>(native.data(), kSoundId),
                              field<std::uint32_t>(native.data(), kSoundPersist),
                              field<void*>(native.data(), kGallery), lc.tick_enabled(), lc.continuous_sound_handle(),
                              static_cast<unsigned>(lc.continuous_sound_descriptor()), lc.continuous_sound_persists(),
                              static_cast<void*>(lc.gallery()));
                mismatch = text;
            }
            break;
        }
        case 1: {
            // No channel (a negative handle) or an idle channel 0..31.
            const bool restart = random.below(2) == 0;
            const std::int32_t handle =
                restart ? -1 - static_cast<std::int32_t>(random.below(1000)) : static_cast<std::int32_t>(random.below(32));
            const std::uint32_t id = random.below(8) == 0 ? 0 : (random.next() | 1u);
            const bool persist = random.below(2) == 0;
            const bool muted = random.below(6) == 0;
            // LC's mixer: the restart's new channel is the first free one,
            // after `busy` sounds already playing.
            FakeSoundSystem system;
            c1::sound::SoundManager manager(system);
            manager.set_cache_capacity(1 << 24);
            const int busy = restart ? static_cast<int>(random.below(5)) : 0;
            for (int k = 0; k < busy; ++k) {
                c1::sound::SoundChannelHandle ignored = -1;
                manager.start_continuous_sound(0x1000 + k, ignored, 0, 0, false);
            }
            FakePlayback playback(manager);
            playback.muted = muted;
            AudibleObject lc;
            ObjectFieldDump::set_continuous_sound_state(lc, handle, id, persist);
            g_lc_calls.clear();
            system.recording = true;
            lc.update_sound(playback);
            system.recording = false;

            field<std::int32_t>(native.data(), kSoundHandle) = handle;
            field<std::uint32_t>(native.data(), kSoundId) = id;
            field<std::uint32_t>(native.data(), kSoundPersist) = persist ? 1 : 0;
            field<std::uint32_t>(document.data(), kDocumentMuted) = muted ? 1 : 0;
            field<std::uint32_t>(mixer.data(), kMixerSuspended) = 0;
            for (int c = 0; c < 32; ++c) {
                field<std::uint32_t>(mixer.data(), kChannels + c * kChannelStride) = 0;
                field<std::uint32_t>(mixer.data(), kChannelActive + c * kChannelStride) = 0;
            }
            g_native_new_handle = busy;
            g_native_calls.clear();
            reinterpret_cast<Call0>(image.at<void*>(kUpdateSound))(native.data());
            const bool same = g_native_calls == g_lc_calls &&
                              field<std::int32_t>(native.data(), kSoundHandle) == lc.continuous_sound_handle() &&
                              field<std::uint32_t>(native.data(), kSoundId) == lc.continuous_sound_descriptor() &&
                              (field<std::uint32_t>(native.data(), kSoundPersist) != 0) ==
                                  lc.continuous_sound_persists();
            if (!same) {
                std::snprintf(text, sizeof text,
                              "handle %d id %08x persist %d muted %d: native ->%d %08x %u calls%s; lc ->%d %08x %d "
                              "calls%s",
                              handle, static_cast<unsigned>(id), persist, muted,
                              field<std::int32_t>(native.data(), kSoundHandle),
                              field<std::uint32_t>(native.data(), kSoundId),
                              field<std::uint32_t>(native.data(), kSoundPersist), calls_text(g_native_calls).c_str(),
                              lc.continuous_sound_handle(), static_cast<unsigned>(lc.continuous_sound_descriptor()),
                              lc.continuous_sound_persists(), calls_text(g_lc_calls).c_str());
                mismatch = text;
            }
            break;
        }
        case 2: {
            const std::int32_t handle = random.below(3) == 0 ? -1 : static_cast<std::int32_t>(random.below(32));
            const bool has_gallery = random.below(3) != 0;
            // LC's gallery is only an address here: the cleanup host gets
            // it and nothing reads it.
            static std::uint8_t lc_gallery_token[1];
            std::vector<std::uint8_t> native_gallery(0x40, 0);
            field<void*>(native_gallery.data(), 0) = gallery_vtable;
            field<std::int32_t>(native_gallery.data(), 0x14) = 1;  // the last reference
            // A zeroed block: the destructor follows the gallery pointer.
            std::fill(native.begin(), native.end(), 0);
            RecordingCleanup cleanup;
            {
                auto lc = std::make_unique<Object>(nullptr, &cleanup);
                ObjectFieldDump::set_continuous_sound_state(*lc, handle, 0x1234, true);
                if (has_gallery) lc->set_gallery(reinterpret_cast<c1::display::Gallery*>(lc_gallery_token));
            }
            field<std::int32_t>(native.data(), kSoundHandle) = handle;
            field<std::uint32_t>(native.data(), kSoundId) = 0x1234;
            field<std::uint32_t>(native.data(), kSoundPersist) = 1;
            field<void*>(native.data(), kGallery) = has_gallery ? native_gallery.data() : nullptr;
            g_native_calls.clear();
            g_native_released_galleries.clear();
            reinterpret_cast<Call0>(image.at<void*>(kDestruct))(native.data());
            std::vector<int> native_stopped;
            for (const MixerCall& c : g_native_calls) {
                if (c.kind == 'x') native_stopped.push_back(static_cast<int>(c.a));
            }
            const bool native_released = g_native_released_galleries.size() == 1 &&
                                         g_native_released_galleries[0] == native_gallery.data();
            const bool lc_released = cleanup.released.size() == 1 && cleanup.released[0] == lc_gallery_token;
            if (native_stopped != cleanup.stopped || native_released != lc_released ||
                (!native_released && !g_native_released_galleries.empty()) ||
                (!lc_released && !cleanup.released.empty())) {
                std::snprintf(text, sizeof text,
                              "handle %d gallery %d: native stopped %u%s released %u; lc stopped %u%s released %u",
                              handle, has_gallery, static_cast<unsigned>(native_stopped.size()),
                              native_stopped.empty() ? "" : (" " + std::to_string(native_stopped[0])).c_str(),
                              static_cast<unsigned>(g_native_released_galleries.size()),
                              static_cast<unsigned>(cleanup.stopped.size()),
                              cleanup.stopped.empty() ? "" : (" " + std::to_string(cleanup.stopped[0])).c_str(),
                              static_cast<unsigned>(cleanup.released.size()));
                mismatch = text;
            }
            break;
        }
        default: {
            // The original reads past the three selectors for a state of 3
            // or more; LC answers event 0 there.  Only states 0..2 are run.
            c1::objects::SimpleObject lc;
            const std::uint32_t state = random.below(3);
            lc.set_current_interaction_event_id_for_script(state);
            native[kInteractionEvent] = static_cast<std::uint8_t>(state);
            for (std::size_t k = 0; k < 3; ++k) {
                const auto selector = static_cast<std::int8_t>(static_cast<int>(random.below(8)) - 1);
                lc.click_event_for_interaction_state[k] = selector;
                native[kClickSelectors + k] = static_cast<std::uint8_t>(selector);
            }
            const int x = static_cast<int>(random.below(4000)), y = static_cast<int>(random.below(1200));
            const int native_event =
                reinterpret_cast<ClickEvent>(image.at<void*>(kSimpleClickEvent))(native.data(), x, y);
            const int lc_event = static_cast<int>(lc.click_event_id_at_world_position(x, y));
            if (native_event != lc_event) {
                std::snprintf(text, sizeof text, "state %u selectors %d %d %d: native %d lc %d",
                              static_cast<unsigned>(state), lc.click_event_for_interaction_state[0],
                              lc.click_event_for_interaction_state[1], lc.click_event_for_interaction_state[2],
                              native_event, lc_event);
                mismatch = text;
            }
            break;
        }
        }
        if (!mismatch.empty()) {
            ++differing_by[which];
            if (++differing <= 10) std::printf("case %ld %s: %s\n", index, names[which], mismatch.c_str());
        }
    }
    for (int k = 0; k < 4; ++k) {
        std::printf("%s: %ld cases, %ld differ\n", names[k], counts[k], differing_by[k]);
    }
    std::printf("%ld cases: %ld differ -> %s\n", cases, differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
