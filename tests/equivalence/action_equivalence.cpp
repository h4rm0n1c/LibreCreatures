// Function-level equivalence: Creature::UpdateActionSelection @0040c630 and
// the Creature::SetAction @0040b680 it calls (CE 10.3), against
// creatures::Creature::update_action_selection.
//
// Each case gives an LC creature (brain grown from a stock genome) and a
// native Creature-shaped block the same state at the offsets the two
// functions read (from their disassembly): dream countdown, the eight
// involuntary actions, the active involuntary action, the selected action
// and its boost, sleep and its indicator object, the motion link, the
// bounds reference and mode, and the decision lobe (6).  Scripts "exist" for
// a random set of classifiers; native ExecuteScriptForClassifier is
// redirected to a recorder that answers from that set, LC's host answers
// from the same set.  Both draw rand from one formula with equal seeds.
//
// Up to six updates run per case.  After each, the state above, every
// script lookup (classifier, in order), the sleep indicator's two calls,
// the immediate events SetAction queues and whether a phrase was spoken
// must match.  A spoken phrase ends the case: native's phrase functions are
// replaced by recorders (they reach the CRT and the speech queue), so the
// two rand streams part there.
//
// Not generated: a creature with no quiescent script (native SetAction then
// retries action 0 for ever), negative selected actions, and decision lobes
// other than the genome's (16 neurons).

#include "native_image.hpp"

#include "biochemistry/biochemistry.hpp"
#include "brain/brain.hpp"
#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "creatures/genome.hpp"
#include "objects/object.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace creatures1::creatures {
// Declared a friend in creature.hpp: the action-selection state.
class CreatureTestAccess {
public:
    static std::uint32_t& selected_action(Creature& c) { return c.selected_action_id_; }
    static std::uint8_t& boost(Creature& c) { return c.action_activation_boost_; }
    static std::uint8_t& active_involuntary(Creature& c) { return c.active_involuntary_action_index_; }
    static objects::Object*& sleep_indicator(Creature& c) { return c.sleep_indicator_object_; }
};
}  // namespace creatures1::creatures

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;
using creatures::CreatureTestAccess;

constexpr std::uint32_t kNativeUpdateActionSelection = 0x0040c630;
constexpr std::uint32_t kNativeExecuteScript = 0x00419e00;
constexpr std::uint32_t kNativeSpeakDrivePhrase = 0x0040b160;
constexpr std::uint32_t kNativeSpeakActionPhrase = 0x0040afa0;
constexpr std::uint32_t kNativeDebugLog = 0x00418000;
constexpr std::uint32_t kNativeActionTargetTable = 0x00454450;
// The immediate object event ring SetAction writes.
constexpr std::uint32_t kRingWrite = 0x00466470, kRingRead = 0x0046646c;
constexpr std::uint32_t kRingStart = 0x0046cf20;
constexpr std::size_t kRingEntry = 0x18;

// Native Creature layout used by the two functions.
constexpr std::size_t kCreatureSize = 0x2d00;
constexpr std::size_t kClassifier = 0x4, kBoundsMode = 0x8, kBoundsReference = 0x1c;
constexpr std::size_t kSleepIndicator = 0x11f, kMotionLink = 0x7f0;
constexpr std::size_t kSelectedAction = 0x840, kBoost = 0x841, kActiveInvoluntary = 0x842;
constexpr std::size_t kSleepIndicatorObject = 0x2c14, kBrain = 0x2c18;
constexpr std::size_t kInvoluntary = 0x2c35;  // 8 x (probability, cooldown)
constexpr std::size_t kDreamCountdown = 0x2cbc;
constexpr std::size_t kBrainSize = 0x800, kLobe6Neurons = 0x554, kLobe6Count = 0x55c;
constexpr std::size_t kNeuronSize = 16, kFiring = 2, kActivation = 3;
// A native object: classifier at +4; "live" for SetAction's event when its
// +0x134 (timer period) is not -1.
constexpr std::size_t kObjectSize = 0x200, kObjectTimer = 0x134;

using NativeUpdate = void(__attribute__((thiscall)) *)(void* creature);

// ---- rand: the MSVC formula, one state per side ----
std::uint32_t g_native_rand_state = 1;
extern "C" int __cdecl native_rand() {
    g_native_rand_state = g_native_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_native_rand_state >> 16) & 0x7fff);
}
std::uint32_t g_lc_rand_state = 1;
extern "C" int __wrap_rand() {
    g_lc_rand_state = g_lc_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_lc_rand_state >> 16) & 0x7fff);
}
extern "C" void __wrap_srand(unsigned seed) { g_lc_rand_state = seed; }

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next()); }
    std::uint32_t below(std::uint32_t n) { return n == 0 ? 0 : next() % n; }
    bool chance(std::uint32_t one_in) { return below(one_in) == 0; }
};

// ---- what each side did in one update ----
struct Trace {
    std::vector<std::uint32_t> scripts;      // classifiers looked up, in order
    std::vector<std::string> indicator;      // sleep indicator calls
    std::vector<std::string> events;         // queued immediate events
    bool spoke = false;
};
Trace g_native_trace, g_lc_trace;
std::set<std::uint32_t> g_scripts;           // classifiers that have a script
std::map<const void*, int> g_native_names;   // native object -> source number

// Native ExecuteScriptForClassifier: ECX owner, EDX source, then classifier
// and flag on the stack; the caller pops them.
extern "C" int __cdecl native_execute_script(void*, void*, std::uint32_t classifier, int) {
    g_native_trace.scripts.push_back(classifier);
    return g_scripts.count(classifier) ? 1 : 0;
}
__asm__(".globl _native_execute_script_thunk\n"
        "_native_execute_script_thunk:\n"
        "    pushl 8(%esp)\n"
        "    pushl 8(%esp)\n"
        "    pushl %edx\n"
        "    pushl %ecx\n"
        "    call _native_execute_script\n"
        "    addl $16, %esp\n"
        "    ret\n");
extern "C" void native_execute_script_thunk();

void __attribute__((thiscall)) native_speak(void*) { g_native_trace.spoke = true; }
void __attribute__((thiscall)) native_indicator_event(void* self, std::uint32_t event,
                                                      void* source, std::uint32_t argument) {
    char text[64];
    std::snprintf(text, sizeof text, "event %u source %s arg %u", event,
                  source == self ? "self" : "other", argument);
    g_native_trace.indicator.push_back(text);
}
void __attribute__((thiscall)) native_indicator_init(void*) {
    g_native_trace.indicator.push_back("initialize");
}
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called on a native object\n");
    std::exit(5);
}
void* g_vtable[64];

// ---- LC hosts ----
class AttentionHost final : public creatures::CreatureAttentionHost {
public:
    std::map<const objects::Object*, std::uint32_t> classifiers;
    std::map<const objects::Object*, bool> live;
    std::map<const objects::Object*, int> names;
    creatures::Creature* creature = nullptr;

    objects::Object& object_for_creature(creatures::Creature& c) const override {
        return c.skeleton();
    }
    void queue_immediate_event(objects::Object& source, objects::Object& target,
                               objects::ObjectEventId event_id, std::uint32_t argument) override {
        char text[64];
        std::snprintf(text, sizeof text, "%s -> %d event %u arg %u",
                      &source == &creature->skeleton() ? "creature" : "other", names.at(&target),
                      static_cast<unsigned>(event_id), argument);
        g_lc_trace.events.push_back(text);
    }
    std::uint32_t classifier_base(const creatures::Creature& c) const override {
        return c.skeleton().classifier_base();
    }
    int execute_script_for_classifier(creatures::Creature&, objects::Object*,
                                      std::uint32_t classifier, bool) override {
        g_lc_trace.scripts.push_back(classifier);
        return g_scripts.count(classifier) ? 1 : 0;
    }
    objects::Object* create_sleep_indicator(creatures::Creature&, int) override {
        std::fprintf(stderr, "create_sleep_indicator was not expected\n");
        std::exit(5);
    }
    bool action_event_target_is_live(const objects::Object& target) const override {
        return live.at(&target);
    }
    std::uint32_t classifier_base(const objects::Object& object) const override {
        return classifiers.at(&object);
    }
    creatures::ActionTargetRequirement action_target_requirement(std::size_t index) const override {
        return creatures::action_target_requirement_for(index);
    }
    creatures::AttentionClassifier classify_object(const objects::Object&) const override {
        std::fprintf(stderr, "classify_object was not expected\n");
        std::exit(5);
    }
    void dispatch_sleep_indicator_event(objects::Object& indicator, objects::ObjectEventId event_id,
                                        objects::Object* source, std::uint32_t argument) override {
        char text[64];
        std::snprintf(text, sizeof text, "event %u source %s arg %u",
                      static_cast<unsigned>(event_id), source == &indicator ? "self" : "other",
                      argument);
        g_lc_trace.indicator.push_back(text);
    }
    void initialize_sleep_indicator(objects::Object&) override {
        g_lc_trace.indicator.push_back("initialize");
    }
    bool debug_console_visible() const override { return false; }
    bool is_selected_creature(const creatures::Creature&) const override { return false; }
    void log_attention_shift(const creatures::Creature&, const objects::Object*) override {}
    void log_override_action_script_selected(const creatures::Creature&, std::uint32_t) override {}
    void log_no_action_script(const creatures::Creature&, std::uint32_t) override {}
    void log_involuntary_action(const creatures::Creature&, int) override {}
    void log_action_selection(const creatures::Creature&, const objects::Object*,
                              std::uint32_t) override {}
};

class TextApi final : public creatures::MultibyteTextApi {
public:
    const char* find_character(const char* text, unsigned char c) const override {
        return std::strchr(text, c);
    }
    const char* find_substring(const char* text, const char* fragment) const override {
        return std::strstr(text, fragment);
    }
    const char* next_character(const char* text) const override { return text + 1; }
    int compare_strings(const char* l, const char* r) const override { return _stricmp(l, r); }
    void copy_string(char* d, std::size_t n, const char* s) const override {
        std::snprintf(d, n, "%s", s);
    }
    void append_string(char* d, std::size_t n, const char* s) const override {
        const std::size_t used = std::strlen(d);
        if (used < n) std::snprintf(d + used, n - used, "%s", s);
    }
};

// Any phrase means the creature spoke; the case ends after this update.
class PhraseHost final : public creatures::Creature::CreatureSpeechPhraseHost {
public:
    objects::Object& object_for_creature(creatures::Creature& c) const override {
        return c.skeleton();
    }
    std::int32_t attention_record_index(const objects::Object&) const override { return 0; }
    void speak_text_with_voice(creatures::Creature&, std::string_view,
                               std::int32_t& delay) override {
        g_lc_trace.spoke = true;
        delay = 0;
    }
    void queue_speech_event(objects::Object&, std::uint32_t, std::int32_t) override {
        g_lc_trace.spoke = true;
    }
};
class SpeechHost final : public creatures::Creature::CreatureSpeechHost {
public:
    void speak(creatures::Creature&, std::string_view) override { g_lc_trace.spoke = true; }
};
class NoRenderables final : public objects::ObjectRenderableSetHost {
public:
    bool contains(const objects::Object&) const override { return false; }
    void insert(objects::Object&) override {}
    void erase(objects::Object&) override {}
};

class OneGenomeFile final : public genome::GenomeFileStore {
public:
    explicit OneGenomeFile(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    std::string secondary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    std::string primary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    bool regular_file_exists(const std::string&) const override { return true; }
    std::vector<std::uint8_t> read_file(const std::string&) override { return bytes_; }
    void write_secondary_genetics_file(genome::GenomeFilenameId,
                                       const std::vector<std::uint8_t>&) override {}

private:
    std::vector<std::uint8_t> bytes_;
};

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}

template <typename T>
T& field(std::vector<std::uint8_t>& block, std::size_t offset) {
    return *reinterpret_cast<T*>(block.data() + offset);
}

std::string join(const std::vector<std::uint32_t>& v) {
    std::string s;
    char text[16];
    for (auto c : v) {
        std::snprintf(text, sizeof text, "%08x ", c);
        s += text;
    }
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n",
                     argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0xac7104u;
    std::vector<std::vector<std::uint8_t>> genomes;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-cases" && i + 1 < argc) {
            cases = std::atol(argv[++i]);
        } else if (arg == "-seed" && i + 1 < argc) {
            seed = std::strtoull(argv[++i], nullptr, 0);
        } else {
            genomes.push_back(read_file(argv[i]));
            if (genomes.back().empty()) {
                std::fprintf(stderr, "cannot read %s\n", argv[i]);
                return 2;
            }
        }
    }

    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    if (!image.bind_import("!rand", reinterpret_cast<void*>(&native_rand))) {
        std::fprintf(stderr, "the exe imports no rand\n");
        return 2;
    }
    image.make_return(kNativeDebugLog);
    image.redirect(kNativeExecuteScript, reinterpret_cast<const void*>(&native_execute_script_thunk));
    image.redirect(kNativeSpeakDrivePhrase, reinterpret_cast<const void*>(&native_speak));
    image.redirect(kNativeSpeakActionPhrase, reinterpret_cast<const void*>(&native_speak));
    const auto native_update = image.at<NativeUpdate>(kNativeUpdateActionSelection);
    for (auto& slot : g_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_vtable[34] = reinterpret_cast<void*>(&native_indicator_event);
    g_vtable[16] = reinterpret_cast<void*>(&native_indicator_init);

    // LC's action-target table must be the original's.
    for (std::size_t a = 0; a < 16; ++a) {
        const auto native_requirement = *image.at<const std::uint8_t*>(kNativeActionTargetTable + a);
        if (native_requirement != static_cast<std::uint8_t>(creatures::action_target_requirement_for(a))) {
            std::printf("action target table differs at action %u: native %u\n",
                        static_cast<unsigned>(a), native_requirement);
            return 1;
        }
    }

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    TextApi text_api;
    PhraseHost phrase_host;
    SpeechHost speech_host;
    NoRenderables renderables;
    Random random{seed};
    long differing = 0, updates = 0, spoken = 0;
    long with_events = 0, with_indicator = 0, with_involuntary = 0, action_changes = 0;
    for (long index = 0; index < cases; ++index) {
        // ---- LC creature ----
        OneGenomeFile file(genomes[random.below(static_cast<std::uint32_t>(genomes.size()))]);
        genome::Genome g(1, random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female,
                         genome::GenomeLifeStage::stage_zero, &file);
        auto creature = std::make_unique<creatures::Creature>();
        auto* brain = new brain::Brain();
        brain->load_genome(g);
        creature->set_brain(brain);
        creature->set_biochemistry(new biochemistry::Biochemistry());
        const std::uint32_t lobe6 = brain->lobe(6).neuron_count_value();
        if (lobe6 != 16) {
            std::fprintf(stderr, "a decision lobe of %u neurons\n", lobe6);
            return 2;
        }

        // ---- native creature ----
        std::vector<std::uint8_t> native(kCreatureSize, 0);
        std::vector<std::uint8_t> native_brain(kBrainSize, 0);
        std::vector<std::uint8_t> neurons6(kNeuronSize * lobe6, 0);
        field<void*>(native, 0) = g_vtable;
        field<void*>(native, kBrain) = native_brain.data();
        field<void*>(native_brain, kLobe6Neurons) = neurons6.data();
        field<std::int32_t>(native_brain, kLobe6Count) = static_cast<std::int32_t>(lobe6);

        // ---- other objects: 0-2 motion link candidates, 3 sleep indicator ----
        AttentionHost host;
        host.creature = creature.get();
        std::vector<std::unique_ptr<objects::Object>> lc_objects;
        std::vector<std::vector<std::uint8_t>> native_objects;
        std::vector<objects::Object*> lc_of(4);
        std::vector<void*> native_of(4);
        g_native_names.clear();
        for (int o = 0; o < 4; ++o) {
            lc_objects.push_back(std::make_unique<objects::Object>());
            native_objects.emplace_back(kObjectSize, 0);
            const std::uint32_t family = 2 + random.below(3);
            const std::uint32_t classifier = (family << 24) | (random.below(40) << 16) |
                                             (random.below(256) << 8);
            const bool live = !random.chance(4);
            lc_of[o] = lc_objects.back().get();
            native_of[o] = native_objects.back().data();
            field<void*>(native_objects.back(), 0) = g_vtable;
            field<std::uint32_t>(native_objects.back(), 4) = classifier;
            field<std::int32_t>(native_objects.back(), kObjectTimer) = live ? 25 : -1;
            host.classifiers[lc_of[o]] = classifier;
            host.live[lc_of[o]] = live;
            host.names[lc_of[o]] = o;
            g_native_names[native_of[o]] = o;
        }

        // ---- shared state ----
        const std::uint32_t own_classifier =
            (4u << 24) | ((1 + random.below(2)) << 16) | (random.below(256) << 8);
        creature->skeleton().set_classifier_base(own_classifier);
        field<std::uint32_t>(native, kClassifier) = own_classifier;
        const std::uint32_t bounds_mode = random.below(5);
        creature->skeleton().set_bounds_mode(bounds_mode, renderables);
        field<std::uint8_t>(native, kBoundsMode) = static_cast<std::uint8_t>(bounds_mode);
        const int motion = static_cast<int>(random.below(5)) - 2;  // <0: none
        creature->skeleton().motion_link = motion >= 0 ? lc_of[motion] : nullptr;
        field<void*>(native, kMotionLink) = motion >= 0 ? native_of[motion] : nullptr;
        const int bounds_reference = static_cast<int>(random.below(5)) - 2;
        creature->skeleton().set_bounds_reference_object(
            bounds_reference >= 0 ? lc_of[bounds_reference] : nullptr);
        field<void*>(native, kBoundsReference) =
            bounds_reference >= 0 ? native_of[bounds_reference] : nullptr;
        const bool asleep = random.chance(4);
        creature->skeleton().sleep_indicator_active = asleep;
        field<std::uint8_t>(native, kSleepIndicator) = asleep;
        const bool indicator = asleep && !random.chance(4);
        CreatureTestAccess::sleep_indicator(*creature) = indicator ? lc_of[3] : nullptr;
        field<void*>(native, kSleepIndicatorObject) = indicator ? native_of[3] : nullptr;
        const std::int32_t dream = random.chance(8) ? static_cast<std::int32_t>(1 + random.below(5)) : 0;
        creature->instinct_runtime_state().dream_countdown = dream;
        field<std::int32_t>(native, kDreamCountdown) = dream;
        const std::uint8_t involuntary_active =
            random.chance(5) ? static_cast<std::uint8_t>(random.below(8)) : 0xff;
        CreatureTestAccess::active_involuntary(*creature) = involuntary_active;
        field<std::uint8_t>(native, kActiveInvoluntary) = involuntary_active;
        const std::uint8_t selected = static_cast<std::uint8_t>(random.below(16));
        CreatureTestAccess::selected_action(*creature) = selected;
        field<std::uint8_t>(native, kSelectedAction) = selected;
        const std::uint8_t boost = random.byte();
        CreatureTestAccess::boost(*creature) = boost;
        field<std::uint8_t>(native, kBoost) = boost;
        auto& involuntary = creature->instinct_runtime_state().control_state.involuntary_actions;
        for (std::size_t i = 0; i < 8; ++i) {
            involuntary[i].trigger_probability =
                random.chance(2) ? 0 : static_cast<std::uint8_t>(random.below(256));
            involuntary[i].cooldown_ticks =
                random.chance(2) ? 0 : static_cast<std::uint8_t>(random.below(5));
            native[kInvoluntary + 2 * i] = involuntary[i].trigger_probability;
            native[kInvoluntary + 2 * i + 1] = involuntary[i].cooldown_ticks;
        }
        for (std::uint32_t n = 0; n < lobe6; ++n) {
            auto& neuron = brain->lobe(6).neuron(n);
            neuron.firing_strength = random.chance(3) ? 0 : random.byte();
            neuron.activation = random.byte();
            neurons6[n * kNeuronSize + kFiring] = neuron.firing_strength;
            neurons6[n * kNeuronSize + kActivation] = neuron.activation;
        }
        // Scripts: always a quiescent one for both offsets (see above), then
        // a random set of actions, target overrides and involuntary actions.
        g_scripts.clear();
        g_scripts.insert(own_classifier + 0x10);
        g_scripts.insert(own_classifier + 0x20);
        for (std::uint32_t a = 1; a < 16; ++a) {
            if (random.chance(2)) g_scripts.insert(own_classifier + a + 0x20);
            if (random.chance(2)) g_scripts.insert(own_classifier + a + 0x10);
            if (motion >= 0 && random.chance(3)) {
                g_scripts.insert(host.classifiers[lc_of[motion]] + a + 0x10);
            }
        }
        for (std::uint32_t i = 0; i < 8; ++i) {
            if (random.chance(2)) g_scripts.insert(own_classifier + 0x40 + i);
        }

        // ---- updates ----
        const std::uint32_t rand_seed = random.next();
        g_native_rand_state = rand_seed;
        g_lc_rand_state = rand_seed;
        bool case_differs = false;
        for (int step = 0; step < 6 && !case_differs; ++step) {
            g_native_trace = {};
            g_lc_trace = {};
            const std::uint32_t ring_start = kRingStart;
            *image.at<std::uint32_t*>(kRingWrite) = ring_start;
            *image.at<std::uint32_t*>(kRingRead) = ring_start;
            creature->update_action_selection(host, text_api, phrase_host, speech_host);
            native_update(native.data());
            ++updates;
            const std::uint32_t written = *image.at<std::uint32_t*>(kRingWrite);
            for (std::uint32_t e = ring_start; e < written; e += kRingEntry) {
                const auto* entry = image.at<const std::uint32_t*>(e);
                char text[64];
                std::snprintf(text, sizeof text, "%s -> %d event %u arg %u",
                              reinterpret_cast<void*>(entry[0]) == native.data() ? "creature" : "other",
                              g_native_names.count(reinterpret_cast<void*>(entry[1]))
                                  ? g_native_names[reinterpret_cast<void*>(entry[1])] : -1,
                              entry[2], entry[3]);
                g_native_trace.events.push_back(text);
            }

            if (!g_native_trace.events.empty()) ++with_events;
            if (!g_native_trace.indicator.empty()) ++with_indicator;
            for (auto c : g_native_trace.scripts) {
                if ((c & 0xff) >= 0x40) { ++with_involuntary; break; }
            }
            if (!g_native_trace.scripts.empty()) ++action_changes;
            std::string first;
            auto note = [&](const std::string& what) { if (first.empty()) first = what; };
            char text[256];
            if (g_native_trace.scripts != g_lc_trace.scripts) {
                note("scripts: native " + join(g_native_trace.scripts) + "lc " + join(g_lc_trace.scripts));
            }
            if (g_native_trace.indicator != g_lc_trace.indicator) note("sleep indicator calls");
            if (g_native_trace.events != g_lc_trace.events) {
                std::string n, l;
                for (auto& e : g_native_trace.events) n += e + "; ";
                for (auto& e : g_lc_trace.events) l += e + "; ";
                note("events: native " + n + "lc " + l);
            }
            // Native's phrase functions are recorders; LC's run and may say
            // nothing.  LC speaking when native chose no phrase is a difference.
            if (!g_native_trace.spoke && g_lc_trace.spoke) note("speech");
            if (native[kSelectedAction] != static_cast<std::uint8_t>(CreatureTestAccess::selected_action(*creature))) {
                std::snprintf(text, sizeof text, "selected action: native %u lc %u", native[kSelectedAction],
                              CreatureTestAccess::selected_action(*creature));
                note(text);
            }
            if (native[kBoost] != CreatureTestAccess::boost(*creature)) note("boost");
            if (native[kActiveInvoluntary] != CreatureTestAccess::active_involuntary(*creature)) {
                std::snprintf(text, sizeof text, "active involuntary: native %u lc %u",
                              native[kActiveInvoluntary], CreatureTestAccess::active_involuntary(*creature));
                note(text);
            }
            if ((native[kSleepIndicator] != 0) != creature->skeleton().sleep_indicator_active) note("sleep");
            if ((field<void*>(native, kSleepIndicatorObject) != nullptr) !=
                (CreatureTestAccess::sleep_indicator(*creature) != nullptr)) {
                note("sleep indicator object");
            }
            for (std::size_t i = 0; i < 8; ++i) {
                if (native[kInvoluntary + 2 * i + 1] != involuntary[i].cooldown_ticks) {
                    std::snprintf(text, sizeof text, "involuntary %u cooldown", static_cast<unsigned>(i));
                    note(text);
                }
            }
            for (std::uint32_t n = 0; n < lobe6; ++n) {
                const auto& neuron = brain->lobe(6).neuron(n);
                if (neurons6[n * kNeuronSize + kFiring] != neuron.firing_strength ||
                    neurons6[n * kNeuronSize + kActivation] != neuron.activation) {
                    std::snprintf(text, sizeof text, "decision neuron %u: native %u/%u lc %u/%u", n,
                                  neurons6[n * kNeuronSize + kFiring],
                                  neurons6[n * kNeuronSize + kActivation], neuron.firing_strength,
                                  neuron.activation);
                    note(text);
                }
            }
            if (g_native_rand_state != g_lc_rand_state && !g_native_trace.spoke) note("rand draws");
            if (!first.empty() || index == verbose_case) {
                std::printf("case %ld step %d: dream %d involuntary-active %u selected %u motion %d "
                            "bounds %d mode %u asleep %d: %s\n",
                            index, step, dream, involuntary_active, selected, motion, bounds_reference,
                            bounds_mode, asleep ? 1 : 0, first.empty() ? "same" : first.c_str());
            }
            if (!first.empty()) case_differs = true;
            if (g_native_trace.spoke) {
                ++spoken;
                break;
            }
        }
        if (case_differs) ++differing;
        creature->skeleton().motion_link = nullptr;
        creature->skeleton().set_bounds_reference_object(nullptr);
        CreatureTestAccess::sleep_indicator(*creature) = nullptr;
        creature.reset();
    }
    std::printf("covered: %ld updates looked up scripts, %ld ran involuntary lookups, %ld queued "
                "events, %ld tore down the sleep indicator\n",
                action_changes, with_involuntary, with_events, with_indicator);
    std::printf("%ld cases, %ld updates (%ld ended by speech): %ld differ%s\n", cases, updates,
                spoken, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
