// Function-level equivalence: the creature skeleton of CE 10.3 (body layout,
// stepping, poses and movement) against creatures::Skeleton and
// Creature::advance_pose_animation.
//
//   RecomputeBodyPartLayout @0043b5a0        recompute_body_part_layout
//   UpdateLimbFramesForPose @0043b8f0        update_limb_frames_for_pose
//   UpdateAnchorAndBounds @0043b950          update_anchor_and_bounds
//   ApplyPoseString @0043ba20                apply_pose_string
//   AdvancePoseAnimation @0043bb20           Creature::advance_pose_animation
//   SetTargetPoseString @0043be00            set_target_pose_string
//   IsPoseAtTarget @0043be70                 is_pose_at_target
//   SetTargetPoseFromTableIndex @0043bf10    set_target_pose_from_table_index
//   SelectTargetPoseForMotion @0043bf40      select_target_pose_for_motion
//   SetDownFootPositionAndRecomputeLayout @0043c070
//                                            set_down_foot_position_and_recompute_layout
//   TranslateBy @0043c0b0                    translate_by
//   SetDownFootPositionAndInvalidateBounds @0043c2f0
//                                            set_down_foot_position_and_invalidate_bounds
//   UpdateAndInvalidateBounds @0043c380      update_and_invalidate_bounds
//
// Each case builds one creature body twice: an LC Skeleton with a Body and
// six limb chains (head 1, legs 3, arms 2, tail 2 parts), and native
// Creature, Body and Limb blocks with the same state at the offsets the
// functions read.  Attachment tables, image sizes (a 256-image gallery,
// so every byte image index is in range), image bases, poses, render
// planes, facing, down foot, eyes, expression, movement bounds, current
// and target poses, the pose table, the motion link and its target are
// random.  Then 16 random operations run on both sides; after each, the
// whole skeleton must match: every part's image, position, pose, view and
// plane, the down foot, chain ends, sprite bounds, previous bounds, the
// boundary flag, facing, poses, animation sequence and cursor, the
// transition count, return values and the queued dirty rectangles.
//
// The native vtable is the Skeleton's for the slots these functions call
// (20 TranslateBy, 26 CopySpriteBounds, 27 body plane, 30/31 sprite
// corner).  The renderer's dirty queue (00412610) and PlaySoundEffect
// (0042ce20) are recorders.  LC's hosts are the game's: render_plane is
// the object's own, move_by is translate_by, sounds are muted (a step is
// seen in the down foot).  UpdateMovementBounds and the nearest-room
// function run as native code on a random room table.
//
// Not compared: the step sound itself.

#include "native_image.hpp"

#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "objects/object.hpp"
#include "world/map.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace creatures1;
using creatures::Skeleton;

constexpr std::uint32_t kNativeDebugLog = 0x00418000;
constexpr std::uint32_t kRecompute = 0x0043b5a0, kLimbFrames = 0x0043b8f0, kAnchor = 0x0043b950,
                        kApplyPose = 0x0043ba20, kAdvancePose = 0x0043bb20, kSetTarget = 0x0043be00,
                        kAtTarget = 0x0043be70, kTargetFromTable = 0x0043bf10,
                        kSelectForMotion = 0x0043bf40, kSetFootRecompute = 0x0043c070,
                        kTranslate = 0x0043c0b0, kSetFootInvalidate = 0x0043c2f0,
                        kUpdateInvalidate = 0x0043c380;
constexpr std::uint32_t kCopySpriteBounds = 0x00406d40, kBodyPlane = 0x00406d60,
                        kSpriteMinX = 0x00406d70, kSpriteMinY = 0x00406d80;
constexpr std::uint32_t kDirtyQueue = 0x00412610, kPlaySound = 0x0042ce20;
constexpr std::uint32_t kDocument = 0x00468d84, kMapData = 0x00468d64, kEditObject = 0x00468d68;

// Native Skeleton (Creature) layout.
constexpr std::size_t kBlockSize = 0x2d00;
constexpr std::size_t kBoundsMode = 0x8, kBoundsFlags = 0x9, kMovementBounds = 0xc,
                      kBody = 0x5c, kChains = 0x60, kFacing = 0x78, kDownFoot = 0x79, kFootX = 0x7c,
                      kFootY = 0x80, kCurrentPose = 0x88, kTargetPose = 0x98, kSequence = 0xa8,
                      kCursor = 0xc8, kChainEndX = 0xcc, kChainEndY = 0xe4, kSpriteBounds = 0xfc,
                      kPreviousBounds = 0x10c, kBoundaryFlag = 0x11c, kExpression = 0x11d,
                      kEyesOpen = 0x11e, kPoseTable = 0x120, kMotionLink = 0x7f0, kMotionX = 0x7f4,
                      kMotionY = 0x7f8, kTransitions = 0x800;
// Native Entity (Body, Limb): gallery +4, image +8, base +9, plane +0xc,
// position +0x10/+0x14, pose +0x20, view +0x24, next +0x28; a limb's
// anchors from +0x2c (a x, a y, b x, b y, ten views each); the body's
// joins from +0x28 (x, six chains of ten) and +0x64 (y).
constexpr std::size_t kPartSize = 0xa0;
constexpr std::size_t kGallery = 0x4, kImage = 0x8, kBase = 0x9, kPlane = 0xc, kX = 0x10, kY = 0x14,
                      kPose = 0x20, kView = 0x24, kNext = 0x28, kLimbAnchors = 0x2c, kBodyJoinX = 0x28,
                      kBodyJoinY = 0x64;
constexpr std::size_t kImageStride = 0x28, kImageWidth = 0x10, kImageHeight = 0x14;
constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14;

const int kChainLengths[6] = {1, 3, 3, 2, 2, 2};

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next()); }
    std::uint32_t below(std::uint32_t n) { return n == 0 ? 0 : next() % n; }
    bool chance(std::uint32_t one_in) { return below(one_in) == 0; }
    int range(int low, int high) { return low + static_cast<int>(below(static_cast<std::uint32_t>(high - low + 1))); }
};

// ---- recorders ----
std::vector<world::WorldRect> g_native_dirty, g_lc_dirty;
int g_native_sounds = 0;
void __attribute__((thiscall)) native_dirty(void*, int l, int t, int r, int b) {
    g_native_dirty.push_back({l, t, r, b});
}
void __attribute__((thiscall)) native_sound(void*, std::uint32_t, int, int) { ++g_native_sounds; }
int __cdecl native_strncpy_s(char* d, std::size_t n, const char* s, std::size_t count) {
    std::size_t i = 0;
    for (; i < count && i + 1 < n && s[i] != '\0'; ++i) d[i] = s[i];
    d[i] = '\0';
    return 0;
}
int __cdecl native_strcpy_s(char* d, std::size_t n, const char* s) {
    std::size_t i = 0;
    for (; i + 1 < n && s[i] != '\0'; ++i) d[i] = s[i];
    d[i] = '\0';
    return 0;
}
struct NativeRect {
    std::int32_t left, top, right, bottom;
};
int __attribute__((stdcall)) point_in_rect(const NativeRect* r, std::int32_t x, std::int32_t y) {
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

// The motion link's render plane (slot 27).
std::map<const void*, int> g_link_plane;
int __attribute__((thiscall)) native_link_plane(void* self) { return g_link_plane.at(self); }
void __attribute__((thiscall)) native_unexpected_virtual(void*) {
    std::fprintf(stderr, "an unexpected virtual was called on a native object\n");
    std::exit(5);
}
void* g_skeleton_vtable[64];
void* g_link_vtable[64];

// ---- LC hosts ----
class PlainObject final : public objects::Object {
public:
    int plane = 0;
    int render_plane() const override { return plane; }
};

class Hosts final : public creatures::CreaturePoseAnimationHost,
                    public creatures::SkeletonWorldHost,
                    public objects::ObjectSoundPlaybackHost,
                    public objects::ObjectMovementBoundsHost {
public:
    world::MapRoomTable rooms;
    // CreaturePoseAnimationHost and SkeletonWorldHost
    int render_plane(const objects::Object& o) const override { return o.render_plane(); }
    void queue_dirty_world_rect(const world::WorldRect& r) override { g_lc_dirty.push_back(r); }
    void move_by(Skeleton& s, int dx, int dy) override { s.translate_by(dx, dy); }
    // ObjectSoundPlaybackHost: muted.
    world::ViewportBounds sound_viewport() const override { return {}; }
    bool sounds_muted() const override { return true; }
    sound::SoundManager& sound_manager() override {
        std::fprintf(stderr, "sound_manager was not expected\n");
        std::exit(5);
    }
    bool debug_console_available() const override { return false; }
    void log_sound_event(LogEvent, sound::SoundId, int) override {}
    // ObjectMovementBoundsHost, as the game's.
    void clear_edit_object() override {}
    void find_nearest_room_bounds_at_point(int x, int y, world::WorldRect& out) const override {
        world::MapRectangle m{out.min_x, out.min_y, out.max_x, out.max_y};
        world::find_nearest_room_bounds_at_point(rooms, x, y, m);
        out = {m.left, m.top, m.right, m.bottom};
    }
    world::WorldRect vehicle_local_bounds(const objects::Object&) const override { return {}; }
    int vehicle_primary_entity_x(const objects::Object&) const override { return 0; }
    int vehicle_primary_entity_y(const objects::Object&) const override { return 0; }
    objects::Object* edit_object() const override { return nullptr; }
};

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

// A pose string: digits, with X (keep) and, for targets, ? and !.
void random_pose(Random& random, char* out, bool target) {
    for (int i = 0; i < 15; ++i) {
        if (random.chance(4)) out[i] = 'X';
        else if (i == 0) out[i] = static_cast<char>('0' + random.below(4));
        else out[i] = static_cast<char>('0' + random.below(i == 1 ? 6 : 10));
    }
    if (target && random.chance(3)) out[0] = random.chance(2) ? '?' : '!';
    if (target && random.chance(3)) out[1] = '?';
    out[15] = '\0';
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <Creatures.exe> [-cases N] [-seed S]\n", argv[0]);
        return 2;
    }
    long cases = 2000;
    std::uint64_t seed = 0x5ce1e7u;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-cases" && i + 1 < argc) cases = std::atol(argv[++i]);
        else if (arg == "-seed" && i + 1 < argc) seed = std::strtoull(argv[++i], nullptr, 0);
    }

    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    if (!image.bind_import("!strncpy_s", reinterpret_cast<void*>(&native_strncpy_s)) ||
        !image.bind_import("!strcpy_s", reinterpret_cast<void*>(&native_strcpy_s)) ||
        !image.bind_import("!PtInRect", reinterpret_cast<void*>(&point_in_rect))) {
        std::fprintf(stderr, "the exe does not import strncpy_s, strcpy_s and PtInRect\n");
        return 2;
    }
    image.make_return(kNativeDebugLog);
    image.redirect(kDirtyQueue, reinterpret_cast<const void*>(&native_dirty));
    image.redirect(kPlaySound, reinterpret_cast<const void*>(&native_sound));
    for (auto& slot : g_skeleton_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    for (auto& slot : g_link_vtable) slot = reinterpret_cast<void*>(&native_unexpected_virtual);
    g_skeleton_vtable[20] = image.at<void*>(kTranslate);
    g_skeleton_vtable[26] = image.at<void*>(kCopySpriteBounds);
    g_skeleton_vtable[27] = image.at<void*>(kBodyPlane);
    g_skeleton_vtable[30] = image.at<void*>(kSpriteMinX);
    g_skeleton_vtable[31] = image.at<void*>(kSpriteMinY);
    g_link_vtable[27] = reinterpret_cast<void*>(&native_link_plane);
    // The document's renderer pointer, for the dirty queue.
    std::vector<std::uint8_t> document(0x200, 0);
    *image.at<void**>(kDocument) = document.data();
    std::vector<std::uint8_t> map_data(0x1000, 0);
    *image.at<void**>(kMapData) = map_data.data();
    *image.at<void**>(kEditObject) = nullptr;

    using Thiscall0 = int(__attribute__((thiscall)) *)(void*);
    using Thiscall1 = int(__attribute__((thiscall)) *)(void*, std::uintptr_t);
    using Thiscall2 = int(__attribute__((thiscall)) *)(void*, std::uintptr_t, std::uintptr_t);
    auto call0 = [&](std::uint32_t at, void* self) { return image.at<Thiscall0>(at)(self); };
    auto call1 = [&](std::uint32_t at, void* self, std::uintptr_t a) { return image.at<Thiscall1>(at)(self, a); };
    auto call2 = [&](std::uint32_t at, void* self, std::uintptr_t a, std::uintptr_t b) {
        return image.at<Thiscall2>(at)(self, a, b);
    };

#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif
    const long verbose_case =
        std::getenv("EQUIVALENCE_CASE") ? std::atol(std::getenv("EQUIVALENCE_CASE")) : -1;
    Random random{seed};
    long differing = 0, operations = 0, steps = 0, wall_hits = 0;
    const char* names[] = {"recompute layout", "update anchor", "limb frames", "apply pose",
                           "advance pose", "set target pose", "pose at target", "target from table",
                           "select for motion", "set foot and recompute", "translate",
                           "set foot and invalidate", "update and invalidate"};
    for (long index = 0; index < cases; ++index) {
        // ---- galleries: 256 images of random size ----
        display::Gallery lc_gallery;
        lc_gallery.image_count = 256;
        lc_gallery.images = std::make_unique<display::Image[]>(256);
        std::vector<std::uint8_t> native_images(256 * kImageStride, 0);
        std::vector<std::uint8_t> native_gallery(16, 0);
        field<void*>(native_gallery.data(), 4) = native_images.data();
        for (int i = 0; i < 256; ++i) {
            const int w = random.range(4, 60), h = random.range(4, 60);
            lc_gallery.images[i].configure(&lc_gallery, w, h, 0);
            field<std::int32_t>(native_images.data() + i * kImageStride, kImageWidth) = w;
            field<std::int32_t>(native_images.data() + i * kImageStride, kImageHeight) = h;
        }

        // ---- rooms, for UpdateMovementBounds ----
        std::vector<world::MapRoom> rooms;
        const int base_x = random.range(200, 8000), base_y = random.range(200, 1000);
        for (std::uint32_t r = 0, n = random.below(6); r < n; ++r) {
            world::MapRoom room;
            const int left = base_x + random.range(-600, 200), top = base_y + random.range(-300, 0);
            room.bounds = {left, top, left + random.range(100, 900), top + random.range(100, 400)};
            rooms.push_back(room);
        }
        std::memset(map_data.data(), 0, map_data.size());
        field<std::int32_t>(map_data.data(), kRoomCount) = static_cast<std::int32_t>(rooms.size());
        for (std::size_t r = 0; r < rooms.size(); ++r) {
            std::memcpy(map_data.data() + kRooms + r * kRoomStride, &rooms[r].bounds, 16);
        }

        // ---- the creature, both sides ----
        auto creature = std::make_unique<creatures::Creature>();
        Skeleton& lc = creature->skeleton();
        std::vector<std::uint8_t> native_storage(kBlockSize, 0);
        std::uint8_t* native = native_storage.data();
        field<void*>(native, 0) = g_skeleton_vtable;
        Hosts hosts;
        hosts.rooms = {rooms.data(), rooms.size()};

        std::vector<std::vector<std::uint8_t>> native_parts;
        std::vector<std::unique_ptr<creatures::LimbPart>> lc_limbs;
        std::vector<std::pair<creatures::BodyPart*, std::uint8_t*>> pairs;
        auto make_part = [&](creatures::BodyPart& part, std::uint8_t* raw) {
            const std::uint8_t base = random.byte();
            const std::uint8_t pose = static_cast<std::uint8_t>(random.below(6));
            const std::uint8_t view = static_cast<std::uint8_t>(random.below(10));
            const int plane = random.range(0, 10000);
            const int x = base_x + random.range(-100, 100), y = base_y + random.range(-100, 100);
            part.set_gallery(&lc_gallery);
            part.set_image_index_base(base);
            part.set_image_index(static_cast<std::uint8_t>(base + view));
            part.set_render_plane(plane);
            part.set_world_x(x);
            part.set_world_y(y);
            part.pose_frame_index = pose;
            part.attachment_frame_index = view;
            field<void*>(raw, kGallery) = native_gallery.data();
            raw[kImage] = static_cast<std::uint8_t>(base + view);
            raw[kBase] = base;
            field<std::int32_t>(raw, kPlane) = plane;
            field<std::int32_t>(raw, kX) = x;
            field<std::int32_t>(raw, kY) = y;
            field<std::int32_t>(raw, kPose) = pose;
            field<std::int32_t>(raw, kView) = view;
            pairs.push_back({&part, raw});
        };
        lc.body = creatures::create_body();
        native_parts.emplace_back(kPartSize, 0);
        std::uint8_t* native_body = native_parts.back().data();
        make_part(*lc.body, native_body);
        for (std::size_t c = 0; c < 6; ++c) {
            for (std::size_t v = 0; v < 10; ++v) {
                const std::uint8_t jx = static_cast<std::uint8_t>(random.below(64));
                const std::uint8_t jy = static_cast<std::uint8_t>(random.below(64));
                lc.body->attachment_table.join_x[c][v] = jx;
                lc.body->attachment_table.join_y[c][v] = jy;
                native_body[kBodyJoinX + 10 * c + v] = jx;
                native_body[kBodyJoinY + 10 * c + v] = jy;
            }
        }
        field<void*>(native, kBody) = native_body;
        for (std::size_t c = 0; c < 6; ++c) {
            creatures::LimbPart* previous = nullptr;
            std::uint8_t* previous_raw = nullptr;
            for (int k = 0; k < kChainLengths[c]; ++k) {
                lc_limbs.push_back(creatures::create_limb());
                creatures::LimbPart& limb = *lc_limbs.back();
                native_parts.emplace_back(kPartSize, 0);
                std::uint8_t* raw = native_parts.back().data();
                make_part(limb, raw);
                for (std::size_t v = 0; v < 10; ++v) {
                    auto& t = limb.attachment_table;
                    t.anchor_a_x[v] = static_cast<std::uint8_t>(random.below(40));
                    t.anchor_a_y[v] = static_cast<std::uint8_t>(random.below(40));
                    t.anchor_b_x[v] = static_cast<std::uint8_t>(random.below(40));
                    t.anchor_b_y[v] = static_cast<std::uint8_t>(random.below(40));
                    raw[kLimbAnchors + v] = t.anchor_a_x[v];
                    raw[kLimbAnchors + 10 + v] = t.anchor_a_y[v];
                    raw[kLimbAnchors + 20 + v] = t.anchor_b_x[v];
                    raw[kLimbAnchors + 30 + v] = t.anchor_b_y[v];
                }
                if (previous == nullptr) {
                    lc.limb_chain_heads[c] = &limb;
                    field<void*>(native, kChains + 4 * c) = raw;
                } else {
                    previous->next_in_chain = &limb;
                    field<void*>(previous_raw, kNext) = raw;
                }
                previous = &limb;
                previous_raw = raw;
            }
        }

        const auto facing = static_cast<creatures::FacingDirection>(random.below(4));
        lc.facing_direction = facing;
        native[kFacing] = static_cast<std::uint8_t>(facing);
        const bool right_foot = random.chance(2);
        lc.down_foot = right_foot ? creatures::DownFoot::right : creatures::DownFoot::left;
        native[kDownFoot] = right_foot ? 1 : 0;
        const int foot_x = base_x + random.range(-50, 50), foot_y = base_y + random.range(-50, 50);
        lc.down_foot_x = foot_x;
        lc.down_foot_y = foot_y;
        field<std::int32_t>(native, kFootX) = foot_x;
        field<std::int32_t>(native, kFootY) = foot_y;
        const bool eyes = !random.chance(4);
        lc.eyes_open = eyes;
        native[kEyesOpen] = eyes;
        const std::uint8_t expression = random.chance(2) ? 0 : static_cast<std::uint8_t>(random.below(6));
        lc.drive_threshold_state = expression;
        native[kExpression] = expression;
        const world::WorldRect movement = {foot_x - random.range(-40, 300), foot_y - random.range(100, 400),
                                           foot_x + random.range(-40, 300), foot_y + random.range(-60, 60)};
        lc.set_movement_bounds(movement);
        field<world::WorldRect>(native, kMovementBounds) = movement;
        const std::uint8_t flags = random.chance(2) ? 0x40 : 0;
        lc.set_bounds_flags_for_script(flags);
        native[kBoundsFlags] = flags;
        native[kBoundsMode] = 0;
        char pose[16];
        random_pose(random, pose, false);
        for (int i = 0; i < 15; ++i) if (pose[i] == 'X') pose[i] = '1';
        std::memcpy(lc.current_pose.characters.data(), pose, 15);
        std::memcpy(native + kCurrentPose, pose, 16);
        random_pose(random, pose, true);
        std::memcpy(lc.target_pose.characters.data(), pose, 15);
        std::memcpy(native + kTargetPose, pose, 16);
        for (std::size_t p = 0; p < creatures::kPoseTableEntryCount; ++p) {
            random_pose(random, pose, true);
            std::memcpy(lc.pose_string_table[p].characters.data(), pose, 15);
            std::memcpy(native + kPoseTable + 16 * p, pose, 16);
        }
        lc.animation_sequence.fill('\0');
        lc.animation_cursor = 0;
        field<std::uint8_t*>(native, kCursor) = native + kSequence;
        // The motion link: none, another object, or a vehicle this creature rides.
        PlainObject link;
        std::vector<std::uint8_t> native_link(0x200, 0);
        field<void*>(native_link.data(), 0) = g_link_vtable;
        const int link_kind = static_cast<int>(random.below(4));
        if (link_kind != 0) {
            link.plane = random.range(0, 10000);
            g_link_plane[native_link.data()] = link.plane;
            lc.motion_link = &link;
            field<void*>(native, kMotionLink) = native_link.data();
            if (link_kind == 3) {
                link.set_bounds_reference_object(&lc);
                field<void*>(native_link.data(), 0x1c) = native;
            }
        }
        const int motion_x = foot_x + random.range(-200, 200), motion_y = foot_y + random.range(-200, 60);
        lc.motion_target_x = motion_x;
        lc.motion_target_y = motion_y;
        field<std::int32_t>(native, kMotionX) = motion_x;
        field<std::int32_t>(native, kMotionY) = motion_y;
        lc.pose_transition_component_count = 0;

        // Start from a laid-out body.
        lc.recompute_body_part_layout();
        call0(kRecompute, native);

        std::string first;
        char text[256];
        for (int op = 0; op < 16 && first.empty(); ++op, ++operations) {
            g_native_dirty.clear();
            g_lc_dirty.clear();
            const int which = static_cast<int>(random.below(13));
            const auto foot_before = lc.down_foot;
            int lc_result = 0, native_result = 0;
            const int dx = random.range(-40, 40), dy = random.range(-40, 40);
            const int new_x = foot_x + random.range(-300, 300), new_y = foot_y + random.range(-200, 200);
            random_pose(random, pose, which == 5);
            const std::uint32_t table_index = random.below(100);
            const bool force = random.chance(2);
            switch (which) {
            case 0: lc.recompute_body_part_layout(); call0(kRecompute, native); break;
            case 1: lc.update_anchor_and_bounds(hosts); call0(kAnchor, native); break;
            case 2: lc.update_limb_frames_for_pose(); call0(kLimbFrames, native); break;
            case 3:
                lc.apply_pose_string(std::string_view(pose, 15), hosts);
                call1(kApplyPose, native, reinterpret_cast<std::uintptr_t>(pose));
                break;
            case 4: creature->advance_pose_animation(hosts, hosts); call0(kAdvancePose, native); break;
            case 5:
                lc_result = lc.set_target_pose_string(std::string_view(pose, 15));
                native_result = call1(kSetTarget, native, reinterpret_cast<std::uintptr_t>(pose));
                break;
            case 6: lc_result = lc.is_pose_at_target(); native_result = call0(kAtTarget, native); break;
            case 7:
                lc.set_target_pose_from_table_index(table_index);
                call2(kTargetFromTable, native, table_index, 0);
                break;
            case 8:
                lc_result = static_cast<int>(lc.select_target_pose_for_motion(force));
                native_result = call1(kSelectForMotion, native, force ? 1 : 0);
                break;
            case 9:
                lc.set_down_foot_position_and_recompute_layout(new_x, new_y, hosts, hosts);
                call2(kSetFootRecompute, native, static_cast<std::uintptr_t>(new_x),
                      static_cast<std::uintptr_t>(new_y));
                break;
            case 10:
                lc.translate_by(dx, dy);
                call2(kTranslate, native, static_cast<std::uintptr_t>(dx), static_cast<std::uintptr_t>(dy));
                break;
            case 11:
                lc.set_down_foot_position_and_invalidate_bounds(new_x, new_y, hosts, hosts);
                call2(kSetFootInvalidate, native, static_cast<std::uintptr_t>(new_x),
                      static_cast<std::uintptr_t>(new_y));
                break;
            default:
                lc.update_and_invalidate_bounds(dx, dy, hosts);
                call2(kUpdateInvalidate, native, static_cast<std::uintptr_t>(dx), static_cast<std::uintptr_t>(dy));
                break;
            }
            if (index == verbose_case) {
                std::printf("  op %d %s: native %.15s -> %.15s, lc %.15s -> %.15s\n", op, names[which],
                            reinterpret_cast<const char*>(native + kCurrentPose),
                            reinterpret_cast<const char*>(native + kTargetPose), lc.current_pose.characters.data(),
                            lc.target_pose.characters.data());
            }
            if (lc.down_foot != foot_before) ++steps;
            if (lc.boundary_correction_pending) ++wall_hits;

            // ---- compare ----
            auto note = [&](const std::string& what) {
                if (first.empty()) {
                    std::snprintf(text, sizeof text, "op %d %s: ", op, names[which]);
                    first = text + what;
                }
            };
            if (lc_result != native_result) {
                std::snprintf(text, sizeof text, "result native %d lc %d", native_result, lc_result);
                note(text);
            }
            for (std::size_t p = 0; p < pairs.size(); ++p) {
                const creatures::BodyPart& part = *pairs[p].first;
                std::uint8_t* raw = pairs[p].second;
                if (raw[kImage] != part.current_image_index() || field<std::int32_t>(raw, kX) != part.world_x() ||
                    field<std::int32_t>(raw, kY) != part.world_y() ||
                    field<std::int32_t>(raw, kPose) != part.pose_frame_index ||
                    field<std::int32_t>(raw, kView) != part.attachment_frame_index ||
                    field<std::int32_t>(raw, kPlane) != part.render_plane()) {
                    std::snprintf(text, sizeof text,
                                  "part %u: native img %u (%d,%d) pose %d view %d plane %d, lc img %u (%d,%d) "
                                  "pose %u view %u plane %d",
                                  static_cast<unsigned>(p), raw[kImage], field<std::int32_t>(raw, kX),
                                  field<std::int32_t>(raw, kY), field<std::int32_t>(raw, kPose),
                                  field<std::int32_t>(raw, kView), field<std::int32_t>(raw, kPlane),
                                  part.current_image_index(), part.world_x(), part.world_y(), part.pose_frame_index,
                                  part.attachment_frame_index, part.render_plane());
                    note(text);
                }
            }
            auto same_rect = [](const world::WorldRect& a, const world::WorldRect& b) {
                return a.min_x == b.min_x && a.min_y == b.min_y && a.max_x == b.max_x && a.max_y == b.max_y;
            };
            if (field<std::int32_t>(native, kFootX) != lc.down_foot_x ||
                field<std::int32_t>(native, kFootY) != lc.down_foot_y ||
                native[kDownFoot] != static_cast<std::uint8_t>(lc.down_foot) ||
                native[kFacing] != static_cast<std::uint8_t>(lc.facing_direction)) {
                std::snprintf(text, sizeof text, "foot native (%d,%d) %u facing %u, lc (%d,%d) %u facing %u",
                              field<std::int32_t>(native, kFootX), field<std::int32_t>(native, kFootY),
                              native[kDownFoot], native[kFacing], lc.down_foot_x, lc.down_foot_y,
                              static_cast<unsigned>(lc.down_foot), static_cast<unsigned>(lc.facing_direction));
                note(text);
            }
            for (std::size_t c = 0; c < 6; ++c) {
                if (field<std::int32_t>(native, kChainEndX + 4 * c) != lc.limb_chain_end_x[c] ||
                    field<std::int32_t>(native, kChainEndY + 4 * c) != lc.limb_chain_end_y[c]) {
                    std::snprintf(text, sizeof text, "chain %u end: native (%d,%d) lc (%d,%d)",
                                  static_cast<unsigned>(c), field<std::int32_t>(native, kChainEndX + 4 * c),
                                  field<std::int32_t>(native, kChainEndY + 4 * c), lc.limb_chain_end_x[c],
                                  lc.limb_chain_end_y[c]);
                    note(text);
                }
            }
            const auto& nb = field<world::WorldRect>(native, kSpriteBounds);
            const auto& np = field<world::WorldRect>(native, kPreviousBounds);
            if (!same_rect(nb, lc.sprite_bounds)) {
                std::snprintf(text, sizeof text, "sprite bounds: native {%d,%d,%d,%d} lc {%d,%d,%d,%d}", nb.min_x,
                              nb.min_y, nb.max_x, nb.max_y, lc.sprite_bounds.min_x, lc.sprite_bounds.min_y,
                              lc.sprite_bounds.max_x, lc.sprite_bounds.max_y);
                note(text);
            }
            if (!same_rect(np, lc.previous_sprite_bounds)) note("previous sprite bounds");
            if (!same_rect(field<world::WorldRect>(native, kMovementBounds), lc.movement_bounds())) {
                note("movement bounds");
            }
            if ((native[kBoundaryFlag] != 0) != lc.boundary_correction_pending) note("boundary flag");
            if (std::memcmp(native + kCurrentPose, lc.current_pose.characters.data(), 15) != 0 ||
                std::memcmp(native + kTargetPose, lc.target_pose.characters.data(), 15) != 0) {
                std::snprintf(text, sizeof text, "poses: native %.15s / %.15s lc %.15s / %.15s",
                              reinterpret_cast<const char*>(native + kCurrentPose),
                              reinterpret_cast<const char*>(native + kTargetPose), lc.current_pose.characters.data(),
                              lc.target_pose.characters.data());
                note(text);
            }
            if (std::strcmp(reinterpret_cast<const char*>(native + kSequence), lc.animation_sequence.data()) != 0 ||
                field<std::uint8_t*>(native, kCursor) - (native + kSequence) !=
                    static_cast<std::ptrdiff_t>(lc.animation_cursor)) {
                note("animation sequence");
            }
            if (native[kTransitions] != lc.pose_transition_component_count) note("transition count");
            bool dirty_same = g_native_dirty.size() == g_lc_dirty.size();
            for (std::size_t i = 0; dirty_same && i < g_lc_dirty.size(); ++i) {
                dirty_same = same_rect(g_native_dirty[i], g_lc_dirty[i]);
            }
            if (!dirty_same) {
                std::snprintf(text, sizeof text, "dirty rectangles: native %u lc %u",
                              static_cast<unsigned>(g_native_dirty.size()), static_cast<unsigned>(g_lc_dirty.size()));
                note(text);
            }
        }
        // Parts belong to this case, not to the Skeleton.
        for (auto& head : lc.limb_chain_heads) head = nullptr;
        lc.motion_link = nullptr;
        link.set_bounds_reference_object(nullptr);

        if (!first.empty() || index == verbose_case) {
            std::printf("case %ld: facing %u foot %u: %s\n", index, static_cast<unsigned>(facing),
                        right_foot ? 1u : 0u, first.empty() ? "same" : first.c_str());
        }
        if (!first.empty()) ++differing;
    }
    std::printf("covered: %ld operations, %ld steps, %ld after a wall\n", operations, steps, wall_hits);
    std::printf("%ld cases: %ld differ%s\n", cases, differing,
                all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
