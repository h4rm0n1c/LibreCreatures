#pragma once

#include <functional>

#include "mfc_object_archive.hpp"

#include "../archive/string_archive.hpp"
#include "../biochemistry/biochemistry.hpp"
#include "../brain/brain.hpp"
#include "../brain/instinct.hpp"
#include "../creatures/creature.hpp"

namespace creatures1::platform {

// These adapters deliberately contain no C1 policy. They translate the
// already-implemented semantic archive interfaces to the MFC dynamic-object
// stream owned by MfcObjectArchive.
class MfcBrainArchive final : public brain::BrainArchive {
public:
    explicit MfcBrainArchive(MfcObjectArchive& archive) : archive_(archive) {}

    bool is_loading() const override;
    std::uint32_t read_u32() override;
    std::uint8_t read_byte() override;
    void write_u32(std::uint32_t value) override;
    void write_byte(std::uint8_t value) override;

private:
    MfcObjectArchive& archive_;
};

class MfcInstinctArchive final : public brain::InstinctArchive {
public:
    explicit MfcInstinctArchive(MfcObjectArchive& archive)
        : archive_(archive) {}

    bool is_loading() const override;
    std::uint32_t read_u32() override;
    void write_u32(std::uint32_t value) override;

private:
    MfcObjectArchive& archive_;
};

class MfcBiochemistryArchive final
    : public biochemistry::BiochemistryArchive {
public:
    // The archive stores a Creature record under the Skeleton the registry
    // holds, so the owner reference needs the document's Object -> Creature
    // mapping; the constructing host supplies it.
    using CreatureResolver =
        std::function<creatures::Creature*(void*)>;
    using CreatureIdentity =
        std::function<const void*(const creatures::Creature*)>;

    MfcBiochemistryArchive(MfcObjectArchive& archive,
                           CreatureResolver resolve_creature,
                           CreatureIdentity creature_identity)
        : archive_(archive),
          resolve_creature_(std::move(resolve_creature)),
          creature_identity_(std::move(creature_identity)) {}

    bool is_loading() const override;
    creatures::Creature* read_creature_reference() override;
    void write_creature_reference(creatures::Creature* creature) override;
    std::uint32_t read_uint32() override;
    void write_uint32(std::uint32_t value) override;
    std::uint8_t read_byte() override;
    void write_byte(std::uint8_t value) override;

private:
    MfcObjectArchive& archive_;
    CreatureResolver resolve_creature_;
    CreatureIdentity creature_identity_;
};

// Biochemistry's serializer owns domain selection, while Brain and Creature
// own the native locus storage. This adapter keeps that recovered boundary
// explicit when a CBiochemistry record is read through the MFC table.
class MfcBiochemistryLocusHost final
    : public biochemistry::BiochemistryLocusHost {
public:
    explicit MfcBiochemistryLocusHost(biochemistry::Biochemistry& value)
        : value_(value) {}

    std::uint8_t* resolve_brain_locus(
        creatures::GenomeLocusKind kind, std::uint8_t tissue_index,
        std::uint8_t locus_index) override;
    std::uint8_t* resolve_creature_locus(
        creatures::GenomeLocusKind kind, std::uint8_t tissue_index,
        std::uint8_t locus_index) override;

private:
    biochemistry::Biochemistry& value_;
};

class MfcStringArchive final : public archive::StringArchive {
public:
    explicit MfcStringArchive(MfcObjectArchive& archive) : archive_(archive) {}

    bool is_loading() const override;
    std::string read_string() override;
    void write_string(std::string_view value) override;

private:
    MfcObjectArchive& archive_;
};

// CreatureArchive combines Object/Skeleton strings with the dynamic
// Instinct records. It is intentionally a forwarding composition rather than
// an MFC-derived object or a second archive implementation.
class MfcCreatureArchive final : public creatures::CreatureArchive {
public:
    explicit MfcCreatureArchive(MfcObjectArchive& archive) : archive_(archive) {}

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
    std::uint32_t read_script_classifier() override;
    std::string read_script_text() override;
    std::string read_string() override;
    void write_string(std::string_view value) override;
    void read_instinct_reference(brain::Instinct& destination) override;
    void write_instinct_reference(const brain::Instinct& instinct) override;

private:
    MfcObjectArchive& archive_;
};

} // namespace creatures1::platform
