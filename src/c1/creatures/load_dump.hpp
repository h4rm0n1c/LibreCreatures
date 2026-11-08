#pragma once

// Not native: the load equivalence test (tests/equivalence/run_world.sh
// --lc-dump).  After a world load, the original (world_roundtrip_native.exe)
// and LibreCreatures (the C1_WORLD_ROUNDTRIP hook) each write the fields in
// load_dump_fields.inc, one line per value, through these formatters, so two
// lines differ only when the two values do.
//
// Standard library only: the native harness includes this without any of
// LibreCreatures.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace creatures1::load_dump {

// The kind of a field, which also says how the original stores it.
struct U8 {};                       // one byte
struct Bool {};                     // one byte, compared as zero / not zero
struct U32 {};                      // four bytes, unsigned
struct I32 {};                      // four bytes, signed
struct Ptr {};                      // a pointer: only null / not null compares
struct Rect {};                     // four int32: min x, min y, max x, max y
struct Text { std::size_t length; };   // at most `length` bytes, NUL ends it
struct Bytes { std::size_t length; };  // exactly `length` bytes, in hex
struct CursorFrom { std::size_t base; };  // a pointer into the same object:
                                          // its distance from offset `base`

inline std::string unsigned_text(std::uint32_t value) { return std::to_string(value); }
inline std::string signed_text(std::int32_t value) { return std::to_string(value); }
inline std::string bool_text(bool value) { return value ? "1" : "0"; }
inline std::string pointer_text(bool non_null) { return non_null ? "set" : "null"; }

inline std::string rect_text(std::int32_t min_x, std::int32_t min_y,
                             std::int32_t max_x, std::int32_t max_y) {
    return std::to_string(min_x) + "," + std::to_string(min_y) + "," +
           std::to_string(max_x) + "," + std::to_string(max_y);
}

// Bytes outside 0x21..0x7e and '\' are written as \xHH.
inline std::string text_text(const char* text, std::size_t length) {
    std::string out;
    for (std::size_t i = 0; i < length && text[i] != '\0'; ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte < 0x21 || byte > 0x7e || byte == '\\') {
            char escaped[5];
            std::snprintf(escaped, sizeof escaped, "\\x%02x", byte);
            out += escaped;
        } else {
            out += static_cast<char>(byte);
        }
    }
    return out;
}

inline std::string bytes_text(const void* data, std::size_t length) {
    static const char digits[] = "0123456789abcdef";
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::string out;
    for (std::size_t i = 0; i < length; ++i) {
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 15];
    }
    return out;
}

// "creature <c> <field>" or "creature <c> <field>[<i>]", then the value.
inline void write_line(std::FILE* file, std::size_t creature, const char* field,
                       std::size_t count, std::size_t index, const std::string& value) {
    if (count > 1) {
        std::fprintf(file, "creature %u %s[%u] %s\n", static_cast<unsigned>(creature), field,
                     static_cast<unsigned>(index), value.c_str());
    } else {
        std::fprintf(file, "creature %u %s %s\n", static_cast<unsigned>(creature), field,
                     value.c_str());
    }
}

} // namespace creatures1::load_dump
