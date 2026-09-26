#pragma once

// A creature's genome file, "<moniker>.gen" in the world's Genetics folder,
// read the way the game reads it.  Header-only and portable.
//
// The file is a run of genes, each a ten-byte header and a payload, ended by
// the four bytes "gend":
//
//   "gene" family subtype id generation switch-on-stage flags  payload...
//
// A payload runs to the next "gene" or "gend" (the game finds the end of a
// gene the same way).  The families and subtypes below, and the payload
// layouts of the genus and lobe genes, are those the game's genome loaders
// read (LibreCreatures src/c1/creatures/genome.hpp, skeleton.cpp, and
// brain/lobe.cpp and rules.cpp).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace c1kit {

enum GeneFamily : std::uint8_t {
    kGeneFamilyBrain = 0,
    kGeneFamilyBiochemistry = 1,
    kGeneFamilyCreature = 2,
};

// Gene flags (CGenome's own tests; genome.hpp in the game).
constexpr std::uint8_t kGeneMutable = 0x01;
constexpr std::uint8_t kGeneDuplicable = 0x02;
constexpr std::uint8_t kGeneDeletable = 0x04;
constexpr std::uint8_t kGeneMaleOnly = 0x08;
constexpr std::uint8_t kGeneFemaleOnly = 0x10;

struct Gene {
    std::uint8_t family = 0;
    std::uint8_t subtype = 0;
    std::uint8_t id = 0;
    std::uint8_t generation = 0;
    std::uint8_t switch_on_stage = 0;
    std::uint8_t flags = 0;
    std::vector<std::uint8_t> payload;
};

constexpr std::size_t kGeneHeaderBytes = 10;

inline bool tag_at(const std::vector<std::uint8_t>& bytes, std::size_t at,
                   const char* tag) {
    return at + 4 <= bytes.size() && bytes[at] == tag[0] &&
           bytes[at + 1] == tag[1] && bytes[at + 2] == tag[2] &&
           bytes[at + 3] == tag[3];
}

// False when the file does not start with a gene or never reaches "gend".
inline bool parse_genome(const std::vector<std::uint8_t>& bytes,
                         std::vector<Gene>& out) {
    out.clear();
    std::size_t at = 0;
    if (!tag_at(bytes, 0, "gene")) {
        return false;
    }
    while (at < bytes.size()) {
        if (tag_at(bytes, at, "gend")) {
            return true;
        }
        if (!tag_at(bytes, at, "gene") || at + kGeneHeaderBytes > bytes.size()) {
            return false;
        }
        Gene gene;
        gene.family = bytes[at + 4];
        gene.subtype = bytes[at + 5];
        gene.id = bytes[at + 6];
        gene.generation = bytes[at + 7];
        gene.switch_on_stage = bytes[at + 8];
        gene.flags = bytes[at + 9];
        std::size_t end = at + kGeneHeaderBytes;
        while (end < bytes.size() && !tag_at(bytes, end, "gene") &&
               !tag_at(bytes, end, "gend")) {
            ++end;
        }
        gene.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at + kGeneHeaderBytes),
                            bytes.begin() + static_cast<std::ptrdiff_t>(end));
        out.push_back(std::move(gene));
        at = end;
    }
    return false;
}

// The file a creature's genome is kept in: its moniker id's four bytes, as
// characters ("4b5a4633" is stored as 33 46 5a 4b: "3FZK.gen").
inline std::string moniker_characters(const std::string& moniker) {
    const unsigned long id = std::strtoul(moniker.c_str(), nullptr, 16);
    std::string text;
    for (int shift = 0; shift < 32; shift += 8) {
        text.push_back(static_cast<char>((id >> shift) & 0xff));
    }
    return text;
}

inline std::string genome_file_name(const std::string& moniker) {
    return moniker_characters(moniker) + ".gen";
}

// The reverse, for monikers stored as four raw bytes (the genus gene's
// parents): the Register's spelling, lower-case hex of the little-endian id.
inline std::string moniker_from_bytes(const std::uint8_t* four) {
    const unsigned long id = static_cast<unsigned long>(four[0]) |
                             (static_cast<unsigned long>(four[1]) << 8) |
                             (static_cast<unsigned long>(four[2]) << 16) |
                             (static_cast<unsigned long>(four[3]) << 24);
    char text[16];
    std::snprintf(text, sizeof(text), "%lx", id);
    return text;
}

// The gene types a genome editor names.  Subtype 6 of the creature family
// carries a two-byte payload (pigment channel and amount: the game's
// CreaturePigmentGenePayload).
inline const char* gene_type_name(std::uint8_t family, std::uint8_t subtype) {
    switch (family) {
    case kGeneFamilyBrain:
        return subtype == 0 ? "Brain lobe" : nullptr;
    case kGeneFamilyBiochemistry:
        switch (subtype) {
        case 0: return "Chemical receptor";
        case 1: return "Chemical emitter";
        case 2: return "Chemical reaction";
        case 3: return "Half-lives";
        case 4: return "Initial concentration";
        default: return nullptr;
        }
    case kGeneFamilyCreature:
        switch (subtype) {
        case 0: return "Stimulus";
        case 1: return "Genus";
        case 2: return "Appearance";
        case 3: return "Pose";
        case 4: return "Gait";
        case 5: return "Instinct";
        case 6: return "Pigment";
        default: return nullptr;
        }
    default:
        return nullptr;
    }
}

// Genus: the classifier genus is the gene's selector (masked to two bits)
// plus one (Skeleton::LoadGenome), named as ClassifierNames.txt names
// family 4's genera.
inline const char* species_name(int genus) {
    switch (genus) {
    case 1: return "Norn";
    case 2: return "Grendel";
    case 3: return "Ettin";
    default: return "Unknown";
    }
}

// A lobe gene's payload: grid x, y, width, height, then (after nine lobe
// settings and an eight-token expression) the winner-take-all byte and two
// dendrite rules of 47 bytes, each starting with its source lobe and the
// fewest and most dendrites a neuron grows under it.
struct LobeGene {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int dendrites_min[2] = {0, 0};
    int dendrites_max[2] = {0, 0};
    int source_lobe[2] = {0, 0};
};

constexpr std::size_t kLobeGenePayloadBytes = 112;
constexpr std::size_t kLobeRuleOffset = 18;
constexpr std::size_t kLobeRuleBytes = 47;

inline bool decode_lobe_gene(const Gene& gene, LobeGene& out) {
    if (gene.family != kGeneFamilyBrain || gene.subtype != 0 ||
        gene.payload.size() < kLobeGenePayloadBytes) {
        return false;
    }
    const std::vector<std::uint8_t>& p = gene.payload;
    out.x = p[0];
    out.y = p[1];
    out.width = p[2];
    out.height = p[3];
    for (int rule = 0; rule < 2; ++rule) {
        const std::size_t at = kLobeRuleOffset + kLobeRuleBytes * static_cast<std::size_t>(rule);
        out.source_lobe[rule] = p[at];
        out.dendrites_min[rule] = p[at + 1];
        // The game raises a maximum below the minimum to the minimum
        // (normalize_range_max).
        out.dendrites_max[rule] = p[at + 2] < p[at + 1] ? p[at + 1] : p[at + 2];
    }
    return true;
}

// How the brain's lobes are wired, as the game builds it from the genome
// (Brain::load_genome, Lobe::load_genome, LobeConnectionRule::load_from_genome):
// each lobe's two dendrite rules name the lobe its dendrites reach into, and a
// lobe may also copy its firing into the Perception lobe (lobe 0).  Which lobe
// is which is positional: the brain genes (family % 3 == 0, any subtype)
// switched on at stage 0 that apply to the creature's sex, those with header
// byte 7 at zero first and then the rest, up to 32; a rule's source is folded
// into that count.  The game does not report its wiring (the DDE item
// "BrainWiring" answers nothing), so this is the genome's plan: which lobes
// feed which, and how many dendrites a neuron may grow, not the dendrites.
struct DendriteRule {
    int source = 0;        // lobe index its dendrites reach into
    int fewest = 0;        // dendrites a neuron grows under it
    int most = 0;
    int spread = 0;        // how far from the matching cell they may land
    // 0: they stay where they were placed; 1 (attach loose dendrites) and 2
    // (migrate): loose ones move to firing neurons of the source as the
    // creature learns (Lobe::update_late_phase), so where they were placed
    // is only where they started.
    int mode = 0;
};

struct LobeWiring {
    int x = 0;             // where the genome places it, clamped as the game does
    int y = 0;
    DendriteRule rules[2];
    int perception_copy = 0;  // 0 none, 1 copies, 2 copies (mutually exclusive)
};

constexpr std::size_t kLobeGeneWiringBytes = kLobeRuleOffset + 2 * kLobeRuleBytes;
constexpr std::size_t kMaximumLobes = 32;

// `extended`: as LibreCreatures lays lobes out with its extended brain grid
// on, where an offset is the whole byte and nothing is pulled back here (the
// game keeps the lobe inside 208 x 208 at its final size, which
// wiring_matches allows for).
inline std::vector<LobeWiring> brain_wiring(const std::vector<Gene>& genes, bool male,
                                            bool extended = false) {
    std::vector<LobeWiring> lobes;
    for (int pass = 0; pass < 2; ++pass) {
        for (const Gene& gene : genes) {
            if (lobes.size() >= kMaximumLobes) break;
            if (gene.family % 3 != 0 || gene.switch_on_stage != 0) continue;
            const bool restricted = (gene.flags & (kGeneMaleOnly | kGeneFemaleOnly)) != 0;
            if (restricted && !(gene.flags & (male ? kGeneMaleOnly : kGeneFemaleOnly))) continue;
            if ((pass == 0) != (gene.generation == 0)) continue;
            if (gene.payload.size() < kLobeGeneWiringBytes) return {};  // the game would read past it
            const std::vector<std::uint8_t>& p = gene.payload;
            LobeWiring lobe;
            const int width = (p[2] + 62) % 63 + 1;
            const int height = (p[3] + 62) % 63 + 1;
            if (extended) {
                lobe.x = p[0];
                lobe.y = p[1];
            } else {
                lobe.x = p[0] < 0x40 ? p[0] : (p[0] & 0x3f);
                lobe.y = p[1] < 0x40 ? p[1] : (p[1] & 0x3f);
                if (lobe.x + width > 0x40) lobe.x = 0x40 - width;
                if (lobe.y + height > 0x40) lobe.y = 0x40 - height;
            }
            lobe.perception_copy = p[4] > 2 ? p[4] % 3 : p[4];
            for (int r = 0; r < 2; ++r) {
                const std::size_t at = kLobeRuleOffset + kLobeRuleBytes * static_cast<std::size_t>(r);
                DendriteRule& rule = lobe.rules[r];
                rule.source = p[at];
                rule.fewest = p[at + 1];
                rule.most = p[at + 2] < p[at + 1] ? p[at + 1] : p[at + 2];
                rule.spread = p[at + 4] > 8 ? p[at + 4] % 9 : p[at + 4];
                rule.mode = p[at + 9] > 2 ? p[at + 9] % 3 : p[at + 9];
            }
            lobes.push_back(lobe);
        }
    }
    for (LobeWiring& lobe : lobes) {
        for (DendriteRule& rule : lobe.rules) {
            rule.source %= static_cast<int>(lobes.size());
        }
    }
    return lobes;
}

// Where one neuron's dendrites under a rule land in the lobe they read, as
// Brain::initialize_connections places them when the brain is built: the
// first on the matching spot (neuron * source area / own neurons, as a cell
// of the source lobe), each other one within `spread` cells of it in x and y,
// kept inside the lobe.  In the source lobe's own cells; nothing if the
// neuron grows no dendrites under the rule.
struct DendriteReach {
    int spot_x = 0;
    int spot_y = 0;
    int left = 0;     // the cells the others may land on, inclusive
    int top = 0;
    int right = 0;
    int bottom = 0;
};

inline bool dendrite_reach(int neuron, int own_neurons, int source_width, int source_height,
                           int spread, int dendrites, DendriteReach& out) {
    if (dendrites <= 0 || own_neurons <= 0 || source_width <= 0 || source_height <= 0 ||
        neuron < 0 || neuron >= own_neurons) {
        return false;
    }
    const long spot = static_cast<long>(neuron) * source_width * source_height / own_neurons;
    out.spot_x = static_cast<int>(spot % source_width);
    out.spot_y = static_cast<int>(spot / source_width);
    const int reach = dendrites > 1 ? spread : 0;
    const auto clamp = [](int v, int extent) { return v < 0 ? 0 : v > extent - 1 ? extent - 1 : v; };
    out.left = clamp(out.spot_x - reach, source_width);
    out.right = clamp(out.spot_x + reach, source_width);
    out.top = clamp(out.spot_y - reach, source_height);
    out.bottom = clamp(out.spot_y + reach, source_height);
    return true;
}

// Whether a wiring plan is this brain's: as many lobes, each where the
// game's `lobe` reply puts it.
template <typename Layout>
bool wiring_matches(const std::vector<LobeWiring>& wiring, const std::vector<Layout>& lobes,
                    bool extended = false) {
    if (wiring.empty() || wiring.size() != lobes.size()) return false;
    constexpr int kExtent = 208;
    for (std::size_t i = 0; i < wiring.size(); ++i) {
        int x = wiring[i].x;
        int y = wiring[i].y;
        if (extended) {  // kept inside the grid at the game's final size
            if (x + lobes[i].width > kExtent) x = kExtent - lobes[i].width;
            if (y + lobes[i].height > kExtent) y = kExtent - lobes[i].height;
        }
        if (x != lobes[i].x || y != lobes[i].y) return false;
    }
    return true;
}

struct GeneTypeCount {
    std::uint8_t family = 0;
    std::uint8_t subtype = 0;
    int genes = 0;
    int bytes = 0;  // header and payload, as stored
};

struct GenomeSummary {
    int total_genes = 0;
    int total_bytes = 0;  // the file's length
    std::vector<GeneTypeCount> types;  // in family/subtype order
    bool has_genus = false;
    int genus = 0;  // classifier genus (1 Norn, 2 Grendel, 3 Ettin)
    std::string mother;  // moniker, Register spelling
    std::string father;
    int male_only_genes = 0;
    int female_only_genes = 0;
    int mutable_genes = 0;
    int highest_generation = 0;
    std::vector<LobeGene> lobes;
};

inline GenomeSummary summarize_genome(const std::vector<Gene>& genes,
                                      std::size_t file_bytes) {
    GenomeSummary summary;
    summary.total_genes = static_cast<int>(genes.size());
    summary.total_bytes = static_cast<int>(file_bytes);
    for (const Gene& gene : genes) {
        GeneTypeCount* type = nullptr;
        for (GeneTypeCount& existing : summary.types) {
            if (existing.family == gene.family && existing.subtype == gene.subtype) {
                type = &existing;
            }
        }
        if (type == nullptr) {
            GeneTypeCount added;
            added.family = gene.family;
            added.subtype = gene.subtype;
            std::size_t at = 0;
            while (at < summary.types.size() &&
                   (summary.types[at].family < gene.family ||
                    (summary.types[at].family == gene.family &&
                     summary.types[at].subtype < gene.subtype))) {
                ++at;
            }
            summary.types.insert(summary.types.begin() + static_cast<std::ptrdiff_t>(at), added);
            type = &summary.types[at];
        }
        ++type->genes;
        type->bytes += static_cast<int>(kGeneHeaderBytes + gene.payload.size());
        if (gene.flags & kGeneMaleOnly) ++summary.male_only_genes;
        if (gene.flags & kGeneFemaleOnly) ++summary.female_only_genes;
        if (gene.flags & kGeneMutable) ++summary.mutable_genes;
        if (gene.generation > summary.highest_generation) {
            summary.highest_generation = gene.generation;
        }
        if (gene.family == kGeneFamilyCreature && gene.subtype == 1 &&
            !summary.has_genus && gene.payload.size() >= 9) {
            summary.has_genus = true;
            summary.genus = (gene.payload[0] & 3) + 1;
            summary.mother = moniker_from_bytes(&gene.payload[1]);
            summary.father = moniker_from_bytes(&gene.payload[5]);
        }
        LobeGene lobe;
        if (decode_lobe_gene(gene, lobe)) {
            summary.lobes.push_back(lobe);
        }
    }
    return summary;
}

// How many dendrites a brain can grow, given each lobe's neuron count (from
// the game's `lobe` reply; the genome's widths are adjusted as the lobes are
// built): the sums of every neuron's fewest and most, over both rules.
inline void dendrite_range(const std::vector<LobeGene>& lobes,
                           const std::vector<int>& neurons_per_lobe,
                           long& fewest, long& most) {
    fewest = 0;
    most = 0;
    for (std::size_t i = 0; i < lobes.size() && i < neurons_per_lobe.size(); ++i) {
        const long neurons = neurons_per_lobe[i];
        fewest += neurons * (lobes[i].dendrites_min[0] + lobes[i].dendrites_min[1]);
        most += neurons * (lobes[i].dendrites_max[0] + lobes[i].dendrites_max[1]);
    }
}

} // namespace c1kit
