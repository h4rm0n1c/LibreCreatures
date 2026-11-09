// A creature's bacterium: the original's Creature::UpdateBacteriumAndEnvironment
// @00409190 against creatures::Creature::update_bacterium_and_environment.
//
//   bacterium_equivalence.exe <Creatures.exe> [cases]
//
// The original's creature is a byte block with its biochemistry block at
// +0x2c18+4; the original's AddChemicalMoles (00401890) runs as native
// code.  Both creatures are dead (+0x2ccc), so the function stops after the
// bacterium: the environment and goal-direction updates it then calls are
// not part of this test.  LC's bacterium is set through its own archive
// load; that only puts the state in, the pairing below is behaviour.
//
// Each case: a random bacterium (state 0 to 2, input chemical, kill and
// activation thresholds, four outputs, some of them none) and random
// chemical concentrations, then one update.  The bacterium's state and all
// 256 concentrations must match.  The same results prove the offsets the
// load test's table uses: state +0x2cd4, input +0x2cd5, kill threshold
// +0x2cd6, activation threshold +0x2cd7, outputs +0x2cd8.
#include "native_image.hpp"

#include "biochemistry/biochemistry.hpp"
#include "creatures/bacterium.hpp"
#include "creatures/creature.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace creatures1::creatures {
// Declared a friend in creature.hpp.
class CreatureTestAccess {
public:
    static std::uint8_t& death_state(Creature& c) { return c.death_state_; }
};
} // namespace creatures1::creatures

namespace c1 = creatures1;
using c1::creatures::Creature;
using c1::creatures::CreatureTestAccess;

namespace {

constexpr std::uint32_t kUpdate = 0x00409190, kDebugConsole = 0x00467d28;
constexpr std::size_t kBlockSize = 0x2d00, kBiochemistry = 0x2c1c, kDeathState = 0x2ccc;
constexpr std::size_t kBiochemistrySize = 0x1300, kChemicals = 8;
// The fields under test.
constexpr std::size_t kState = 0x2cd4, kInput = 0x2cd5, kKill = 0x2cd6, kActivation = 0x2cd7, kOutputs = 0x2cd8;

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint32_t below(std::uint32_t n) { return next() % n; }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next()); }
};

class BytesArchive final : public c1::creatures::BacteriumArchive {
public:
    explicit BytesArchive(const std::uint8_t* bytes) : bytes_(bytes) {}
    bool is_loading() const override { return true; }
    std::uint8_t read_byte() override { return bytes_[at_++]; }
    void write_byte(std::uint8_t) override { std::abort(); }

private:
    const std::uint8_t* bytes_;
    std::size_t at_ = 0;
};

class Host final : public c1::creatures::CreatureBacteriumEnvironmentHost {
public:
    c1::creatures::CreatureEnvironmentHost& environment_host() override { std::abort(); }
    c1::creatures::CreatureGoalDirectionHost& goal_direction_host() override { std::abort(); }
    bool is_selected_creature(const Creature&) const override { return false; }
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: bacterium_equivalence <Creatures.exe> [cases]\n");
        return 2;
    }
    const long cases = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 100000;
    equivalence::NativeImage image;
    std::string error;
    if (!image.load(argv[1], error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }
    *image.at<std::uint32_t*>(kDebugConsole) = 0;
    using Call0 = void(__attribute__((thiscall)) *)(void*);
    Random random{0xbb67ae8584caa73bull};
    long differing = 0, active = 0, killed = 0, activated = 0;
    for (long index = 0; index < cases; ++index) {
        std::vector<std::uint8_t> native(kBlockSize, 0), native_biochemistry(kBiochemistrySize, 0);
        *reinterpret_cast<void**>(native.data() + kBiochemistry) = native_biochemistry.data();
        native[kDeathState] = 1;
        auto lc = std::make_unique<Creature>();
        lc->set_biochemistry(new c1::biochemistry::Biochemistry());
        CreatureTestAccess::death_state(*lc) = 1;
        auto& chemicals = lc->biochemistry()->chemical_states();
        for (int c = 0; c < 256; ++c) {
            const std::uint8_t conc = random.byte(), life = random.byte();
            chemicals[c].concentration = conc;
            chemicals[c].half_life_selector = life;
            native_biochemistry[kChemicals + 2 * c] = conc;
            native_biochemistry[kChemicals + 2 * c + 1] = life;
        }
        // The input chemical is one of the bacterial ids (-8..-1); its
        // concentration is read 8 entries lower.
        std::uint8_t b[8];
        b[0] = static_cast<std::uint8_t>(random.below(3));
        b[1] = static_cast<std::uint8_t>(0xf8 + random.below(8));
        b[2] = random.byte();
        b[3] = random.byte();
        for (int k = 0; k < 4; ++k) b[4 + k] = random.below(3) == 0 ? 0 : random.byte();
        BytesArchive archive(b);
        lc->bacterium().serialize(archive);
        native[kState] = b[0];
        native[kInput] = b[1];
        native[kKill] = b[2];
        native[kActivation] = b[3];
        for (int k = 0; k < 4; ++k) native[kOutputs + k] = b[4 + k];
        if (b[0] != 0) ++active;

        Host host;
        lc->update_bacterium_and_environment(host, nullptr);
        reinterpret_cast<Call0>(image.at<void*>(kUpdate))(native.data());

        const auto lc_state = static_cast<std::uint8_t>(lc->bacterium().activity_state());
        if (b[0] != 0 && lc_state == 0) ++killed;
        if (b[0] == 1 && lc_state == 2) ++activated;
        std::string mismatch;
        char text[200];
        if (native[kState] != lc_state) {
            std::snprintf(text, sizeof text, "state %u input %02x kill %u activation %u: native -> %u, lc -> %u", b[0],
                          b[1], b[2], b[3], native[kState], lc_state);
            mismatch = text;
        }
        for (int c = 0; c < 256 && mismatch.empty(); ++c) {
            if (native_biochemistry[kChemicals + 2 * c] != chemicals[c].concentration) {
                std::snprintf(text, sizeof text, "state %u input %02x: chemical %d: native %u, lc %u", b[0], b[1], c,
                              native_biochemistry[kChemicals + 2 * c], chemicals[c].concentration);
                mismatch = text;
            }
        }
        if (!mismatch.empty() && ++differing <= 10) std::printf("case %ld: %s\n", index, mismatch.c_str());
    }
    std::printf("covered: %ld with a live bacterium, %ld killed, %ld activated\n", active, killed, activated);
    std::printf("%ld cases: %ld differ -> %s\n", cases, differing, differing == 0 ? "OK" : "FAILED");
    return differing == 0 ? 0 : 1;
}
