#include "mfc_document_archives.hpp"

namespace creatures1::platform {

bool MfcFavouritePlaceArchive::is_loading() const { return stream_.loading(); }
std::string MfcFavouritePlaceArchive::read_string() {
    return stream_.read_string();
}
void MfcFavouritePlaceArchive::write_string(std::string_view value) {
    stream_.write_string(value);
}
std::int16_t MfcFavouritePlaceArchive::read_int16() {
    return static_cast<std::int16_t>(stream_.read_uint16());
}
void MfcFavouritePlaceArchive::write_int16(std::int16_t value) {
    stream_.write_uint16(static_cast<std::uint16_t>(value));
}

bool MfcToolbarArchive::loading() const { return stream_.loading(); }
std::int16_t MfcToolbarArchive::read_selector_count() {
    return static_cast<std::int16_t>(stream_.read_uint16());
}
void MfcToolbarArchive::write_selector_count(std::int16_t count) {
    stream_.write_uint16(static_cast<std::uint16_t>(count));
}
std::string MfcToolbarArchive::read_string() { return stream_.read_string(); }
void MfcToolbarArchive::write_string(std::string_view value) {
    stream_.write_string(value);
}

bool MfcScoreArchive::is_loading() const { return stream_.loading(); }
std::int32_t MfcScoreArchive::read_int32() { return stream_.read_int32(); }
void MfcScoreArchive::write_int32(std::int32_t value) {
    stream_.write_int32(value);
}

bool MfcEventBarArchive::is_loading() const { return stream_.loading(); }
std::int32_t MfcEventBarArchive::read_displayed_object_count() {
    return stream_.read_int32();
}
objects::Object* MfcEventBarArchive::read_object() {
    return static_cast<objects::Object*>(objects_.read_object_reference("Object"));
}
void MfcEventBarArchive::write_displayed_object_count(std::int32_t count) {
    stream_.write_int32(count);
}
void MfcEventBarArchive::write_object(objects::Object* object) {
    objects_.write_object_reference(object, "Object");
}
std::int32_t MfcEventBarArchive::read_legacy_state_word() {
    return stream_.read_int32();
}
void MfcEventBarArchive::write_legacy_state_word(std::int32_t value) {
    stream_.write_int32(value);
}

bool MfcMacroArchive::loading() const { return stream_.loading(); }
std::uint32_t MfcMacroArchive::read_uint32() { return stream_.read_uint32(); }
std::int32_t MfcMacroArchive::read_int32() { return stream_.read_int32(); }
std::string MfcMacroArchive::read_string() { return stream_.read_string(); }
void MfcMacroArchive::write_uint32(std::uint32_t value) {
    stream_.write_uint32(value);
}
void MfcMacroArchive::write_int32(std::int32_t value) {
    stream_.write_int32(value);
}
void MfcMacroArchive::write_string(std::string_view value) {
    stream_.write_string(value);
}
objects::Object* MfcMacroArchive::read_object() {
    return static_cast<objects::Object*>(objects_.read_object_reference("Object"));
}
void MfcMacroArchive::write_object(objects::Object* object) {
    objects_.write_object_reference(object, "Object");
}

} // namespace creatures1::platform
