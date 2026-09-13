#include "mfc_creature_archives.hpp"

#include <stdexcept>

namespace creatures1::platform {

bool MfcBrainArchive::is_loading() const { return archive_.is_loading(); }
std::uint32_t MfcBrainArchive::read_u32() { return archive_.read_uint32(); }
std::uint8_t MfcBrainArchive::read_byte() { return archive_.read_byte(); }
void MfcBrainArchive::write_u32(std::uint32_t value) {
    archive_.write_uint32(value);
}
void MfcBrainArchive::write_byte(std::uint8_t value) {
    archive_.write_byte(value);
}

bool MfcInstinctArchive::is_loading() const { return archive_.is_loading(); }
std::uint32_t MfcInstinctArchive::read_u32() {
    return archive_.read_uint32();
}
void MfcInstinctArchive::write_u32(std::uint32_t value) {
    archive_.write_uint32(value);
}

bool MfcBiochemistryArchive::is_loading() const {
    return archive_.is_loading();
}
creatures::Creature* MfcBiochemistryArchive::read_creature_reference() {
    void* owner = archive_.read_object_reference("Creature");
    return resolve_creature_ ? resolve_creature_(owner)
                             : static_cast<creatures::Creature*>(owner);
}
void MfcBiochemistryArchive::write_creature_reference(
    creatures::Creature* creature) {
    archive_.write_object_reference(
        creature_identity_ ? creature_identity_(creature)
                           : static_cast<const void*>(creature),
        "Creature");
}
std::uint32_t MfcBiochemistryArchive::read_uint32() {
    return archive_.read_uint32();
}
void MfcBiochemistryArchive::write_uint32(std::uint32_t value) {
    archive_.write_uint32(value);
}
std::uint8_t MfcBiochemistryArchive::read_byte() {
    return archive_.read_byte();
}
void MfcBiochemistryArchive::write_byte(std::uint8_t value) {
    archive_.write_byte(value);
}

std::uint8_t* MfcBiochemistryLocusHost::resolve_brain_locus(
    creatures::GenomeLocusKind kind, std::uint8_t tissue_index,
    std::uint8_t locus_index) {
    creatures::Creature* const owner = value_.owner();
    if (owner == nullptr || owner->brain() == nullptr) {
        throw std::logic_error(
            "C1 biochemistry archive has no owning Brain for locus resolution");
    }
    return owner->brain()->resolve_genome_locus(
        static_cast<brain::GenomeLocusKind>(kind), tissue_index, locus_index);
}

std::uint8_t* MfcBiochemistryLocusHost::resolve_creature_locus(
    creatures::GenomeLocusKind kind, std::uint8_t tissue_index,
    std::uint8_t locus_index) {
    creatures::Creature* const owner = value_.owner();
    if (owner == nullptr) {
        throw std::logic_error(
            "C1 biochemistry archive has no owning Creature for locus resolution");
    }
    return owner->resolve_genome_locus(kind, tissue_index, locus_index);
}

bool MfcStringArchive::is_loading() const { return archive_.is_loading(); }
std::string MfcStringArchive::read_string() {
    return archive_.read_script_text();
}
void MfcStringArchive::write_string(std::string_view value) {
    archive_.write_script_text(value);
}

bool MfcCreatureArchive::is_loading() const { return archive_.is_loading(); }
std::uint32_t MfcCreatureArchive::read_uint32() {
    return archive_.read_uint32();
}
std::uint8_t MfcCreatureArchive::read_byte() { return archive_.read_byte(); }
std::int32_t MfcCreatureArchive::read_int32() {
    return archive_.read_int32();
}
void MfcCreatureArchive::write_uint32(std::uint32_t value) {
    archive_.write_uint32(value);
}
void MfcCreatureArchive::write_byte(std::uint8_t value) {
    archive_.write_byte(value);
}
void MfcCreatureArchive::write_int32(std::int32_t value) {
    archive_.write_int32(value);
}
void MfcCreatureArchive::read_bytes(void* destination, std::size_t count) {
    archive_.read_bytes(destination, count);
}
void MfcCreatureArchive::write_bytes(const void* source, std::size_t count) {
    archive_.write_bytes(source, count);
}
void* MfcCreatureArchive::read_object_reference(
    std::string_view runtime_class_name) {
    return archive_.read_object_reference(runtime_class_name);
}
void MfcCreatureArchive::write_object_reference(
    const void* object, std::string_view runtime_class_name) {
    archive_.write_object_reference(object, runtime_class_name);
}
void MfcCreatureArchive::write_script_count(std::uint32_t count) {
    archive_.write_script_count(count);
}
void MfcCreatureArchive::write_classifier(
    scripting::ScriptClassifier classifier) {
    archive_.write_classifier(classifier);
}
void MfcCreatureArchive::write_script_text(std::string_view text) {
    archive_.write_script_text(text);
}
std::int32_t MfcCreatureArchive::read_script_count() {
    return archive_.read_script_count();
}
std::uint32_t MfcCreatureArchive::read_script_classifier() {
    return archive_.read_script_classifier();
}
std::string MfcCreatureArchive::read_script_text() {
    return archive_.read_script_text();
}
std::string MfcCreatureArchive::read_string() {
    return archive_.read_script_text();
}
void MfcCreatureArchive::write_string(std::string_view value) {
    archive_.write_script_text(value);
}
void MfcCreatureArchive::read_instinct_reference(
    brain::Instinct& destination) {
    auto* object = static_cast<brain::Instinct*>(
        archive_.read_object_reference("CInstinct"));
    if (object != nullptr) {
        destination = *object;
    } else {
        destination = {};
    }
}
void MfcCreatureArchive::write_instinct_reference(
    const brain::Instinct& instinct) {
    archive_.write_object_reference(&instinct, "CInstinct");
}

} // namespace creatures1::platform
