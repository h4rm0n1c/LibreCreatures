// Function-level equivalence: what a creature does with heard words, CE 10.3
// Creature::ProcessHeardWords @0040a630 (with the "look" search @0040ac90,
// UpdateLearnedWordRecord, TriggerBuiltInStimulus, ApplyStimulus,
// SpeakActionPhrase and ApplyGoalDirection it calls) against
// creatures::Creature::process_heard_words.
//
// The native creature is a Creature-shaped block with the state these
// functions read: the 80 learned-word records, the 40 attention records,
// the built-in stimulus contexts, the goal matrix and goal-direction state,
// the stimulus-source (2), verb (3), sensory (5) and attention (7) lobes,
// all chemicals, sleep, the motion link, the movement bounds, the bounds
// reference, the selected action and the classifier.  Eight objects of
// random kind and the hand stand near it; the native object registry and
// LC's host list them alike, with bounds and sound positions from one
// table.  The CRT text functions and rand are bound as in the learned-word
// test; Speak, SpeakTextWithVoice and the speech-range event queue are
// recorders on both sides.  LC's hosts are the game's
// (WindowsCreatureWordsHost and friends).
//
// Each case: a creature with a brain grown from a stock genome and random
// state, then 8 sentences of 1 to 4 words: the creature's name, nouns,
// verbs, yes/no/look/what, words it does not know, in either case, with
// extra spaces sometimes; from the hand, another creature or an object;
// sometimes while asleep.  After each sentence, all learned-word records,
// attention records, the four lobes, the chemicals, the goal matrix, the
// goal-direction state, everything said and queued, and the rand state
// must match.
//
// Not generated: words of vowels only (row 80), genera with no attention
// record (row 10), words too long for a record.

#include "native_image.hpp"

#include "biochemistry/biochemistry.hpp"
#include "brain/brain.hpp"
#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "creatures/genome.hpp"
#include "creatures/learned_words.hpp"
#include "objects/object.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace creatures1::creatures {
// Declared a friend in creature.hpp.
class CreatureTestAccess {
public:
    static Creature::GoalDirectionState& goal(Creature& c) { return c.goal_direction_state_; }
    static std::uint32_t& selected_action(Creature& c) { return c.selected_action_id_; }
};
}  // namespace creatures1::creatures

namespace {

using namespace creatures1;
namespace genome = creatures1::creatures;
using creatures::CreatureTestAccess;

constexpr std::uint32_t kProcessHeardWords = 0x0040a630;
constexpr std::uint32_t kNativeDebugLog = 0x00418000, kSpeak = 0x0040b5f0, kSpeakWithVoice = 0x0040b560,
                        kSpeechEvent = 0x00422f90;
constexpr std::uint32_t kPointerTool = 0x00467dbc, kRegistryData = 0x004704a8, kRegistryCount = 0x004704ac;

constexpr std::size_t kCreatureSize = 0x2d00;
constexpr std::size_t kClassifier = 0x4, kBoundsFlags = 0x9, kMovementBounds = 0xc, kBoundsReference = 0x1c;
constexpr std::size_t kSleep = 0x11f, kMotionLink = 0x7f0, kSelectedAction = 0x840;
constexpr std::size_t kAttentionRecords = 0x844, kAttentionStride = 20;
constexpr std::size_t kRecords = 0xb64, kRecordSize = 0x38, kRecordCount = 80;
constexpr std::size_t kGoalRecord = 0x1ce4, kGoalVerb = 0x1ce8, kGoalCommitment = 0x1cec, kGoalCountdown = 0x1ced;
constexpr std::size_t kGoalMatrix = 0x1cf0, kContexts = 0x26f0, kContextStride = 20;
constexpr std::size_t kBrain = 0x2c18, kBiochemistry = 0x2c1c, kDeathState = 0x2ccc;
constexpr std::size_t kBrainSize = 0x800;
constexpr std::size_t lobe_neurons(int lobe) { return 8 + 200 * lobe + 156; }
constexpr std::size_t lobe_count(int lobe) { return 8 + 200 * lobe + 164; }
constexpr std::size_t kNeuronSize = 16, kFiring = 2, kActivation = 3;
constexpr int kLobes[] = {2, 3, 5, 7};
constexpr std::size_t kBiochemistrySize = 8 + 2 * 256 + 64;

// ---- rand and the CRT text functions, as in the learned-word test ----
std::uint32_t g_native_rand = 1, g_lc_rand = 1;
extern "C" int __cdecl native_rand() {
    g_native_rand = g_native_rand * 214013u + 2531011u;
    return static_cast<int>((g_native_rand >> 16) & 0x7fff);
}
extern "C" int __wrap_rand() {
    g_lc_rand = g_lc_rand * 214013u + 2531011u;
    return static_cast<int>((g_lc_rand >> 16) & 0x7fff);
}
extern "C" void __wrap_srand(unsigned seed) { g_lc_rand = seed; }
bool g_overflow = false;
extern "C" const char* __cdecl mbsstr(const char* s, const char* f) { return std::strstr(s, f); }
extern "C" const char* __cdecl mbsinc(const char* s) { return s + 1; }
extern "C" const char* __cdecl mbschr(const char* s, unsigned c) { return std::strchr(s, static_cast<int>(c)); }
extern "C" int __cdecl mbsicmp(const char* a, const char* b) { return _stricmp(a, b); }
extern "C" int __cdecl strcpy_s_impl(char* d, std::size_t n, const char* s) {
    if (std::strlen(s) + 1 > n) { g_overflow = true; d[0] = '\0'; return 34; }
    std::memcpy(d, s, std::strlen(s) + 1);
    return 0;
}
extern "C" int __cdecl strcat_s_impl(char* d, std::size_t n, const char* s) {
    const std::size_t used = std::strlen(d);
    if (used + std::strlen(s) + 1 > n) { g_overflow = true; d[0] = '\0'; return 34; }
    std::memcpy(d + used, s, std::strlen(s) + 1);
    return 0;
}
int __attribute__((stdcall)) point_in_rect(const world::WorldRect* r, std::int32_t x, std::int32_t y) {
    return x >= r->min_x && x < r->max_x && y >= r->min_y && y < r->max_y;
}
class TextApi final : public creatures::MultibyteTextApi {
public:
    const char* find_character(const char* t, unsigned char c) const override { return mbschr(t, c); }
    const char* find_substring(const char* t, const char* f) const override { return mbsstr(t, f); }
    const char* next_character(const char* t) const override { return mbsinc(t); }
    int compare_strings(const char* l, const char* r) const override { return mbsicmp(l, r); }
    void copy_string(char* d, std::size_t n, const char* s) const override { strcpy_s_impl(d, n, s); }
    void append_string(char* d, std::size_t n, const char* s) const override { strcat_s_impl(d, n, s); }
};

// ---- speech, recorded on both sides ----
std::vector<std::string> g_native_said, g_lc_said;
void __attribute__((thiscall)) native_speak(void*, const char* text) { g_native_said.push_back(text); }
void __attribute__((thiscall)) native_speak_with_voice(void*, const char* text, int* delay) {
    g_native_said.push_back(std::string("voice:") + text);
    *delay = 5;
}
extern "C" void __cdecl native_speech_event(void*, std::uint32_t event, std::uint32_t argument, std::uint32_t,
                                            std::int32_t delay) {
    char t[64];
    std::snprintf(t, sizeof t, "event %u arg %u delay %d", event, argument, delay);
    g_native_said.push_back(t);
}
__asm__(".globl _native_speech_event_thunk\n"
        "_native_speech_event_thunk:\n"
        "    pushl 12(%esp)\n"
        "    pushl 12(%esp)\n"
        "    pushl 12(%esp)\n"
        "    pushl %edx\n"
        "    pushl %ecx\n"
        "    call _native_speech_event\n"
        "    addl $20, %esp\n"
        "    ret\n");
extern "C" void native_speech_event_thunk();

// ---- native objects: bounds and sound position from one table ----
struct Facts {
    world::WorldRect bounds{};
    int x = 0, y = 0;
};
std::map<const void*, Facts> g_native_facts;
std::map<const objects::Object*, Facts> g_lc_facts;
void __attribute__((thiscall)) native_get_bounds(void* self, world::WorldRect* out) { *out = g_native_facts.at(self).bounds; }
int __attribute__((thiscall)) native_sound_x(void* self) { return g_native_facts.at(self).x; }
int __attribute__((thiscall)) native_sound_y(void* self) { return g_native_facts.at(self).y; }
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called on a native object\n");
    std::exit(5);
}
void* g_vtable[64];

class PlainObject final : public objects::Object {
public:
    bool get_bounds(world::WorldRect* out) const override {
        *out = g_lc_facts.at(this).bounds;
        return true;
    }
    int sound_source_x() const override { return g_lc_facts.at(this).x; }
    int sound_source_y() const override { return g_lc_facts.at(this).y; }
};

// ---- LC hosts, as the game's ----
class Hosts final : public creatures::Creature::StimulusSourceHost,
                    public creatures::Creature::CreatureHeardWordsHost,
                    public creatures::PointerAttentionApi,
                    public creatures::Creature::CreatureSpeechPhraseHost,
                    public creatures::Creature::CreatureSpeechHost,
                    public creatures::CreatureGoalDirectionHost {
public:
    creatures::Creature* creature = nullptr;
    objects::Object* hand = nullptr;
    std::vector<objects::Object*> registry;

    // StimulusSourceHost
    creatures::AttentionClassifier classify(const objects::Object& o) const override {
        const std::uint32_t c = o.classifier_base();
        return {static_cast<creatures::AttentionObjectFamily>(c >> 24), static_cast<std::uint8_t>(c >> 16)};
    }
    int sound_source_x(const objects::Object& o) const override { return g_lc_facts.at(&o).x; }
    int sound_source_y(const objects::Object& o) const override { return g_lc_facts.at(&o).y; }
    bool is_this_creature(const objects::Object& o, const creatures::Creature& c) const override {
        return &o == &c.skeleton();
    }
    // CreatureHeardWordsHost
    char* mutable_words_for_event(const objects::QueuedObjectEvent&) override { return nullptr; }
    bool is_pointer_tool(const objects::Object& o) const override { return &o == hand; }
    creatures::PointerAttentionApi& pointer_attention_api() override { return *this; }
    creatures::Creature::CreatureSpeechPhraseHost& speech_phrase_host() override { return *this; }
    creatures::CreatureGoalDirectionHost& goal_direction_host() override { return *this; }
    // PointerAttentionApi (WindowsCreatureWordsHost)
    int creature_sound_source_x() const override { return g_lc_facts.at(&creature->skeleton()).x; }
    int pointer_tool_sound_source_x() const override { return g_lc_facts.at(hand).x; }
    world::WorldRect creature_movement_bounds() const override {
        const objects::Object* reference = creature->skeleton().bounds_reference_object();
        return reference != nullptr ? reference->movement_bounds() : creature->skeleton().movement_bounds();
    }
    std::size_t non_scenery_object_count() const override { return registry.size(); }
    bool read_non_scenery_object(std::size_t i, creatures::PointerAttentionCandidate& out) const override {
        objects::Object* o = registry[i];
        out.bounds = g_lc_facts.at(o).bounds;
        out.classifier = classify(*o);
        out.is_pointer_tool = o == hand;
        out.is_this_creature = o == &creature->skeleton();
        out.blocks_pointer_attention = o->has_bounds_flag(0x10);
        return true;
    }
    creatures::AttentionClassifier pointer_tool_classifier() const override { return classify(*hand); }
    void clear_pointer_tool_lobe_neuron(std::uint32_t n) override {
        brain::Lobe& lobe = creature->brain()->lobe(7);
        if (n < lobe.neuron_count_value()) {
            lobe.neuron(n).firing_strength = 0;
            lobe.neuron(n).activation = 0;
        }
    }
    void report_registry_bounds_failure() override {}
    // Speech
    objects::Object& object_for_creature(creatures::Creature& c) const override { return c.skeleton(); }
    std::int32_t attention_record_index(const objects::Object& o) const override {
        return static_cast<std::int32_t>(creatures::get_attention_record_index(classify(o)));
    }
    void speak_text_with_voice(creatures::Creature&, std::string_view phrase, std::int32_t& delay) override {
        g_lc_said.push_back("voice:" + std::string(phrase));
        delay = 5;
    }
    void queue_speech_event(objects::Object&, std::uint32_t argument, std::int32_t delay) override {
        char t[64];
        std::snprintf(t, sizeof t, "event %u arg %u delay %d", 7u, argument, delay);
        g_lc_said.push_back(t);
    }
    void speak(creatures::Creature&, std::string_view phrase) override { g_lc_said.emplace_back(phrase); }
    // CreatureGoalDirectionHost (only attention_record_index is reached)
    std::int32_t room_index_at(int, int) const override { return -1; }
    std::int32_t room_class_at(int, int) const override { return 1; }
    std::uint32_t current_interaction_event_id(const objects::Object&) const override { return 0; }
};

class OneGenomeFile final : public genome::GenomeFileStore {
public:
    explicit OneGenomeFile(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    std::string secondary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    std::string primary_genetics_path(genome::GenomeFilenameId) const override { return "g"; }
    bool regular_file_exists(const std::string&) const override { return true; }
    std::vector<std::uint8_t> read_file(const std::string&) override { return bytes_; }
    void write_secondary_genetics_file(genome::GenomeFilenameId, const std::vector<std::uint8_t>&) override {}

private:
    std::vector<std::uint8_t> bytes_;
};
std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}

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
    int range(int low, int high) { return low + static_cast<int>(below(static_cast<std::uint32_t>(high - low + 1))); }
};
template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}
bool all_vowels(const char* w) {
    for (; *w != '\0'; ++w) {
        if (std::strchr("aeiouAEIOU", *w) == nullptr) return false;
    }
    return true;
}
std::uint32_t random_classifier(Random& random, std::uint32_t family) {
    const std::uint32_t genus = family == 2 ? random.below(40) : family == 3 ? random.below(15) : random.below(5);
    return (family << 24) | (genus << 16) | (random.below(256) << 8);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> <genome.gen>... [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x4ea5du;
    std::vector<std::vector<std::uint8_t>> genomes;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-cases" && i + 1 < argc) cases = std::atol(argv[++i]);
        else if (arg == "-seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 0);
        else {
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
    const struct { const char* name; void* fn; } imports[] = {
        {"!rand", reinterpret_cast<void*>(&native_rand)},       {"!_mbsstr", reinterpret_cast<void*>(&mbsstr)},
        {"!_mbsinc", reinterpret_cast<void*>(&mbsinc)},         {"!_mbschr", reinterpret_cast<void*>(&mbschr)},
        {"!_mbsicmp", reinterpret_cast<void*>(&mbsicmp)},       {"!strcpy_s", reinterpret_cast<void*>(&strcpy_s_impl)},
        {"!strcat_s", reinterpret_cast<void*>(&strcat_s_impl)}, {"!PtInRect", reinterpret_cast<void*>(&point_in_rect)},
    };
    for (const auto& import : imports) {
        if (!image.bind_import(import.name, import.fn)) {
            std::fprintf(stderr, "the exe does not import %s\n", import.name + 1);
            return 2;
        }
    }
    image.make_return(kNativeDebugLog);
    image.redirect(kSpeak, reinterpret_cast<const void*>(&native_speak));
    image.redirect(kSpeakWithVoice, reinterpret_cast<const void*>(&native_speak_with_voice));
    image.redirect(kSpeechEvent, reinterpret_cast<const void*>(&native_speech_event_thunk));
    for (auto& s : g_vtable) s = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_vtable[26] = reinterpret_cast<void*>(&native_get_bounds);
    g_vtable[30] = reinterpret_cast<void*>(&native_sound_x);
    g_vtable[31] = reinterpret_cast<void*>(&native_sound_y);
    using Process = void(__attribute__((thiscall)) *)(void*, const void*, char*);
    const auto native_process = image.at<Process>(kProcessHeardWords);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    const TextApi text;
    long differing = 0, sentences = 0, looks = 0, learned = 0;
    for (long index = 0; index < cases; ++index) {
        // ---- the creature, both sides ----
        OneGenomeFile file(genomes[random.below(static_cast<std::uint32_t>(genomes.size()))]);
        genome::Genome g(1, random.chance(2) ? genome::GenomeSex::male : genome::GenomeSex::female,
                         genome::GenomeLifeStage::stage_zero, &file);
        auto creature = std::make_unique<creatures::Creature>();
        auto* brain = new brain::Brain();
        brain->load_genome(g);
        creature->set_brain(brain);
        creature->set_biochemistry(new biochemistry::Biochemistry());
        std::vector<std::uint8_t> native_storage(kCreatureSize, 0);
        std::uint8_t* native = native_storage.data();
        std::vector<std::uint8_t> native_brain(kBrainSize, 0), native_bio(kBiochemistrySize, 0);
        std::map<int, std::vector<std::uint8_t>> neurons;
        field<void*>(native, 0) = g_vtable;
        field<void*>(native, kBrain) = native_brain.data();
        field<void*>(native, kBiochemistry) = native_bio.data();
        for (int lobe : kLobes) {
            const std::uint32_t count = brain->lobe(lobe).neuron_count_value();
            neurons[lobe].assign(kNeuronSize * (count + 1), 0);
            field<void*>(native_brain.data(), lobe_neurons(lobe)) = neurons[lobe].data();
            field<std::int32_t>(native_brain.data(), lobe_count(lobe)) = static_cast<std::int32_t>(count);
            for (std::uint32_t n = 0; n < count; ++n) {
                auto& neuron = brain->lobe(lobe).neuron(n);
                neuron.firing_strength = random.byte();
                neuron.activation = random.chance(3) ? 0 : random.byte();
                neurons[lobe][n * kNeuronSize + kFiring] = neuron.firing_strength;
                neurons[lobe][n * kNeuronSize + kActivation] = neuron.activation;
            }
        }
        auto& chemicals = creature->biochemistry()->chemical_states();
        for (int c = 0; c < 256; ++c) {
            chemicals[c].concentration = random.byte();
            native_bio[8 + 2 * c] = chemicals[c].concentration;
        }
        auto& matrix = creature->goal_direction_weight_matrix();
        for (std::size_t r = 0; r < 40; ++r) {
            for (std::size_t c = 0; c < 16; ++c) {
                matrix[r][c] = random.byte();
                field<std::int32_t>(native, kGoalMatrix + (r * 16 + c) * 4) = matrix[r][c];
            }
        }
        auto& contexts = creature->built_in_stimulus_contexts();
        for (std::size_t k = 0; k < 16; ++k) {
            auto& context = contexts[k];
            context.descriptor = {random.byte(), random.byte(), random.byte(), static_cast<std::uint8_t>(random.below(8))};
            context.chemical_ids = {random.chance(2) ? std::uint8_t(0) : random.byte(), random.byte(),
                                    static_cast<std::uint8_t>(0x21 + random.below(16)), 0};
            context.chemical_amounts = {random.byte(), random.byte(), random.byte(), random.byte()};
            std::memcpy(native + kContexts + k * kContextStride + 8, &context.descriptor, 12);
        }
        // Words: the default vocabulary with some records changed.
        creature->initialize_default_vocabulary();
        for (std::size_t r = 0; r < kRecordCount; ++r) {
            auto& record = creature->learned_word_record(r);
            if (record.recognized_word[0] == '\0' || all_vowels(record.recognized_word)) {
                std::snprintf(record.recognized_word, sizeof record.recognized_word, "w%ux", static_cast<unsigned>(r));
            }
            if (random.chance(3)) std::snprintf(record.response_word, sizeof record.response_word, "%sa", record.recognized_word);
            record.reinforcement = random.below(256);
            std::memcpy(native + kRecords + r * kRecordSize, &record, kRecordSize);
        }

        // ---- objects: 8 around, the hand, another creature ----
        Hosts hosts;
        hosts.creature = creature.get();
        g_native_facts.clear();
        g_lc_facts.clear();
        std::vector<std::unique_ptr<PlainObject>> lc_objects;
        std::vector<std::vector<std::uint8_t>> native_objects;
        std::vector<objects::Object*> lc_all;
        std::vector<std::uint8_t*> native_all;
        const int base_x = random.range(1000, 7000), base_y = random.range(300, 800);
        auto place = [&](const objects::Object* lc, const void* nat, bool creature_self) {
            Facts f;
            const int x = base_x + random.range(-700, 700), y = base_y + random.range(-200, 200);
            f.bounds = {x, y, x + random.range(10, 120), y + random.range(10, 120)};
            f.x = creature_self ? f.bounds.min_x : x + random.range(0, 40);
            f.y = creature_self ? f.bounds.min_y : y;
            g_native_facts[nat] = f;
            g_lc_facts[lc] = f;
        };
        for (int o = 0; o < 10; ++o) {
            lc_objects.push_back(std::make_unique<PlainObject>());
            native_objects.emplace_back(kCreatureSize, 0);
            PlainObject& lc = *lc_objects.back();
            std::uint8_t* nat = native_objects.back().data();
            field<void*>(nat, 0) = g_vtable;
            const std::uint32_t classifier = o == 8 ? 0x02010000u : o == 9 ? random_classifier(random, 4)
                                                                          : random_classifier(random, 2 + random.below(3));
            lc.set_classifier_base(classifier);
            field<std::uint32_t>(nat, kClassifier) = classifier;
            const std::uint8_t flags = random.chance(6) ? 0x10 : 0;
            lc.set_bounds_flags_for_script(flags);
            nat[kBoundsFlags] = flags;
            place(&lc, nat, false);
            lc_all.push_back(&lc);
            native_all.push_back(nat);
        }
        objects::Object* hand = lc_all[8];
        hosts.hand = hand;
        *image.at<void**>(kPointerTool) = native_all[8];
        // The creature itself, in the registry too.
        auto& skeleton = creature->skeleton();
        const std::uint32_t own_classifier = random_classifier(random, 4);
        skeleton.set_classifier_base(own_classifier);
        field<std::uint32_t>(native, kClassifier) = own_classifier;
        place(&skeleton, native, true);
        skeleton.sprite_bounds = g_lc_facts[&skeleton].bounds;
        const world::WorldRect movement = {base_x - random.range(0, 900), base_y - random.range(0, 300),
                                           base_x + random.range(0, 900), base_y + random.range(0, 300)};
        skeleton.set_movement_bounds(movement);
        field<world::WorldRect>(native, kMovementBounds) = movement;
        // Sometimes in a vehicle: a bounds reference with its own movement
        // bounds, which "look" searches.
        if (random.chance(3)) {
            const int k = static_cast<int>(random.below(8));
            const world::WorldRect ref_bounds = {base_x - random.range(0, 500), base_y - random.range(0, 200),
                                                 base_x + random.range(0, 500), base_y + random.range(0, 200)};
            lc_objects[k]->set_movement_bounds(ref_bounds);
            field<world::WorldRect>(native_objects[k].data(), kMovementBounds) = ref_bounds;
            skeleton.set_bounds_reference_object(lc_objects[k].get());
            field<void*>(native, kBoundsReference) = native_objects[k].data();
        }
        lc_all.push_back(&skeleton);
        native_all.push_back(native);
        std::vector<std::uint32_t> registry;
        for (std::size_t o = 0; o < lc_all.size(); ++o) {
            hosts.registry.push_back(lc_all[o]);
            registry.push_back(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(native_all[o])));
        }
        *image.at<std::uint32_t**>(kRegistryData) = registry.data();
        *image.at<std::int32_t*>(kRegistryCount) = static_cast<std::int32_t>(registry.size());
        auto native_for = [&](const objects::Object* o) -> void* {
            for (std::size_t i = 0; i < lc_all.size(); ++i) if (lc_all[i] == o) return native_all[i];
            return nullptr;
        };
        // Attention records, motion link, goal state, selected action.
        auto& records = creature->attention_records();
        for (std::size_t r = 0; r < records.size(); ++r) {
            auto& record = records[r];
            record.target = random.chance(3) ? lc_all[random.below(8)] : nullptr;
            record.world_x = random.range(0, 8000);
            record.world_y = random.range(0, 1100);
            record.target_neuron_index = random.byte();
            record.lobe_activation = random.byte();
            record.visible = random.chance(2);
            record.stimulus_seen = random.chance(2);
            std::uint8_t* raw = native + kAttentionRecords + r * kAttentionStride;
            field<void*>(raw, 0) = native_for(record.target);
            raw[4] = record.target_neuron_index;
            raw[5] = record.lobe_activation;
            field<std::int32_t>(raw, 8) = record.world_x;
            field<std::int32_t>(raw, 12) = record.world_y;
            raw[16] = record.visible;
            raw[17] = record.stimulus_seen;
        }
        auto& goal = CreatureTestAccess::goal(*creature);
        goal.attention_record_index = -1;
        field<std::int32_t>(native, kGoalRecord) = -1;
        const std::uint8_t action = static_cast<std::uint8_t>(random.below(16));
        CreatureTestAccess::selected_action(*creature) = action;
        native[kSelectedAction] = action;

        std::string first;
        char buffer[256];
        for (int s = 0; s < 8 && first.empty(); ++s, ++sentences) {
            g_native_said.clear();
            g_lc_said.clear();
            g_overflow = false;
            g_native_rand = g_lc_rand = random.next();
            const bool asleep = random.chance(8);
            skeleton.sleep_indicator_active = asleep;
            native[kSleep] = asleep;
            const int motion = static_cast<int>(random.below(14)) - 4;
            skeleton.motion_link = motion >= 0 ? lc_all[motion % 10] : nullptr;
            field<void*>(native, kMotionLink) = native_for(skeleton.motion_link);
            const int who = static_cast<int>(random.below(3));  // hand, creature, object
            objects::Object* source = who == 0 ? hand : who == 1 ? lc_all[9] : lc_all[random.below(8)];
            // The sentence.
            std::string sentence;
            for (int w = 0, n = random.range(1, 4); w < n; ++w) {
                std::string word;
                switch (random.below(7)) {
                case 0: word = creature->learned_word_record(0x10).recognized_word; break;
                case 1: word = creature->learned_word_record(0x11 + random.below(0x27)).recognized_word; break;
                case 2: word = creature->learned_word_record(random.below(16)).recognized_word; break;
                case 3: word = creature->learned_word_record(0x48 + random.below(4)).recognized_word; break;
                case 4: word = creature->learned_word_record(random.below(kRecordCount)).response_word; break;
                default: {
                    static const char* kUnknown[] = {"zorp", "blik", "frumb", "stelt", "crax", "pib", "drongo"};
                    word = kUnknown[random.below(7)];
                    break;
                }
                }
                if (word.empty() || all_vowels(word.c_str())) word = "blik";
                if (random.chance(4)) word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
                if (!sentence.empty()) sentence += random.chance(8) ? "  " : " ";
                sentence += word;
            }
            if (random.chance(10)) sentence += " ";
            if (sentence.find("look") != std::string::npos) ++looks;
            char lc_words[128], native_words[128];
            std::snprintf(lc_words, sizeof lc_words, "%s", sentence.c_str());
            std::snprintf(native_words, sizeof native_words, "%s", sentence.c_str());
            creature->process_heard_words(source, lc_words, hosts, text, hosts, hosts);
            native_process(native, native_for(source), native_words);

            // ---- compare ----
            auto note = [&](const std::string& what) {
                if (first.empty()) {
                    std::snprintf(buffer, sizeof buffer, "sentence %d \"%s\" from %d%s: ", s, sentence.c_str(), who,
                                  asleep ? " asleep" : "");
                    first = buffer + what;
                }
            };
            if (g_overflow) note("a word overflowed a record");
            for (std::size_t k = 0; k < kRecordCount; ++k) {
                const auto& lc_record = creature->learned_word_record(k);
                const std::uint8_t* raw = native + kRecords + k * kRecordSize;
                if (std::strcmp(reinterpret_cast<const char*>(raw), lc_record.recognized_word) != 0 ||
                    std::strcmp(reinterpret_cast<const char*>(raw + 25), lc_record.response_word) != 0 ||
                    *reinterpret_cast<const std::uint32_t*>(raw + 0x34) != lc_record.reinforcement) {
                    std::snprintf(buffer, sizeof buffer, "word record %u: native %s/%s/%u lc %s/%s/%u",
                                  static_cast<unsigned>(k), reinterpret_cast<const char*>(raw),
                                  reinterpret_cast<const char*>(raw + 25), *reinterpret_cast<const std::uint32_t*>(raw + 0x34),
                                  lc_record.recognized_word, lc_record.response_word, lc_record.reinforcement);
                    note(buffer);
                }
            }
            for (std::size_t r = 0; r < records.size(); ++r) {
                const auto& record = records[r];
                const std::uint8_t* raw = native + kAttentionRecords + r * kAttentionStride;
                if (*reinterpret_cast<void* const*>(raw) != native_for(record.target) ||
                    raw[4] != record.target_neuron_index || raw[5] != record.lobe_activation ||
                    *reinterpret_cast<const std::int32_t*>(raw + 8) != record.world_x ||
                    *reinterpret_cast<const std::int32_t*>(raw + 12) != record.world_y ||
                    (raw[16] != 0) != record.visible || (raw[17] != 0) != record.stimulus_seen) {
                    std::snprintf(buffer, sizeof buffer, "attention record %u", static_cast<unsigned>(r));
                    note(buffer);
                }
            }
            for (int lobe : kLobes) {
                const std::uint32_t count = brain->lobe(lobe).neuron_count_value();
                for (std::uint32_t n = 0; n < count; ++n) {
                    const auto& neuron = brain->lobe(lobe).neuron(n);
                    const std::uint8_t* raw = neurons[lobe].data() + n * kNeuronSize;
                    if (raw[kFiring] != neuron.firing_strength || raw[kActivation] != neuron.activation) {
                        std::snprintf(buffer, sizeof buffer, "lobe %d neuron %u: native %u/%u lc %u/%u", lobe, n,
                                      raw[kFiring], raw[kActivation], neuron.firing_strength, neuron.activation);
                        note(buffer);
                    }
                }
            }
            for (int c = 0; c < 256; ++c) {
                if (native_bio[8 + 2 * c] != chemicals[c].concentration) {
                    std::snprintf(buffer, sizeof buffer, "chemical %d", c);
                    note(buffer);
                }
            }
            for (std::size_t r = 0; r < 40; ++r) {
                for (std::size_t c = 0; c < 16; ++c) {
                    if (field<std::int32_t>(native, kGoalMatrix + (r * 16 + c) * 4) != matrix[r][c]) note("goal matrix");
                }
            }
            if (field<std::int32_t>(native, kGoalRecord) != goal.attention_record_index ||
                field<std::int32_t>(native, kGoalVerb) != goal.action_lobe_neuron_index ||
                native[kGoalCommitment] != goal.commitment || native[kGoalCountdown] != goal.delivery_countdown) {
                std::snprintf(buffer, sizeof buffer, "goal state: native %d/%d/%u lc %d/%d/%u",
                              field<std::int32_t>(native, kGoalRecord), field<std::int32_t>(native, kGoalVerb),
                              native[kGoalCommitment], goal.attention_record_index, goal.action_lobe_neuron_index,
                              goal.commitment);
                note(buffer);
            }
            if (g_native_said != g_lc_said) {
                std::snprintf(buffer, sizeof buffer, "said: native %u [%s] lc %u [%s]",
                              static_cast<unsigned>(g_native_said.size()),
                              g_native_said.empty() ? "" : g_native_said[0].c_str(),
                              static_cast<unsigned>(g_lc_said.size()), g_lc_said.empty() ? "" : g_lc_said[0].c_str());
                note(buffer);
            }
            if (g_native_rand != g_lc_rand) note("rand state");
            for (std::size_t k = 0x10; k < 0x38; ++k) {
                if (creature->learned_word_record(k).reinforcement == 0x3c) ++learned;
            }
        }
        skeleton.motion_link = nullptr;
        skeleton.set_bounds_reference_object(nullptr);
        for (auto& r : records) r.target = nullptr;
        for (auto& c : contexts) c.target = nullptr;
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: %s\n", index, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
        creature.reset();
    }
    std::printf("covered: %ld sentences, %ld with look\n", sentences, looks);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
