#pragma once

#include "mfc_archive.hpp"

#include "../objects/entity.hpp"
#include "../objects/object.hpp"

#include <functional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace creatures1::platform {

// The clean object model must not inherit from MFC's CObject merely to make
// CArchive::ReadObject compile.  The native runtime/class table and the clean
// registry are therefore joined by explicit callbacks at this boundary.
class MfcObjectArchive final : public objects::ObjectArchive,
                               public scripting::ScriptArchiveReader {
public:
    using ObjectReferenceReader = std::function<void*(std::string_view)>;
    using ObjectReferenceWriter =
        std::function<void(const void*, std::string_view)>;

    MfcObjectArchive(MfcArchiveStream& stream,
                     ObjectReferenceReader read_object_reference,
                     ObjectReferenceWriter write_object_reference)
        : stream_(stream),
          read_object_reference_(std::move(read_object_reference)),
          write_object_reference_(std::move(write_object_reference)) {}

    bool is_loading() const override;
    std::uint32_t read_uint32() override;
    std::uint8_t read_byte() override;
    std::int32_t read_int32() override;
    void write_uint32(std::uint32_t value) override;
    void write_byte(std::uint8_t value) override;
    void write_int32(std::int32_t value) override;
    void read_bytes(void* destination, std::size_t count) override;
    void write_bytes(const void* source, std::size_t count) override;
    void* read_object_reference(std::string_view runtime_class_name) override;
    void write_object_reference(const void* object,
                                std::string_view runtime_class_name) override;

    void write_script_count(std::uint32_t count) override;
    void write_classifier(scripting::ScriptClassifier classifier) override;
    void write_script_text(std::string_view text) override;
    std::int32_t read_script_count() override;
    scripting::ScriptClassifier read_classifier() override;
    std::uint32_t read_script_classifier() override;
    std::string read_script_text() override;

    scripting::ScriptArchiveReader* script_reader() override {
        return script_install_host_ == nullptr ? nullptr : this;
    }
    scripting::ScriptDefinitionInstallHost* script_install_host() override {
        return script_install_host_;
    }
    void set_script_install_host(scripting::ScriptDefinitionInstallHost* host) {
        script_install_host_ = host;
    }

private:
    void* read_reference(std::string_view runtime_class_name) const;
    void write_reference(const void* object,
                         std::string_view runtime_class_name) const;

    MfcArchiveStream& stream_;
    ObjectReferenceReader read_object_reference_;
    ObjectReferenceWriter write_object_reference_;
    scripting::ScriptDefinitionInstallHost* script_install_host_ = nullptr;
};

// Entity serialization has the same MFC reference boundary but a smaller
// source-level protocol. Keeping it separate prevents an ObjectArchive from
// being passed accidentally to Entity::serialize.
class MfcEntityArchive final : public objects::EntityArchive {
public:
    using ObjectReferenceReader = MfcObjectArchive::ObjectReferenceReader;
    using ObjectReferenceWriter = MfcObjectArchive::ObjectReferenceWriter;

    MfcEntityArchive(MfcArchiveStream& stream,
                     ObjectReferenceReader read_object_reference,
                     ObjectReferenceWriter write_object_reference)
        : stream_(stream),
          read_object_reference_(std::move(read_object_reference)),
          write_object_reference_(std::move(write_object_reference)) {}

    bool is_loading() const override;
    display::Gallery* read_gallery() override;
    void write_gallery(display::Gallery* gallery) override;
    std::uint8_t read_byte() override;
    std::int32_t read_int32() override;
    void write_byte(std::uint8_t value) override;
    void write_int32(std::int32_t value) override;
    void read_bytes(void* destination, std::size_t count) override;
    void write_bytes(const void* source, std::size_t count) override;
    void* read_object_reference(std::string_view runtime_class_name) override;
    void write_object_reference(const void* object,
                                std::string_view runtime_class_name) override;

private:
    void* read_reference(std::string_view runtime_class_name) const;
    void write_reference(const void* object,
                         std::string_view runtime_class_name) const;

    MfcArchiveStream& stream_;
    ObjectReferenceReader read_object_reference_;
    ObjectReferenceWriter write_object_reference_;
};

// CArchive::ReadObject/WriteObject use one index space for both runtime-class
// records and object records.  This table mirrors that framing without
// forcing the clean objects to inherit from MFC's CObject.  The callbacks are
// deliberately responsible for construction, destruction, and dispatch to
// the correct clean serializer; this class only owns archive identity and
// framing.
class MfcDynamicObjectTable final {
public:
    using NewObjectFactory = std::function<void*(
        std::string_view runtime_class_name, std::uint16_t schema)>;
    using NewObjectReader = std::function<void(
        void* object, std::string_view runtime_class_name,
        MfcObjectArchive& archive)>;
    using NewObjectWriter = std::function<void(
        const void* object, std::string_view runtime_class_name,
        MfcObjectArchive& archive)>;
    using RuntimeClassResolver = std::function<std::string(
        const void* object, std::string_view requested_class_name)>;
    using ClassSchemaResolver =
        std::function<std::uint16_t(std::string_view runtime_class_name)>;
    using ClassCompatibility = std::function<bool(
        std::string_view actual_class_name,
        std::string_view requested_class_name)>;

    MfcDynamicObjectTable(
        MfcArchiveStream& stream, NewObjectFactory create_new_object,
        NewObjectReader read_new_object,
        NewObjectWriter write_new_object,
        RuntimeClassResolver resolve_runtime_class,
        ClassSchemaResolver resolve_class_schema,
        ClassCompatibility class_compatibility = {})
        : stream_(stream),
          create_new_object_(std::move(create_new_object)),
          read_new_object_(std::move(read_new_object)),
          write_new_object_(std::move(write_new_object)),
          resolve_runtime_class_(std::move(resolve_runtime_class)),
          resolve_class_schema_(std::move(resolve_class_schema)),
          class_compatibility_(std::move(class_compatibility)) {}

    void* read_object_reference(std::string_view requested_class_name);
    void write_object_reference(const void* object,
                                std::string_view requested_class_name);

    // The script table objects' own scripts are installed into on load.
    void set_script_install_host(scripting::ScriptDefinitionInstallHost* host) {
        script_install_host_ = host;
    }

private:
    enum class EntryKind { runtime_class, object };

    struct Entry {
        EntryKind kind = EntryKind::runtime_class;
        std::string runtime_class_name;
        std::uint16_t schema = 0;
        void* object = nullptr;
    };

    MfcObjectArchive make_nested_archive();
    std::size_t append_class(std::string runtime_class_name,
                             std::uint16_t schema);
    std::size_t append_object(void* object, std::string runtime_class_name);
    void write_class(std::string_view runtime_class_name);

    // The most recently decoded class name, used only to say where a
    // desynchronised read sequence stopped.
    std::string last_class_name_;
    std::string read_new_class(std::uint16_t& schema);
    void validate_requested_class(std::string_view actual_class_name,
                                  std::string_view requested_class_name) const;
    void write_reference_tag(std::size_t index);
    std::size_t read_reference_tag(std::uint16_t first_word,
                                   bool& class_tag);
    const Entry& entry_at(std::size_t index) const;

    MfcArchiveStream& stream_;
    NewObjectFactory create_new_object_;
    NewObjectReader read_new_object_;
    NewObjectWriter write_new_object_;
    RuntimeClassResolver resolve_runtime_class_;
    ClassSchemaResolver resolve_class_schema_;
    ClassCompatibility class_compatibility_;
    std::vector<Entry> entries_{{}}; // MFC reserves index zero for NULL.
    std::unordered_map<std::string, std::size_t> class_indices_;
    std::unordered_map<const void*, std::size_t> object_indices_;
    scripting::ScriptDefinitionInstallHost* script_install_host_ = nullptr;
};

} // namespace creatures1::platform
