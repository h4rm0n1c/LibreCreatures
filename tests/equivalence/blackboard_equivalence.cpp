// The blackboard: the original's Blackboard::InitializeState @0042c910,
// RedrawDisplay @0042c9a0, SetEditMode @0042ca90 and SetCurrentWordText
// @0042cb70 against brain::Blackboard.
//
//   blackboard_equivalence.exe <Creatures.exe> [cases]
//
// The original's blackboard is a byte block with its primary part's entity
// at +0x54.  Both sides draw into a 400 x 300 image of their own: the
// original through its own fill (004235f0) and text (00423660) code, with
// the image's pixels from a stub (00442ac0) and the charset tables (glyphs
// at the pointer 0x468db4, advance widths at 0x468dc0) filled with random
// glyphs; LC through Entity's fill and text code, with a raster host that
// serves the same glyphs.  strcpy_s and strcat_s are plain copies.
//
// Each case: a random board (16 words of 0 to 10 letters, 16 values, the
// five display bytes, the selected word in object variable 0, edit mode on
// or off), then 12 random operations: initialize, redraw (with or without
// the word), edit mode on or off, set the current word's text.  After each,
// the 16 words, the 16 values, the display bytes, the edit flag, the image
// and the text input configuration (target, maximum length, allowed
// characters) must match.
//
// The same results prove the offsets the load test's table uses for the
// board's words (+0x144, 11 bytes each), values (+0x1f4), display bytes
// (+0x234..+0x238) and edit flag (+0x239).
#include "native_image.hpp"

#include "brain/blackboard.hpp"
#include "objects/entity.hpp"
#include "objects/object.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace c1 = creatures1;
using c1::brain::Blackboard;

namespace {

constexpr std::uint32_t kInitializeState = 0x0042c910, kRedrawDisplay = 0x0042c9a0, kSetEditMode = 0x0042ca90,
                        kSetCurrentWordText = 0x0042cb70;
constexpr std::uint32_t kImagePixels = 0x00442ac0, kQueueDirty = 0x0042b2b0;
constexpr std::uint32_t kCharset = 0x00468db4, kAdvanceWidths = 0x00468dc0;
constexpr std::uint32_t kInputTarget = 0x00467d44, kInputLength = 0x00467d48, kInputBuffer = 0x00467d50,
                        kInputMaximum = 0x0046645c, kInputAllowed = 0x00466458, kPointerTool = 0x00467dbc;
constexpr std::uint32_t kBlackboardVtable = 0x00457554;

// The fields under test.
constexpr std::size_t kObjectVariable0 = 0x34, kPartEntity = 0x54, kWords = 0x144, kWordSize = 11,
                      kValues = 0x1f4, kDisplay = 0x234, kEditMode = 0x239;
constexpr std::size_t kBlockSize = 0x300;
constexpr int kWidth = 400, kHeight = 300, kGlyphBytes = 72;

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

std::vector<std::uint8_t> g_native_pixels(kWidth * kHeight), g_lc_pixels(kWidth * kHeight);
std::vector<std::uint8_t> g_glyphs(256 * kGlyphBytes);
std::uint16_t* g_widths = nullptr;

std::uint8_t* __attribute__((thiscall)) native_image_pixels(void*) { return g_native_pixels.data(); }
void __attribute__((thiscall)) native_queue_dirty(void*) {}
int __cdecl plain_strcpy_s(char* out, std::size_t size, const char* in) {
    std::size_t n = std::strlen(in);
    if (n + 1 > size) std::abort();
    std::memcpy(out, in, n + 1);
    return 0;
}
int __cdecl plain_strcat_s(char* out, std::size_t size, const char* in) {
    std::size_t at = std::strlen(out);
    if (at + std::strlen(in) + 1 > size) std::abort();
    std::memcpy(out + at, in, std::strlen(in) + 1);
    return 0;
}

class Raster final : public c1::objects::EntityRasterHost {
public:
    std::uint8_t* current_image_pixels(c1::objects::Entity&, int& width, int& height) override {
        width = kWidth;
        height = kHeight;
        return g_lc_pixels.data();
    }
    const std::uint8_t* charset_glyph_rows(std::uint8_t code) const override {
        return g_glyphs.data() + code * kGlyphBytes;
    }
    int charset_glyph_advance_width(std::uint8_t code) const override { return g_widths[code]; }
};

class Redraw final : public c1::objects::CompoundObjectMoveRedrawHost {
public:
    void redraw_after_compound_object_move(c1::objects::CompoundObject&, const c1::world::WorldRect&,
                                           const c1::world::WorldRect&) override {}
    void queue_compound_object_dirty_rect(c1::objects::CompoundObject&, const c1::world::WorldRect&) override {}
};

class Display final : public c1::brain::BlackboardDisplayHost {
public:
    Raster raster;
    Redraw redraw;
    c1::objects::EntityRasterHost& raster_host() override { return raster; }
    c1::objects::CompoundObjectMoveRedrawHost& redraw_host() override { return redraw; }
};

class Runtime final : public c1::brain::BlackboardRuntimeHost {
public:
    c1::objects::Object* tool = nullptr;
    const c1::objects::Object* target = nullptr;
    std::uint32_t maximum = 0, allowed = 0;
    int cleared = 0;
    c1::world::ViewportBounds sound_viewport() const override { return {0, 0, 0x2000, 0x1000}; }
    c1::objects::Object* pointer_tool() const override { return tool; }
    void clear_text_input() override { ++cleared; }
    void configure_text_input(c1::objects::Object* t, std::uint32_t m, std::uint32_t a) override {
        target = t;
        maximum = m;
        allowed = a;
    }
    void update_text_input(c1::objects::Object&, std::string_view) override { std::abort(); }
};

std::string random_word(Random& random, std::size_t max_length) {
    std::string w;
    const std::size_t n = random.below(static_cast<std::uint32_t>(max_length + 1));
    for (std::size_t i = 0; i < n; ++i) w += static_cast<char>((random.below(2) ? 'a' : 'A') + random.below(26));
    return w;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: blackboard_equivalence <Creatures.exe> [cases]\n");
        return 2;
    }
    const long cases = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 20000;
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    image.redirect(kImagePixels, reinterpret_cast<const void*>(&native_image_pixels));
    image.redirect(kQueueDirty, reinterpret_cast<const void*>(&native_queue_dirty));
    if (!image.bind_import("!strcpy_s", reinterpret_cast<void*>(&plain_strcpy_s)) ||
        !image.bind_import("!strcat_s", reinterpret_cast<void*>(&plain_strcat_s))) {
        std::fprintf(stderr, "no strcpy_s / strcat_s import\n");
        return 2;
    }
    Random random{0x5851f42d4c957f2dull};
    // Glyph pixels are palette slots 0..2; advance widths 1..7.
    for (auto& g : g_glyphs) g = static_cast<std::uint8_t>(random.below(3));
    *image.at<std::uint8_t**>(kCharset) = g_glyphs.data();
    g_widths = image.at<std::uint16_t*>(kAdvanceWidths);
    for (int c = 0; c < 256; ++c) g_widths[c] = static_cast<std::uint16_t>(1 + random.below(7));
    // The native image: a gallery whose first image record (40 bytes) has
    // the width at +0x10; the pixels come from the stub.
    std::vector<std::uint8_t> native_images(40, 0), native_gallery(0x20, 0), native_entity(0x40, 0),
        native_tool(0x100, 0);
    field<std::int32_t>(native_images.data(), 0x10) = kWidth;
    field<void*>(native_gallery.data(), 4) = native_images.data();
    field<void*>(native_entity.data(), 4) = native_gallery.data();
    native_entity[8] = 0;
    *image.at<void**>(kPointerTool) = native_tool.data();
    c1::objects::Object lc_tool;

    using Call0 = void(__attribute__((thiscall)) *)(void*);
    using Call1 = void(__attribute__((thiscall)) *)(void*, std::uintptr_t);
    const char* names[] = {"initialize", "redraw", "edit mode", "set word"};
    long counts[4] = {}, differing_by[4] = {}, differing = 0;
    for (long index = 0; index < cases; ++index) {
        std::vector<std::uint8_t> native(kBlockSize, 0);
        field<std::uint32_t>(native.data(), 0) = kBlackboardVtable;
        field<void*>(native.data(), kPartEntity) = native_entity.data();
        Blackboard lc;
        lc.part(0).entity = std::make_unique<c1::objects::Entity>();
        Display display;
        Runtime runtime;
        runtime.tool = &lc_tool;
        std::fill(g_native_pixels.begin(), g_native_pixels.end(), 0x55);
        std::fill(g_lc_pixels.begin(), g_lc_pixels.end(), 0x55);

        auto set_selected = [&](std::uint32_t k) {
            lc.set_object_variable(0, k);
            field<std::uint32_t>(native.data(), kObjectVariable0) = k;
        };
        auto reset_input = [&]() {
            *image.at<void**>(kInputTarget) = nullptr;
            *image.at<std::uint32_t*>(kInputMaximum) = 0x7777;
            *image.at<std::uint8_t*>(kInputAllowed) = 0x77;
            *image.at<std::uint32_t*>(kInputLength) = 0x7777;
            *image.at<std::uint8_t*>(kInputBuffer) = 0x77;
            runtime.target = nullptr;
            runtime.maximum = 0x7777;
            runtime.allowed = 0x77;
            runtime.cleared = 0;
        };
        // A random board.  Edit mode first: turning it on clears the
        // selected word, which is then written.
        set_selected(random.below(16));
        if (random.below(3) == 0) {
            lc.set_edit_mode(true, runtime, display);
            reinterpret_cast<Call1>(image.at<void*>(kSetEditMode))(native.data(), 1);
        }
        for (std::size_t w = 0; w < Blackboard::kWordCount; ++w) {
            const std::string word = random_word(random, 10);
            auto& text = lc.word(w).text;
            std::fill(text.begin(), text.end(), 0);
            std::memcpy(text.data(), word.data(), word.size());
            std::memset(native.data() + kWords + w * kWordSize, 0, kWordSize);
            std::memcpy(native.data() + kWords + w * kWordSize, word.data(), word.size());
            const std::uint32_t value = random.next();
            lc.set_word_value(w, value);
            field<std::uint32_t>(native.data(), kValues + 4 * w) = value;
        }
        std::uint8_t config[5];
        for (auto& b : config) b = static_cast<std::uint8_t>(random.below(256));
        lc.display_config() = {config[0], config[1], config[2], config[3], config[4]};
        std::memcpy(native.data() + kDisplay, config, 5);

        std::string mismatch;
        int which = 0;
        for (int op = 0; op < 12 && mismatch.empty(); ++op) {
            which = static_cast<int>(random.below(4));
            ++counts[which];
            reset_input();
            set_selected(random.below(16));
            switch (which) {
            case 0:
                lc.initialize_state();
                reinterpret_cast<Call0>(image.at<void*>(kInitializeState))(native.data());
                break;
            case 1: {
                const bool draw = random.below(4) != 0;
                lc.redraw_display(draw, display);
                reinterpret_cast<Call1>(image.at<void*>(kRedrawDisplay))(native.data(), draw ? 1 : 0);
                break;
            }
            case 2: {
                const bool on = random.below(2) == 0;
                lc.set_edit_mode(on, runtime, display);
                reinterpret_cast<Call1>(image.at<void*>(kSetEditMode))(native.data(), on ? 1 : 0);
                break;
            }
            default: {
                const std::string word = random_word(random, 10);
                lc.set_current_word_text(word, display);
                reinterpret_cast<Call1>(image.at<void*>(kSetCurrentWordText))(
                    native.data(), reinterpret_cast<std::uintptr_t>(word.c_str()));
                break;
            }
            }
            char text[256];
            for (std::size_t w = 0; w < Blackboard::kWordCount && mismatch.empty(); ++w) {
                const char* n = reinterpret_cast<const char*>(native.data() + kWords + w * kWordSize);
                const char* l = lc.word(w).text.data();
                if (std::strncmp(n, l, kWordSize) != 0 ||
                    field<std::uint32_t>(native.data(), kValues + 4 * w) != lc.word_value(w)) {
                    std::snprintf(text, sizeof text, "word %u: native [%.11s] %08x lc [%.11s] %08x",
                                  static_cast<unsigned>(w), n, field<std::uint32_t>(native.data(), kValues + 4 * w),
                                  l, static_cast<unsigned>(lc.word_value(w)));
                    mismatch = text;
                }
            }
            const auto& c = lc.display_config();
            const std::uint8_t lc_config[5] = {c.fill_palette_index, c.text_render_config_1, c.text_render_config_2,
                                               c.tile_x, c.tile_y};
            if (mismatch.empty() && (std::memcmp(native.data() + kDisplay, lc_config, 5) != 0 ||
                                     (native[kEditMode] != 0) != lc.edit_mode_enabled() || native[kEditMode] > 1)) {
                std::snprintf(text, sizeof text, "display: native %02x %02x %02x %02x %02x edit %u; lc %02x %02x %02x "
                              "%02x %02x edit %d",
                              native[kDisplay], native[kDisplay + 1], native[kDisplay + 2], native[kDisplay + 3],
                              native[kDisplay + 4], native[kEditMode], lc_config[0], lc_config[1], lc_config[2],
                              lc_config[3], lc_config[4], lc.edit_mode_enabled());
                mismatch = text;
            }
            if (mismatch.empty() && g_native_pixels != g_lc_pixels) {
                std::size_t at = 0;
                while (g_native_pixels[at] == g_lc_pixels[at]) ++at;
                std::snprintf(text, sizeof text, "image: first difference at (%d,%d): native %u lc %u",
                              static_cast<int>(at % kWidth), static_cast<int>(at / kWidth), g_native_pixels[at],
                              g_lc_pixels[at]);
                mismatch = text;
            }
            if (mismatch.empty() && which == 2) {
                void* native_target = *image.at<void**>(kInputTarget);
                const bool same_target = (native_target == native.data() && runtime.target == &lc) ||
                                         (native_target == native_tool.data() && runtime.target == &lc_tool);
                if (!same_target || *image.at<std::uint32_t*>(kInputMaximum) != runtime.maximum ||
                    *image.at<std::uint8_t*>(kInputAllowed) != runtime.allowed || runtime.cleared != 1 ||
                    *image.at<std::uint32_t*>(kInputLength) != 0 || *image.at<std::uint8_t*>(kInputBuffer) != 0) {
                    std::snprintf(text, sizeof text, "text input: native max %u allowed %02x; lc max %u allowed %02x",
                                  *image.at<std::uint32_t*>(kInputMaximum), *image.at<std::uint8_t*>(kInputAllowed),
                                  runtime.maximum, runtime.allowed);
                    mismatch = text;
                }
            }
        }
        if (!mismatch.empty()) {
            ++differing_by[which];
            if (++differing <= 10) std::printf("case %ld %s: %s\n", index, names[which], mismatch.c_str());
        }
    }
    for (int k = 0; k < 4; ++k) {
        std::printf("%s: %ld operations, %ld cases first differ here\n", names[k], counts[k], differing_by[k]);
    }
    std::printf("%ld cases: %ld differ -> %s\n", cases, differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
