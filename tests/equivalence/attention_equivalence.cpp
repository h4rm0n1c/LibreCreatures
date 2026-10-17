// Function-level equivalence: Creature::UpdateAttention @0040bbc0 (CE 10.3),
// with the Creature::SetAction @0040b680 it calls, against
// creatures::Creature::update_attention.
//
// Each case gives an LC creature (brain grown from a stock genome) and a
// native Creature-shaped block the same state at the offsets the functions
// read: the 40 attention records, the attention (7), decision (6), sensory
// (5), verb (3) and stimulus-source (2) lobes, the motion link, sleep and its
// indicator, the goal-direction state (pending record, verb neuron,
// commitment, countdown), the selected action, the bounds reference and
// mode.  Scripts exist for a random set of classifiers as in the action
// test (SetAction looks them up).  A new target's part centre comes from the
// LC object paired with the native one, so both sides see the same point.
//
// One update per case.  After it, the motion link and target point, every
// attention record's sense neuron, all neurons of the five lobes (firing,
// activation, decision exclusions), the goal-direction state, the sleep
// indicator calls, the script lookups and the queued events must match.
//
// Not generated: genera with no attention record (row 10), negative verb
// neurons, attention lobes of other than 40 and decision lobes of other
// than 16 neurons.

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
    static Creature::GoalDirectionState& goal(Creature& c) { return c.goal_direction_state_; }
};
}  // namespace creatures1::creatures

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;
using creatures::CreatureTestAccess;

constexpr std::uint32_t kNativeUpdateAttention = 0x0040bbc0;
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
constexpr std::size_t kDeathState = 0x2ccc;
constexpr std::size_t kAttentionRecords = 0x844, kAttentionStride = 20;
constexpr std::size_t kMotionX = 0x7f4, kMotionY = 0x7f8, kMotionPart = 0x7fc;
constexpr std::size_t kGoalRecord = 0x1ce4, kGoalVerb = 0x1ce8, kGoalCommitment = 0x1cec,
                      kGoalCountdown = 0x1ced;
// Native CBrain: lobe i at 8 + 200 i, neurons at +156, count at +164.
constexpr std::size_t kBrainSize = 0x800;
constexpr std::size_t lobe_neurons(int lobe) { return 8 + 200 * lobe + 156; }
constexpr std::size_t lobe_count(int lobe) { return 8 + 200 * lobe + 164; }
constexpr std::size_t kNeuronSize = 16, kFiring = 2, kActivation = 3, kExcluded = 14;
constexpr int kLobes[] = {2, 3, 5, 6, 7};
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
// Native object -> its LC twin, for the part centre.
std::map<const void*, objects::Object*> g_lc_twin;
void __attribute__((thiscall)) native_part_centre(void* self, int* x, int* y, int part) {
    g_lc_twin.at(self)->get_part_center(x, y, part);
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
    creatures::AttentionClassifier classify_object(const objects::Object& object) const override {
        const std::uint32_t c = classifiers.at(&object);
        return {static_cast<creatures::AttentionObjectFamily>(c >> 24),
                static_cast<std::uint8_t>(c >> 16)};
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
    std::uint64_t seed = 0xa77e40u;
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
    const auto native_update = image.at<NativeUpdate>(kNativeUpdateAttention);
    for (auto& slot : g_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_vtable[29] = reinterpret_cast<void*>(&native_part_centre);
    g_vtable[34] = reinterpret_cast<void*>(&native_indicator_event);
    g_vtable[16] = reinterpret_cast<void*>(&native_indicator_init);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    NoRenderables renderables;
    Random random{seed};
    long differing = 0, shifts = 0, deliveries = 0;
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
        if (brain->lobe(7).neuron_count_value() != 40 || brain->lobe(6).neuron_count_value() != 16) {
            std::fprintf(stderr, "a genome with %u attention and %u decision neurons\n",
                         brain->lobe(7).neuron_count_value(), brain->lobe(6).neuron_count_value());
            return 2;
        }

        // ---- native creature and brain ----
        std::vector<std::uint8_t> native(kCreatureSize, 0);
        std::vector<std::uint8_t> native_brain(kBrainSize, 0);
        std::map<int, std::vector<std::uint8_t>> neurons;
        field<void*>(native, 0) = g_vtable;
        field<void*>(native, kBrain) = native_brain.data();
        for (int lobe : kLobes) {
            const std::uint32_t count = brain->lobe(lobe).neuron_count_value();
            neurons[lobe].assign(kNeuronSize * (count + 1), 0);
            field<void*>(native_brain, lobe_neurons(lobe)) = neurons[lobe].data();
            field<std::int32_t>(native_brain, lobe_count(lobe)) = static_cast<std::int32_t>(count);
            for (std::uint32_t n = 0; n < count; ++n) {
                auto& neuron = brain->lobe(lobe).neuron(n);
                neuron.firing_strength = random.chance(3) ? 0 : random.byte();
                neuron.activation = random.byte();
                neuron.winner_take_all_excluded = static_cast<std::uint8_t>(random.below(2));
                std::uint8_t* raw = neurons[lobe].data() + n * kNeuronSize;
                raw[kFiring] = neuron.firing_strength;
                raw[kActivation] = neuron.activation;
                raw[kExcluded] = neuron.winner_take_all_excluded;
            }
        }

        // ---- objects: 0-5 attention targets, 6 sleep indicator ----
        AttentionHost host;
        host.creature = creature.get();
        std::vector<std::unique_ptr<objects::Object>> lc_objects;
        std::vector<std::vector<std::uint8_t>> native_objects;
        std::vector<objects::Object*> lc_of(7);
        std::vector<void*> native_of(7);
        g_native_names.clear();
        g_lc_twin.clear();
        for (int o = 0; o < 7; ++o) {
            lc_objects.push_back(std::make_unique<objects::Object>());
            native_objects.emplace_back(kObjectSize, 0);
            const std::uint32_t family = 2 + random.below(3);
            const std::uint32_t genus = family == 2 ? random.below(40) : family == 3 ? random.below(15)
                                                                                     : random.below(5);
            const std::uint32_t classifier = (family << 24) | (genus << 16) | (random.below(256) << 8);
            const bool live = !random.chance(4);
            lc_of[o] = lc_objects.back().get();
            lc_of[o]->set_classifier_base(classifier);
            lc_of[o]->set_bounds_mode(random.below(5), renderables);
            native_of[o] = native_objects.back().data();
            field<void*>(native_objects.back(), 0) = g_vtable;
            field<std::uint32_t>(native_objects.back(), 4) = classifier;
            field<std::int32_t>(native_objects.back(), kObjectTimer) = live ? 25 : -1;
            host.classifiers[lc_of[o]] = classifier;
            host.live[lc_of[o]] = live;
            host.names[lc_of[o]] = o;
            g_native_names[native_of[o]] = o;
            g_lc_twin[native_of[o]] = lc_of[o];
        }
        auto native_for = [&](const objects::Object* object) -> void* {
            for (int o = 0; o < 7; ++o) {
                if (lc_of[o] == object) return native_of[o];
            }
            return nullptr;
        };

        // ---- creature state ----
        const std::uint32_t own_classifier =
            (4u << 24) | ((1 + random.below(2)) << 16) | (random.below(256) << 8);
        creature->skeleton().set_classifier_base(own_classifier);
        field<std::uint32_t>(native, kClassifier) = own_classifier;
        const std::uint32_t bounds_mode = random.below(5);
        creature->skeleton().set_bounds_mode(bounds_mode, renderables);
        field<std::uint8_t>(native, kBoundsMode) = static_cast<std::uint8_t>(bounds_mode);
        const int bounds_reference = static_cast<int>(random.below(9)) - 3;
        creature->skeleton().set_bounds_reference_object(
            bounds_reference >= 0 ? lc_of[bounds_reference] : nullptr);
        field<void*>(native, kBoundsReference) =
            bounds_reference >= 0 ? native_of[bounds_reference] : nullptr;
        auto& records = creature->attention_records();
        for (std::size_t r = 0; r < records.size(); ++r) {
            auto& record = records[r];
            const int target = static_cast<int>(random.below(10)) - 4;
            record.target = target >= 0 ? lc_of[target] : nullptr;
            record.target_neuron_index = random.chance(2) ? static_cast<std::uint8_t>(random.below(40))
                                                          : random.byte();
            record.lobe_activation = random.byte();
            record.world_x = static_cast<int>(random.below(8352));
            record.world_y = static_cast<int>(random.below(1200));
            record.visible = random.chance(2);
            std::uint8_t* raw = native.data() + kAttentionRecords + r * kAttentionStride;
            *reinterpret_cast<void**>(raw) = native_for(record.target);
            raw[4] = record.target_neuron_index;
            raw[5] = record.lobe_activation;
            *reinterpret_cast<int*>(raw + 8) = record.world_x;
            *reinterpret_cast<int*>(raw + 12) = record.world_y;
            raw[16] = record.visible;
        }
        const int motion = static_cast<int>(random.below(9)) - 3;
        creature->skeleton().motion_link = motion >= 0 ? lc_of[motion] : nullptr;
        field<void*>(native, kMotionLink) = motion >= 0 ? native_of[motion] : nullptr;
        const int motion_x = static_cast<int>(random.below(8352)), motion_y = static_cast<int>(random.below(1200));
        const std::int32_t motion_part = static_cast<std::int32_t>(random.below(4));
        creature->skeleton().motion_target_x = motion_x;
        creature->skeleton().motion_target_y = motion_y;
        creature->skeleton().motion_target_part_index = motion_part;
        field<std::int32_t>(native, kMotionX) = motion_x;
        field<std::int32_t>(native, kMotionY) = motion_y;
        field<std::int32_t>(native, kMotionPart) = motion_part;
        const bool asleep = random.chance(3);
        creature->skeleton().sleep_indicator_active = asleep;
        field<std::uint8_t>(native, kSleepIndicator) = asleep;
        const bool indicator = asleep && !random.chance(4);
        CreatureTestAccess::sleep_indicator(*creature) = indicator ? lc_of[6] : nullptr;
        field<void*>(native, kSleepIndicatorObject) = indicator ? native_of[6] : nullptr;
        const std::uint8_t selected = static_cast<std::uint8_t>(random.below(16));
        CreatureTestAccess::selected_action(*creature) = selected;
        field<std::uint8_t>(native, kSelectedAction) = selected;
        CreatureTestAccess::active_involuntary(*creature) = 0xff;
        field<std::uint8_t>(native, kActiveInvoluntary) = 0xff;
        auto& goal = CreatureTestAccess::goal(*creature);
        goal.attention_record_index = random.chance(3) ? -1 : static_cast<std::int32_t>(random.below(40));
        goal.action_lobe_neuron_index = static_cast<std::int32_t>(random.below(20));
        goal.commitment = random.byte();
        goal.delivery_countdown = random.chance(3) ? 0 : static_cast<std::uint8_t>(random.below(4));
        if (random.chance(2)) {
            // Aim the pending verb at an object that then wins attention.
            const int o = static_cast<int>(random.below(6));
            const std::uint32_t c = host.classifiers[lc_of[o]];
            const std::uint32_t family = c >> 24, genus = (c >> 16) & 0xff;
            goal.attention_record_index = static_cast<std::int32_t>(
                family == 2 ? genus : family == 3 ? genus + 25 : genus + 35);
            const std::size_t k = 1 + random.below(39);
            records[k].target = lc_of[o];
            *reinterpret_cast<void**>(native.data() + kAttentionRecords + k * kAttentionStride) =
                native_of[o];
            brain->lobe(7).neuron(static_cast<std::uint32_t>(k)).firing_strength = 255;
            neurons[7][k * kNeuronSize + kFiring] = 255;
            if (random.chance(2)) goal.delivery_countdown = static_cast<std::uint8_t>(1 + random.below(3));
        }
        field<std::int32_t>(native, kGoalRecord) = goal.attention_record_index;
        native[kGoalCountdown] = goal.delivery_countdown;
        field<std::int32_t>(native, kGoalVerb) = goal.action_lobe_neuron_index;
        native[kGoalCommitment] = goal.commitment;
        g_scripts.clear();
        g_scripts.insert(own_classifier + 0x10);
        g_scripts.insert(own_classifier + 0x20);
        for (int o = 0; o < 6; ++o) {
            if (random.chance(3)) g_scripts.insert(host.classifiers[lc_of[o]] + 0x10);
        }

        // ---- one update ----
        g_native_trace = {};
        g_lc_trace = {};
        *image.at<std::uint32_t*>(kRingWrite) = kRingStart;
        *image.at<std::uint32_t*>(kRingRead) = kRingStart;
        const objects::Object* motion_before = creature->skeleton().motion_link;
        const std::int32_t goal_before = goal.attention_record_index;
        creature->update_attention(host);
        native_update(native.data());
        if (creature->skeleton().motion_link != motion_before) ++shifts;
        if (goal_before != -1 && goal.attention_record_index == -1) ++deliveries;
        const std::uint32_t written = *image.at<std::uint32_t*>(kRingWrite);
        for (std::uint32_t e = kRingStart; e < written; e += kRingEntry) {
            const auto* entry = image.at<const std::uint32_t*>(e);
            char text[64];
            std::snprintf(text, sizeof text, "%s -> %d event %u arg %u",
                          reinterpret_cast<void*>(entry[0]) == native.data() ? "creature" : "other",
                          g_native_names.count(reinterpret_cast<void*>(entry[1]))
                              ? g_native_names[reinterpret_cast<void*>(entry[1])] : -1,
                          entry[2], entry[3]);
            g_native_trace.events.push_back(text);
        }

        // ---- compare ----
        std::string first;
        auto note = [&](const std::string& what) { if (first.empty()) first = what; };
        char text[256];
        if (field<void*>(native, kMotionLink) != native_for(creature->skeleton().motion_link)) {
            note("motion link");
        }
        if (field<std::int32_t>(native, kMotionX) != creature->skeleton().motion_target_x ||
            field<std::int32_t>(native, kMotionY) != creature->skeleton().motion_target_y ||
            field<std::int32_t>(native, kMotionPart) != creature->skeleton().motion_target_part_index) {
            note("motion target point");
        }
        for (std::size_t r = 0; r < records.size(); ++r) {
            const std::uint8_t* raw = native.data() + kAttentionRecords + r * kAttentionStride;
            if (raw[4] != records[r].target_neuron_index) {
                std::snprintf(text, sizeof text, "record %u sense neuron", static_cast<unsigned>(r));
                note(text);
            }
        }
        for (int lobe : kLobes) {
            const std::uint32_t count = brain->lobe(lobe).neuron_count_value();
            for (std::uint32_t n = 0; n < count; ++n) {
                const auto& neuron = brain->lobe(lobe).neuron(n);
                const std::uint8_t* raw = neurons[lobe].data() + n * kNeuronSize;
                if (raw[kFiring] != neuron.firing_strength || raw[kActivation] != neuron.activation ||
                    raw[kExcluded] != neuron.winner_take_all_excluded) {
                    std::snprintf(text, sizeof text,
                                  "lobe %d neuron %u: native %u/%u/%u lc %u/%u/%u", lobe, n,
                                  raw[kFiring], raw[kActivation], raw[kExcluded],
                                  neuron.firing_strength, neuron.activation,
                                  neuron.winner_take_all_excluded);
                    note(text);
                }
            }
        }
        if (field<std::int32_t>(native, kGoalRecord) != goal.attention_record_index ||
            native[kGoalCountdown] != goal.delivery_countdown) {
            std::snprintf(text, sizeof text, "goal direction: native record %d countdown %u, lc %d %u",
                          field<std::int32_t>(native, kGoalRecord), native[kGoalCountdown],
                          goal.attention_record_index, goal.delivery_countdown);
            note(text);
        }
        if ((native[kSleepIndicator] != 0) != creature->skeleton().sleep_indicator_active) note("sleep");
        if (g_native_trace.indicator != g_lc_trace.indicator) note("sleep indicator calls");
        if (g_native_trace.scripts != g_lc_trace.scripts) {
            note("scripts: native " + join(g_native_trace.scripts) + "lc " + join(g_lc_trace.scripts));
        }
        if (g_native_trace.events != g_lc_trace.events) note("events");
        if (native[kSelectedAction] != static_cast<std::uint8_t>(CreatureTestAccess::selected_action(*creature))) {
            note("selected action");
        }
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: motion %d asleep %d goal %d countdown %u: %s\n", index, motion,
                        asleep ? 1 : 0, goal_before, native[kGoalCountdown],
                        first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
        creature->skeleton().motion_link = nullptr;
        creature->skeleton().set_bounds_reference_object(nullptr);
        CreatureTestAccess::sleep_indicator(*creature) = nullptr;
        for (auto& record : records) record.target = nullptr;
        creature.reset();
    }
    std::printf("covered: %ld attention shifts, %ld verb deliveries\n", shifts, deliveries);
    std::printf("%ld cases: %ld differ%s\n", cases, differing,
                all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
