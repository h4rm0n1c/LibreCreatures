// The pointer tool's bubbles and shift-click: the original's PointerTool
// transient-bubble text @004299b0, persistent-bubble text @00429960 and the
// shift-click edit path of ProcessPendingInput @00428860 against
// ui::PointerTool::set_transient_bubble_text, set_persistent_bubble_text
// and process_pending_input.
//
//   pointer_tool_equivalence.exe <Creatures.exe> [cases]
//
// The original's pointer tool is a byte block with its class's vtable.  Its
// bubble creation (00426580), bubble text (0042a1a0), the bubble's dismiss
// (bubble vtable +0x40) and the speech-range event (00422f90) are recorders;
// LC's runtime host records the same calls.  strcpy_s is a plain copy.
//
// Each case: no bubble or a persistent one, a random text in the buffer,
// then 8 random operations, each a transient or a persistent text of 0 to
// 24 letters.  After each, the calls (create transient / create persistent
// with its text, set text, dismiss, the speech event), the bubble held and
// the text in the buffer (up to its NUL) must match.  The bytes after the
// NUL are not compared: LC clears them, the original's strcpy_s leaves them.
//
// Shift-click: a case also runs 4 shift-clicks.  The pointer's entity and
// hotspot are random; the registry holds 0 to 6 objects and the pointer,
// the scenery list 0 to 4 objects, each with random bounds near the
// hotspot (some do not write their bounds: the original keeps one
// rectangle for every get_bounds) and a random render plane, which the
// choice must not depend on.  The original's UpdateMovementBounds
// runs as native code; the objects are in the default bounds mode.  The
// object chosen for editing, and the pending input flags (+0x150 takes
// +0x154, which is cleared), must match.
//
// The same results prove the offsets the load test's table uses for the
// pointer tool's hotspot (+0x5c, +0x60), bubble (+0x64) and text (+0x68,
// 25 bytes).
#include "native_image.hpp"

#include "objects/object.hpp"
#include "objects/simple_object.hpp"
#include "ui/tools.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace c1 = creatures1;
using c1::objects::Object;
using c1::ui::PointerTool;

namespace {

constexpr std::uint32_t kTransientText = 0x004299b0, kPersistentText = 0x00429960;
constexpr std::uint32_t kCreateBubble = 0x00426580, kBubbleSetText = 0x0042a1a0, kSpeechRangeEvent = 0x00422f90;
constexpr std::uint32_t kPointerToolVtable = 0x004578ec;
constexpr std::uint32_t kProcessPendingInput = 0x00428860;
constexpr std::uint32_t kDocument = 0x00468d84, kPointerToolGlobal = 0x00467dbc, kEditObject = 0x00468d68,
                        kRegistryData = 0x004704a8, kRegistryCount = 0x004704ac, kSceneryData = 0x004704e4,
                        kSceneryCount = 0x004704e8;
constexpr std::size_t kPendingInput = 0x154, kPreviousInput = 0x150, kEntity = 0x50, kEntityX = 0x10,
                      kEntityY = 0x14;
constexpr std::size_t kHotspotX = 0x5c, kHotspotY = 0x60;

// The fields under test.
constexpr std::size_t kBubble = 0x64, kText = 0x68, kTextSize = 25;
constexpr std::size_t kBlockSize = 0x100;

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint32_t below(std::uint32_t n) { return next() % n; }
};

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

std::vector<std::string> g_native_calls, g_lc_calls;
// Bubbles: the native ones are blocks with a vtable whose slot 16 is the
// dismiss recorder; the LC ones are addresses.  Bubble k is the k-th made
// in a case (0: the one a case may start with).
constexpr int kBubbles = 64;
std::uint8_t g_native_bubbles[kBubbles][0x20];
std::uint8_t g_lc_bubbles[kBubbles];
int g_made = 0;
void* g_bubble_vtable[32];

int native_bubble_index(const void* p) {
    for (int k = 0; k < kBubbles; ++k) {
        if (p == g_native_bubbles[k]) return k;
    }
    return -1;
}
int lc_bubble_index(const void* p) {
    for (int k = 0; k < kBubbles; ++k) {
        if (p == &g_lc_bubbles[k]) return k;
    }
    return -1;
}

void* __attribute__((thiscall)) native_create_bubble(void*, const char* text, int a, int b) {
    const char* kind = a == 10 && b == 0 ? "transient" : a == 0 && b == 1 ? "persistent" : "other";
    const int k = g_made++;
    g_native_calls.push_back(std::string("create ") + kind + " " + std::to_string(k) + " [" + text + "]");
    return g_native_bubbles[k];
}
void __attribute__((thiscall)) native_bubble_set_text(void* bubble, const char* text) {
    g_native_calls.push_back("set " + std::to_string(native_bubble_index(bubble)) + " [" + text + "]");
}
void __attribute__((thiscall)) native_bubble_dismiss(void* bubble) {
    g_native_calls.push_back("dismiss " + std::to_string(native_bubble_index(bubble)));
}
} // namespace

extern "C" {
void* g_speech_source = nullptr;
std::uint32_t g_speech_event = 0;
}

namespace {

extern "C" void native_speech_record() {
    g_native_calls.push_back("speech " + std::to_string(g_speech_event));
}
// Objects for the shift-click: both sides' get_bounds write the same
// rectangle, or (writes false) leave the caller's untouched.
struct BoundsSpec {
    c1::world::WorldRect bounds;
    bool writes;
    int plane;  // LC's only: the original's choice does not look at it
};
constexpr int kObjects = 12;
BoundsSpec g_specs[kObjects];
std::uint8_t g_native_objects[kObjects][0x40];
void* g_object_vtable[64];
int native_object_index(const void* p) {
    for (int k = 0; k < kObjects; ++k) {
        if (p == g_native_objects[k]) return k;
    }
    return -1;
}
void __attribute__((thiscall)) native_get_bounds(void* object, c1::world::WorldRect* out) {
    const int k = native_object_index(object);
    if (k >= 0 && g_specs[k].writes) *out = g_specs[k].bounds;
}
int __attribute__((stdcall)) plain_pt_in_rect(const c1::world::WorldRect* r, int x, int y) {
    return x >= r->min_x && x < r->max_x && y >= r->min_y && y < r->max_y;
}
class BoundsObject final : public Object {
public:
    int index = 0;
    bool get_bounds(c1::world::WorldRect* out) const override {
        if (g_specs[index].writes) *out = g_specs[index].bounds;
        return g_specs[index].writes;
    }
    int render_plane() const override { return g_specs[index].plane; }
};

int __cdecl plain_strcpy_s(char* out, std::size_t size, const char* in) {
    const std::size_t n = std::strlen(in);
    if (n + 1 > size) std::abort();
    std::memcpy(out, in, n + 1);
    return 0;
}

} // namespace

// 00422f90 takes the source in ECX and the event in EDX, three more on the
// stack, and its caller pops them.
extern "C" void native_speech_thunk();
asm(".globl _native_speech_thunk\n"
    "_native_speech_thunk:\n"
    "    movl %ecx, _g_speech_source\n"
    "    movl %edx, _g_speech_event\n"
    "    call _native_speech_record\n"
    "    ret\n");

namespace {

[[noreturn]] void unreachable(const char* what) {
    std::fprintf(stderr, "unexpected host call: %s\n", what);
    std::abort();
}

class Runtime final : public c1::ui::PointerToolRuntimeHost {
public:
    using Creature = c1::creatures::Creature;
    using WorldRect = c1::world::WorldRect;
    // The calls under test.
    void dismiss_pointer_bubble(c1::objects::Bubble& bubble) override {
        g_lc_calls.push_back("dismiss " + std::to_string(lc_bubble_index(&bubble)));
    }
    c1::objects::Bubble* create_persistent_pointer_bubble(PointerTool&, std::string_view text) override {
        const int k = g_made++;
        g_lc_calls.push_back("create persistent " + std::to_string(k) + " [" + std::string(text) + "]");
        return reinterpret_cast<c1::objects::Bubble*>(&g_lc_bubbles[k]);
    }
    c1::objects::Bubble* create_transient_pointer_bubble(PointerTool&, std::string_view text) override {
        const int k = g_made++;
        g_lc_calls.push_back("create transient " + std::to_string(k) + " [" + std::string(text) + "]");
        return reinterpret_cast<c1::objects::Bubble*>(&g_lc_bubbles[k]);
    }
    void set_pointer_bubble_text(c1::objects::Bubble& bubble, std::string_view text) override {
        g_lc_calls.push_back("set " + std::to_string(lc_bubble_index(&bubble)) + " [" + std::string(text) + "]");
    }
    void queue_speech_range_event(Object&, c1::objects::ObjectEventId event) override {
        g_lc_calls.push_back("speech " + std::to_string(static_cast<int>(event)));
    }
    // Never reached by the functions under test.
    bool contains(const Object&) const override { unreachable("contains"); }
    void insert(Object&) override { unreachable("insert"); }
    void erase(Object&) override { unreachable("erase"); }
    void clear_edit_object() override { unreachable("clear_edit_object"); }
    void find_nearest_room_bounds_at_point(int, int, WorldRect&) const override { unreachable("room"); }
    WorldRect vehicle_local_bounds(const Object&) const override { unreachable("vehicle_local_bounds"); }
    int vehicle_primary_entity_x(const Object&) const override { unreachable("vehicle x"); }
    int vehicle_primary_entity_y(const Object&) const override { unreachable("vehicle y"); }
    Object* edit_object() const override { return edit; }
    std::size_t object_count() const override { return registry.size(); }
    Object* object_at(std::size_t i) const override { return registry[i]; }
    void report_invalid_index() const override { unreachable("report_invalid_index"); }
    bool is_pointer_tool(const Object&) const override { unreachable("is_pointer_tool"); }
    WorldRect pointer_tool_bounds(const Object&) const override { unreachable("pointer_tool_bounds"); }
    WorldRect vehicle_interaction_bounds(const Object&) const override { unreachable("vehicle bounds"); }
    void queue_immediate_event(Object&, Object&, c1::objects::ObjectEventId, std::uint32_t) override {
        unreachable("queue_immediate_event");
    }
    int execute_script_for_classifier(Object&, Object*, std::uint32_t, bool) override { unreachable("script"); }
    void redraw_after_simple_object_move(c1::objects::SimpleObject&, const WorldRect&, const WorldRect&) override {
        unreachable("redraw");
    }
    Object* pointer_tool() const override { return tool; }
    int pointer_world_x() const override { unreachable("pointer_world_x"); }
    int pointer_world_y() const override { unreachable("pointer_world_y"); }
    std::size_t non_scenery_object_count() const override { unreachable("non_scenery_object_count"); }
    Object* non_scenery_object_at(std::size_t) const override { unreachable("non_scenery_object_at"); }
    void report_invalid_non_scenery_index() const override { unreachable("non-scenery index"); }
    Creature* creature_for_object(Object&) const override { unreachable("creature_for_object"); }
    std::size_t creature_count() const override { unreachable("creature_count"); }
    Creature* creature_at(std::size_t) const override { unreachable("creature_at"); }
    void report_invalid_creature_index() const override { unreachable("creature index"); }
    void select_creature(Creature&, bool) override { unreachable("select_creature"); }
    bool is_creature_object(const Object&) const override { unreachable("is_creature_object"); }
    void update_creature_attention(Creature&) override { unreachable("update_creature_attention"); }
    void queue_pointer_tool_stimulus(Creature&, PointerTool&) override { unreachable("stimulus"); }
    void set_edit_object(Object* object) override {
        edit = object;
        ++edit_sets;
    }
    std::size_t scenery_object_count() const override { return scenery.size(); }
    Object* scenery_object_at(std::size_t i) const override { return scenery[i]; }
    void report_invalid_scenery_index() const override { unreachable("scenery index"); }
    bool point_in_world_rect(const WorldRect& r, int x, int y) const override {
        return plain_pt_in_rect(&r, x, y) != 0;
    }
    std::uint32_t pending_input_flags() const override { return pending; }
    void finish_pending_input() override {
        previous = pending;
        pending = 0;
    }

    Object* tool = nullptr;
    std::vector<Object*> registry, scenery;
    Object* edit = nullptr;
    int edit_sets = 0;
    std::uint32_t pending = 0, previous = 0;
    void synchronize_creature_selector(std::string_view) override { unreachable("selector"); }
};

std::string random_text(Random& random, std::size_t max_length) {
    std::string t;
    const std::size_t n = random.below(static_cast<std::uint32_t>(max_length + 1));
    for (std::size_t i = 0; i < n; ++i) t += static_cast<char>('a' + random.below(26));
    return t;
}

std::string joined(const std::vector<std::string>& calls) {
    std::string out;
    for (const auto& c : calls) out += (out.empty() ? "" : "; ") + c;
    return out.empty() ? "none" : out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: pointer_tool_equivalence <Creatures.exe> [cases]\n");
        return 2;
    }
    const long cases = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 20000;
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    image.redirect(kCreateBubble, reinterpret_cast<const void*>(&native_create_bubble));
    image.redirect(kBubbleSetText, reinterpret_cast<const void*>(&native_bubble_set_text));
    image.redirect(kSpeechRangeEvent, reinterpret_cast<const void*>(&native_speech_thunk));
    if (!image.bind_import("!strcpy_s", reinterpret_cast<void*>(&plain_strcpy_s))) {
        std::fprintf(stderr, "no strcpy_s import\n");
        return 2;
    }
    if (!image.bind_import("!PtInRect", reinterpret_cast<void*>(&plain_pt_in_rect))) {
        std::fprintf(stderr, "no PtInRect import\n");
        return 2;
    }
    g_object_vtable[0x68 / 4] = reinterpret_cast<void*>(&native_get_bounds);
    for (auto& o : g_native_objects) *reinterpret_cast<void**>(o) = g_object_vtable;
    std::vector<std::uint8_t> document(0x400, 0), native_entity(0x40, 0);
    *image.at<void**>(kDocument) = document.data();
    BoundsObject lc_objects[kObjects];
    for (int k = 0; k < kObjects; ++k) lc_objects[k].index = k;
    long clicks = 0, chosen_registry = 0, chosen_scenery = 0;
    g_bubble_vtable[16] = reinterpret_cast<void*>(&native_bubble_dismiss);
    for (auto& b : g_native_bubbles) *reinterpret_cast<void**>(b) = g_bubble_vtable;

    using SetText = void(__attribute__((thiscall)) *)(void*, const char*);
    Random random{0x14057b7ef767814full};
    long differing = 0, operations = 0, with_bubble = 0;
    for (long index = 0; index < cases; ++index) {
        std::vector<std::uint8_t> native(kBlockSize, 0);
        field<std::uint32_t>(native.data(), 0) = kPointerToolVtable;
        PointerTool lc;
        Runtime runtime;
        g_native_calls.clear();
        g_lc_calls.clear();
        g_made = 0;
        if (random.below(2) == 0) {
            lc.persistent_bubble = reinterpret_cast<c1::objects::Bubble*>(&g_lc_bubbles[0]);
            field<void*>(native.data(), kBubble) = g_native_bubbles[0];
            g_made = 1;
        }
        const std::string start = random_text(random, 24);
        std::memcpy(native.data() + kText, start.c_str(), start.size() + 1);
        std::fill(lc.text_buffer.begin(), lc.text_buffer.end(), '\0');
        std::memcpy(lc.text_buffer.data(), start.c_str(), start.size() + 1);

        std::string mismatch;
        for (int op = 0; op < 8 && mismatch.empty() && g_made < kBubbles - 2; ++op, ++operations) {
            if (field<void*>(native.data(), kBubble) != nullptr) ++with_bubble;
            const bool transient = random.below(2) == 0;
            const std::string text = random_text(random, 24);
            const int made = g_made;
            if (transient) {
                lc.set_transient_bubble_text(text, runtime);
            } else {
                lc.set_persistent_bubble_text(text, runtime);
            }
            const int lc_made = g_made;
            g_made = made;
            reinterpret_cast<SetText>(image.at<void*>(transient ? kTransientText : kPersistentText))(native.data(),
                                                                                                  text.c_str());
            if (g_made != lc_made) g_made = std::max(g_made, lc_made);
            const char* native_text = reinterpret_cast<const char*>(native.data() + kText);
            const int native_bubble = native_bubble_index(field<void*>(native.data(), kBubble));
            const int lc_bubble = lc_bubble_index(lc.persistent_bubble);
            if (g_native_calls != g_lc_calls || std::strncmp(native_text, lc.text_buffer.data(), kTextSize) != 0 ||
                native_bubble != lc_bubble) {
                char t[1024];
                std::snprintf(t, sizeof t, "op %d %s [%s]: native bubble %d text [%.25s] calls %s; lc bubble %d text "
                              "[%.25s] calls %s",
                              op, transient ? "transient" : "persistent", text.c_str(), native_bubble, native_text,
                              joined(g_native_calls).c_str(), lc_bubble, lc.text_buffer.data(),
                              joined(g_lc_calls).c_str());
                mismatch = t;
            }
            g_native_calls.clear();
            g_lc_calls.clear();
        }
        // ---- shift-clicks ----
        lc.set_entity(std::make_unique<c1::objects::Entity>());
        field<void*>(native.data(), kEntity) = native_entity.data();
        *image.at<void**>(kPointerToolGlobal) = native.data();
        runtime.tool = &lc;
        for (int click = 0; click < 4 && mismatch.empty(); ++click, ++clicks) {
            const int x = 1000 + static_cast<int>(random.below(200)), y = 500 + static_cast<int>(random.below(100));
            const int hx = static_cast<int>(random.below(9)) - 2, hy = static_cast<int>(random.below(9)) - 2;
            lc.entity()->set_world_x(x);
            lc.entity()->set_world_y(y);
            lc.cursor_hotspot_offset_x = hx;
            lc.cursor_hotspot_offset_y = hy;
            field<std::int32_t>(native_entity.data(), kEntityX) = x;
            field<std::int32_t>(native_entity.data(), kEntityY) = y;
            field<std::int32_t>(native.data(), kHotspotX) = hx;
            field<std::int32_t>(native.data(), kHotspotY) = hy;
            for (int k = 0; k < kObjects; ++k) {
                const int x0 = x + hx - 12 + static_cast<int>(random.below(20));
                const int y0 = y + hy - 12 + static_cast<int>(random.below(20));
                g_specs[k] = {{x0, y0, x0 + static_cast<int>(random.below(16)), y0 + static_cast<int>(random.below(16))},
                              random.below(6) != 0, static_cast<int>(random.below(5000))};
                std::memset(g_native_objects[k] + 4, 0, sizeof g_native_objects[k] - 4);
            }
            // The registry: objects 0..r-1 and the pointer at a random place;
            // the scenery list: objects 6..6+n-1.
            const int r = static_cast<int>(random.below(7)), n = static_cast<int>(random.below(5));
            const int tool_at = static_cast<int>(random.below(static_cast<std::uint32_t>(r + 1)));
            std::vector<void*> native_registry, native_scenery;
            runtime.registry.clear();
            runtime.scenery.clear();
            for (int k = 0; k <= r; ++k) {
                if (k == tool_at) {
                    native_registry.push_back(native.data());
                    runtime.registry.push_back(&lc);
                }
                if (k < r) {
                    native_registry.push_back(g_native_objects[k]);
                    runtime.registry.push_back(&lc_objects[k]);
                }
            }
            for (int k = 0; k < n; ++k) {
                native_scenery.push_back(g_native_objects[6 + k]);
                runtime.scenery.push_back(&lc_objects[6 + k]);
            }
            *image.at<void***>(kRegistryData) = native_registry.data();
            *image.at<std::int32_t*>(kRegistryCount) = static_cast<std::int32_t>(native_registry.size());
            *image.at<void***>(kSceneryData) = native_scenery.data();
            *image.at<std::int32_t*>(kSceneryCount) = static_cast<std::int32_t>(native_scenery.size());
            *image.at<void**>(kEditObject) = reinterpret_cast<void*>(0x1);
            field<std::uint32_t>(document.data(), kPendingInput) = 4;
            field<std::uint32_t>(document.data(), kPreviousInput) = 0;
            runtime.pending = 4;
            runtime.previous = 0;
            runtime.edit = reinterpret_cast<Object*>(0x1);
            runtime.edit_sets = 0;

            lc.process_pending_input(lc, runtime);
            reinterpret_cast<void(__attribute__((thiscall)) *)(void*)>(image.at<void*>(kProcessPendingInput))(
                native.data());

            const int native_edit = native_object_index(*image.at<void**>(kEditObject));
            int lc_edit = -1;
            for (int k = 0; k < kObjects; ++k) {
                if (runtime.edit == &lc_objects[k]) lc_edit = k;
            }
            const bool native_none = *image.at<void**>(kEditObject) == nullptr;
            if (native_edit != lc_edit || native_none != (runtime.edit == nullptr) || runtime.edit_sets != 1 ||
                field<std::uint32_t>(document.data(), kPendingInput) != runtime.pending ||
                field<std::uint32_t>(document.data(), kPreviousInput) != runtime.previous) {
                char t[256];
                std::snprintf(t, sizeof t, "shift-click %d (registry %d, scenery %d): native edit %d%s lc edit %d%s",
                              click, r, n, native_edit, native_none ? " (none)" : "", lc_edit,
                              runtime.edit == nullptr ? " (none)" : "");
                mismatch = t;
            }
            if (native_edit >= 0 && native_edit < 6) ++chosen_registry;
            if (native_edit >= 6) ++chosen_scenery;
        }
        lc.persistent_bubble = nullptr;
        if (!mismatch.empty() && ++differing <= 10) std::printf("case %ld: %s\n", index, mismatch.c_str());
    }
    std::printf("covered: %ld operations, %ld with a bubble held; %ld shift-clicks, %ld chose a registry object, "
                "%ld a scenery object\n",
                operations, with_bubble, clicks, chosen_registry, chosen_scenery);
    std::printf("%ld cases: %ld differ -> %s\n", cases, differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
