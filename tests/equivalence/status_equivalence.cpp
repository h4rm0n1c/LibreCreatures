// A creature's status for kits: the original's
// Creature::FormatStatusForExternalQuery @0040e520 against
// creatures::Creature::format_status_for_external_query.
//
//   status_equivalence.exe <Creatures.exe> <install-dir> [cases]
//
// The original's creature is a byte block; its history names are CStrings
// (built and assigned by the install's real MFC, only read) and its string
// resources come from a stub of CString::LoadString that gives "<id>" for
// each id.  LC's StatusStrings hold the same texts for the ids its comments
// name.  The map's rooms (0x468d64) are the same random rooms on both sides
// (the geometry test's layout).  Both write ten '|'-separated fields.
//
// Each case: a random display name and genome moniker, age, sex, child,
// death state, down foot, the gestation, life-force and sickness
// chemicals, and 0 to 6 rooms.  The two texts must be the same.  The same
// results prove the offsets the load test's table uses for the display
// name (+0x814), the genome moniker (+0x810) and the age (+0x838).
//
// Rows 15 (the pregnancy stage 0 field) and 43 (the room) are switches in
// the code under test; the rows build fixes them alone.
#include "native_image.hpp"

#include "biochemistry/biochemistry.hpp"
#include "common/compat.hpp"
#include "creatures/creature.hpp"
#include "world/map.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace creatures1::creatures {
// Declared a friend in creature.hpp.
class CreatureTestAccess {
public:
    static GenomeSex& sex(Creature& c) { return c.genome_sex_; }
    static std::uint8_t& death_state(Creature& c) { return c.death_state_; }
};
} // namespace creatures1::creatures

namespace c1 = creatures1;
using c1::creatures::Creature;
using c1::creatures::CreatureTestAccess;

namespace {

constexpr std::uint32_t kFormatStatus = 0x0040e520, kMapData = 0x00468d64;
constexpr std::uint32_t kCStringConstruct = 0x00450d1c, kCStringAssign = 0x00450c70,
                        kCStringLoadString = 0x00450c68;
constexpr std::size_t kBlockSize = 0x2d00;
constexpr std::size_t kFootX = 0x7c, kFootY = 0x80, kBiochemistry = 0x2c1c, kSex = 0x2cc1, kChild = 0x2cc8,
                      kDeathState = 0x2ccc, kChemicals = 8;
constexpr std::size_t kRoomCount = 0x8, kRooms = 0xc, kRoomStride = 0x14;
// The fields under test.
constexpr std::size_t kGenomeMoniker = 0x810, kDisplayName = 0x814, kAge = 0x838;

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint32_t below(std::uint32_t n) { return next() % n; }
    std::int32_t range(std::int32_t a, std::int32_t b) {
        return a + static_cast<std::int32_t>(below(static_cast<std::uint32_t>(b - a + 1)));
    }
};

template <typename T>
T& field(std::uint8_t* block, std::size_t offset) {
    return *reinterpret_cast<T*>(block + offset);
}

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

std::string random_word(Random& random, std::size_t max_length) {
    std::string w;
    const std::size_t n = 1 + random.below(static_cast<std::uint32_t>(max_length));
    for (std::size_t i = 0; i < n; ++i) w += static_cast<char>('a' + random.below(26));
    return w;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: status_equivalence <Creatures.exe> <install-dir> [cases]\n");
        return 2;
    }
    const long cases = argc > 3 ? std::strtol(argv[3], nullptr, 10) : 20000;
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
    std::vector<std::uint8_t> map_data(0x400, 0);
    *image.at<void**>(kMapData) = map_data.data();
#if defined(LC_NATIVE_MODE) && !defined(EQUIVALENCE_EXPECT)
    const bool all_native = true;
#else
    const bool all_native = false;
#endif

    const Creature::StatusStrings strings{resource_text(0xef2b), resource_text(0xef2c), resource_text(0xef2d),
                                          resource_text(0xef2f), resource_text(0xef30)};
    using Format = void(__attribute__((thiscall)) *)(void*, char*, int);
    Random random{0x3c6ef372fe94f82bull};
    long differing = 0, pregnant = 0, dead = 0, in_room = 0;
    for (long index = 0; index < cases; ++index) {
        std::vector<std::uint8_t> native(kBlockSize, 0), native_biochemistry(0x1300, 0);
        field<void*>(native.data(), kBiochemistry) = native_biochemistry.data();
        construct(native.data() + kGenomeMoniker);
        construct(native.data() + kDisplayName);
        auto lc = std::make_unique<Creature>();
        lc->set_biochemistry(new c1::biochemistry::Biochemistry());

        const std::string name = random_word(random, 20), moniker = random_word(random, 12);
        g_assign(native.data() + kDisplayName, name.c_str());
        g_assign(native.data() + kGenomeMoniker, moniker.c_str());
        lc->register_state().history().display_name = name;
        lc->register_state().history().genome_moniker = moniker;
        const std::uint32_t age = random.below(4) == 0 ? random.below(700) : random.next() % 2000000;
        field<std::uint32_t>(native.data(), kAge) = age;
        lc->register_state().set_age_ticks(age);
        const bool male = random.below(2) == 0;
        native[kSex] = male ? 1 : 2;
        CreatureTestAccess::sex(*lc) = male ? c1::creatures::GenomeSex::male : c1::creatures::GenomeSex::female;
        const std::uint32_t child = random.below(2) == 0 ? 0 : 1 + random.below(1000);
        field<std::uint32_t>(native.data(), kChild) = child;
        lc->set_child_genome_source_filename(child);
        const std::uint8_t death = random.below(6) == 0 ? 1 : 0;
        native[kDeathState] = death;
        CreatureTestAccess::death_state(*lc) = death;
        auto& chemicals = lc->biochemistry()->chemical_states();
        for (int c : {0x42, 0x3b, 248, 249, 250, 251, 252, 253, 254, 255, 232, 233, 234}) {
            const std::uint8_t v = static_cast<std::uint8_t>(
                random.below(3) == 0 ? 0 : (c >= 232 ? random.below(0x40) : random.below(256)));
            chemicals[c].concentration = v;
            native_biochemistry[kChemicals + 2 * c] = v;
        }
        const std::int32_t fx = random.range(0, 3000), fy = random.range(0, 1200);
        lc->skeleton().down_foot_x = fx;
        lc->skeleton().down_foot_y = fy;
        field<std::int32_t>(native.data(), kFootX) = fx;
        field<std::int32_t>(native.data(), kFootY) = fy;
        std::vector<c1::world::MapRoom> rooms(random.below(7));
        for (auto& room : rooms) {
            const std::int32_t left = random.range(0, 2500), top = random.range(0, 1000);
            room.bounds = {left, top, left + random.range(10, 800), top + random.range(10, 400)};
            room.room_type = random.below(4);
        }
        std::memset(map_data.data(), 0, map_data.size());
        field<std::int32_t>(map_data.data(), kRoomCount) = static_cast<std::int32_t>(rooms.size());
        for (std::size_t r = 0; r < rooms.size(); ++r) {
            std::uint8_t* raw = map_data.data() + kRooms + r * kRoomStride;
            std::memcpy(raw, &rooms[r].bounds, 16);
            field<std::uint32_t>(raw, 16) = rooms[r].room_type;
        }

        std::vector<char> native_text(0x4000, 0);
        reinterpret_cast<Format>(image.at<void*>(kFormatStatus))(native.data(), native_text.data(), 0);
        const std::string lc_text =
            lc->format_status_for_external_query(c1::world::MapRoomTable{rooms.data(), rooms.size()}, strings);
        if (child != 0 && !male) ++pregnant;
        if (death != 0) ++dead;
        if (lc_text.substr(lc_text.rfind('|', lc_text.rfind('|', lc_text.rfind('|') - 1) - 1) + 1, 2) != "-1") {
            ++in_room;
        }
        if (lc_text != native_text.data() && ++differing <= 10) {
            std::printf("case %ld:\n  native %s\n  lc     %s\n", index, native_text.data(), lc_text.c_str());
        } else if (lc_text != native_text.data()) {
            std::printf("case %ld differs\n", index);
        }
    }
    std::printf("covered: %ld pregnant, %ld dead, %ld in a room\n", pregnant, dead, in_room);
    std::printf("%ld cases: %ld differ%s\n", cases, differing, all_native ? " (native build: must be 0)" : "");
    return all_native && differing != 0 ? 1 : 0;
}
