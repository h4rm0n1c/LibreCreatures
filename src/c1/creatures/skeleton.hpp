#pragma once

#include "../display/gallery.hpp"
#include "../display/image.hpp"
#include "../objects/object.hpp"
#include "../world/geometry.hpp"

#include "body.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace creatures1::creatures {

class Skeleton;

class SkeletonWorldHost {
public:
    virtual ~SkeletonWorldHost() = default;
    virtual void move_by(Skeleton& skeleton, int delta_x, int delta_y) = 0;
    virtual void queue_dirty_world_rect(const world::WorldRect& bounds) = 0;
};

class SkeletonLifetimeHost {
public:
    virtual ~SkeletonLifetimeHost() = default;
    virtual void destroy_limb(LimbPart& limb) = 0;
    virtual void stop_continuous_sound(int sound_handle) = 0;
    virtual void remove_from_renderable_set(Skeleton& skeleton) = 0;
    virtual void unregister_from_object_registry(Skeleton& skeleton) = 0;
    virtual void release_gallery(display::Gallery& gallery) = 0;
};

class BodySpriteFileHost {
public:
    virtual ~BodySpriteFileHost() = default;
    virtual std::string body_sprite_path(std::string_view image_directory,
                                         std::uint32_t genome_filename) const = 0;
    virtual bool read_sprite_header(std::string_view path,
                                    std::uint16_t& image_count,
                                    std::uint32_t& first_frame_offset,
                                    std::uint16_t& first_frame_width,
                                    std::uint16_t& first_frame_height) = 0;
};

// The native LoadGenome render-plane scan reads the creature registry and
// dispatches the Skeleton/Object virtual render-plane accessor on each entry.
// Random generation and registry storage remain application/runtime policy;
// Skeleton owns only the candidate range and collision interval.
class SkeletonRenderPlaneHost {
public:
    virtual ~SkeletonRenderPlaneHost() = default;
    virtual std::uint32_t next_random_value() = 0;
    virtual std::size_t creature_count() const = 0;
    virtual objects::Object* creature_at(std::size_t index) const = 0;
};

// LoadGenome receives these services from the application/display boundary.
// Skeleton owns the body graph and part initialization; the host owns staged
// body text, gallery lifetime, and any Entity-array registration used by the
// platform adapter.
struct SkeletonBodyBuildServices {
    BodyResourceHost& body_resources;
    objects::EntityRegistryHost* entity_registry = nullptr;
    display::Gallery* gallery = nullptr;
    GenomeSex sex = GenomeSex::male;
    GenomeLifeStage life_stage = GenomeLifeStage::stage_zero;
    std::array<std::uint8_t, 14> image_index_base{};
    int normal_render_plane = 0;
};

// This is the explicit LoadGenome sprite-extraction boundary recovered from
// the native Skeleton implementation.  Skeleton owns the part order, frame
// sizing, palette controls, and image-base table; display owns gallery/image
// metadata and pixel-cache policy. This bundle owns its resolved directory
// strings; the resource/platform hosts own the native file handles.
struct SkeletonSpriteBuildServices {
    BodyResourceHost& body_resources;
    SkeletonLifetimeHost& gallery_lifetime_host;
    display::GalleryHost& gallery_host;
    display::GalleryRegistry& gallery_registry;
    display::SpriteIndexOutputFileSystem& output_files;
    display::BinaryResourceFileSystem& binary_files;
    display::PixelCacheState& pixel_cache;
    display::SpriteFileCache& sprite_files;
    const display::PaletteDtaBuffer& palette;
    std::uint32_t& palette_build_count;
    std::string secondary_image_directory;
    std::string primary_image_directory;
    objects::EntityRegistryHost* entity_registry = nullptr;
    int normal_render_plane = 100;
};

constexpr std::size_t kPoseStringLength = 15;
constexpr std::size_t kPoseTableEntryCount = 100;
constexpr std::size_t kAnimationSequenceCapacity = 32;
constexpr std::size_t kGaitTableEntryCount = 8;
constexpr std::size_t kGaitAnimationSequenceCapacity = 18;

struct PoseString {
    std::array<char, kPoseStringLength> characters{};
    char nul_terminator = '\0';

    bool operator==(const PoseString& other) const {
        return characters == other.characters;
    }
};

// These are the five controls passed by C1 Skeleton::LoadGenome to the
// display palette remapper.  The first three are averages of CREATURE subtype
// 6 pigment genes; the last two are optionally blended from a following gext
// extension record.  Palette generation remains in display/palette.cpp.
struct CreaturePaletteControls {
    std::array<std::uint8_t, 3> pigment_channels{0x80, 0x80, 0x80};
    std::uint8_t hue_rotation = 0x80;
    std::uint8_t colour_swap = 0x80;
};

// Skeleton extends the Object archive with the MFC string-record operation.
// Object keeps the primitive/reference contract; Skeleton owns the fixed
// pose-table counts and the Body/Limb reference order.
class SkeletonArchive : public objects::ObjectArchive {
public:
    virtual ~SkeletonArchive() = default;
    virtual std::string read_string() = 0;
    virtual void write_string(std::string_view value) = 0;
};

CreaturePaletteControls read_creature_palette_controls(Genome& genome);

// Native Creature *is* its Skeleton's Object, and overrides Object's
// ReferencesObject and ClearReferencesToObject (vtable slots 18 and 19,
// Creature::ReferencesObject @0x0040e4d0 and ClearReferencesToObject
// @0x0040edc0) to cover its sleep indicator, its CAOS pointer and all forty
// attention records.  Here the Creature holds the Skeleton and only the
// Skeleton is in the object registry, so the Skeleton forwards to its owner
// for the state it cannot see.
class SkeletonReferenceOwner {
public:
    virtual ~SkeletonReferenceOwner() = default;
    virtual bool owner_references_object(
        const objects::Object* candidate) const = 0;
    virtual void owner_clear_references_to(
        const objects::Object* candidate) = 0;
};

class Skeleton : public objects::Object {
public:
    void set_reference_owner(SkeletonReferenceOwner* owner) {
        reference_owner_ = owner;
    }
    explicit Skeleton(SkeletonLifetimeHost* lifetime_host = nullptr);
    ~Skeleton() override;

    Skeleton(const Skeleton&) = delete;
    Skeleton& operator=(const Skeleton&) = delete;

    void set_lifetime_host(SkeletonLifetimeHost* lifetime_host) {
        lifetime_host_ = lifetime_host;
    }

    // This is the source-level owner operation represented by the native
    // destructor.  The platform host owns registry/render-set bookkeeping;
    // Skeleton owns the six chains, body, and gallery reference policy.
    void clear_body_parts_and_gallery(SkeletonLifetimeHost& lifetime_host);

    void initialize_pose_and_motion_state();
    // Reads the C1 CREATURE genus records and applies the native classifier
    // byte layout. The genome payload owns the source data; Skeleton owns the
    // resulting identity used by body/resource construction.
    void load_genus_identity(Genome& genome);
    // Reads and stores the C1 appearance-gene selections. Sprite extraction
    // and gallery ownership remain separate stages of LoadGenome.
    void load_appearance_genes(Genome& genome);
    // Constructs the recovered Body plus six linked limb chains. The caller
    // supplies the already-built creature gallery and per-part image bases;
    // Skeleton owns the resulting graph and its ATT data.
    bool construct_body_parts(const SkeletonBodyBuildServices& services);
    // Rebuilds the temporary creature SPR from the recovered part galleries,
    // then acquires the final gallery for genome_source_filename. This is a
    // semantic owner operation, not a generic free-function export of the
    // decompiler's local buffers.
    bool build_creature_sprite_gallery(
        Genome& genome, const SkeletonSpriteBuildServices& services);
    // Runs the recovered Skeleton-owned LoadGenome stages in native order.
    // Creature's stimulus/pose/gait/instinct consumers remain its own caller
    // stages; this operation owns only identity, appearance, sprite, and
    // body-graph construction.
    bool load_genome(Genome& genome,
                     const SkeletonSpriteBuildServices& services,
                     SkeletonRenderPlaneHost& render_plane_host,
                     objects::ObjectSoundPlaybackHost& sound_host);
    void update_limb_frames_for_pose();

    // Reads a bracketed animation sequence such as "[13141516R]".  The
    // returned pointer preserves the executable's caller contract: it points
    // two bytes past the closing bracket.
    char* parse_animation_sequence(char* sequence_text, const char* sequence_end);
    bool is_animation_sequence_complete() const;
    void set_target_pose_from_table_index(std::size_t pose_table_index);

    // Skeleton is an Object at the CAOS dispatch boundary.  These overrides
    // are the creature implementations of the native Object vtable slots
    // used by ANIM, OVER, and POSE; the helpers above are the Skeleton-side
    // operations those virtual entry points delegate to.
    char* parse_image_sequence(char* sequence_text, const char* sequence_end,
                               int part_index) override;
    bool image_sequence_is_empty(int part_index) const override;
    bool set_relative_image_index(objects::CaosValue relative_index,
                                  int part_index) override;

    bool set_target_pose_string(std::string_view target_pose);
    bool is_pose_at_target() const;
    void apply_pose_string(std::string_view pose_text,
                           objects::ObjectSoundPlaybackHost& sound_host);
    void update_anchor_and_bounds(
        objects::ObjectSoundPlaybackHost& sound_host);
    void serialize(SkeletonArchive& archive,
                   objects::ObjectSoundPlaybackHost* sound_host = nullptr);
    void set_down_foot_position_and_recompute_layout(
        int x, int y, objects::ObjectMovementBoundsHost& world_host,
        objects::ObjectSoundPlaybackHost& sound_host);
    void set_down_foot_position_and_invalidate_bounds(
        int x, int y, SkeletonWorldHost& world_host,
        objects::ObjectSoundPlaybackHost& sound_host);
    std::uint32_t select_target_pose_for_motion(bool force_interaction_pose);
    std::uint32_t select_target_pose_for_motion_guarded(
        bool force_interaction_pose);
    // True when the creature's generated <moniker>.spr disagrees with the
    // gallery built from its genome, which is native's cue to write the file
    // again (ValidateBodySprites @00408080).  A file that cannot be opened at
    // all is not a mismatch: native leaves BL clear on that path.
    bool body_sprites_are_stale(std::string_view image_directory,
                                BodySpriteFileHost& file_host) const;

    bool references_object(objects::Object* object) const override;
    void clear_references_to(objects::Object* object) override;

    // Skeleton's vtable slot 20 -- Object's MoveBy -- is TranslateBy
    // (0x0043c0b0), not Object's no-op stub.  Vehicle::Tick @0x0042bd90 calls
    // that slot on every object whose bounds reference is the vehicle, which
    // is how a creature riding a lift is carried.  Without the override the
    // base no-op ran, so a passenger's movement bounds followed the lift (its
    // x stayed clamped to the cabin) while its position did not: going up the
    // bounds clamp in UpdateAnchorAndBounds shoved it along, going down the
    // floor moved away beneath it and nothing pulled it after.
    void move_by(int delta_x, int delta_y) override;

    void translate_by(int delta_x, int delta_y);
    void update_and_invalidate_bounds(int delta_x,
                                      int delta_y,
                                      SkeletonWorldHost& world_host);

    int sprite_bounds_width() const;
    int sprite_bounds_height() const;
    // Object's bounds accessor. Skeleton::CopySpriteBounds @ 0x00406d40 fills
    // slot 26 of the Creature vtable, so a creature's bounds are its sprite
    // bounds; the base class returns an always-empty rectangle.
    bool get_bounds(world::WorldRect* out_bounds) const override;
    // Creature::GetPartCentre @0x004096b0, slot 29.  Part 1 is the body
    // image's centre; every other part index is the head chain's end.  With
    // no override the Object no-op ran, so a creature's centre read as
    // whatever the caller had initialised -- (0,0) in the attention sweep,
    // which dropped any creature as an attention target every tick, and left
    // motion_target unset when one creature attended another.
    void get_part_center(int* out_x, int* out_y,
                         std::int32_t part_index) const override;
    int render_plane() const override;
    int body_render_plane() const;
    std::uint32_t classifier_base() const { return Object::classifier_base(); }
    int sprite_bounds_min_x() const;
    int sprite_bounds_min_y() const;
    // Native Skeleton::vftable slots 30-33 (GetSpriteBoundsMinX/MinY/Width/
    // Height @ 0x00406d70/0x00406d80/0x00406d20/0x00406d30) override
    // Object's sound-source/visual-size virtuals directly with
    // sprite_bounds' own min_x/min_y and max-min width/height -- confirmed
    // via disassembly of Skeleton's real vftable at 0x0045ac8c. Without
    // these overrides every Creature falls through to Object's base stubs
    // (always 0), which explains every creature reporting position (0,0)
    // and size (0,0) to CAOS (`posl`/`post`, sound panning/attenuation) and
    // to any other virtual-based caller of these four members.
    int sound_source_x() const override;
    int sound_source_y() const override;
    int current_visual_width() const override;
    int current_visual_height() const override;

    std::unique_ptr<Body> body;
    std::array<LimbPart*, kLimbChainCount> limb_chain_heads{};
    std::array<int, kLimbChainCount> limb_chain_end_x{};
    std::array<int, kLimbChainCount> limb_chain_end_y{};
    int down_foot_x = 0;
    int down_foot_y = 0;
    world::WorldRect sprite_bounds{};
    world::WorldRect previous_sprite_bounds{};
    std::uint32_t genome_source_filename = 0;
    std::uint32_t father_moniker = 0;
    std::uint32_t mother_moniker = 0;
    // C1 normalizes the CREATURE genus selector to the four values 0..3;
    // resource filename generation adds the sex/genus directory encoding.
    std::uint8_t creature_genus_selector = 0;
    std::array<std::uint8_t, 14> body_part_variant_by_index{};
    std::array<std::uint8_t, 14> body_part_image_index_base{};
    std::uint32_t tail_variant_count = 0;
    PoseString current_pose{};
    PoseString target_pose{};
    std::array<PoseString, kPoseTableEntryCount> pose_string_table{};
    // Each genome gait byte expands to two decimal pose digits. The native
    // table stores at most eight gait bytes, then 'R' and a terminator.
    std::array<std::array<char, kGaitAnimationSequenceCapacity>,
               kGaitTableEntryCount>
        gait_animation_table{};
    std::array<char, kAnimationSequenceCapacity> animation_sequence{};
    std::size_t animation_cursor = 0;
    std::uint8_t drive_threshold_state = 0;
    bool sleep_indicator_active = false;
    // Ghidra's exact Skeleton layout identifies this as the native byte at
    // Skeleton+0x800; Creature's emitter locus resolver addresses it
    // directly.
    std::uint8_t pose_transition_component_count = 0;
    bool eyes_open = false;
    FacingDirection facing_direction = FacingDirection::north;
    DownFoot down_foot = DownFoot::left;

    objects::Object* motion_link = nullptr;
    std::int32_t motion_target_part_index = 0;
    int motion_target_x = 0;
    int motion_target_y = 0;
    objects::Object* caos_object_pointer = nullptr;
    SkeletonReferenceOwner* reference_owner_ = nullptr;
    bool boundary_correction_pending = false;
    int normal_render_plane = 100;
    int continuous_sound_handle = -1;
    // The creature's composite sprite gallery is the Object one native
    // keeps at +0x40: the world save restores it there, and
    // ValidateBodySprites compares the generated .spr against it.  A
    // second Skeleton-local copy left that restored pointer unread.

    // The native record stores Body separately from the Object base and keeps
    // its six Limb chains as linked allocations.  Those ownership facts are
    // explicit here instead of being emitted as DAT/FUN-shaped fields.
    SkeletonLifetimeHost* lifetime_host_ = nullptr;

    // Rebuilds all body/limb image indices, positions, chain endpoints, and
    // the wrapped sprite bounds from the current down-foot pose.  This is the
    // semantic owner of Ghidra 0043b5a0 (Skeleton::RecomputeBodyPartLayout).
    void recompute_body_part_layout();

private:
    void select_normal_render_plane(SkeletonRenderPlaneHost& host);
};

} // namespace creatures1::creatures
