#include "creature_text_platform.hpp"

// The pinned UCRT exposes the secure-copy declarations only when this feature
// switch is enabled before its headers are parsed.  The implementation below
// is a boundary call into that CRT; it is not a C1 string-policy substitute.
#ifdef __STDC_WANT_SECURE_LIB__
#undef __STDC_WANT_SECURE_LIB__
#endif
#define __STDC_WANT_SECURE_LIB__ 1
#include <string.h>
#include <mbstring.h>

namespace creatures1::platform {

const char* c1_multibyte_find_character(const char* text,
                                         unsigned char character) {
    if (text == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<const char*>(
        _mbschr(reinterpret_cast<const unsigned char*>(text), character));
}

const char* c1_multibyte_find_substring(const char* text,
                                         const char* fragment) {
    if (text == nullptr || fragment == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<const char*>(
        _mbsstr(reinterpret_cast<const unsigned char*>(text),
                reinterpret_cast<const unsigned char*>(fragment)));
}

const char* c1_multibyte_next_character(const char* text) {
    if (text == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<const char*>(
        _mbsinc(reinterpret_cast<const unsigned char*>(text)));
}

int c1_multibyte_compare_strings(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) {
        return left == right ? 0 : (left == nullptr ? -1 : 1);
    }
    return _mbsicmp(reinterpret_cast<const unsigned char*>(left),
                    reinterpret_cast<const unsigned char*>(right));
}

void c1_multibyte_copy_string(char* destination,
                              std::size_t destination_capacity,
                              const char* source) {
    if (destination == nullptr || destination_capacity == 0) {
        return;
    }
    strncpy_s(destination, destination_capacity,
              source == nullptr ? "" : source, _TRUNCATE);
}

void c1_multibyte_append_string(char* destination,
                                std::size_t destination_capacity,
                                const char* source) {
    if (destination == nullptr || destination_capacity == 0) {
        return;
    }
    strncat_s(destination, destination_capacity,
              source == nullptr ? "" : source, _TRUNCATE);
}

} // namespace creatures1::platform
