// Function-level equivalence: a creature's learned words of CE 10.3 against
// creatures::Creature.
//
//   InitializeLearnedWordRecords @00409e80   initialize_learned_word_records
//   InitializeDefaultVocabulary @0040a490    initialize_default_vocabulary
//   UpdateLearnedWordRecord @0040a0c0        update_learned_word_record
//   RegenerateLearnedWordResponse @0040a1f0  regenerate_learned_word_response
//   NormalizeLearnedWordResponse @0040a380   normalize_learned_word_response
//   SpeakDominantDrivePhrase @0040b160       speak_dominant_drive_phrase
//   SpeakActionPhrase @0040afa0              speak_action_phrase
//
// The native creature is a Creature-shaped block with the 80 learned-word
// records at +0xb64 (56 bytes each: recognized word, response word,
// reinforcement at +0x34), the classifier at +4 and the sleep flag at
// +0x11f.  rand, _mbsstr, _mbsinc, _mbschr, _mbsicmp, strcpy_s and
// strcat_s are bound to plain (single-byte) implementations, and LC's text
// API is the same.  Speak (0040b5f0), SpeakTextWithVoice (0040b560, delay
// 5) and QueueEventForCreaturesInSpeechRange (00422f90) are recorders;
// LC's speech hosts record the same.
//
// Each case: random records (words of 1 to 12 letters, real and made up,
// any reinforcement), a random classifier (either sex partition), then 24
// random operations: initialize the records, initialize the default
// vocabulary, hear a word (a record's own word, another record's word or a
// new word; from an object or a creature; awake or asleep), regenerate a
// response, normalize a response, say the dominant drive's phrase (random
// drive levels), say the action phrase (random action, with or without a
// motion link).  After each, all 80 records, the return
// value, the words spoken and the rand state must match.
//
// Not generated: words that would not fit the 25-byte record (native
// strcpy_s stops the program), and the response of a word of vowels only
// (row 80: native scans and writes past the word).

#include "native_image.hpp"

#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "creatures/learned_words.hpp"
#include "objects/object.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace creatures1::creatures {
// Declared a friend in creature.hpp.
class CreatureTestAccess {
public:
    static void update(Creature& c, char* word, std::size_t index, objects::Object* source,
                       const Creature::StimulusSourceHost& sources, const MultibyteTextApi& text,
                       Creature::CreatureSpeechHost& speech) {
        c.update_learned_word_record(word, index, source, sources, text, speech);
    }
    static int initialize(Creature& c, std::uint32_t classifier, const MultibyteTextApi& text) {
        return c.initialize_learned_word_records(classifier, text);
    }
    static void default_vocabulary(Creature& c) { c.initialize_default_vocabulary(); }
    static std::uint32_t& selected_action(Creature& c) { return c.selected_action_id_; }
    static void speak_drive(Creature& c, const MultibyteTextApi& text, Creature::CreatureSpeechPhraseHost& phrases,
                            Creature::CreatureSpeechHost& speech) {
        c.speak_dominant_drive_phrase(text, phrases, speech);
    }
    static void speak_action(Creature& c, const MultibyteTextApi& text, Creature::CreatureSpeechPhraseHost& phrases,
                             Creature::CreatureSpeechHost& speech) {
        c.speak_action_phrase(text, phrases, speech);
    }
};
}  // namespace creatures1::creatures

namespace {

using namespace creatures1;
using creatures::CreatureTestAccess;

constexpr std::uint32_t kNativeDebugLog = 0x00418000, kSpeak = 0x0040b5f0;
constexpr std::uint32_t kInitialize = 0x00409e80, kDefaultVocabulary = 0x0040a490, kUpdate = 0x0040a0c0,
                        kRegenerate = 0x0040a1f0, kNormalize = 0x0040a380;
constexpr std::uint32_t kDrivePhrase = 0x0040b160, kActionPhrase = 0x0040afa0;
constexpr std::size_t kBlockSize = 0x2d00, kClassifier = 0x4, kAsleep = 0x11f, kRecords = 0xb64,
                      kRecordSize = 0x38, kRecordCount = 80, kDrives = 0x2c56, kSelectedAction = 0x840,
                      kMotionLink = 0x7f0;

// ---- rand, one state per side ----
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

// ---- the CRT text functions, single-byte, on both sides ----
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

class TextApi final : public creatures::MultibyteTextApi {
public:
    const char* find_character(const char* t, unsigned char c) const override { return mbschr(t, c); }
    const char* find_substring(const char* t, const char* f) const override { return mbsstr(t, f); }
    const char* next_character(const char* t) const override { return mbsinc(t); }
    int compare_strings(const char* l, const char* r) const override { return mbsicmp(l, r); }
    void copy_string(char* d, std::size_t n, const char* s) const override { strcpy_s_impl(d, n, s); }
    void append_string(char* d, std::size_t n, const char* s) const override { strcat_s_impl(d, n, s); }
};

// ---- speech ----
std::vector<std::string> g_native_said, g_lc_said;
void __attribute__((thiscall)) native_speak(void*, const char* text) { g_native_said.push_back(text); }
// SpeakTextWithVoice @0040b560 (the voice; it reports the voice's delay)
// and QueueEventForCreaturesInSpeechRange @00422f90 (ECX source, EDX event,
// then argument, unused, delay; the caller pops).
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
class PhraseHost final : public creatures::Creature::CreatureSpeechPhraseHost {
public:
    objects::Object& object_for_creature(creatures::Creature& c) const override { return c.skeleton(); }
    std::int32_t attention_record_index(const objects::Object& o) const override {
        const std::uint32_t c = o.classifier_base();
        return static_cast<std::int32_t>(creatures::get_attention_record_index(
            {static_cast<creatures::AttentionObjectFamily>(c >> 24), static_cast<std::uint8_t>(c >> 16)}));
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
};
class SpeechHost final : public creatures::Creature::CreatureSpeechHost {
public:
    void speak(creatures::Creature&, std::string_view phrase) override { g_lc_said.emplace_back(phrase); }
};
class SourceHost final : public creatures::Creature::StimulusSourceHost {
public:
    creatures::AttentionClassifier classify(const objects::Object& o) const override {
        const std::uint32_t c = o.classifier_base();
        return {static_cast<creatures::AttentionObjectFamily>(c >> 24), static_cast<std::uint8_t>(c >> 16)};
    }
    int sound_source_x(const objects::Object&) const override { return 0; }
    int sound_source_y(const objects::Object&) const override { return 0; }
    bool is_this_creature(const objects::Object&, const creatures::Creature&) const override { return false; }
};

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint32_t below(std::uint32_t n) { return n == 0 ? 0 : next() % n; }
    bool chance(std::uint32_t one_in) { return below(one_in) == 0; }
};

const char* kRealWords[] = {"food", "drink", "school", "strength", "apple", "carrot", "Norn", "Grendel",
                            "push", "pull", "stop", "orange", "phone", "train", "xyz", "b",
                            "Ettin", "lemon", "ball", "music", "egg", "honey", "spring", "thrush"};
// A word of vowels only makes the native response scan run past its end
// (row 80, a guard in LC); such words are not generated.
bool all_vowels(const char* w) {
    for (; *w != '\0'; ++w) {
        if (std::strchr("aeiouAEIOU", *w) == nullptr) return false;
    }
    return true;
}
std::string random_word(Random& random) {
    if (random.chance(2)) return kRealWords[random.below(sizeof kRealWords / sizeof *kRealWords)];
    static const char kLetters[] = "aeioubcdfghjklmnpqrstvwxyzAEIOUBCDNPRST";
    std::string w;
    const std::uint32_t n = 1 + random.below(12);
    for (std::uint32_t i = 0; i < n; ++i) w.push_back(kLetters[random.below(sizeof kLetters - 1)]);
    if (all_vowels(w.c_str())) w.push_back('b');
    return w;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x30fd5u;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-cases" && i + 1 < argc) cases = std::atol(argv[++i]);
        else if (arg == "-seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 0);
    }
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    const struct { const char* name; void* fn; } imports[] = {
        {"!rand", reinterpret_cast<void*>(&native_rand)},
        {"!_mbsstr", reinterpret_cast<void*>(&mbsstr)},
        {"!_mbsinc", reinterpret_cast<void*>(&mbsinc)},
        {"!_mbschr", reinterpret_cast<void*>(&mbschr)},
        {"!_mbsicmp", reinterpret_cast<void*>(&mbsicmp)},
        {"!strcpy_s", reinterpret_cast<void*>(&strcpy_s_impl)},
        {"!strcat_s", reinterpret_cast<void*>(&strcat_s_impl)},
    };
    for (const auto& import : imports) {
        if (!image.bind_import(import.name, import.fn)) {
            std::fprintf(stderr, "the exe does not import %s\n", import.name + 1);
            return 2;
        }
    }
    image.make_return(kNativeDebugLog);
    image.redirect(kSpeak, reinterpret_cast<const void*>(&native_speak));
    image.redirect(0x0040b560, reinterpret_cast<const void*>(&native_speak_with_voice));
    image.redirect(0x00422f90, reinterpret_cast<const void*>(&native_speech_event_thunk));
    using Call0 = int(__attribute__((thiscall)) *)(void*);
    using Call1 = int(__attribute__((thiscall)) *)(void*, std::uintptr_t);
    using Call3 = int(__attribute__((thiscall)) *)(void*, std::uintptr_t, std::uintptr_t, std::uintptr_t);

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    const TextApi text;
    SpeechHost speech;
    SourceHost sources;
    PhraseHost phrases;
    long differing = 0, operations = 0, spoken = 0, relearned = 0;
    const char* names[] = {"initialize records", "default vocabulary", "hear a word", "hear a word", "hear a word",
                           "regenerate", "normalize", "drive phrase", "action phrase"};
    for (long index = 0; index < cases; ++index) {
        auto creature = std::make_unique<creatures::Creature>();
        std::vector<std::uint8_t> native(kBlockSize, 0);
        const std::uint32_t classifier = random.chance(2) ? 0x04010000u : 0x04020000u | (random.below(256) << 8);
        creature->skeleton().set_classifier_base(classifier);
        *reinterpret_cast<std::uint32_t*>(native.data() + kClassifier) = classifier;
        for (std::size_t r = 0; r < kRecordCount; ++r) {
            auto& record = creature->learned_word_record(r);
            const std::string a = random_word(random), b = random.chance(3) ? a : random_word(random);
            std::memset(&record, 0, sizeof record);
            std::memcpy(record.recognized_word, a.c_str(), a.size() + 1);
            std::memcpy(record.response_word, b.c_str(), b.size() + 1);
            record.reinforcement = random.below(256);
            std::memcpy(native.data() + kRecords + r * kRecordSize, &record, kRecordSize);
        }
        objects::Object plain, creature_source;
        plain.set_classifier_base(0x02050000u);
        creature_source.set_classifier_base(0x04010000u);
        std::vector<std::uint8_t> native_plain(0x40, 0), native_creature_source(0x40, 0);
        *reinterpret_cast<std::uint32_t*>(native_plain.data() + 4) = 0x02050000u;
        *reinterpret_cast<std::uint32_t*>(native_creature_source.data() + 4) = 0x04010000u;

        std::string first;
        char text_buffer[256];
        for (int op = 0; op < 24 && first.empty(); ++op, ++operations) {
            g_native_said.clear();
            g_lc_said.clear();
            g_overflow = false;
            g_native_rand = g_lc_rand = random.next();
            const bool asleep = random.chance(8);
            creature->skeleton().sleep_indicator_active = asleep;
            native[kAsleep] = asleep;
            int which = static_cast<int>(random.below(9));
            const std::size_t r = random.below(kRecordCount);
            // Not generated: the response of a vowel-only word (row 80).
            if (which >= 2 && which <= 6 && all_vowels(creature->learned_word_record(r).recognized_word)) which = 1;
            int lc_result = 0, native_result = 0;
            switch (which) {
            case 0:
                lc_result = CreatureTestAccess::initialize(*creature, classifier, text);
                native_result = reinterpret_cast<Call0>(image.at<void*>(kInitialize))(native.data());
                break;
            case 1:
                CreatureTestAccess::default_vocabulary(*creature);
                reinterpret_cast<Call0>(image.at<void*>(kDefaultVocabulary))(native.data());
                break;
            case 2: case 3: case 4: {
                std::string word;
                if (which == 2) word = creature->learned_word_record(r).recognized_word;
                else if (which == 3) word = creature->learned_word_record(random.below(kRecordCount)).recognized_word;
                else word = random_word(random);
                if (all_vowels(word.c_str())) word.push_back('b');
                if (random.chance(3)) word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
                char lc_word[32], native_word[32];
                std::snprintf(lc_word, sizeof lc_word, "%s", word.c_str());
                std::snprintf(native_word, sizeof native_word, "%s", word.c_str());
                const bool from_creature = random.chance(2);
                CreatureTestAccess::update(*creature, lc_word, r, from_creature ? &creature_source : &plain, sources,
                                           text, speech);
                reinterpret_cast<Call3>(image.at<void*>(kUpdate))(
                    native.data(), reinterpret_cast<std::uintptr_t>(native_word), r,
                    reinterpret_cast<std::uintptr_t>(from_creature ? native_creature_source.data()
                                                                   : native_plain.data()));
                break;
            }
            case 5:
                creature->regenerate_learned_word_response(r, text);
                reinterpret_cast<Call1>(image.at<void*>(kRegenerate))(native.data(), r);
                break;
            case 7: {
                // Drives, death, and then the dominant drive's phrase.
                auto& drives = creature->control_state().goal_direction_drive_levels;
                for (std::size_t d = 0; d < 16; ++d) {
                    drives[d] = static_cast<std::uint8_t>(random.chance(3) ? random.below(0x40) : random.below(256));
                    native[kDrives + d] = drives[d];
                }
                CreatureTestAccess::speak_drive(*creature, text, phrases, speech);
                reinterpret_cast<Call0>(image.at<void*>(kDrivePhrase))(native.data());
                break;
            }
            case 8: {
                const std::uint8_t action = static_cast<std::uint8_t>(random.below(16));
                CreatureTestAccess::selected_action(*creature) = action;
                native[kSelectedAction] = action;
                const bool linked = random.chance(2);
                creature->skeleton().motion_link = linked ? &plain : nullptr;
                *reinterpret_cast<void**>(native.data() + kMotionLink) = linked ? native_plain.data() : nullptr;
                CreatureTestAccess::speak_action(*creature, text, phrases, speech);
                reinterpret_cast<Call0>(image.at<void*>(kActionPhrase))(native.data());
                creature->skeleton().motion_link = nullptr;
                break;
            }
            default:
                creatures::normalize_learned_word_response(creature->learned_word_record(r),
                                                           creatures::kLearnedWordPhonemeSubstitutions,
                                                           creatures::kLearnedWordPhonemeTableSize, text);
                reinterpret_cast<Call1>(image.at<void*>(kNormalize))(native.data(), r);
                break;
            }
            spoken += static_cast<long>(g_native_said.size());
            auto note = [&](const std::string& what) {
                if (first.empty()) {
                    std::snprintf(text_buffer, sizeof text_buffer, "op %d %s (record %u): ", op, names[which],
                                  static_cast<unsigned>(r));
                    first = text_buffer + what;
                }
            };
            if (g_overflow) note("a word overflowed a record (not generated on purpose)");
            if (lc_result != native_result) {
                std::snprintf(text_buffer, sizeof text_buffer, "result native %d lc %d", native_result, lc_result);
                note(text_buffer);
            }
            for (std::size_t k = 0; k < kRecordCount; ++k) {
                const auto& lc_record = creature->learned_word_record(k);
                const std::uint8_t* raw = native.data() + kRecords + k * kRecordSize;
                const auto native_reinforcement = *reinterpret_cast<const std::uint32_t*>(raw + 0x34);
                if (std::strcmp(reinterpret_cast<const char*>(raw), lc_record.recognized_word) != 0 ||
                    std::strcmp(reinterpret_cast<const char*>(raw + 25), lc_record.response_word) != 0 ||
                    native_reinforcement != lc_record.reinforcement) {
                    std::snprintf(text_buffer, sizeof text_buffer, "record %u: native %s/%s/%u lc %s/%s/%u",
                                  static_cast<unsigned>(k), reinterpret_cast<const char*>(raw),
                                  reinterpret_cast<const char*>(raw + 25), native_reinforcement,
                                  lc_record.recognized_word, lc_record.response_word, lc_record.reinforcement);
                    note(text_buffer);
                }
            }
            if (g_native_said != g_lc_said) {
                std::snprintf(text_buffer, sizeof text_buffer, "said: native [%s] lc [%s]",
                              g_native_said.empty() ? "" : g_native_said[0].c_str(),
                              g_lc_said.empty() ? "" : g_lc_said[0].c_str());
                note(text_buffer);
            }
            if (g_native_rand != g_lc_rand) note("rand state");
            if (which >= 2 && which <= 4 && creature->learned_word_record(r).reinforcement == 0x3c) ++relearned;
        }
        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: %s\n", index, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld operations, %ld words spoken, %ld relearned\n", operations, spoken, relearned);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
