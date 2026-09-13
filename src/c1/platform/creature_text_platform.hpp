#pragma once

#include <cstddef>

namespace creatures1::platform {

const char* c1_multibyte_find_character(const char* text,
                                         unsigned char character);
const char* c1_multibyte_find_substring(const char* text,
                                         const char* fragment);
const char* c1_multibyte_next_character(const char* text);
int c1_multibyte_compare_strings(const char* left, const char* right);
void c1_multibyte_copy_string(char* destination,
                              std::size_t destination_capacity,
                              const char* source);
void c1_multibyte_append_string(char* destination,
                                std::size_t destination_capacity,
                                const char* source);

} // namespace creatures1::platform
