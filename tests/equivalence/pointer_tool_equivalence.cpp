// The pointer tool's bubbles: the original's PointerTool transient-bubble
// text @004299b0 and persistent-bubble text @00429960 against
// ui::PointerTool::set_transient_bubble_text and set_persistent_bubble_text.
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
// The same results prove the offsets the load test's table uses for the
// pointer tool's bubble (+0x64) and text (+0x68, 25 bytes).
#include "native_image.hpp"

#include "objects/object.hpp"
#include "objects/simple_object.hpp"
#include "ui/tools.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace c1 = creatures1;
using c1::objects::Object;
using c1::ui::PointerTool;

namespace {

constexpr std::uint32_t kTransientText = 0x004299b0, kPersistentText = 0x00429960;
constexpr std::uint32_t kCreateBubble = 0x00426580, kBubbleSetText = 0x0042a1a0, kSpeechRangeEvent = 0x00422f90;
constexpr std::uint32_t kPointerToolVtable = 0x004578ec;

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
    Object* edit_object() const override { unreachable("edit_object"); }
    std::size_t object_count() const override { unreachable("object_count"); }
    Object* object_at(std::size_t) const override { unreachable("object_at"); }
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
    Object* pointer_tool() const override { unreachable("pointer_tool"); }
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
    void set_edit_object(Object*) override { unreachable("set_edit_object"); }
    std::size_t scenery_object_count() const override { unreachable("scenery_object_count"); }
    Object* scenery_object_at(std::size_t) const override { unreachable("scenery_object_at"); }
    void report_invalid_scenery_index() const override { unreachable("scenery index"); }
    bool point_in_world_rect(const WorldRect&, int, int) const override { unreachable("point_in_world_rect"); }
    std::uint32_t pending_input_flags() const override { unreachable("pending_input_flags"); }
    void finish_pending_input() override { unreachable("finish_pending_input"); }
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
        lc.persistent_bubble = nullptr;
        if (!mismatch.empty() && ++differing <= 10) std::printf("case %ld: %s\n", index, mismatch.c_str());
    }
    std::printf("covered: %ld operations, %ld with a bubble held\n", operations, with_bubble);
    std::printf("%ld cases: %ld differ -> %s\n", cases, differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
