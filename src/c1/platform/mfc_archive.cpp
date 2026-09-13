#include "mfc_archive.hpp"

#include <limits>

namespace creatures1::platform {

namespace {

template <typename Value>
Value read_value(CArchive& archive) {
    Value value{};
    archive.Read(&value, static_cast<UINT>(sizeof(value)));
    return value;
}

template <typename Value>
void write_value(CArchive& archive, Value value) {
    archive.Write(&value, static_cast<UINT>(sizeof(value)));
}

UINT archive_count(std::size_t count) {
    if (count > (std::numeric_limits<UINT>::max)()) {
        AfxThrowArchiveException(CArchiveException::badIndex);
    }
    return static_cast<UINT>(count);
}

} // namespace

std::uint8_t MfcArchiveStream::read_byte() {
    position_ += sizeof(std::uint8_t);
    return read_value<std::uint8_t>(archive_);
}

std::uint16_t MfcArchiveStream::read_uint16() {
    position_ += sizeof(std::uint16_t);
    return read_value<std::uint16_t>(archive_);
}

std::int32_t MfcArchiveStream::read_int32() {
    position_ += sizeof(std::int32_t);
    return read_value<std::int32_t>(archive_);
}

std::uint32_t MfcArchiveStream::read_uint32() {
    position_ += sizeof(std::uint32_t);
    return read_value<std::uint32_t>(archive_);
}

void MfcArchiveStream::write_byte(std::uint8_t value) {
    position_ += sizeof(value);
    write_value(archive_, value);
}

void MfcArchiveStream::write_uint16(std::uint16_t value) {
    position_ += sizeof(value);
    write_value(archive_, value);
}

void MfcArchiveStream::write_int32(std::int32_t value) {
    position_ += sizeof(value);
    write_value(archive_, value);
}

void MfcArchiveStream::write_uint32(std::uint32_t value) {
    position_ += sizeof(value);
    write_value(archive_, value);
}

void MfcArchiveStream::read_bytes(void* destination, std::size_t count) {
    position_ += count;
    archive_.Read(destination, archive_count(count));
}

void MfcArchiveStream::write_bytes(const void* source, std::size_t count) {
    position_ += count;
    archive_.Write(source, archive_count(count));
}

// AfxReadStringLength spends one byte on lengths below 255, three on lengths
// below 65535, and seven beyond that -- enough to keep the byte counter honest
// without duplicating MFC's reader.
std::size_t MfcArchiveStream::string_length_prefix_size(std::size_t length) {
    if (length < 0xFF) {
        return 1;
    }
    return length < 0xFFFF ? 3 : 7;
}

std::string MfcArchiveStream::read_string() {
    CStringA value;
    archive_ >> value;
    const std::size_t length = static_cast<std::size_t>(value.GetLength());
    position_ += string_length_prefix_size(length) + length;
    return std::string(value.GetString(), length);
}

void MfcArchiveStream::write_string(std::string_view value) {
    const CStringA text(value.data(), static_cast<int>(value.size()));
    position_ += string_length_prefix_size(value.size()) + value.size();
    archive_ << text;
}

} // namespace creatures1::platform
