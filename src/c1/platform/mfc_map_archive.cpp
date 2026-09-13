#include "mfc_map_archive.hpp"

#include <stdexcept>
#include <utility>

namespace creatures1::platform {

bool MfcMapDataArchive::is_loading() const {
    return stream_.loading();
}

display::Gallery* MfcMapDataArchive::read_gallery() {
    if (read_gallery_ == nullptr) {
        throw std::logic_error(
            "MFC map archive has no gallery reader binding");
    }
    return read_gallery_();
}

void MfcMapDataArchive::write_gallery(display::Gallery* gallery) {
    if (write_gallery_ == nullptr) {
        throw std::logic_error(
            "MFC map archive has no gallery writer binding");
    }
    write_gallery_(gallery);
}

std::uint8_t MfcMapDataArchive::read_byte() {
    return stream_.read_byte();
}

std::int32_t MfcMapDataArchive::read_int32() {
    return stream_.read_int32();
}

std::uint32_t MfcMapDataArchive::read_uint32() {
    return stream_.read_uint32();
}

void MfcMapDataArchive::write_byte(std::uint8_t value) {
    stream_.write_byte(value);
}

void MfcMapDataArchive::write_int32(std::int32_t value) {
    stream_.write_int32(value);
}

void MfcMapDataArchive::write_uint32(std::uint32_t value) {
    stream_.write_uint32(value);
}

} // namespace creatures1::platform
