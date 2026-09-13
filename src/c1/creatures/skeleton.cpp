#include "skeleton.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <utility>

namespace creatures1::creatures {
namespace {

int wrap_world_x_once(int x) {
    // C1's world movement contract adjusts a coordinate by one world width;
    // callers provide deltas within that established range.
    if (x < 0) {
        return x + world::kWorldWidth;
    }
    if (x >= world::kWorldWidth) {
        return x - world::kWorldWidth;
    }
    return x;
}

bool pose_target_character_matches(char current, char target) {
    // Pose strings contain digits or the non-digit wildcard encoding (usually
    // 'X').  The executable accepts every non-digit target character here.
    return target < '0' || target > '9' || current == target;
}

std::uint8_t attachment_frame_for(std::uint8_t pose_frame,
                                  FacingDirection facing) {
    std::uint8_t frame = 9;
    if (facing == FacingDirection::south) {
        frame = 8;
    } else if (facing == FacingDirection::east) {
        frame = pose_frame;
    } else if (facing == FacingDirection::west) {
        frame = static_cast<std::uint8_t>(pose_frame + 4);
    }

    // Poses 4 and 5 are the special attachment views used by C1.  Other
    // poses above the four directional frames fall back to the ordinary
    // south-facing frame.
    if (pose_frame > 3) {
        frame = pose_frame == 5 ? 9 : 8;
    }
    return frame;
}

const display::Image* current_image(const BodyPart& part) {
    if (part.gallery() != nullptr &&
        part.current_image_index() < part.gallery()->image_count) {
        return &part.gallery()->images[part.current_image_index()];
    }
    return nullptr;
}

int floor_divide(int value, int divisor) {
    const int quotient = value / divisor;
    const int remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

int image_width(const BodyPart& part) {
    const display::Image* image = current_image(part);
    return image != nullptr ? image->width() : 0;
}

int image_height(const BodyPart& part) {
    const display::Image* image = current_image(part);
    return image != nullptr ? image->height() : 0;
}

std::size_t chain_index(BodyLimbChain chain) {
    return static_cast<std::size_t>(chain);
}

void load_fixed_text(std::string_view text, char* destination,
                     std::size_t capacity) {
    std::fill(destination, destination + capacity, '\0');
    const std::size_t copy_count =
        std::min(text.size(), capacity == 0 ? 0 : capacity - 1);
    std::copy_n(text.data(), copy_count, destination);
}

std::string_view fixed_text(const char* text, std::size_t capacity) {
    std::size_t length = 0;
    while (length < capacity && text[length] != '\0') {
        ++length;
    }
    return std::string_view(text, length);
}

std::uint8_t average_palette_control(std::uint8_t previous,
                                     std::uint8_t incoming) {
    // 0043cc38 uses the exact double-half constant at 0045d460.  Both
    // operands are non-negative bytes, so the native floating conversion is
    // the integer floor of this sum divided by two.
    return static_cast<std::uint8_t>(
        (static_cast<unsigned int>(previous) + incoming) / 2U);
}

struct SpriteSourceResolution {
    BodyPartResourcePath resource;
    std::string directory;
};

SpriteSourceResolution resolve_sprite_source(
    BodyPartIndex part,
    std::uint32_t genus,
    GenomeSex sex,
    GenomeLifeStage life_stage,
    std::uint32_t variant,
    std::uint32_t genome_source_filename,
    const BodyResourceHost& resources) {
    // Native LoadGenome probes body-image directory 13 first.  A source that
    // resolves to the creature's own output filename is deliberately moved to
    // the fallback directory 4 so the temporary output cannot be read as an
    // input while it is being rebuilt.
    BodyPartResourcePath resolved =
        resolve_existing_body_part_filename_with_fallback(
            part, genus, sex, life_stage, variant, "SPR", 13, resources);
    int directory_index = 13;
    if (resolved.stem == 0 || resolved.stem == genome_source_filename) {
        resolved = resolve_existing_body_part_filename_with_fallback(
            part, genus, sex, life_stage, variant, "SPR", 4, resources);
        directory_index = 4;
    }
    if (resolved.stem == 0) {
        return {};
    }
    return {std::move(resolved),
            resources.resource_directory(directory_index)};
}

// These two tables are the executable's 0045ac50/0045ac30 data referenced by
// Skeleton::ApplyPoseString.  They describe the six limb chains in pose-string
// order; they are not compiler-generated decompiler temporaries.
constexpr std::array<int, kLimbChainCount> kPoseChainLengths{
    1, 3, 3, 2, 2, 2};
constexpr std::array<int, kLimbChainCount> kPoseChainOffsets{
    1, 3, 6, 9, 11, 13};

} // namespace

void Skeleton::serialize(SkeletonArchive& archive,
                         objects::ObjectSoundPlaybackHost* sound_host) {
    Object::serialize(archive);

    if (archive.is_loading()) {
        genome_source_filename = archive.read_uint32();
        mother_moniker = archive.read_uint32();
        father_moniker = archive.read_uint32();
        body.reset(static_cast<Body*>(archive.read_object_reference("Body")));
        for (LimbPart*& chain_head : limb_chain_heads) {
            chain_head = static_cast<LimbPart*>(
                archive.read_object_reference("Limb"));
        }
        facing_direction = static_cast<FacingDirection>(archive.read_byte());
        down_foot = static_cast<DownFoot>(archive.read_byte());
        down_foot_x = archive.read_int32();
        down_foot_y = archive.read_int32();
        normal_render_plane = archive.read_int32();
        load_fixed_text(archive.read_string(), current_pose.characters.data(),
                        current_pose.characters.size());
        drive_threshold_state = archive.read_byte();
        eyes_open = archive.read_byte() != 0;
        sleep_indicator_active = archive.read_byte() != 0;
        for (PoseString& pose : pose_string_table) {
            load_fixed_text(archive.read_string(), pose.characters.data(),
                            pose.characters.size());
        }
        for (auto& gait : gait_animation_table) {
            load_fixed_text(archive.read_string(), gait.data(), gait.size());
        }

        // The executable restores the target pose from the current pose;
        // target_pose is not a second archive string.
        target_pose = current_pose;
        if (sound_host != nullptr) {
            update_anchor_and_bounds(*sound_host);
        } else {
            recompute_body_part_layout();
        }
        return;
    }

    archive.write_uint32(genome_source_filename);
    archive.write_uint32(mother_moniker);
    archive.write_uint32(father_moniker);
    archive.write_object_reference(body.get(), "Body");
    for (LimbPart* chain_head : limb_chain_heads) {
        archive.write_object_reference(chain_head, "Limb");
    }
    archive.write_byte(static_cast<std::uint8_t>(facing_direction));
    archive.write_byte(static_cast<std::uint8_t>(down_foot));
    archive.write_int32(down_foot_x);
    archive.write_int32(down_foot_y);
    archive.write_int32(normal_render_plane);
    archive.write_string(fixed_text(current_pose.characters.data(),
                                     current_pose.characters.size()));
    archive.write_byte(drive_threshold_state);
    archive.write_byte(eyes_open ? 1 : 0);
    archive.write_byte(sleep_indicator_active ? 1 : 0);
    for (const PoseString& pose : pose_string_table) {
        archive.write_string(fixed_text(pose.characters.data(),
                                        pose.characters.size()));
    }
    for (const auto& gait : gait_animation_table) {
        archive.write_string(fixed_text(gait.data(), gait.size()));
    }
}

CreaturePaletteControls read_creature_palette_controls(Genome& genome) {
    constexpr std::uint8_t kCreatureFamily =
        static_cast<std::uint8_t>(GenomeGeneFamily::creature);
    constexpr std::uint8_t kPigmentSubtype = 6;
    constexpr std::uint8_t kSubtypeModulus = 7;
    constexpr std::uint32_t kGextTag = 0x74786567U; // little-endian "gext"

    CreaturePaletteControls controls{};
    std::array<std::uint32_t, 3> sums{};
    std::array<std::uint32_t, 3> counts{};

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(
        kCreatureFamily, kPigmentSubtype, kSubtypeModulus,
        GenomeStageFilter::ignore)) {
        CreaturePigmentGenePayload pigment{};
        if (!genome.read_payload_record(pigment)) {
            break;
        }

        const std::size_t extension_start = genome.cursor();
        const std::vector<std::uint8_t>& payload = genome.payload();
        const std::uint8_t channel = static_cast<std::uint8_t>(
            pigment.pigment_channel % 3U);
        sums[channel] += pigment.amount;
        ++counts[channel];

        // Native LoadGenome checks the four bytes immediately after the
        // pigment payload for "gext".  A valid extension adds rotation and
        // swap bytes, then advances the scan cursor past all six bytes.
        if (extension_start + 6 <= payload.size()) {
            const std::uint32_t extension_tag =
                static_cast<std::uint32_t>(payload[extension_start]) |
                (static_cast<std::uint32_t>(payload[extension_start + 1])
                 << 8) |
                (static_cast<std::uint32_t>(payload[extension_start + 2])
                 << 16) |
                (static_cast<std::uint32_t>(payload[extension_start + 3])
                 << 24);
            if (extension_tag == kGextTag) {
                controls.hue_rotation = average_palette_control(
                    controls.hue_rotation, payload[extension_start + 4]);
                controls.colour_swap = average_palette_control(
                    controls.colour_swap, payload[extension_start + 5]);
                genome.set_cursor(extension_start + 6);
            }
        }
    }

    for (std::size_t channel = 0; channel < sums.size(); ++channel) {
        controls.pigment_channels[channel] = counts[channel] == 0
                                                 ? 0x80
                                                 : static_cast<std::uint8_t>(
                                                       sums[channel] /
                                                       counts[channel]);
    }
    return controls;
}

Skeleton::Skeleton(SkeletonLifetimeHost* lifetime_host)
    : objects::Object(), lifetime_host_(lifetime_host) {
    initialize_pose_and_motion_state();
}

Skeleton::~Skeleton() {
    if (lifetime_host_ != nullptr) {
        clear_body_parts_and_gallery(*lifetime_host_);
        lifetime_host_->remove_from_renderable_set(*this);
        if (continuous_sound_handle >= 0) {
            lifetime_host_->stop_continuous_sound(continuous_sound_handle);
            continuous_sound_handle = -1;
        }
        lifetime_host_->unregister_from_object_registry(*this);
    }
}

void Skeleton::initialize_pose_and_motion_state() {
    body.reset();
    limb_chain_heads.fill(nullptr);
    limb_chain_end_x.fill(0);
    limb_chain_end_y.fill(0);
    sprite_bounds = {};
    previous_sprite_bounds = {};
    current_pose = {};
    target_pose = {};
    pose_string_table = {};
    gait_animation_table = {};
    animation_sequence = {};
    animation_cursor = 0;
    genome_source_filename = 0;
    father_moniker = 0;
    mother_moniker = 0;
    creature_genus_selector = 0;
    body_part_variant_by_index.fill(0);
    body_part_image_index_base.fill(0);
    tail_variant_count = 0;
    facing_direction = FacingDirection::east;
    down_foot = DownFoot::right;
    down_foot_x = 0;
    down_foot_y = 0;
    normal_render_plane = 100;
    boundary_correction_pending = false;
    motion_link = nullptr;
    motion_target_x = 0;
    motion_target_y = 0;
    set_bounds_reference_object(nullptr);
    caos_object_pointer = nullptr;
    drive_threshold_state = 1;
    eyes_open = true;
    gallery = nullptr;
    continuous_sound_handle = -1;

    constexpr std::string_view neutral_pose = "242212212011200";
    auto set_pose = [](PoseString& pose, std::string_view text) {
        for (std::size_t index = 0; index < kPoseStringLength; ++index) {
            pose.characters[index] = text[index];
        }
        pose.nul_terminator = '\0';
    };
    set_pose(current_pose, neutral_pose);
    set_pose(target_pose, neutral_pose);
    for (PoseString& pose : pose_string_table) {
        set_pose(pose, neutral_pose);
    }
    gait_animation_table[0][0] = '1';
    gait_animation_table[0][1] = '3';
    gait_animation_table[0][2] = '1';
    gait_animation_table[0][3] = '4';
    gait_animation_table[0][4] = '1';
    gait_animation_table[0][5] = '5';
    gait_animation_table[0][6] = '1';
    gait_animation_table[0][7] = '6';
    gait_animation_table[0][8] = 'R';
}

void Skeleton::load_genus_identity(Genome& genome) {
    constexpr std::uint8_t kCreatureFamily =
        static_cast<std::uint8_t>(GenomeGeneFamily::creature);
    constexpr std::uint8_t kGenusSubtype = 1;
    constexpr std::uint8_t kSubtypeModulus = 7;

    genome_source_filename = genome.source_filename();
    creature_genus_selector = 0;
    mother_moniker = 0;
    father_moniker = 0;

    genome.set_cursor(0);
    while (genome.find_next_matching_gene(
        kCreatureFamily, kGenusSubtype, kSubtypeModulus,
        GenomeStageFilter::ignore)) {
        CreatureGenusGenePayload payload{};
        if (!genome.read_payload_record(payload)) {
            break;
        }

        // The native selector is reduced with & 3 (equivalent to modulo 4
        // for this unsigned byte) before it becomes the creature genus.
        creature_genus_selector = static_cast<std::uint8_t>(
            payload.genus_selector & 3U);
        mother_moniker = payload.mother_moniker;
        father_moniker = payload.father_moniker;
    }

    // Native LoadGenome produces family 4, genus 1..4, and species 0/1 for
    // male/female. Object stores those four bytes independently, so this is
    // the source-level equivalent of its packed classifier expression.
    set_classifier_components(
        0, static_cast<std::uint8_t>(genome.sex()),
        static_cast<std::uint8_t>(creature_genus_selector + 1U), 4);
}

void Skeleton::load_appearance_genes(Genome& genome) {
    const BodyAppearanceSelection selection =
        read_body_appearance_genes(genome);
    body_part_variant_by_index = selection.variant_by_part;
    tail_variant_count = selection.tail_variant_count;
}

bool Skeleton::construct_body_parts(
    const SkeletonBodyBuildServices& services) {
    if (services.gallery == nullptr || body != nullptr) {
        return false;
    }
    for (LimbPart* chain_head : limb_chain_heads) {
        if (chain_head != nullptr) {
            return false;
        }
    }

    if (services.gallery->image_count == 0) {
        return false;
    }

    auto new_limb = [&](BodyPartIndex part, std::uint32_t variant,
                        LimbPart* next) {
        std::unique_ptr<LimbPart> limb =
            create_limb(services.entity_registry);
        if (limb == nullptr) {
            return std::unique_ptr<LimbPart>{};
        }
        limb->attachment_table = load_body_part_attachment_data(
            part, creature_genus_selector, services.sex, services.life_stage,
            variant, services.body_resources);
        limb->next_in_chain = next;
        limb->configure_render_state(
            services.gallery,
            services.image_index_base[body_part_number(part)],
            services.normal_render_plane);
        return limb;
    };

    std::unique_ptr<Body> new_body =
        create_body(services.entity_registry);
    if (new_body == nullptr ||
        !new_body->load_attachment_table(
            creature_genus_selector,
            services.sex, services.life_stage,
            body_part_variant_by_index[body_part_number(BodyPartIndex::body)],
            services.body_resources)) {
        return false;
    }
    new_body->configure_render_state(
        services.gallery,
        services.image_index_base[body_part_number(BodyPartIndex::body)],
        services.normal_render_plane);

    // Allocation order is deliberately distal-first because the native code
    // then prepends each proximal node to the stored chain head.
    std::array<std::unique_ptr<LimbPart>, 14> new_limbs{};
    new_limbs[body_part_number(BodyPartIndex::head)] =
        new_limb(BodyPartIndex::head,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::head)], nullptr);

    new_limbs[body_part_number(BodyPartIndex::right_foot)] =
        new_limb(BodyPartIndex::right_foot,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::right_foot)], nullptr);
    new_limbs[body_part_number(BodyPartIndex::right_shin)] =
        new_limb(BodyPartIndex::right_shin,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::right_shin)],
                 new_limbs[body_part_number(BodyPartIndex::right_foot)].get());
    new_limbs[body_part_number(BodyPartIndex::right_thigh)] =
        new_limb(BodyPartIndex::right_thigh,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::right_thigh)],
                 new_limbs[body_part_number(BodyPartIndex::right_shin)].get());

    new_limbs[body_part_number(BodyPartIndex::left_foot)] =
        new_limb(BodyPartIndex::left_foot,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::left_foot)], nullptr);
    new_limbs[body_part_number(BodyPartIndex::left_shin)] =
        new_limb(BodyPartIndex::left_shin,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::left_shin)],
                 new_limbs[body_part_number(BodyPartIndex::left_foot)].get());
    new_limbs[body_part_number(BodyPartIndex::left_thigh)] =
        new_limb(BodyPartIndex::left_thigh,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::left_thigh)],
                 new_limbs[body_part_number(BodyPartIndex::left_shin)].get());

    new_limbs[body_part_number(BodyPartIndex::left_radius)] =
        new_limb(BodyPartIndex::left_radius,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::left_radius)], nullptr);
    new_limbs[body_part_number(BodyPartIndex::left_humerus)] =
        new_limb(BodyPartIndex::left_humerus,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::left_humerus)],
                 new_limbs[body_part_number(BodyPartIndex::left_radius)].get());

    new_limbs[body_part_number(BodyPartIndex::right_radius)] =
        new_limb(BodyPartIndex::right_radius,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::right_radius)], nullptr);
    new_limbs[body_part_number(BodyPartIndex::right_humerus)] =
        new_limb(BodyPartIndex::right_humerus,
                 body_part_variant_by_index[body_part_number(
                     BodyPartIndex::right_humerus)],
                 new_limbs[body_part_number(BodyPartIndex::right_radius)].get());

    if (tail_variant_count != 0) {
        new_limbs[body_part_number(BodyPartIndex::tail_root)] =
            new_limb(BodyPartIndex::tail_root, 0, nullptr);
        new_limbs[body_part_number(BodyPartIndex::tail_tip)] =
            new_limb(BodyPartIndex::tail_tip, 0,
                     new_limbs[body_part_number(BodyPartIndex::tail_root)].get());
    }

    constexpr std::array<BodyPartIndex, 12> required_parts{
        BodyPartIndex::head,
        BodyPartIndex::body,
        BodyPartIndex::left_thigh,
        BodyPartIndex::left_shin,
        BodyPartIndex::left_foot,
        BodyPartIndex::right_thigh,
        BodyPartIndex::right_shin,
        BodyPartIndex::right_foot,
        BodyPartIndex::left_humerus,
        BodyPartIndex::left_radius,
        BodyPartIndex::right_humerus,
        BodyPartIndex::right_radius};
    for (BodyPartIndex part : required_parts) {
        if (part != BodyPartIndex::body &&
            new_limbs[body_part_number(part)] == nullptr) {
            return false;
        }
    }
    if (tail_variant_count != 0 &&
        (new_limbs[body_part_number(BodyPartIndex::tail_root)] == nullptr ||
         new_limbs[body_part_number(BodyPartIndex::tail_tip)] == nullptr)) {
        return false;
    }

    body_part_image_index_base = services.image_index_base;
    normal_render_plane = services.normal_render_plane;
    gallery = services.gallery;
    body = std::move(new_body);
    // The chain links are non-owning by design. Transfer every node before
    // the temporary owners leave scope; SkeletonLifetimeHost later walks the
    // links and destroys each node exactly once.
    std::array<LimbPart*, 14> built_limbs{};
    for (std::unique_ptr<LimbPart>& limb : new_limbs) {
        built_limbs[static_cast<std::size_t>(&limb - new_limbs.data())] =
            limb.get();
        (void)limb.release();
    }
    limb_chain_heads[0] = built_limbs[body_part_number(BodyPartIndex::head)];
    limb_chain_heads[1] =
        built_limbs[body_part_number(BodyPartIndex::left_thigh)];
    limb_chain_heads[2] =
        built_limbs[body_part_number(BodyPartIndex::right_thigh)];
    limb_chain_heads[3] =
        built_limbs[body_part_number(BodyPartIndex::left_humerus)];
    limb_chain_heads[4] =
        built_limbs[body_part_number(BodyPartIndex::right_humerus)];
    limb_chain_heads[5] = tail_variant_count == 0
                              ? nullptr
                              : built_limbs[body_part_number(BodyPartIndex::tail_tip)];
    return true;
}

bool Skeleton::build_creature_sprite_gallery(
    Genome& genome, const SkeletonSpriteBuildServices& services) {
    if (gallery != nullptr || genome_source_filename == 0) {
        return false;
    }

    const std::size_t part_count = tail_variant_count == 0 ? 12 : 14;
    std::array<std::uint8_t, 14> image_index_base{};
    std::uint32_t expected_frame_count = 0;
    for (std::size_t part = 0; part < part_count; ++part) {
        image_index_base[part] =
            static_cast<std::uint8_t>(expected_frame_count);
        expected_frame_count += kBodyPartSpriteFrameCounts[part];
    }

    const CreaturePaletteControls controls =
        read_creature_palette_controls(genome);
    display::PaletteRemapTable palette_remap{};
    display::build_creature_palette_remap(
        palette_remap, services.palette, controls.pigment_channels[0],
        controls.pigment_channels[1], controls.pigment_channels[2],
        controls.hue_rotation, controls.colour_swap,
        services.palette_build_count);

    const std::string temporary_path = display::sprite_index_path(
        services.secondary_image_directory, genome_source_filename);
    std::unique_ptr<display::SpriteIndexOutputFile> output =
        services.output_files.open_for_create(temporary_path);
    if (output == nullptr) {
        return false;
    }

    display::SpriteIndexWriter writer(*output, expected_frame_count);
    if (!writer.begin()) {
        return false;
    }

    for (std::size_t part_number = 0; part_number < part_count;
         ++part_number) {
        const BodyPartIndex part = static_cast<BodyPartIndex>(part_number);
        const SpriteSourceResolution source = resolve_sprite_source(
            part, creature_genus_selector, genome.sex(), genome.life_stage(),
            body_part_variant_by_index[part_number], genome_source_filename,
            services.body_resources);
        if (source.resource.stem == 0) {
            return false;
        }

        // Use the resolved directory for both sides of this search.  This
        // preserves the native directory decision even when the same stem is
        // present in both image roots, while SpriteFileCache still owns the
        // bounded open-file cache.
        const display::SpriteFileSearchPaths source_paths{
            source.directory, source.directory};
        display::Gallery* source_gallery = nullptr;
        bool append_succeeded = false;
        try {
            source_gallery = display::acquire_gallery(
                source.resource.stem, 0,
                kBodyPartSpriteFrameCounts[part_number], false,
                source_paths.secondary_image_directory,
                source_paths.primary_image_directory, services.gallery_host,
                services.gallery_registry);
            append_succeeded =
                source_gallery != nullptr && writer.append_gallery_frames(
                    *source_gallery, palette_remap, services.pixel_cache,
                    services.sprite_files, source_paths,
                    services.binary_files);
        } catch (const std::exception&) {
            if (source_gallery != nullptr) {
                services.gallery_lifetime_host.release_gallery(*source_gallery);
            }
            return false;
        }

        if (source_gallery != nullptr) {
            services.gallery_lifetime_host.release_gallery(*source_gallery);
        }
        if (!append_succeeded) {
            return false;
        }
    }

    if (!writer.finish()) {
        return false;
    }

    display::Gallery* final_gallery = nullptr;
    try {
        final_gallery = display::acquire_gallery(
            genome_source_filename, 0, expected_frame_count, false,
            services.secondary_image_directory,
            services.primary_image_directory, services.gallery_host,
            services.gallery_registry);
    } catch (const std::exception&) {
        return false;
    }
    if (final_gallery == nullptr) {
        return false;
    }

    body_part_image_index_base = image_index_base;
    gallery = final_gallery;
    return true;
}

bool Skeleton::load_genome(
    Genome& genome, const SkeletonSpriteBuildServices& services,
    SkeletonRenderPlaneHost& render_plane_host,
    objects::ObjectSoundPlaybackHost& sound_host) {
    // Native LoadGenome starts by invalidating the shared SPR file cache and
    // deleting the prior body/gallery graph.  The lifetime host is the
    // application-owned implementation of those release operations.
    lifetime_host_ = &services.gallery_lifetime_host;
    services.sprite_files.clear();
    clear_body_parts_and_gallery(services.gallery_lifetime_host);

    load_genus_identity(genome);
    load_appearance_genes(genome);
    if (!build_creature_sprite_gallery(genome, services)) {
        return false;
    }

    select_normal_render_plane(render_plane_host);

    const SkeletonBodyBuildServices body_services{
        services.body_resources,
        services.entity_registry,
        gallery,
        genome.sex(),
        genome.life_stage(),
        body_part_image_index_base,
        normal_render_plane};
    if (construct_body_parts(body_services)) {
        apply_pose_string(
            std::string_view(current_pose.characters.data(),
                            kPoseStringLength),
            sound_host);
        return true;
    }

    // build_creature_sprite_gallery commits only the final gallery before
    // body construction. Release that reference on the failed transaction;
    // no partially committed limb graph exists here.
    if (gallery != nullptr) {
        services.gallery_lifetime_host.release_gallery(*gallery);
        gallery = nullptr;
    }
    body_part_image_index_base.fill(0);
    return false;
}

void Skeleton::select_normal_render_plane(SkeletonRenderPlaneHost& host) {
    constexpr std::uint32_t kPlaneRange = 0x7d1U;
    constexpr int kPlaneMinimum = 1000;
    constexpr int kConflictWidth = 8;

    for (;;) {
        const int candidate = static_cast<int>(
            host.next_random_value() % kPlaneRange) + kPlaneMinimum;
        bool conflicts = false;
        for (std::size_t index = 0; index < host.creature_count(); ++index) {
            objects::Object* object = host.creature_at(index);
            if (object == nullptr || object == this) {
                continue;
            }
            const int object_plane = object->render_plane();
            if (object_plane >= candidate &&
                object_plane <= candidate + kConflictWidth) {
                conflicts = true;
                break;
            }
        }
        if (!conflicts) {
            normal_render_plane = candidate;
            return;
        }
    }
}

void Skeleton::update_limb_frames_for_pose() {
    if (body == nullptr) {
        return;
    }
    const std::size_t facing = static_cast<std::size_t>(facing_direction);
    for (std::size_t chain = 0; chain < kLimbChainCount; ++chain) {
        int render_offset = pose_chain_render_plane_offsets[facing][chain];
        for (LimbPart* limb = limb_chain_heads[chain]; limb != nullptr;
             limb = limb->next_in_chain) {
            limb->set_render_plane(body->render_plane() + render_offset);
            render_offset += render_offset < 0 ? -1 : 1;
        }
    }
}

void Skeleton::clear_body_parts_and_gallery(
    SkeletonLifetimeHost& lifetime_host) {
    for (LimbPart*& chain_head : limb_chain_heads) {
        LimbPart* limb = chain_head;
        chain_head = nullptr;
        while (limb != nullptr) {
            LimbPart* next = limb->next_in_chain;
            limb->next_in_chain = nullptr;
            lifetime_host.destroy_limb(*limb);
            limb = next;
        }
    }
    body.reset();
    if (gallery != nullptr) {
        lifetime_host.release_gallery(*gallery);
        gallery = nullptr;
    }
}

std::uint32_t Skeleton::select_target_pose_for_motion(
    bool force_interaction_pose) {
    if (motion_link == nullptr || motion_target_y > down_foot_y) {
        return 0xffffffffU;
    }
    if (motion_link == this) {
        return 1;
    }

    const int pose_column = std::clamp(
        floor_divide(motion_target_x - down_foot_x, 0x12), 0, 2);
    const int pose_row = std::clamp(floor_divide(down_foot_y - motion_target_y,
                                                0x12),
                                      0, 3);
    animation_sequence[0] = '\0';
    animation_cursor = 0;

    PoseString forced_pose{};
    const PoseString* source = &pose_string_table[
        static_cast<std::size_t>(pose_column * 4 + pose_row)];
    if (force_interaction_pose) {
        forced_pose = *source;
        forced_pose.characters[1] = '4';
        source = &forced_pose;
    }
    return set_target_pose_string(std::string_view(
               source->characters.data(), kPoseStringLength))
        ? 1U
        : 0U;
}

std::uint32_t Skeleton::select_target_pose_for_motion_guarded(
    bool force_interaction_pose) {
    const int horizontal_delta = down_foot_x - motion_target_x;
    if (std::abs(horizontal_delta) > 0x36) {
        return 0xffffffffU;
    }
    return select_target_pose_for_motion(force_interaction_pose);
}

void Skeleton::validate_body_sprites(std::string_view image_directory,
                                     BodySpriteFileHost& file_host) const {
    if (gallery == nullptr || gallery->image_count == 0) {
        return;
    }
    const std::string path = file_host.body_sprite_path(
        image_directory, genome_source_filename);
    std::uint16_t image_count = 0;
    std::uint32_t first_frame_offset = 0;
    std::uint16_t first_frame_width = 0;
    std::uint16_t first_frame_height = 0;
    if (file_host.read_sprite_header(path, image_count, first_frame_offset,
                                     first_frame_width, first_frame_height) &&
        image_count == gallery->image_count) {
        (void)first_frame_offset;
        (void)first_frame_width;
        (void)first_frame_height;
    }
}

bool Skeleton::references_object(objects::Object* object) const {
    return motion_link == object || bounds_reference_object() == object ||
           caos_object_pointer == object || objects::Object::references_object(object);
}

void Skeleton::clear_references_to(objects::Object* object) {
    if (motion_link == object) {
        motion_link = nullptr;
    }
    if (bounds_reference_object() == object) {
        set_bounds_reference_object(nullptr);
    }
    if (caos_object_pointer == object) {
        caos_object_pointer = nullptr;
    }
    objects::Object::clear_references_to(object);
}

char* Skeleton::parse_animation_sequence(char* sequence_text) {
    animation_cursor = 0;
    std::size_t write_index = 0;
    char* read_cursor = sequence_text + 1;

    // The original stores at most 31 characters in its 32-byte sequence
    // buffer.  Valid C1 gait strings terminate with ']' before that limit.
    while (*read_cursor != ']' &&
           write_index + 1 < animation_sequence.size()) {
        animation_sequence[write_index++] = *read_cursor++;
    }
    animation_sequence[write_index] = '\0';
    animation_cursor = 0;

    // Keep the native parser's delimiter-stepping convention for callers
    // that continue parsing the enclosing animation text.
    return read_cursor + 2;
}

bool Skeleton::is_animation_sequence_complete() const {
    return animation_cursor >= animation_sequence.size() ||
           animation_sequence[animation_cursor] == '\0';
}

void Skeleton::set_target_pose_from_table_index(std::size_t pose_table_index) {
    animation_sequence[0] = '\0';
    animation_cursor = 0;
    set_target_pose_string(
        std::string_view(pose_string_table[pose_table_index].characters.data(),
                         kPoseStringLength));
}

bool Skeleton::set_target_pose_string(std::string_view target_pose_text) {
    bool differs = false;
    const std::size_t copy_length =
        std::min(target_pose_text.size(), kPoseStringLength);
    for (std::size_t index = 0; index < kPoseStringLength; ++index) {
        const char incoming = index < copy_length ? target_pose_text[index] : '\0';
        if (target_pose.characters[index] != incoming) {
            differs = true;
        }
        target_pose.characters[index] = incoming;
    }
    target_pose.nul_terminator = '\0';

    if (differs) {
        return false;
    }
    return is_pose_at_target();
}

bool Skeleton::is_pose_at_target() const {
    for (std::size_t index = 0; index < kPoseStringLength; ++index) {
        if (!pose_target_character_matches(current_pose.characters[index],
                                           target_pose.characters[index])) {
            return false;
        }
    }
    return true;
}

void Skeleton::apply_pose_string(
    std::string_view pose_text,
    objects::ObjectSoundPlaybackHost& sound_host) {
    // The native routine consumes exactly fifteen bytes.  Callers obtain
    // these from a PoseString or a genome gait expansion, so retaining the
    // fixed-width contract keeps the recovered field layout visible.
    if (pose_text.size() < kPoseStringLength) {
        return;
    }

    if (pose_text[0] != 'X') {
        facing_direction = static_cast<FacingDirection>(pose_text[0] - '0');
        update_limb_frames_for_pose();
    }
    if (pose_text[2] != 'X' && body != nullptr) {
        body->pose_frame_index =
            static_cast<std::uint8_t>(pose_text[2] - '0');
    }

    for (std::size_t chain = 0; chain < kLimbChainCount; ++chain) {
        LimbPart* limb = limb_chain_heads[chain];
        for (int limb_index = 0;
             limb != nullptr && limb_index < kPoseChainLengths[chain];
             ++limb_index, limb = limb->next_in_chain) {
            const char pose_character =
                pose_text[static_cast<std::size_t>(kPoseChainOffsets[chain] +
                                                    limb_index)];
            if (pose_character != 'X') {
                limb->pose_frame_index = static_cast<std::uint8_t>(
                    pose_character - '0');
            }
        }
    }

    update_anchor_and_bounds(sound_host);

    // Three five-character groups cover all fifteen pose components.  The
    // executable merges only concrete characters; 'X' means retain the
    // component already held in current_pose.
    for (std::size_t group = 0; group < 3; ++group) {
        const std::size_t source = group * 5;
        const std::size_t destination = group * 5;
        for (std::size_t component = 0; component < 5; ++component) {
            if (pose_text[source + component] != 'X') {
                current_pose.characters[destination + component] =
                    pose_text[source + component];
            }
        }
    }
}

void Skeleton::update_anchor_and_bounds(
    objects::ObjectSoundPlaybackHost& sound_host) {
    recompute_body_part_layout();

    // The native check examines the opposite leg's endpoint.  With the
    // recovered enum values (left=0, right=1), that endpoint is
    // (down_foot == left) + 1, while RecomputeBodyPartLayout anchors from the
    // currently selected leg at index (down_foot != left) + 1.
    const std::size_t opposite_leg =
        static_cast<std::size_t>(down_foot == DownFoot::left) + 1;
    if (movement_bounds().max_y < limb_chain_end_y[opposite_leg]) {
        down_foot_x = limb_chain_end_x[opposite_leg];
        down_foot_y = movement_bounds().max_y;
        down_foot = down_foot == DownFoot::left ? DownFoot::right
                                                : DownFoot::left;
        recompute_body_part_layout();
        play_sound_effect(0x70657473U, 0, false, sound_host);
    }

    const int sprite_max_x = sprite_bounds.max_x;
    const int movement_max_x = movement_bounds().max_x;
    const int sprite_min_x = sprite_bounds.min_x;
    const int movement_min_x = movement_bounds().min_x;
    int horizontal_correction = 0;

    if (movement_max_x < sprite_max_x) {
        horizontal_correction = movement_max_x - sprite_max_x;
        boundary_correction_pending = true;
        if (movement_min_x > sprite_min_x + horizontal_correction) {
            const int combined_correction_sum =
                (horizontal_correction - sprite_min_x) + movement_min_x;
            boundary_correction_pending = false;
            horizontal_correction = combined_correction_sum / 2;
        }
    } else {
        if (movement_min_x <= sprite_min_x) {
            return;
        }
        boundary_correction_pending = true;
        horizontal_correction = movement_min_x - sprite_min_x;
        if (sprite_max_x + horizontal_correction > movement_max_x) {
            const int combined_correction_sum =
                horizontal_correction + (movement_max_x - sprite_max_x);
            boundary_correction_pending = false;
            horizontal_correction = combined_correction_sum / 2;
        }
    }

    if (horizontal_correction != 0) {
        down_foot_x += horizontal_correction;
        recompute_body_part_layout();
    }
}

void Skeleton::set_down_foot_position_and_recompute_layout(
    int x, int y, objects::ObjectMovementBoundsHost& world_host,
    objects::ObjectSoundPlaybackHost& sound_host) {
    down_foot_x = x;
    down_foot_y = y;
    recompute_body_part_layout();
    update_movement_bounds(world_host);
    update_anchor_and_bounds(sound_host);
}

void Skeleton::set_down_foot_position_and_invalidate_bounds(
    int x, int y, SkeletonWorldHost& world_host,
    objects::ObjectSoundPlaybackHost& sound_host) {
    previous_sprite_bounds = sprite_bounds;
    down_foot_x = x;
    down_foot_y = y;
    update_anchor_and_bounds(sound_host);
    world_host.queue_dirty_world_rect(previous_sprite_bounds);
    world_host.queue_dirty_world_rect(sprite_bounds);
}

int Skeleton::sprite_bounds_width() const {
    return sprite_bounds.max_x - sprite_bounds.min_x;
}

int Skeleton::sprite_bounds_height() const {
    return sprite_bounds.max_y - sprite_bounds.min_y;
}

bool Skeleton::get_bounds(world::WorldRect* out_bounds) const {
    *out_bounds = sprite_bounds;
    return true;
}

int Skeleton::render_plane() const {
    return body_render_plane();
}

int Skeleton::body_render_plane() const {
    return body != nullptr ? body->render_plane() : normal_render_plane;
}

int Skeleton::sprite_bounds_min_x() const {
    return sprite_bounds.min_x;
}

int Skeleton::sprite_bounds_min_y() const {
    return sprite_bounds.min_y;
}

void Skeleton::translate_by(int delta_x, int delta_y) {
    auto translate_part = [delta_x, delta_y](BodyPart& part) {
        part.set_world_x(wrap_world_x_once(part.world_x() + delta_x));
        part.set_world_y(part.world_y() + delta_y);
    };

    if (body != nullptr) {
        translate_part(*body);
    }
    for (LimbPart* chain_head : limb_chain_heads) {
        for (LimbPart* limb = chain_head; limb != nullptr;
             limb = limb->next_in_chain) {
            translate_part(*limb);
        }
    }

    limb_chain_end_x[0] = wrap_world_x_once(limb_chain_end_x[0] + delta_x);
    for (std::size_t index = 1; index < kLimbChainCount; ++index) {
        limb_chain_end_x[index] =
            wrap_world_x_once(limb_chain_end_x[index] + delta_x);
    }
    for (int& end_y : limb_chain_end_y) {
        end_y += delta_y;
    }

    previous_sprite_bounds = sprite_bounds;
    const int old_min_x = sprite_bounds.min_x;
    const int old_width = sprite_bounds.max_x - old_min_x;
    sprite_bounds.min_x = wrap_world_x_once(old_min_x + delta_x);
    sprite_bounds.max_x = sprite_bounds.min_x + old_width;
    sprite_bounds.min_y += delta_y;
    sprite_bounds.max_y += delta_y;
}

void Skeleton::update_and_invalidate_bounds(int delta_x,
                                            int delta_y,
                                            SkeletonWorldHost& world_host) {
    previous_sprite_bounds = sprite_bounds;
    world_host.move_by(*this, delta_x, delta_y);

    world::WorldRect dirty_bounds{};
    world::union_wrapped_world_rects(dirty_bounds, sprite_bounds,
                                     previous_sprite_bounds);
    world_host.queue_dirty_world_rect(dirty_bounds);
}

void Skeleton::recompute_body_part_layout() {
    if (body == nullptr) {
        return;
    }
    body->attachment_frame_index =
        attachment_frame_for(body->pose_frame_index, facing_direction);
    body->set_image_index(static_cast<std::uint8_t>(
        body->image_index_base() + body->attachment_frame_index));

    for (LimbPart* chain_head : limb_chain_heads) {
        for (LimbPart* limb = chain_head; limb != nullptr;
             limb = limb->next_in_chain) {
            limb->attachment_frame_index =
                attachment_frame_for(limb->pose_frame_index, facing_direction);
            limb->set_image_index(static_cast<std::uint8_t>(
                limb->image_index_base() + limb->attachment_frame_index));
        }
    }

    const BodyLimbChain down_leg =
        down_foot == DownFoot::left ? BodyLimbChain::left_leg
                                    : BodyLimbChain::right_leg;
    int leg_offset_x = 0;
    int leg_offset_y = 0;
    for (LimbPart* limb = limb_chain_heads[chain_index(down_leg)];
         limb != nullptr; limb = limb->next_in_chain) {
        const std::size_t view = limb->attachment_frame_index;
        leg_offset_x += static_cast<int>(limb->attachment_table.anchor_b_x[view]) -
                        static_cast<int>(limb->attachment_table.anchor_a_x[view]);
        leg_offset_y += static_cast<int>(limb->attachment_table.anchor_b_y[view]) -
                        static_cast<int>(limb->attachment_table.anchor_a_y[view]);
    }

    const std::size_t body_view = body->attachment_frame_index;
    const std::size_t leg_index = chain_index(down_leg);
    body->set_world_x(wrap_world_x_once(
        down_foot_x - static_cast<int>(body->attachment_table.join_x[leg_index][body_view]) -
        leg_offset_x));
    body->set_world_y(down_foot_y -
                      static_cast<int>(body->attachment_table.join_y[leg_index][body_view]) -
                      leg_offset_y);

    sprite_bounds = {
        body->world_x(),
        body->world_y(),
        body->world_x() + image_width(*body),
        body->world_y() + image_height(*body),
    };

    for (std::size_t chain = 0; chain < kLimbChainCount; ++chain) {
        int next_anchor_x = body->world_x() +
                            static_cast<int>(body->attachment_table.join_x[chain][body_view]);
        int next_anchor_y = body->world_y() +
                            static_cast<int>(body->attachment_table.join_y[chain][body_view]);

        for (LimbPart* limb = limb_chain_heads[chain]; limb != nullptr;
             limb = limb->next_in_chain) {
            const std::size_t view = limb->attachment_frame_index;
            const int limb_x = wrap_world_x_once(
                next_anchor_x - static_cast<int>(limb->attachment_table.anchor_a_x[view]));
            const int limb_y = next_anchor_y -
                               static_cast<int>(limb->attachment_table.anchor_a_y[view]);
            limb->set_world_x(limb_x);
            limb->set_world_y(limb_y);

            const world::WorldRect limb_bounds{
                limb_x,
                limb_y,
                limb_x + image_width(*limb),
                limb_y + image_height(*limb),
            };
            world::WorldRect union_bounds{};
            world::union_wrapped_world_rects(union_bounds, sprite_bounds,
                                             limb_bounds);
            sprite_bounds = union_bounds;

            next_anchor_x += static_cast<int>(limb->attachment_table.anchor_b_x[view]) -
                             static_cast<int>(limb->attachment_table.anchor_a_x[view]);
            next_anchor_y += static_cast<int>(limb->attachment_table.anchor_b_y[view]) -
                             static_cast<int>(limb->attachment_table.anchor_a_y[view]);
        }

        limb_chain_end_x[chain] = next_anchor_x;
        limb_chain_end_y[chain] = next_anchor_y;
    }

    // C1 folds the drive-threshold state into the head sprite frame.  The
    // original eyes-open branch is an empty compiler-preserved branch, so the
    // eye state is intentionally represented as state input without changing
    // the proven frame arithmetic here.
    LimbPart* head = limb_chain_heads[chain_index(BodyLimbChain::head)];
    if (head != nullptr) {
        std::uint8_t head_frame_offset = 0;
        if (drive_threshold_state != 0 &&
            (facing_direction == FacingDirection::south ||
             head->pose_frame_index == 4)) {
            head_frame_offset = static_cast<std::uint8_t>(drive_threshold_state + 1);
        }
        head_frame_offset = static_cast<std::uint8_t>(head_frame_offset + 13);
        head->set_image_index(static_cast<std::uint8_t>(
            head->image_index_base() + head_frame_offset +
            head->attachment_frame_index));
    }
}

} // namespace creatures1::creatures
