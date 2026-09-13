#pragma once

#include "genome.hpp"

#include "../display/image.hpp"
#include "../objects/entity.hpp"
#include "../world/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace creatures1::creatures {

// Body filename probing and file I/O are resource/package concerns.  The
// body module owns the exact fallback order and .ATT interpretation; the
// host supplies directories, existence checks, and text from the C1 project
// resource staging area.  Keeping the text boundary here preserves the
// native std::istream ownership without making the clean body model depend
// on MSVC stream internals.
class BodyResourceHost {
public:
    virtual ~BodyResourceHost() = default;
    virtual std::string resource_directory(int directory_index) const = 0;
    virtual std::string body_data_directory() const = 0;
    virtual bool regular_file_exists(std::string_view path) const = 0;
    virtual bool read_text_file(std::string_view path,
                                std::string& text) const = 0;
};

constexpr std::size_t kAttachmentViewCount = 10;
constexpr std::size_t kLimbChainCount = 6;

// The six body chains are ordered as they are in C1's BodyData table and in
// the original Entity header: head, left leg, right leg, left arm, right arm,
// and tail.
enum class BodyLimbChain : std::size_t {
    head = 0,
    left_leg = 1,
    right_leg = 2,
    left_arm = 3,
    right_arm = 4,
    tail = 5,
};

enum class FacingDirection : std::uint8_t {
    north = 0,
    south = 1,
    east = 2,
    west = 3,
};

enum class DownFoot : std::uint8_t {
    left = 0,
    right = 1,
};

struct BodyAttachmentTable {
    std::array<std::array<std::uint8_t, kAttachmentViewCount>,
               kLimbChainCount>
        join_x{};
    std::array<std::array<std::uint8_t, kAttachmentViewCount>,
               kLimbChainCount>
        join_y{};
};

struct LimbAttachmentTable {
    std::array<std::uint8_t, kAttachmentViewCount> anchor_a_x{};
    std::array<std::uint8_t, kAttachmentViewCount> anchor_a_y{};
    std::array<std::uint8_t, kAttachmentViewCount> anchor_b_x{};
    std::array<std::uint8_t, kAttachmentViewCount> anchor_b_y{};
};

// Render state shared by the body and each limb.  The gallery is supplied by
// the display/resource owner; body code owns frame selection, attachment
// tables, positions, and the recovered initial state.
struct BodyPart : objects::Entity {
    explicit BodyPart(objects::EntityRegistryHost* registry = nullptr);
    virtual ~BodyPart();

    BodyPart(const BodyPart&) = delete;
    BodyPart& operator=(const BodyPart&) = delete;

    // Native Body and Limb construction share this exact initial render
    // state: pose 1, attachment frame 1, and the first image at base+1.
    // Gallery acquisition remains owned by the display/Skeleton boundary.
    void configure_render_state(display::Gallery* gallery,
                                std::uint8_t image_index_base,
                                int render_plane);

    virtual void serialize(objects::EntityArchive& archive);
    std::uint8_t pose_frame_index = 0;
    std::uint8_t attachment_frame_index = 0;
};

// C1 has a distinct Body class layered over BodyPart.  The body owns the
// six-chain attachment matrices; Skeleton owns the Body instance and limb
// chains.  Keeping this as a real type prevents Body::Serialize and
// BodyPart::Serialize from collapsing into one decompiler-shaped record.
struct Body : BodyPart {
    explicit Body(objects::EntityRegistryHost* registry = nullptr);
    ~Body() override;

    Body(const Body&) = delete;
    Body& operator=(const Body&) = delete;

    bool load_attachment_table(std::uint32_t genus,
                               GenomeSex sex,
                               GenomeLifeStage life_stage,
                               std::uint32_t variant,
                               const BodyResourceHost& resources);

    void serialize(objects::EntityArchive& archive) override;

    BodyAttachmentTable attachment_table{};
};

struct LimbPart : BodyPart {
    explicit LimbPart(objects::EntityRegistryHost* registry = nullptr);
    ~LimbPart() override;

    void serialize(objects::EntityArchive& archive) override;

    LimbAttachmentTable attachment_table{};
    LimbPart* next_in_chain = nullptr;
};

// C1's body resources use a fixed 0..13 part namespace.  Keep the wire/file
// value explicit, but make callers name the recovered part rather than pass
// an unbounded integer through the body loader.
enum class BodyPartIndex : std::uint32_t {
    head = 0,
    body = 1,
    left_thigh = 2,
    left_shin = 3,
    left_foot = 4,
    right_thigh = 5,
    right_shin = 6,
    right_foot = 7,
    left_humerus = 8,
    left_radius = 9,
    right_humerus = 10,
    right_radius = 11,
    tail_root = 12,
    tail_tip = 13,
};

constexpr std::uint32_t body_part_number(BodyPartIndex part) noexcept {
    return static_cast<std::uint32_t>(part);
}

// The appearance gene selects one of five source-owned groups. Each group
// contains up to six body parts; an empty slot terminates that group's native
// loop. These are the exact 0045ab40 entries, expressed as typed optional
// parts rather than the decompiler's raw -1 sentinel.
enum class BodyAppearanceGroup : std::uint8_t {
    head = 0,
    body = 1,
    legs = 2,
    arms = 3,
    tail = 4,
};

constexpr std::array<std::array<std::optional<BodyPartIndex>, 6>, 5>
    kAppearancePartGroupMembers{{
        {{BodyPartIndex::head, std::nullopt, std::nullopt, std::nullopt,
          std::nullopt, std::nullopt}},
        {{BodyPartIndex::body, std::nullopt, std::nullopt, std::nullopt,
          std::nullopt, std::nullopt}},
        {{BodyPartIndex::left_foot, BodyPartIndex::left_shin,
          BodyPartIndex::left_thigh, BodyPartIndex::right_foot,
          BodyPartIndex::right_shin, BodyPartIndex::right_thigh}},
        {{BodyPartIndex::left_radius, BodyPartIndex::left_humerus,
          BodyPartIndex::right_radius, BodyPartIndex::right_humerus,
          std::nullopt, std::nullopt}},
        {{BodyPartIndex::tail_tip, BodyPartIndex::tail_root, std::nullopt,
          std::nullopt, std::nullopt, std::nullopt}},
    }};

// The native table is byte[14] at 0045ab2c. It is indexed by BodyPartIndex,
// not by appearance-group slot: head has 26 frames and every other
// body-resource part has 10.
constexpr std::array<std::uint8_t, 14> kBodyPartSpriteFrameCounts{
    26, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};

struct BodyAppearanceSelection {
    std::array<std::uint8_t, 14> variant_by_part{};
    std::uint32_t tail_variant_count = 0;
};

// Consumes all CREATURE appearance genes (family 2, subtype 2, modulus 7,
// stage ignored), exactly as Skeleton::LoadGenome does before sizing the
// temporary creature SPR. Genome remains the payload/cursor owner; this
// function owns only the body-part mapping and selector normalization.
BodyAppearanceSelection read_body_appearance_genes(Genome& genome);

using BodyPartFilenameStem = std::uint32_t;

struct BodyPartResourcePath {
    BodyPartFilenameStem stem = 0;
    std::string path;
};

BodyPartResourcePath resolve_existing_body_part_filename_with_fallback(
    BodyPartIndex body_part_index,
    std::uint32_t genus,
    GenomeSex sex,
    GenomeLifeStage life_stage,
    std::uint32_t variant,
    std::string_view extension,
    int resource_directory_index,
    const BodyResourceHost& resources);

LimbAttachmentTable load_body_part_attachment_data(
    BodyPartIndex body_part_index,
    std::uint32_t genus,
    GenomeSex sex,
    GenomeLifeStage life_stage,
    std::uint32_t variant,
    const BodyResourceHost& resources);

std::unique_ptr<BodyPart> create_body_part(
    objects::EntityRegistryHost* registry = nullptr);
std::unique_ptr<Body> create_body(
    objects::EntityRegistryHost* registry = nullptr);
std::unique_ptr<LimbPart> create_limb(
    objects::EntityRegistryHost* registry = nullptr);

} // namespace creatures1::creatures
