#include "egg.hpp"

#include "../objects/object.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

namespace creatures1::creatures {
namespace {

// The fixed layout of a version-2 file (all little-endian).
constexpr std::size_t kVersionOffset = 10;
constexpr std::size_t kClassifierOffset = 14;
constexpr std::size_t kSexOffset = 18;
constexpr std::size_t kGenomeSizeOffset = 22;
constexpr std::size_t kGenomeCrcOffset = 26;
constexpr std::size_t kGenomeClassOffset = 30;
constexpr std::size_t kPayloadSizeOffset = 43;
constexpr std::size_t kMonikerOffset = 47;

constexpr std::uint8_t kEggClassRecord[] = {0xff, 0xff, 0x01, 0x00, 0x04, 0x00,
                                            'C', 'E', 'g', 'g'};
constexpr std::uint8_t kGenomeClassRecord[] = {0xff, 0xff, 0x01, 0x00, 0x07,
                                               0x00, 'C', 'G', 'e', 'n',
                                               'o', 'm', 'e'};
constexpr std::size_t kSmallestGenome = 8;  // "gene" ... "gend"

std::uint32_t read_u32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

bool is_moniker_character(std::uint8_t c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z');
}

} // namespace

std::uint32_t Egg::crc32(const std::uint8_t* bytes, std::size_t count) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t index = 0; index < count; ++index) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

bool Egg::is_well_formed_file(const std::uint8_t* bytes, std::size_t size) {
    // Size first, so nothing below reads past the end.
    if (bytes == nullptr || size < kHeaderBytes + kSmallestGenome ||
        size > kHeaderBytes + kMaxGenomeBytes) {
        return false;
    }
    if (std::memcmp(bytes, kEggClassRecord, sizeof kEggClassRecord) != 0 ||
        read_u32(bytes + kVersionOffset) != kVersion ||
        !is_egg_classifier(read_u32(bytes + kClassifierOffset))) {
        return false;
    }
    // 0 is a laid egg's: the world's laying script leaves obv1 0 and
    // `new: crea obv0 obv1` picks the sex when it hatches.
    const std::uint32_t sex = read_u32(bytes + kSexOffset);
    if (sex > 2) {
        return false;
    }
    // The genome size must account for the rest of the file exactly, and
    // the CGenome record must say the same.
    const std::uint32_t genome_size = read_u32(bytes + kGenomeSizeOffset);
    if (genome_size != size - kHeaderBytes ||
        std::memcmp(bytes + kGenomeClassOffset, kGenomeClassRecord,
                    sizeof kGenomeClassRecord) != 0 ||
        read_u32(bytes + kPayloadSizeOffset) != genome_size) {
        return false;
    }
    // The moniker becomes the genome's filename, <moniker>.gen.
    for (std::size_t index = 0; index < 4; ++index) {
        if (!is_moniker_character(bytes[kMonikerOffset + index])) {
            return false;
        }
    }
    const std::uint8_t* genes = bytes + kHeaderBytes;
    return std::memcmp(genes, "gene", 4) == 0 &&
           crc32(genes, genome_size) == read_u32(bytes + kGenomeCrcOffset);
}

void Egg::serialize(objects::ObjectArchive& archive) {
    if (archive.is_loading()) {
        const std::uint32_t version = archive.read_uint32();
        if (version != kVersion) {
            throw std::runtime_error("unsupported egg file version " +
                                     std::to_string(version));
        }
        classifier = archive.read_uint32();
        sex = archive.read_uint32();
        const std::uint32_t genome_size = archive.read_uint32();
        const std::uint32_t genome_crc = archive.read_uint32();
        // The archive's CGenome factory hands the new genome to the reader.
        genome.reset(
            static_cast<Genome*>(archive.read_object_reference("CGenome")));
        if (genome == nullptr || genome->payload().size() != genome_size ||
            crc32(genome->payload().data(), genome->payload().size()) !=
                genome_crc) {
            throw std::runtime_error("egg genome does not match its header");
        }
        return;
    }
    const std::vector<std::uint8_t>& payload = genome->payload();
    archive.write_uint32(kVersion);
    archive.write_uint32(classifier);
    archive.write_uint32(sex);
    archive.write_uint32(static_cast<std::uint32_t>(payload.size()));
    archive.write_uint32(crc32(payload.data(), payload.size()));
    archive.write_object_reference(genome.get(), "CGenome");
}

} // namespace creatures1::creatures
