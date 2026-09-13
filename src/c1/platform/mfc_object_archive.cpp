#include "mfc_object_archive.hpp"

#include <limits>
#include <stdexcept>

namespace creatures1::platform {
namespace {

std::uint32_t pack_classifier(scripting::ScriptClassifier classifier) {
    return static_cast<std::uint32_t>(classifier.event) |
           (static_cast<std::uint32_t>(classifier.species) << 8) |
           (static_cast<std::uint32_t>(classifier.genus) << 16) |
           (static_cast<std::uint32_t>(classifier.family) << 24);
}

scripting::ScriptClassifier unpack_classifier(std::uint32_t packed) {
    return scripting::ScriptClassifier{
        static_cast<scripting::ScriptEvent>(packed & 0xffU),
        static_cast<std::uint8_t>((packed >> 8) & 0xffU),
        static_cast<std::uint8_t>((packed >> 16) & 0xffU),
        static_cast<std::uint8_t>((packed >> 24) & 0xffU),
    };
}

} // namespace

namespace {

constexpr std::uint16_t kNullTag = 0;
constexpr std::uint16_t kNewClassTag = 0xffff;
constexpr std::uint16_t kClassTag = 0x8000;
constexpr std::uint16_t kBigObjectTag = 0x7fff;
constexpr std::uint32_t kBigClassTag = 0x80000000U;

} // namespace

bool MfcObjectArchive::is_loading() const {
    return stream_.loading();
}

std::uint32_t MfcObjectArchive::read_uint32() {
    return stream_.read_uint32();
}

std::uint8_t MfcObjectArchive::read_byte() {
    return stream_.read_byte();
}

std::int32_t MfcObjectArchive::read_int32() {
    return stream_.read_int32();
}

void MfcObjectArchive::write_uint32(std::uint32_t value) {
    stream_.write_uint32(value);
}

void MfcObjectArchive::write_byte(std::uint8_t value) {
    stream_.write_byte(value);
}

void MfcObjectArchive::write_int32(std::int32_t value) {
    stream_.write_int32(value);
}

void MfcObjectArchive::read_bytes(void* destination, std::size_t count) {
    stream_.read_bytes(destination, count);
}

void MfcObjectArchive::write_bytes(const void* source, std::size_t count) {
    stream_.write_bytes(source, count);
}

void* MfcObjectArchive::read_reference(
    std::string_view runtime_class_name) const {
    if (!read_object_reference_) {
        throw std::logic_error(
            "C1 MFC object archive has no read-reference runtime binding");
    }
    return read_object_reference_(runtime_class_name);
}

void MfcObjectArchive::write_reference(
    const void* object, std::string_view runtime_class_name) const {
    if (!write_object_reference_) {
        throw std::logic_error(
            "C1 MFC object archive has no write-reference runtime binding");
    }
    write_object_reference_(object, runtime_class_name);
}

void* MfcObjectArchive::read_object_reference(
    std::string_view runtime_class_name) {
    return read_reference(runtime_class_name);
}

void MfcObjectArchive::write_object_reference(
    const void* object, std::string_view runtime_class_name) {
    write_reference(object, runtime_class_name);
}

void MfcObjectArchive::write_script_count(std::uint32_t count) {
    stream_.write_uint32(count);
}

void MfcObjectArchive::write_classifier(
    scripting::ScriptClassifier classifier) {
    stream_.write_uint32(pack_classifier(classifier));
}

void MfcObjectArchive::write_script_text(std::string_view text) {
    stream_.write_string(text);
}

std::int32_t MfcObjectArchive::read_script_count() {
    return stream_.read_int32();
}

scripting::ScriptClassifier MfcObjectArchive::read_classifier() {
    return unpack_classifier(stream_.read_uint32());
}

std::uint32_t MfcObjectArchive::read_script_classifier() {
    return pack_classifier(read_classifier());
}

std::string MfcObjectArchive::read_script_text() {
    return stream_.read_string();
}

bool MfcEntityArchive::is_loading() const {
    return stream_.loading();
}

display::Gallery* MfcEntityArchive::read_gallery() {
    return static_cast<display::Gallery*>(read_reference("CGallery"));
}

void MfcEntityArchive::write_gallery(display::Gallery* gallery) {
    write_reference(gallery, "CGallery");
}

std::uint8_t MfcEntityArchive::read_byte() {
    return stream_.read_byte();
}

std::int32_t MfcEntityArchive::read_int32() {
    return stream_.read_int32();
}

void MfcEntityArchive::write_byte(std::uint8_t value) {
    stream_.write_byte(value);
}

void MfcEntityArchive::write_int32(std::int32_t value) {
    stream_.write_int32(value);
}

void MfcEntityArchive::read_bytes(void* destination, std::size_t count) {
    stream_.read_bytes(destination, count);
}

void MfcEntityArchive::write_bytes(const void* source, std::size_t count) {
    stream_.write_bytes(source, count);
}

void* MfcEntityArchive::read_reference(
    std::string_view runtime_class_name) const {
    if (!read_object_reference_) {
        throw std::logic_error(
            "C1 MFC entity archive has no read-reference runtime binding");
    }
    return read_object_reference_(runtime_class_name);
}

void MfcEntityArchive::write_reference(
    const void* object, std::string_view runtime_class_name) const {
    if (!write_object_reference_) {
        throw std::logic_error(
            "C1 MFC entity archive has no write-reference runtime binding");
    }
    write_object_reference_(object, runtime_class_name);
}

void* MfcEntityArchive::read_object_reference(
    std::string_view runtime_class_name) {
    return read_reference(runtime_class_name);
}

void MfcEntityArchive::write_object_reference(
    const void* object, std::string_view runtime_class_name) {
    write_reference(object, runtime_class_name);
}

MfcObjectArchive MfcDynamicObjectTable::make_nested_archive() {
    return MfcObjectArchive(
        stream_,
        [this](std::string_view runtime_class_name) {
            return read_object_reference(runtime_class_name);
        },
        [this](const void* object, std::string_view runtime_class_name) {
            write_object_reference(object, runtime_class_name);
        });
}

std::size_t MfcDynamicObjectTable::append_class(
    std::string runtime_class_name, std::uint16_t schema) {
    const std::size_t index = entries_.size();
    last_class_name_ = runtime_class_name;
    class_indices_.emplace(runtime_class_name, index);
    entries_.push_back({EntryKind::runtime_class, std::move(runtime_class_name),
                        schema, nullptr});
    return index;
}

std::size_t MfcDynamicObjectTable::append_object(
    void* object, std::string runtime_class_name) {
    if (object == nullptr) {
        throw std::logic_error(
            "C1 MFC object table cannot register a null object");
    }
    const std::size_t index = entries_.size();
    object_indices_.emplace(object, index);
    entries_.push_back({EntryKind::object, std::move(runtime_class_name), 0,
                        object});
    return index;
}

void MfcDynamicObjectTable::write_reference_tag(std::size_t index) {
    if (index < kBigObjectTag) {
        stream_.write_uint16(static_cast<std::uint16_t>(index));
        return;
    }
    stream_.write_uint16(kBigObjectTag);
    stream_.write_uint32(static_cast<std::uint32_t>(index));
}

std::size_t MfcDynamicObjectTable::read_reference_tag(
    std::uint16_t first_word, bool& class_tag) {
    class_tag = false;
    if (first_word == kBigObjectTag) {
        const std::uint32_t tag = stream_.read_uint32();
        class_tag = (tag & kBigClassTag) != 0;
        return static_cast<std::size_t>(tag & ~kBigClassTag);
    }
    if (first_word == kNewClassTag) {
        class_tag = true;
        return 0;
    }
    class_tag = (first_word & kClassTag) != 0;
    return static_cast<std::size_t>(first_word & ~kClassTag);
}

const MfcDynamicObjectTable::Entry& MfcDynamicObjectTable::entry_at(
    std::size_t index) const {
    if (index == 0 || index >= entries_.size()) {
        // Name the index and the table size: a bad reference here means the
        // read sequence has desynchronised from the file, and the two numbers
        // say how far the load got before it did.
        throw std::logic_error(
            "C1 MFC object archive reference index is invalid: index " +
            std::to_string(index) + " of " + std::to_string(entries_.size()) +
            " entries, last class '" + last_class_name_ + "', at file offset " +
            std::to_string(stream_.stream_position()));
    }
    return entries_[index];
}

void MfcDynamicObjectTable::validate_requested_class(
    std::string_view actual_class_name,
    std::string_view requested_class_name) const {
    if (requested_class_name.empty() ||
        actual_class_name == requested_class_name) {
        return;
    }
    if (!class_compatibility_ ||
        !class_compatibility_(actual_class_name, requested_class_name)) {
        throw std::logic_error(
            std::string("C1 MFC object archive runtime class is not "
                        "compatible with request: file has '") +
            std::string(actual_class_name) + "', caller asked for '" +
            std::string(requested_class_name) + "', at file offset " +
            std::to_string(stream_.stream_position()));
    }
}

void MfcDynamicObjectTable::write_class(
    std::string_view runtime_class_name) {
    const auto found = class_indices_.find(std::string(runtime_class_name));
    if (found != class_indices_.end()) {
        const std::size_t index = found->second;
        if (index < kBigObjectTag) {
            stream_.write_uint16(static_cast<std::uint16_t>(kClassTag | index));
        } else {
            stream_.write_uint16(kBigObjectTag);
            stream_.write_uint32(kBigClassTag | static_cast<std::uint32_t>(index));
        }
        return;
    }

    if (!resolve_class_schema_) {
        throw std::logic_error(
            "C1 MFC object archive has no class-schema binding");
    }
    const std::uint16_t schema = resolve_class_schema_(runtime_class_name);
    if (runtime_class_name.size() >
        (std::numeric_limits<std::uint16_t>::max)()) {
        throw std::length_error("C1 MFC runtime class name is too long");
    }
    stream_.write_uint16(kNewClassTag);
    stream_.write_uint16(schema);
    stream_.write_uint16(static_cast<std::uint16_t>(runtime_class_name.size()));
    stream_.write_bytes(runtime_class_name.data(), runtime_class_name.size());
    append_class(std::string(runtime_class_name), schema);
}

std::string MfcDynamicObjectTable::read_new_class(
    std::uint16_t& schema) {
    schema = stream_.read_uint16();
    const std::uint16_t length = stream_.read_uint16();
    std::string name(length, '\0');
    if (length != 0) {
        stream_.read_bytes(name.data(), length);
    }
    append_class(name, schema);
    return name;
}

void* MfcDynamicObjectTable::read_object_reference(
    std::string_view requested_class_name) {
    const std::uint16_t first_word = stream_.read_uint16();
    if (first_word == kNullTag) {
        return nullptr;
    }

    bool class_tag = false;
    const std::size_t tag = read_reference_tag(first_word, class_tag);
    if (!class_tag) {
        const Entry& entry = entry_at(tag);
        if (entry.kind != EntryKind::object || entry.object == nullptr) {
            throw std::logic_error(
                "C1 MFC object archive reference does not name an object");
        }
        validate_requested_class(entry.runtime_class_name,
                                 requested_class_name);
        return entry.object;
    }

    std::string actual_class_name;
    std::uint16_t schema = 0;
    if (first_word == kNewClassTag) {
        actual_class_name = read_new_class(schema);
    } else {
        const Entry& class_entry = entry_at(tag);
        if (class_entry.kind != EntryKind::runtime_class) {
            throw std::logic_error(
                "C1 MFC class tag does not name a runtime class");
        }
        actual_class_name = class_entry.runtime_class_name;
        schema = class_entry.schema;
    }
    validate_requested_class(actual_class_name, requested_class_name);
    if (!create_new_object_) {
        throw std::logic_error(
            "C1 MFC object archive has no object-factory binding");
    }
    if (!read_new_object_) {
        throw std::logic_error(
            "C1 MFC object archive has no object-serialization binding");
    }

    void* object = create_new_object_(actual_class_name, schema);
    append_object(object, actual_class_name);
    MfcObjectArchive archive = make_nested_archive();
    read_new_object_(object, actual_class_name, archive);
    return object;
}

void MfcDynamicObjectTable::write_object_reference(
    const void* object, std::string_view requested_class_name) {
    if (object == nullptr) {
        stream_.write_uint16(kNullTag);
        return;
    }
    const auto found = object_indices_.find(object);
    if (found != object_indices_.end()) {
        validate_requested_class(entries_[found->second].runtime_class_name,
                                 requested_class_name);
        write_reference_tag(found->second);
        return;
    }
    if (!resolve_runtime_class_) {
        throw std::logic_error(
            "C1 MFC object archive has no runtime-class binding");
    }
    const std::string actual_class_name =
        resolve_runtime_class_(object, requested_class_name);
    validate_requested_class(actual_class_name, requested_class_name);
    write_class(actual_class_name);
    if (!write_new_object_) {
        throw std::logic_error(
            "C1 MFC object archive has no object-serialization binding");
    }

    // This insertion must precede the callback: self/cyclic references are
    // valid in MFC archives and resolve to this index during Serialize.
    append_object(const_cast<void*>(object), actual_class_name);
    MfcObjectArchive archive = make_nested_archive();
    write_new_object_(object, actual_class_name, archive);
}

} // namespace creatures1::platform
