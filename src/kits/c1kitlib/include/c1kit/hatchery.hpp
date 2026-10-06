#pragma once

// The Hatchery's nest: six eggs, what is in each, and the script that puts
// one in the game's incubator.  Header-only and portable.
//
// In the 1996 kit egg n (0-5) is always a child of the stock genomes
// mum<n+1>.gen and dad<n+1>.gen, crossed afresh each time (`new: gene tokn
// mum<n+1> tokn dad<n+1> obv0`); Scramble Eggs (NestParents) picks other
// pairs, and Make My Creatures Colourful (append_colour_genes) adds colour
// to the crossed genome.  The 1996 kit (CHatcheryView) kept which eggs are left,
// and each one's sex, in the per-user registry value "Eggstra": six
// characters, one per egg, '0' male, '1' female, 'x' taken
// (ConsumeEggstraSlot @ 0x00404780; a double-click on egg 1 of "100110" left
// "x00110").  Once all six were taken it wanted an "Egg Disk" in drive A
// (a:\header.dat, a:\eggx\) before it would give more.

#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "genome.hpp"

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
inline std::string hatch_script_giving_genome(int slot, bool female,
                                              const std::string& genome_command) {
    return "inst,sys: wtop,sys: cmra 2223 724,new: simp eggs 8 " + std::to_string(slot * 8) +
           " 2000 0,pose 3,setv clas 33882624,setv attr 67," + genome_command +
           ",setv obv1 " + (female ? "2" : "1") + ",tick 2400,dde: hatc,mvto " +
           std::to_string(slot * 40 + 2408) + " 870";
}

inline std::string hatch_script(int slot, bool female, int mum, int dad) {
    return hatch_script_giving_genome(
        slot, female,
        "new: gene tokn mum" + std::to_string(mum) + " tokn dad" + std::to_string(dad) + " obv0");
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

// Which parents each egg in the nest is crossed from (1 to 6).  Its own
// pair, mum<n> with dad<n>, unless Scramble Eggs picked others when the nest
// was filled, so each egg can show its pair before it hatches.  Kept in the
// registry value "Egg Parents": six two-digit numbers, mum then dad
// ("41 25 63 14 52 36").
struct NestParents {
    int mum[kEggCount] = {1, 2, 3, 4, 5, 6};
    int dad[kEggCount] = {1, 2, 3, 4, 5, 6};
};

// A scrambled pair for every egg; `random` gives any random numbers.
template <typename Random>
NestParents scrambled_nest_parents(Random random) {
    NestParents parents;
    for (int i = 0; i < kEggCount; ++i) {
        scrambled_parents(static_cast<std::uint32_t>(random()), parents.mum[i], parents.dad[i]);
    }
    return parents;
}

// Scramble Eggs is on: every egg whose pair has the same number (its own
// pair, left from before the mode was on) gets a scrambled one.  True if
// any pair changed.
template <typename Random>
bool scramble_own_pairs(NestParents& parents, Random random) {
    bool changed = false;
    for (int i = 0; i < kEggCount; ++i) {
        if (parents.mum[i] == parents.dad[i]) {
            scrambled_parents(static_cast<std::uint32_t>(random()), parents.mum[i],
                              parents.dad[i]);
            changed = true;
        }
    }
    return changed;
}

inline std::string format_parents(const NestParents& parents) {
    std::string text;
    for (int i = 0; i < kEggCount; ++i) {
        if (i != 0) {
            text.push_back(' ');
        }
        text.push_back(static_cast<char>('0' + parents.mum[i]));
        text.push_back(static_cast<char>('0' + parents.dad[i]));
    }
    return text;
}

// False (and each egg's own pair) when it is not six such numbers.
inline bool parse_parents(const std::string& text, NestParents& out) {
    out = NestParents();
    if (text.size() != kEggCount * 3 - 1) {
        return false;
    }
    NestParents parents;
    for (int i = 0; i < kEggCount; ++i) {
        const std::size_t at = static_cast<std::size_t>(i) * 3;
        if ((i != 0 && text[at - 1] != ' ') || text[at] < '1' ||
            text[at] > '0' + kEggCount || text[at + 1] < '1' ||
            text[at + 1] > '0' + kEggCount) {
            return false;
        }
        parents.mum[i] = text[at] - '0';
        parents.dad[i] = text[at + 1] - '0';
    }
    out = parents;
    return true;
}

// Options > Make My Creatures Colourful.  The egg's genome is crossed first
// by a query that answers its moniker, so the kit can add colour to that
// genome file (and never to mum<n> or dad<n>) before the egg is made with
// it.  The moniker is the game's number for the four characters of the
// file name.
inline std::string cross_script(int mum, int dad) {
    return "new: gene tokn mum" + std::to_string(mum) + " tokn dad" + std::to_string(dad) +
           " var0,dde: putv var0,endm";
}

inline std::string hatch_script_for_genome(int slot, bool female, std::int32_t genome) {
    return hatch_script_giving_genome(slot, female, "setv obv0 " + std::to_string(genome));
}

// The genome file the game wrote for `genome`: its number's four bytes as
// characters, low byte first.
inline std::string genome_file_for(std::int32_t genome) {
    const auto id = static_cast<std::uint32_t>(genome);
    std::string name;
    for (int shift = 0; shift < 32; shift += 8) {
        name.push_back(static_cast<char>((id >> shift) & 0xff));
    }
    return name + ".gen";
}

// The colour added: a tint per pigment channel (red, green, blue; 0x80 is
// none) and the 1.04 "gext" extension's hue rotation and colour swap (0x80
// is none for both).
struct EggColours {
    std::uint8_t tint[3] = {0x80, 0x80, 0x80};
    std::uint8_t hue_rotation = 0x80;
    std::uint8_t colour_swap = 0x80;
};

template <typename Random>
EggColours random_egg_colours(Random random) {
    EggColours colours;
    for (std::uint8_t& tint : colours.tint) {
        tint = static_cast<std::uint8_t>(0x20 + random() % 0xc1);  // 0x20 to 0xe0
    }
    colours.hue_rotation = static_cast<std::uint8_t>(random() % 256);
    colours.colour_swap = static_cast<std::uint8_t>(random() % 256);
    return colours;
}

// Pigment genes added per channel.  The game averages a channel's pigment
// genes, so four beside the stock genomes' few give the tint about half the
// say; it averages each "gext" in turn into the one before, so the last ones
// (these) set the hue and swap.
constexpr int kColourGenesPerChannel = 4;

// Adds the pigment genes, each followed by "gext", before the genome's
// "gend".  Each is a creature-family subtype-6 gene: channel, amount, then
// "gext", hue rotation, colour swap.  New gene numbers follow the highest
// in the file.  They are mutable only: a duplicable gene can start crossover
// copying whole blocks again (genomes grow each generation), and a
// deletable one lets the colour drop out.  False, and the bytes unchanged,
// for a file that does not end in "gend".
inline bool append_colour_genes(std::vector<std::uint8_t>& genome, const EggColours& colours) {
    std::vector<Gene> genes;
    if (!parse_genome(genome, genes) || genome.size() < 4 ||
        !tag_at(genome, genome.size() - 4, "gend")) {
        return false;
    }
    std::uint8_t next_id = 0;
    constexpr std::uint8_t flags = kGeneMutable;
    for (const Gene& gene : genes) {
        if (gene.id >= next_id) {
            next_id = static_cast<std::uint8_t>(gene.id + 1);
        }
    }
    std::vector<std::uint8_t> added;
    for (int copy = 0; copy < kColourGenesPerChannel; ++copy) {
        for (std::uint8_t channel = 0; channel < 3; ++channel) {
            const std::uint8_t gene[] = {'g', 'e', 'n', 'e', kGeneFamilyCreature, 6, next_id++,
                                         0, 0, flags, channel, colours.tint[channel],
                                         'g', 'e', 'x', 't', colours.hue_rotation,
                                         colours.colour_swap};
            added.insert(added.end(), std::begin(gene), std::end(gene));
        }
    }
    genome.insert(genome.end() - 4, added.begin(), added.end());
    return true;
}

} // namespace c1kit
