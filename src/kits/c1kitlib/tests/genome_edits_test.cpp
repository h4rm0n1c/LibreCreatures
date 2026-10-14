// Portable tests for the Designer's genome edits (c1kit/genome_edits.hpp).
//   g++ -std=c++17 -I../include genome_edits_test.cpp && ./a.out [GENOME.gen...]
// Each genome named on the command line must read back byte for byte up to
// and including "gend" (the stock files are padded past it with bytes the
// game never reads).

#include "c1kit/genome_edits.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>

using namespace c1kit;

namespace {

Gene gene(std::uint8_t family, std::uint8_t subtype, std::uint8_t id,
          std::vector<std::uint8_t> payload, std::uint8_t flags = kGeneMutable | kGeneDuplicable) {
    Gene g;
    g.family = family;
    g.subtype = subtype;
    g.id = id;
    g.flags = flags;
    g.payload = std::move(payload);
    return g;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<Gene> genes = {
        gene(0, 0, 0, std::vector<std::uint8_t>(120, 1)),                       // a lobe
        gene(1, 2, 1, {1, kChemicalHungerIncrease, 0, 0, 1, 2, 1, kChemicalPunishment, 40}),
        gene(1, 2, 2, {1, 17, 0, 0, 1, 1, 1, kChemicalPunishment, 40}),          // pain raiser
        gene(1, 2, 3, {1, kChemicalReward, 0, 0, 1, 51, 0, 0, 40}),              // a reinforcer
        gene(1, 0, 4, {1, 0, 6, kChemicalAgeing, 5, 117, 255, 3}),               // old-age death
        gene(1, 0, 5, {1, 0, 2, kChemicalAgeing, 155, 110, 255, 3}),             // ageing -> stage 3
        gene(1, 0, 6, {0, 6, 13, kChemicalReward, 0, 0, 255, 0}),                // brain receptor
        gene(1, 4, 7, {kChemicalGlycogen, 122}),
        gene(1, 4, 8, {kChemicalGlucose, 200}),
        gene(1, 4, 9, {kChemicalFirstAntibody, 130}),
        gene(2, 6, 10, {0, 0x90, 'g', 'e', 'x', 't', 0x40, 0x50}),               // pigment + gext
    };
    const std::vector<std::uint8_t> file = write_genome(genes);
    std::vector<Gene> parsed;
    assert(parse_genome(file, parsed) && parsed.size() == genes.size());
    assert(write_genome(parsed) == file);
    assert(parsed.back().payload.size() == 8 && parsed.back().payload[2] == 'g');

    std::vector<Gene> edited = parsed;
    assert(remove_drive_raise_punishment(edited) == 1);
    assert(edited[1].payload[7] == 0 && edited[1].payload[6] == 0 && edited[1].payload[5] == 2);
    assert(edited[2].payload[7] == kChemicalPunishment);  // pain keeps its punishment

    edited = parsed;
    assert(remove_old_age_death(edited) == 1 && edited.size() == parsed.size() - 1);
    assert(edited[4].id == 5);  // the stage-3 ageing receptor stays

    edited = parsed;
    // The lobe, every reaction making or using a reinforcer (the two drive
    // raisers make Punishment), and the brain receptor.
    assert(lock_learning_genes(edited) == 5);
    assert(edited[0].flags == 0 && edited[1].flags == 0 && edited[3].flags == 0 &&
           edited[6].flags == 0);
    assert(edited[7].flags != 0);

    edited = parsed;
    assert(harden_newborns(edited) == 1 + 7);  // glycogen raised, glucose already 200, 7 antibodies
    assert(edited[7].payload[1] == 160 && edited[8].payload[1] == 200);
    assert(edited.size() == parsed.size() + 7 && edited.back().id == 17);

    std::vector<std::uint8_t> bytes = file;
    DesignerChoices all;
    all.eat_better = all.no_old_age_death = all.lock_learning = all.hardier_babies = true;
    std::string summary;
    assert(apply_designer_choices(bytes, all, &summary) && bytes != file && !summary.empty());
    std::vector<std::uint8_t> broken = {'x', 'y'};
    assert(!apply_designer_choices(broken, all) && broken.size() == 2);

    for (int i = 1; i < argc; ++i) {
        std::ifstream in(argv[i], std::ios::binary);
        const std::vector<std::uint8_t> real((std::istreambuf_iterator<char>(in)),
                                             std::istreambuf_iterator<char>());
        std::vector<Gene> real_genes;
        assert(parse_genome(real, real_genes));
        std::size_t end = 0;
        while (end + 4 <= real.size() && !tag_at(real, end, "gend")) {
            ++end;
        }
        const std::vector<std::uint8_t> used(real.begin(),
                                             real.begin() + static_cast<std::ptrdiff_t>(end + 4));
        assert(write_genome(real_genes) == used);
        std::printf("%s: %zu genes read back exactly\n", argv[i], real_genes.size());
    }
    std::puts("genome_edits_test: ok");
    return 0;
}
