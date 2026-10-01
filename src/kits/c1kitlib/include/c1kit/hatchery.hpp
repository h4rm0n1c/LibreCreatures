#pragma once

// The Hatchery's nest: six eggs, what is in each, and the script that puts
// one in the game's incubator.  Header-only and portable.
//
// Egg n (0-5) is always a child of the stock genomes mum<n+1>.gen and
// dad<n+1>.gen, crossed afresh each time (`new: gene tokn mum<n+1> tokn
// dad<n+1> obv0`).  The 1996 kit (CHatcheryView) kept which eggs are left,
// and each one's sex, in the per-user registry value "Eggstra": six
// characters, one per egg, '0' male, '1' female, 'x' taken
// (ConsumeEggstraSlot @ 0x00404780; a double-click on egg 1 of "100110" left
// "x00110").  Once all six were taken it wanted an "Egg Disk" in drive A
// (a:\header.dat, a:\eggx\) before it would give more.

#include <cstdint>
#include <string>

namespace c1kit {

constexpr int kEggCount = 6;

enum class EggState { male, female, taken };

struct Nest {
    EggState eggs[kEggCount] = {EggState::taken, EggState::taken, EggState::taken,
                                EggState::taken, EggState::taken, EggState::taken};

    bool any_left() const {
        for (const EggState egg : eggs) {
            if (egg != EggState::taken) {
                return true;
            }
        }
        return false;
    }
};

// "100110"; false (and an empty nest) when it is not six such characters.
inline bool parse_nest(const std::string& text, Nest& out) {
    out = Nest();
    if (text.size() != kEggCount) {
        return false;
    }
    for (int i = 0; i < kEggCount; ++i) {
        switch (text[static_cast<std::size_t>(i)]) {
        case '0': out.eggs[i] = EggState::male; break;
        case '1': out.eggs[i] = EggState::female; break;
        case 'x': out.eggs[i] = EggState::taken; break;
        default: out = Nest(); return false;
        }
    }
    return true;
}

inline std::string format_nest(const Nest& nest) {
    std::string text;
    for (const EggState egg : nest.eggs) {
        text.push_back(egg == EggState::male ? '0' : egg == EggState::female ? '1' : 'x');
    }
    return text;
}

// Six new eggs, each male or female by one bit of `random`.
inline Nest fresh_nest(std::uint32_t random) {
    Nest nest;
    for (int i = 0; i < kEggCount; ++i) {
        nest.eggs[i] = (random >> i) & 1 ? EggState::female : EggState::male;
    }
    return nest;
}

// The script a double-click on egg `slot` sent (captured from the 1996
// kit): bring the game to the front, show the incubator, make the egg
// object, cross its parents' genomes, set its sex (obv1: 1 male, 2 female),
// start its hatching timer and move it into the incubator, slot by slot.
// Run on a scheduler (mode 0) holder, with ExecuteMacro.
// `mum` and `dad` (1 to 6) name the genomes crossed, mum<n>.gen and
// dad<n>.gen in the game's Genetics folder; the 1996 kit always crossed the
// slot's own pair.
inline std::string hatch_script(int slot, bool female, int mum, int dad) {
    return "inst,sys: wtop,sys: cmra 2223 724,new: simp eggs 8 " + std::to_string(slot * 8) +
           " 2000 0,pose 3,setv clas 33882624,setv attr 67,new: gene tokn mum" +
           std::to_string(mum) + " tokn dad" + std::to_string(dad) + " obv0,setv obv1 " +
           (female ? "2" : "1") + ",tick 2400,dde: hatc,mvto " +
           std::to_string(slot * 40 + 2408) + " 870";
}

inline std::string hatch_script(int slot, bool female) {
    return hatch_script(slot, female, slot + 1, slot + 1);
}

// Options > Scramble Eggs: any mum with any dad of the six, except the two
// with the same number (the nest's own pairs), each of the 30 pairs equally
// likely.  `random` is any random number.
inline void scrambled_parents(std::uint32_t random, int& mum, int& dad) {
    const std::uint32_t pair = random % (kEggCount * (kEggCount - 1));
    mum = static_cast<int>(pair / (kEggCount - 1)) + 1;
    dad = static_cast<int>(pair % (kEggCount - 1)) + 1;
    if (dad >= mum) {
        ++dad;
    }
}

} // namespace c1kit
