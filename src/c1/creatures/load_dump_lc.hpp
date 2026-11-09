#pragma once

// Not native: LibreCreatures' half of the load equivalence test's
// formatters (load_dump.hpp): one value of LC's in the text the native
// harness writes for the same field.  Used by the game's dump
// (C1_WORLD_FIELD_DUMP) and by tests that compare against the field
// tables.

#include "load_dump.hpp"
#include "../world/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace creatures1::load_dump {

template <typename T>
std::string lc_text(U8, T value) {
    return unsigned_text(static_cast<std::uint32_t>(value) & 0xffu);
}
template <typename T>
std::string lc_text(Bool, T value) {
    return bool_text(static_cast<bool>(value));
}
template <typename T>
std::string lc_text(U32, T value) {
    return unsigned_text(static_cast<std::uint32_t>(value));
}
template <typename T>
std::string lc_text(I32, T value) {
    return signed_text(static_cast<std::int32_t>(value));
}
template <typename T>
std::string lc_text(Ptr, const T* value) {
    return pointer_text(value != nullptr);
}
std::string lc_text(Rect, const creatures1::world::WorldRect& rect) {
    return rect_text(rect.min_x, rect.min_y, rect.max_x, rect.max_y);
}
std::string lc_text(Text kind, const char* text) {
    return text_text(text, kind.length);
}
std::string lc_text(CStr, const std::string& text) {
    return text_text(text.c_str(), 0x1000);
}
std::string lc_text(Bytes kind, const void* data) {
    return bytes_text(data, kind.length);
}
std::string lc_text(CursorFrom, std::size_t cursor) {
    return unsigned_text(static_cast<std::uint32_t>(cursor));
}

} // namespace creatures1::load_dump
