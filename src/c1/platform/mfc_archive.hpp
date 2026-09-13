#pragma once

#include <afx.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace creatures1::platform {

// The MFC CArchive buffer and exception policy stay at the Windows boundary.
// Typed game archive interfaces compose this stream; they do not learn about
// CArchive's cursor, CString representation, or runtime-class table.
class MfcArchiveStream final {
public:
    explicit MfcArchiveStream(CArchive& archive) : archive_(archive) {}

    bool loading() const { return archive_.IsLoading() != FALSE; }

    std::uint8_t read_byte();
    std::uint16_t read_uint16();
    std::int32_t read_int32();
    std::uint32_t read_uint32();
    void write_byte(std::uint8_t value);
    void write_uint16(std::uint16_t value);
    void write_int32(std::int32_t value);
    void write_uint32(std::uint32_t value);

    void read_bytes(void* destination, std::size_t count);
    void write_bytes(const void* source, std::size_t count);

    std::string read_string();
    void write_string(std::string_view value);

    // Bytes consumed from (or emitted to) the archive so far. CArchive's own
    // buffer pointers are protected, so the count is kept here instead; every
    // read/write helper below advances it. Diagnostics only.
    std::size_t stream_position() const { return position_; }

    static std::size_t string_length_prefix_size(std::size_t length);

    CArchive& native_archive() { return archive_; }

private:
    CArchive& archive_;
    std::size_t position_ = 0;
};

} // namespace creatures1::platform
