#pragma once

// The original's half of the load equivalence test's formatters
// (src/c1/creatures/load_dump.hpp): one field of an original object, by
// its offset, in the text LibreCreatures' dump writes for the same field.
// Standard library only.

#include "creatures/load_dump.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace equivalence {

namespace dump = creatures1::load_dump;

template <typename T>
T value_at(const std::uint8_t* at) {
    T value;
    std::memcpy(&value, at, sizeof value);
    return value;
}

std::string native_text(dump::U8, const std::uint8_t* creature, std::size_t at) {
    return dump::unsigned_text(creature[at]);
}
std::string native_text(dump::Bool, const std::uint8_t* creature, std::size_t at) {
    return dump::bool_text(creature[at] != 0);
}
std::string native_text(dump::U32, const std::uint8_t* creature, std::size_t at) {
    return dump::unsigned_text(value_at<std::uint32_t>(creature + at));
}
std::string native_text(dump::I32, const std::uint8_t* creature, std::size_t at) {
    return dump::signed_text(value_at<std::int32_t>(creature + at));
}
std::string native_text(dump::Ptr, const std::uint8_t* creature, std::size_t at) {
    return dump::pointer_text(value_at<std::uint32_t>(creature + at) != 0);
}
std::string native_text(dump::Rect, const std::uint8_t* creature, std::size_t at) {
    return dump::rect_text(value_at<std::int32_t>(creature + at), value_at<std::int32_t>(creature + at + 4),
                           value_at<std::int32_t>(creature + at + 8), value_at<std::int32_t>(creature + at + 12));
}
std::string native_text(dump::Text kind, const std::uint8_t* creature, std::size_t at) {
    return dump::text_text(reinterpret_cast<const char*>(creature + at), kind.length);
}
std::string native_text(dump::CStr, const std::uint8_t* creature, std::size_t at) {
    const char* text = value_at<const char*>(creature + at);
    return text == nullptr ? std::string() : dump::text_text(text, 0x1000);
}
std::string native_text(dump::Bytes kind, const std::uint8_t* creature, std::size_t at) {
    return dump::bytes_text(creature + at, kind.length);
}
std::string native_text(dump::CursorFrom kind, const std::uint8_t* creature, std::size_t at) {
    const std::uint32_t pointer = value_at<std::uint32_t>(creature + at);
    const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(creature + kind.base));
    return dump::unsigned_text(pointer - base);
}

} // namespace equivalence
