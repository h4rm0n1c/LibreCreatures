#pragma once

#include <cstddef>
#include <cstdint>

namespace creatures1::creatures {

inline constexpr std::size_t kLearnedWordTextCapacity = 25;
inline constexpr std::size_t kLearnedWordPhonemeTableSize = 18;

struct LearnedWordRecord {
    char recognized_word[kLearnedWordTextCapacity]{};
    char response_word[kLearnedWordTextCapacity]{};
    std::uint8_t alignment_padding_before_reinforcement[2]{};
    std::uint32_t reinforcement = 0;
};

// The executable stores eight five-word banks.  The first four banks are
// selected for the primary classifier-sex partition and the last four for
// the other partition; each bank supplies the five learned-word classes.
struct VocabularyWordBank {
    const char* word_0 = nullptr;
    const char* word_1 = nullptr;
    const char* word_2 = nullptr;
    const char* word_3 = nullptr;
    const char* word_4 = nullptr;
};

extern const VocabularyWordBank kRandomizedVocabularyBanks[8];

struct LearnedWordPhonemeSubstitution {
    char source_fragment[4]{};
    char replacement_fragment[4]{};
};

// The executable contains this exact ordered table at 004663c0.  The order
// matters: the original applies the first matching fragment and stops.
extern const LearnedWordPhonemeSubstitution
    kLearnedWordPhonemeSubstitutions[kLearnedWordPhonemeTableSize];

// _mbsinc/_mbsstr are supplied by the selected Microsoft CRT.  C1 owns the
// normalization policy; the CRT owns code-page character traversal/search.
class MultibyteTextApi {
public:
    virtual ~MultibyteTextApi() = default;

    virtual const char* find_character(const char* text,
                                       unsigned char character) const = 0;
    virtual const char* find_substring(const char* text,
                                       const char* fragment) const = 0;
    virtual const char* next_character(const char* text) const = 0;

    // _mbsicmp is supplied by the selected Microsoft CRT adapter. C1 only
    // owns the learned-word reinforcement policy that consumes its result.
    virtual int compare_strings(const char* left, const char* right) const = 0;

    // These calls are supplied by the selected Microsoft CRT adapter.  The
    // C1 sources specify the destination capacities; they do not reimplement
    // the CRT's invalid-parameter/error handling.
    virtual void copy_string(char* destination,
                             std::size_t destination_capacity,
                             const char* source) const = 0;
    virtual void append_string(char* destination,
                               std::size_t destination_capacity,
                               const char* source) const = 0;
};

// Copies recognized_word to response_word, then applies at most one ordered
// substitution.  The record fields are the recovered 25-byte C1 buffers.
void normalize_learned_word_response(
    LearnedWordRecord& record,
    const LearnedWordPhonemeSubstitution* substitutions,
    std::size_t substitution_count,
    const MultibyteTextApi& text_api);

} // namespace creatures1::creatures
