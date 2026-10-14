#pragma once

// Genome edits for the Hatchery's Designer: whole-genome changes a player
// can switch on before an egg is made.  Each one finds the genes it changes
// by what they do (chemicals, loci), not by gene number, so it works on any
// genome, and each reports how many genes it changed.  The changes are the
// Libre Set's (~/creatures/genome/recipe/libreset.yaml), which took them
// from Creature Labs' Forest norns, Feliz Norns (MuppetBoy) and Jessica's
// Discover Albia.  Header-only and portable.

#include <cstdint>
#include <string>
#include <vector>

#include "genome.hpp"

namespace c1kit {

// Chemical numbers (allchemicals.str).
constexpr std::uint8_t kChemicalNeedForPleasureIncrease = 18;
constexpr std::uint8_t kChemicalHungerIncrease = 19;
constexpr std::uint8_t kChemicalTirednessIncrease = 22;
constexpr std::uint8_t kChemicalSleepinessIncrease = 23;
constexpr std::uint8_t kChemicalBoredomIncrease = 27;
constexpr std::uint8_t kChemicalSexDriveIncrease = 29;
constexpr std::uint8_t kChemicalReward = 49;
constexpr std::uint8_t kChemicalPunishment = 50;
constexpr std::uint8_t kChemicalLastReinforcer = 55;  // Reward .. Punishment Echo: 49..55
constexpr std::uint8_t kChemicalAgeing = 56;
constexpr std::uint8_t kChemicalGlucose = 58;
constexpr std::uint8_t kChemicalGlycogen = 59;
constexpr std::uint8_t kChemicalFirstAntibody = 240;

// The whole genome back as a file: each gene's header and payload, then
// "gend".  A pigment "gext" lives inside its gene's payload and is kept.
// Anything after "gend" (the stock files are padded to 10001 bytes with
// bytes the game never reads) is not.
inline std::vector<std::uint8_t> write_genome(const std::vector<Gene>& genes) {
    std::vector<std::uint8_t> bytes;
    for (const Gene& gene : genes) {
        const std::uint8_t header[] = {'g', 'e', 'n', 'e', gene.family, gene.subtype, gene.id,
                                       gene.generation, gene.switch_on_stage, gene.flags};
        bytes.insert(bytes.end(), std::begin(header), std::end(header));
        bytes.insert(bytes.end(), gene.payload.begin(), gene.payload.end());
    }
    const char end[] = {'g', 'e', 'n', 'd'};
    bytes.insert(bytes.end(), std::begin(end), std::end(end));
    return bytes;
}

inline bool is_reaction(const Gene& gene) {
    return gene.family == kGeneFamilyBiochemistry && gene.subtype == 2 && gene.payload.size() >= 9;
}

inline bool is_receptor(const Gene& gene) {
    return gene.family == kGeneFamilyBiochemistry && gene.subtype == 0 && gene.payload.size() >= 8;
}

inline bool is_emitter(const Gene& gene) {
    return gene.family == kGeneFamilyBiochemistry && gene.subtype == 1 && gene.payload.size() >= 8;
}

inline bool is_initial_concentration(const Gene& gene) {
    return gene.family == kGeneFamilyBiochemistry && gene.subtype == 4 && gene.payload.size() >= 2;
}

// "Eat better".  The drive raisers for Need for Pleasure, Hunger,
// Tiredness, Sleepiness, Boredom and Sex Drive (X Increase -> X +
// Punishment) punish whatever the norn is doing while the drive rises,
// eating included, so norns learn to avoid what keeps them alive.  Their
// Punishment goes; the other drive raisers (pain, cold, heat, loneliness,
// crowding, fear, anger) keep theirs.  Creature Labs' Forest norns removed
// the first five; the Libre Set (v0.2) also removed sex drive's.
inline int remove_drive_raise_punishment(std::vector<Gene>& genes) {
    const std::uint8_t raisers[] = {kChemicalNeedForPleasureIncrease, kChemicalHungerIncrease,
                                    kChemicalTirednessIncrease, kChemicalSleepinessIncrease,
                                    kChemicalBoredomIncrease, kChemicalSexDriveIncrease};
    int changed = 0;
    for (Gene& gene : genes) {
        if (!is_reaction(gene)) {
            continue;
        }
        std::vector<std::uint8_t>& p = gene.payload;
        bool raiser = false;
        for (const std::uint8_t chemical : raisers) {
            raiser = raiser || (p[1] == chemical && p[0] != 0) || (p[3] == chemical && p[2] != 0);
        }
        if (!raiser) {
            continue;
        }
        bool edited = false;
        for (const std::size_t product : {std::size_t{5}, std::size_t{7}}) {
            if (p[product] == kChemicalPunishment) {
                p[product] = 0;
                p[product - 1] = 0;
                edited = true;
            }
        }
        changed += edited ? 1 : 0;
    }
    return changed;
}

// "No old-age death".  Removes the receptors that end life by age: Ageing
// on the creature's last life stage (stage 7, death) or on its death
// locus.  The stock genomes have none; the Forest norns and Updated C1
// genomes (and the Libre Set's old-age-death build) do.
inline int remove_old_age_death(std::vector<Gene>& genes) {
    int removed = 0;
    for (std::size_t i = 0; i < genes.size();) {
        const Gene& gene = genes[i];
        if (is_receptor(gene) && gene.payload[3] == kChemicalAgeing &&
            (gene.payload[0] & 1) == 1 &&
            ((gene.payload[1] == 0 && gene.payload[2] == 6) ||   // life stage -> 7
             (gene.payload[1] == 3 && gene.payload[2] == 0))) {  // death
            genes.erase(genes.begin() + static_cast<std::ptrdiff_t>(i));
            ++removed;
        } else {
            ++i;
        }
    }
    return removed;
}

// "Lock learning genes".  The brain's lobes, every receptor and emitter
// wired to a brain locus, and every reaction that makes or uses a
// reinforcer (Reward .. Punishment Echo) can no longer mutate, duplicate
// or be deleted, so offspring keep a brain that learns.  Mutation stays
// everywhere else.  From Jessica's no-mutate norns (Discover Albia), as the
// Libre Set does it.
inline int lock_learning_genes(std::vector<Gene>& genes) {
    constexpr std::uint8_t kLocks = kGeneMutable | kGeneDuplicable | kGeneDeletable;
    int changed = 0;
    for (Gene& gene : genes) {
        bool learning = gene.family == kGeneFamilyBrain;
        if ((is_receptor(gene) || is_emitter(gene)) && (gene.payload[0] & 1) == 0) {
            learning = true;
        }
        if (is_reaction(gene)) {
            for (const std::size_t chemical : {1, 3, 5, 7}) {
                const std::uint8_t c = gene.payload[chemical];
                learning = learning || (c >= kChemicalReward && c <= kChemicalLastReinforcer);
            }
        }
        if (learning && (gene.flags & kLocks) != 0) {
            gene.flags = static_cast<std::uint8_t>(gene.flags & ~kLocks);
            ++changed;
        }
    }
    return changed;
}

// "Hardier babies".  A newborn starts with at least 160 Glycogen and 190
// Glucose (stock: 122 and 159) against stillbirth and early starvation,
// and with 130 of every antibody it has none of (stock gives antibodies 0,
// 1, 3 and 4 only), so it starts immune to antigens 2, 5, 6 and 7 too.
// Feliz Norns (MuppetBoy) and Jessica's Discover Albia, as the Libre Set
// does it.  New genes take the next free numbers.
inline int harden_newborns(std::vector<Gene>& genes) {
    int changed = 0;
    bool antibody_present[8] = {};
    std::uint8_t next_id = 0;
    for (Gene& gene : genes) {
        if (gene.id >= next_id) {
            next_id = static_cast<std::uint8_t>(gene.id + 1);
        }
        if (!is_initial_concentration(gene)) {
            continue;
        }
        std::vector<std::uint8_t>& p = gene.payload;
        const std::uint8_t floor = p[0] == kChemicalGlycogen ? 160 : p[0] == kChemicalGlucose ? 190 : 0;
        if (floor != 0 && p[1] < floor) {
            p[1] = floor;
            ++changed;
        }
        if (p[0] >= kChemicalFirstAntibody && p[0] < kChemicalFirstAntibody + 8) {
            antibody_present[p[0] - kChemicalFirstAntibody] = true;
        }
    }
    for (int antibody = 0; antibody < 8; ++antibody) {
        if (antibody_present[antibody]) {
            continue;
        }
        Gene gene;
        gene.family = kGeneFamilyBiochemistry;
        gene.subtype = 4;
        gene.id = next_id++;
        gene.flags = kGeneMutable;
        gene.payload = {static_cast<std::uint8_t>(kChemicalFirstAntibody + antibody), 130};
        genes.push_back(gene);
        ++changed;
    }
    return changed;
}

// What the Designer applies, as switches.
struct DesignerChoices {
    bool eat_better = false;
    bool no_old_age_death = false;
    bool lock_learning = false;
    bool hardier_babies = false;
};

// Applies the chosen edits to a genome file; false, and the bytes
// unchanged, when the file does not parse.  `summary` says what changed.
inline bool apply_designer_choices(std::vector<std::uint8_t>& genome,
                                   const DesignerChoices& choices, std::string* summary = nullptr) {
    std::vector<Gene> genes;
    if (!parse_genome(genome, genes)) {
        return false;
    }
    std::string text;
    const auto note = [&text](const char* what, int count) {
        text += std::string(text.empty() ? "" : "; ") + what + " " + std::to_string(count);
    };
    if (choices.eat_better) {
        note("drive raisers no longer punishing:", remove_drive_raise_punishment(genes));
    }
    if (choices.no_old_age_death) {
        note("old-age death receptors removed:", remove_old_age_death(genes));
    }
    if (choices.lock_learning) {
        note("learning genes locked:", lock_learning_genes(genes));
    }
    if (choices.hardier_babies) {
        note("newborn genes hardened:", harden_newborns(genes));
    }
    genome = write_genome(genes);
    if (summary != nullptr) {
        *summary = text;
    }
    return true;
}

} // namespace c1kit
