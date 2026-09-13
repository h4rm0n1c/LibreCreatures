#include "learned_words.hpp"

#include <algorithm>
#include <cstring>

namespace creatures1::creatures {

namespace {

constexpr char kGoo[] = "goo";
constexpr char kDis[] = "dis";
constexpr char kGaa[] = "gaa";
constexpr char kBa[] = "ba";
constexpr char kEm[] = "em";
constexpr char kFoo[] = "foo";
constexpr char kDat[] = "dat";
constexpr char kOogh[] = "oogh";
constexpr char kCoo[] = "coo";
constexpr char kMe[] = "me";
constexpr char kFlib[] = "flib";
constexpr char kBab[] = "bab";
constexpr char kBibble[] = "bibble";
constexpr char kFlub[] = "flub";
constexpr char kEem[] = "eem";
constexpr char kDoo[] = "doo";
constexpr char kDaa[] = "daa";
constexpr char kBoop[] = "boop";
constexpr char kNa[] = "na";
constexpr char kBub[] = "bub";
constexpr char kGrah[] = "grah";
constexpr char kNarg[] = "narg";
constexpr char kGraaaah[] = "graaaah";
constexpr char kGrish[] = "grish";
constexpr char kGrndl[] = "grndl";
constexpr char kBeh[] = "beh";
constexpr char kBuh[] = "buh";
constexpr char kBleh[] = "bleh";
constexpr char kBih[] = "bih";
constexpr char kUrg[] = "urg";
constexpr char kArg[] = "arg";
constexpr char kOogle[] = "oogle";
constexpr char kNug[] = "nug";
constexpr char kUng[] = "ung";
constexpr char kUrrh[] = "urrh";
constexpr char kBilp[] = "bilp";
constexpr char kFlabber[] = "flabber";
constexpr char kFig[] = "fig";

} // namespace

const VocabularyWordBank kRandomizedVocabularyBanks[8] = {
    {kGoo, kDis, kGaa, kBa, kEm},
    {kFoo, kDat, kOogh, kCoo, kMe},
    {kFlib, kBab, kBibble, kFlub, kEem},
    {kDoo, kDaa, kBoop, kNa, kBub},
    {kGrah, kNarg, kGraaaah, kGrish, kGrndl},
    {kBeh, kBuh, kBleh, kBih, kBeh},
    {kUrg, kArg, kOogle, kNug, kUng},
    {kUrrh, kBilp, kNarg, kFlabber, kFig},
};

const LearnedWordPhonemeSubstitution
    kLearnedWordPhonemeSubstitutions[kLearnedWordPhonemeTableSize] = {
        {{'p', 'h', '\0', '\0'}, {'f', '\0', '\0', '\0'}},
        {{'s', 's', '\0', '\0'}, {'s', '\0', '\0', '\0'}},
        {{'s', 'h', '\0', '\0'}, {'t', 'h', '\0', '\0'}},
        {{'s', '\0', '\0', '\0'}, {'t', 'h', '\0', '\0'}},
        {{'t', 'h', '\0', '\0'}, {'d', '\0', '\0', '\0'}},
        {{'j', '\0', '\0', '\0'}, {'d', '\0', '\0', '\0'}},
        {{'r', '\0', '\0', '\0'}, {'w', '\0', '\0', '\0'}},
        {{'i', 'o', 'n', '\0'}, {'u', 'n', '\0', '\0'}},
        {{'i', 'n', '\0', '\0'}, {'i', 'm', '\0', '\0'}},
        {{'n', 'n', '\0', '\0'}, {'n', '\0', '\0', '\0'}},
        {{'r', 'r', '\0', '\0'}, {'r', '\0', '\0', '\0'}},
        {{'e', 'a', '\0', '\0'}, {'e', 'e', '\0', '\0'}},
        {{'a', 'i', '\0', '\0'}, {'a', 'y', '\0', '\0'}},
        {{'v', '\0', '\0', '\0'}, {'w', '\0', '\0', '\0'}},
        {{'c', 'k', '\0', '\0'}, {'k', '\0', '\0', '\0'}},
        {{'n', 'g', '\0', '\0'}, {'n', 'k', '\0', '\0'}},
        {{'o', 'a', '\0', '\0'}, {'o', 'w', '\0', '\0'}},
        {{'o', 'r', '\0', '\0'}, {'a', 'w', '\0', '\0'}},
    };

namespace {

void copy_record_text(char (&destination)[kLearnedWordTextCapacity],
                      const char* source,
                      const MultibyteTextApi& text_api) {
    text_api.copy_string(destination, kLearnedWordTextCapacity, source);
}

} // namespace

void normalize_learned_word_response(
    LearnedWordRecord& record,
    const LearnedWordPhonemeSubstitution* substitutions,
    std::size_t substitution_count,
    const MultibyteTextApi& text_api) {
    copy_record_text(record.response_word, record.recognized_word, text_api);

    for (std::size_t index = 0; index < substitution_count; ++index) {
        const LearnedWordPhonemeSubstitution& substitution = substitutions[index];
        const char* match = text_api.find_substring(
            record.response_word, substitution.source_fragment);
        if (match == nullptr) {
            continue;
        }

        const char* response_start = record.response_word;
        const char* source_start = substitution.source_fragment;
        const char* source_end = source_start;
        while (*source_end != '\0') {
            const char* next = text_api.next_character(source_end);
            if (next <= source_end) {
                break;
            }
            source_end = next;
        }

        const std::size_t prefix_size = static_cast<std::size_t>(
            match - response_start);
        const std::size_t source_size = static_cast<std::size_t>(
            source_end - source_start);
        const char* suffix = match + source_size;
        const std::size_t response_size =
            std::strlen(record.response_word);
        const std::size_t suffix_size = response_size - prefix_size - source_size;

        char normalized[30]{};
        const std::size_t replacement_size =
            std::strlen(substitution.replacement_fragment);
        const std::size_t normalized_size =
            prefix_size + replacement_size + suffix_size;
        if (normalized_size >= sizeof(normalized)) {
            return;
        }

        std::memcpy(normalized, response_start, prefix_size);
        std::memcpy(normalized + prefix_size,
                    substitution.replacement_fragment,
                    replacement_size);
        std::memcpy(normalized + prefix_size + replacement_size,
                    suffix + source_size,
                    suffix_size + 1);
        copy_record_text(record.response_word, normalized, text_api);
        return;
    }
}

} // namespace creatures1::creatures
