#include "body.hpp"

#include "../objects/entity.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

namespace creatures1::creatures {
namespace {

std::string filename_from_stem(BodyPartFilenameStem stem,
                               std::string_view extension) {
    std::string filename;
    filename.resize(8);
    filename[0] = static_cast<char>(stem & 0xffU);
    filename[1] = static_cast<char>((stem >> 8) & 0xffU);
    filename[2] = static_cast<char>((stem >> 16) & 0xffU);
    filename[3] = static_cast<char>((stem >> 24) & 0xffU);
    filename[4] = '.';
    filename[5] = extension.size() > 0 ? extension[0] : '\0';
    filename[6] = extension.size() > 1 ? extension[1] : '\0';
    filename[7] = extension.size() > 2 ? extension[2] : '\0';
    return filename;
}

std::string append_filename(std::string directory,
                            BodyPartFilenameStem stem,
                            std::string_view extension) {
    directory += filename_from_stem(stem, extension);
    return directory;
}

bool read_attachment_values(const BodyResourceHost& resources,
                            std::string_view path,
                            std::size_t value_count,
                            std::vector<int>& values) {
    std::string text;
    if (!resources.read_text_file(path, text)) {
        return false;
    }

    std::istringstream input{std::move(text)};
    values.clear();
    values.reserve(value_count);
    for (std::size_t index = 0; index < value_count; ++index) {
        int value = 0;
        if (!(input >> value)) {
            values.clear();
            return false;
        }
        values.push_back(value);
    }
    return true;
}

} // namespace

BodyAppearanceSelection read_body_appearance_genes(Genome& genome) {
    BodyAppearanceSelection selection;
    constexpr std::uint8_t kCreatureFamily =
        static_cast<std::uint8_t>(GenomeGeneFamily::creature);
    constexpr std::uint8_t kAppearanceSubtype = 2;
    constexpr std::uint8_t kSubtypeModulus = 7;

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(
        kCreatureFamily, kAppearanceSubtype, kSubtypeModulus,
        GenomeStageFilter::ignore)) {
        CreatureAppearanceGenePayload payload{};
        if (!genome.read_payload_record(payload)) {
            break;
        }

        std::uint8_t group_value = payload.body_part_group_selector;
        if (group_value > static_cast<std::uint8_t>(BodyAppearanceGroup::tail)) {
            group_value = static_cast<std::uint8_t>(
                group_value % (static_cast<std::uint8_t>(
                                   BodyAppearanceGroup::tail) + 1U));
        }

        std::uint8_t variant = payload.variant_index;
        if (variant > 9) {
            variant = static_cast<std::uint8_t>(variant % 10U);
        }

        const auto& members = kAppearancePartGroupMembers[group_value];
        for (const std::optional<BodyPartIndex>& part : members) {
            if (!part.has_value()) {
                break;
            }
            selection.variant_by_part[body_part_number(*part)] = variant;
        }
        if (group_value == static_cast<std::uint8_t>(BodyAppearanceGroup::tail)) {
            ++selection.tail_variant_count;
        }
    }
    return selection;
}

void BodyPart::serialize(objects::EntityArchive& archive) {
    // Native BodyPart::Serialize delegates to Entity::Serialize first, then
    // appends the two 32-bit frame values.  The archive owns MFC buffering and
    // dynamic-object framing; Entity owns the recovered prefix contract.
    objects::Entity::serialize(archive);
    if (archive.is_loading()) {
        pose_frame_index = static_cast<std::uint8_t>(archive.read_int32());
        attachment_frame_index =
            static_cast<std::uint8_t>(archive.read_int32());
        return;
    }
    archive.write_int32(static_cast<std::int32_t>(pose_frame_index));
    archive.write_int32(static_cast<std::int32_t>(attachment_frame_index));
}

void BodyPart::configure_render_state(display::Gallery* next_gallery,
                                      std::uint8_t next_image_index_base,
                                      int next_render_plane) {
    set_gallery(next_gallery);
    set_image_index_base(next_image_index_base);
    set_render_plane(next_render_plane);
    pose_frame_index = 1;
    attachment_frame_index = 1;
    set_image_index(static_cast<std::uint8_t>(
        image_index_base() + attachment_frame_index));
}

bool Body::load_attachment_table(std::uint32_t genus,
                                 GenomeSex sex,
                                 GenomeLifeStage life_stage,
                                 std::uint32_t variant,
                                 const BodyResourceHost& resources) {
    const BodyPartResourcePath resolved =
        resolve_existing_body_part_filename_with_fallback(
            BodyPartIndex::body, genus, sex, life_stage, variant, "ATT", 6,
            resources);
    if (resolved.stem == 0) {
        return false;
    }

    const std::string attachment_path = append_filename(
        resources.body_data_directory(), resolved.stem, "ATT");
    std::vector<int> values;
    // Native Body::LoadGenome consumes each view's six chain X/Y pairs;
    // storage is transposed into the two contiguous [chain][view] matrices.
    if (!read_attachment_values(resources, attachment_path,
                                kAttachmentViewCount * kLimbChainCount * 2,
                                values)) {
        return false;
    }

    std::size_t value_index = 0;
    for (std::size_t view = 0; view < kAttachmentViewCount; ++view) {
        for (std::size_t chain = 0; chain < kLimbChainCount; ++chain) {
            attachment_table.join_x[chain][view] =
                static_cast<std::uint8_t>(values[value_index++]);
            attachment_table.join_y[chain][view] =
                static_cast<std::uint8_t>(values[value_index++]);
        }
    }
    return true;
}

void Body::serialize(objects::EntityArchive& archive) {
    BodyPart::serialize(archive);
    // C1 writes each attachment view as an interleaved X/Y pair.  The in-
    // memory table is transposed ([chain][view]), but the archive stream is
    // six rows of ten (X,Y) records, not an X matrix followed by a Y matrix.
    for (std::size_t view = 0; view < kAttachmentViewCount; ++view) {
        for (std::size_t chain = 0; chain < kLimbChainCount; ++chain) {
            if (archive.is_loading()) {
                attachment_table.join_x[chain][view] = archive.read_byte();
                attachment_table.join_y[chain][view] = archive.read_byte();
            } else {
                archive.write_byte(attachment_table.join_x[chain][view]);
                archive.write_byte(attachment_table.join_y[chain][view]);
            }
        }
    }
}

void LimbPart::serialize(objects::EntityArchive& archive) {
    // BodyPart::Serialize already streams both frame indices; Limb::Serialize
    // @00416060 goes straight from it into the attachment bytes.  Repeating
    // them here consumed eight extra bytes per limb and desynchronised every
    // creature in the archive.
    BodyPart::serialize(archive);
    if (archive.is_loading()) {
        for (std::size_t frame = 0; frame < kAttachmentViewCount; ++frame) {
            attachment_table.anchor_a_x[frame] = archive.read_byte();
            attachment_table.anchor_a_y[frame] = archive.read_byte();
            attachment_table.anchor_b_x[frame] = archive.read_byte();
            attachment_table.anchor_b_y[frame] = archive.read_byte();
        }
        next_in_chain = static_cast<LimbPart*>(
            archive.read_object_reference("Limb"));
        return;
    }

    for (std::size_t frame = 0; frame < kAttachmentViewCount; ++frame) {
        archive.write_byte(attachment_table.anchor_a_x[frame]);
        archive.write_byte(attachment_table.anchor_a_y[frame]);
        archive.write_byte(attachment_table.anchor_b_x[frame]);
        archive.write_byte(attachment_table.anchor_b_y[frame]);
    }
    archive.write_object_reference(next_in_chain, "Limb");
}

BodyPart::BodyPart(objects::EntityRegistryHost* registry) : Entity(registry) {}

BodyPart::~BodyPart() = default;

Body::Body(objects::EntityRegistryHost* registry) : BodyPart(registry) {}

Body::~Body() = default;

LimbPart::LimbPart(objects::EntityRegistryHost* registry)
    : BodyPart(registry) {}

LimbPart::~LimbPart() = default;

BodyPartResourcePath resolve_existing_body_part_filename_with_fallback(
    BodyPartIndex body_part_index,
    std::uint32_t genus,
    GenomeSex sex,
    GenomeLifeStage life_stage,
    std::uint32_t variant,
    std::string_view extension,
    int resource_directory_index,
    const BodyResourceHost& resources) {
    if (extension.size() < 3) {
        return {};
    }

    // C1 encodes male/female as one/two in the body filename family, while
    // GenomeSex is the clean zero-based application enum.
    const int sex_code = static_cast<int>(sex) + 1;
    const int requested_life_stage =
        static_cast<int>(static_cast<std::uint8_t>(life_stage));
    const int requested_variant = static_cast<int>(variant);

    for (int sex_pass = 0; sex_pass < 2; ++sex_pass) {
        const int genus_index = sex_pass == 0
                                    ? (sex_code - 1) * 4 +
                                          static_cast<int>(genus)
                                    : static_cast<int>(genus) -
                                          (sex_code - 1) * 4 + 4;
        for (int variant_cursor = requested_variant; variant_cursor >= 0;
             --variant_cursor) {
            for (int life_stage_cursor = requested_life_stage;
                 life_stage_cursor >= 0; --life_stage_cursor) {
                const BodyPartFilenameStem stem =
                    (static_cast<BodyPartFilenameStem>(
                         static_cast<std::uint8_t>(
                             body_part_number(body_part_index) + 0x41U))) |
                    (static_cast<BodyPartFilenameStem>(
                         static_cast<std::uint8_t>(genus_index + 0x30))
                     << 8) |
                    (static_cast<BodyPartFilenameStem>(
                         static_cast<std::uint8_t>(life_stage_cursor + 0x30))
                     << 16) |
                    (static_cast<BodyPartFilenameStem>(
                         static_cast<std::uint8_t>(variant_cursor + 0x30))
                     << 24);

                std::string directory;
                if (resource_directory_index != -1) {
                    directory = resources.resource_directory(
                        resource_directory_index);
                }
                const std::string path =
                    append_filename(std::move(directory), stem, extension);
                if (resources.regular_file_exists(path)) {
                    return {stem, path};
                }
            }
        }
    }
    return {};
}

LimbAttachmentTable load_body_part_attachment_data(
    BodyPartIndex body_part_index,
    std::uint32_t genus,
    GenomeSex sex,
    GenomeLifeStage life_stage,
    std::uint32_t variant,
    const BodyResourceHost& resources) {
    LimbAttachmentTable attachment_data{};
    const BodyPartResourcePath resolved =
        resolve_existing_body_part_filename_with_fallback(
            body_part_index, genus, sex, life_stage, variant, "ATT", 6,
            resources);
    if (resolved.stem == 0) {
        return attachment_data;
    }

    // The executable resolves the name against directory slot 6, then opens
    // the body-data directory explicitly. Keep that two-stage ownership
    // visible so resource staging can reproduce it exactly.
    const std::string attachment_path = append_filename(
        resources.body_data_directory(), resolved.stem, "ATT");
    std::vector<int> values;
    if (!read_attachment_values(resources, attachment_path,
                                kAttachmentViewCount * 4, values)) {
        return attachment_data;
    }
    for (std::size_t frame = 0; frame < kAttachmentViewCount; ++frame) {
        attachment_data.anchor_a_x[frame] =
            static_cast<std::uint8_t>(values[frame * 4]);
        attachment_data.anchor_a_y[frame] =
            static_cast<std::uint8_t>(values[frame * 4 + 1]);
        attachment_data.anchor_b_x[frame] =
            static_cast<std::uint8_t>(values[frame * 4 + 2]);
        attachment_data.anchor_b_y[frame] =
            static_cast<std::uint8_t>(values[frame * 4 + 3]);
    }
    return attachment_data;
}

std::unique_ptr<BodyPart> create_body_part(
    objects::EntityRegistryHost* registry) {
    return std::unique_ptr<BodyPart>(new (std::nothrow)
                                         BodyPart(registry));
}

std::unique_ptr<Body> create_body(objects::EntityRegistryHost* registry) {
    return std::unique_ptr<Body>(new (std::nothrow) Body(registry));
}

std::unique_ptr<LimbPart> create_limb(objects::EntityRegistryHost* registry) {
    return std::unique_ptr<LimbPart>(new (std::nothrow)
                                         LimbPart(registry));
}

} // namespace creatures1::creatures
