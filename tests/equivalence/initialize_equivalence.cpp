// A creature's runtime reset: the original's Creature::InitializeRuntimeState
// @00408520 against creatures::Creature::initialize_runtime_state, compared
// field by field through the load test's table
// (src/c1/creatures/load_dump_fields.inc).
//
//   initialize_equivalence.exe <Creatures.exe> <install-dir> [cases]
//
// The original runs twice on a creature block, filled first with 0x00 and
// then with 0xff (its ten history CStrings built by the install's real MFC,
// only read; its string resource from a stub of CString::LoadString that
// gives "<id>").  A table field that comes out the same both times is one
// the reset writes, and it must match LC's after LC's reset.  The other
// fields are left as they were and are not compared.  LC's host gives what
// the game's does: the default stimulus contexts, the "<id>" text for the
// name's resource and the "%lx" monikers.
//
// Each case: random genome, mother and father monikers (+0x50, +0x54,
// +0x58).  The reset copies them into the history as text, so the history's
// father (+0x818) and mother (+0x81c) are proven by which moniker each
// holds, and the birthplace (+0x824) by its text.
#include "native_image.hpp"
#include "load_dump_native.hpp"

#include "creatures/creature.hpp"
#include "creatures/creature_field_dump.hpp"
#include "creatures/load_dump_lc.hpp"
#include "creatures/stimulus.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace c1 = creatures1;
using c1::creatures::Creature;
namespace dump = c1::load_dump;
using equivalence::native_text;

namespace {

constexpr std::uint32_t kInitializeRuntimeState = 0x00408520;
constexpr std::uint32_t kCStringConstruct = 0x00450d1c, kCStringAssign = 0x00450c70,
                        kCStringLoadString = 0x00450c68;
constexpr std::size_t kBlockSize = 0x2d00, kHistory = 0x810, kHistoryStrings = 10;
constexpr std::size_t kGenome = 0x50, kMother = 0x54, kFather = 0x58;

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
};

using CStringConstruct = void(__attribute__((thiscall)) *)(void*);
using CStringAssign = void*(__attribute__((thiscall)) *)(void*, const char*);
CStringAssign g_assign = nullptr;

std::string resource_text(unsigned id) {
    char t[16];
    std::snprintf(t, sizeof t, "<%x>", id);
    return t;
}
int __attribute__((thiscall)) stub_load_string(void* self, unsigned id) {
    g_assign(self, resource_text(id).c_str());
    return 1;
}

// What the game's C1WindowsDocument gives (windows_document_host.cpp).
class Host final : public Creature::InitializationHost {
public:
    const c1::creatures::StimulusContext* default_stimulus_context(std::size_t index) const override {
        return index < c1::creatures::kDefaultStimulusContexts.size()
                   ? &c1::creatures::kDefaultStimulusContexts[index]
                   : nullptr;
    }
    std::string localized_birthplace() const override { return resource_text(0xef1f); }
    std::string format_moniker(std::uint32_t value) const override {
        char t[16];
        std::snprintf(t, sizeof t, "%lx", static_cast<unsigned long>(value));
        return t;
    }
};

struct Line {
    std::string name, native0, native1, lc;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: initialize_equivalence <Creatures.exe> <install-dir> [cases]\n");
        return 2;
    }
    const long cases = argc > 3 ? std::strtol(argv[3], nullptr, 10) : 500;
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    std::vector<std::string> missing;
    image.bind_real_imports(argv[2], missing);  // what is missing stays a trap
    const auto construct = *image.at<CStringConstruct*>(kCStringConstruct);
    g_assign = *image.at<CStringAssign*>(kCStringAssign);
    *image.at<void**>(kCStringLoadString) = reinterpret_cast<void*>(&stub_load_string);
    using Initialize = void(__attribute__((thiscall)) *)(void*, int);

    Random random{0x510e527fade682d1ull};
    long differing = 0, compared_rows = 0, skipped_rows = 0;
    for (long index = 0; index < cases; ++index) {
        const std::uint32_t genome = random.next() ^ (random.next() << 16);
        const std::uint32_t mother = random.next() ^ (random.next() << 16);
        const std::uint32_t father = random.next() ^ (random.next() << 16);
        std::vector<std::uint8_t> native[2] = {std::vector<std::uint8_t>(kBlockSize, 0x00),
                                               std::vector<std::uint8_t>(kBlockSize, 0xff)};
        for (auto& block : native) {
            for (std::size_t k = 0; k < kHistoryStrings; ++k) construct(block.data() + kHistory + 4 * k);
            std::memcpy(block.data() + kGenome, &genome, 4);
            std::memcpy(block.data() + kMother, &mother, 4);
            std::memcpy(block.data() + kFather, &father, 4);
            reinterpret_cast<Initialize>(image.at<void*>(kInitializeRuntimeState))(block.data(), 0);
        }
        auto creature = std::make_unique<Creature>();
        Creature& c = *creature;
        auto& s = c.skeleton();
        using P = c1::creatures::CreatureFieldDump;
        s.genome_source_filename = genome;
        s.mother_moniker = mother;
        s.father_moniker = father;
        Host host;
        c.initialize_runtime_state(host);

        std::vector<Line> lines;
        using dump::U8; using dump::Bool; using dump::U32; using dump::I32; using dump::Ptr;
        using dump::Rect; using dump::Text; using dump::Bytes; using dump::CursorFrom; using dump::CStr;
#define LOAD_DUMP_FIELD(name, offset, kind, count_, stride, proven_by, lc)                                  \
        for (std::size_t i = 0; i < (count_); ++i) {                                                    \
            const std::size_t at = (offset) + i * (stride);                                             \
            lines.push_back({std::string(#name) + ((count_) > 1 ? "[" + std::to_string(i) + "]" : ""),  \
                             native_text(kind, native[0].data(), at), native_text(kind, native[1].data(), at), \
                             lc_text(kind, lc)});                                                       \
        }
#include "creatures/load_dump_fields.inc"
#undef LOAD_DUMP_FIELD
        (void)sizeof(P*);
        std::string mismatch;
        for (const Line& line : lines) {
            if (line.native0 != line.native1) {
                if (index == 0) ++skipped_rows;
                continue;
            }
            if (index == 0) ++compared_rows;
            if (line.native0 != line.lc && mismatch.empty()) {
                mismatch = line.name + ": native " + line.native0 + ", lc " + line.lc;
            }
        }
        if (!mismatch.empty() && ++differing <= 10) std::printf("case %ld: %s\n", index, mismatch.c_str());
    }
    std::printf("rows the reset writes: %ld compared, %ld left as they were\n", compared_rows, skipped_rows);
    std::printf("%ld cases: %ld differ -> %s\n", cases, differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
