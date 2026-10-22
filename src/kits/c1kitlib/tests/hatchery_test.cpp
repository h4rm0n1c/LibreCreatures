// Portable tests for the Hatchery's nest (c1kit/hatchery.hpp).
//   g++ -std=c++17 -I../include hatchery_test.cpp

#include "c1kit/game_sprite.hpp"
#include "c1kit/hatchery.hpp"

#include <cassert>
#include <cstdio>

using namespace c1kit;

namespace {

// Up to nine parents: scrambles draw only from those there, every pair can
// come up, never a same-numbered one; "Egg Parents" takes 7 to 9 too.
void test_more_parents() {
    ParentSet set;
    set.mums = {1, 2, 7, 9};
    set.dads = {2, 8, 9};
    bool seen[10][10] = {};
    for (std::uint32_t r = 0; r < 200; ++r) {
        int mum = 0, dad = 0;
        scrambled_parents(r, set, mum, dad);
        assert(mum != dad);
        assert(mum == 1 || mum == 2 || mum == 7 || mum == 9);
        assert(dad == 2 || dad == 8 || dad == 9);
        seen[mum][dad] = true;
    }
    int pairs = 0;
    for (const auto& row : seen) for (const bool pair : row) pairs += pair ? 1 : 0;
    assert(pairs == 4 * 3 - 2);  // 2-2 and 9-9 never
    NestParents parents;
    assert(parse_parents("79 18 92 21 33 44", parents) && parents.mum[0] == 7 && parents.dad[2] == 2);
    assert(format_parents(parents) == "79 18 92 21 33 44");
    ParentSet none;
    none.mums = {3};
    none.dads = {3};
    int mum = 0, dad = 0;
    scrambled_parents(5, none, mum, dad);
    assert(mum == 1 && dad == 1);
}

// The face: genus and head variant from the genes, and the files to try in
// the game's order.
void test_parent_look() {
    std::vector<Gene> genes(4);
    for (Gene& gene : genes) gene.family = kGeneFamilyCreature;
    genes[0].subtype = 1;  // genus: selector 6 & 3 = 2 (Ettin)
    genes[0].payload = {6, 0, 0, 0, 0, 0, 0, 0, 0};
    genes[1].subtype = 2;  // head (group 0) variant 3
    genes[1].payload = {0, 3};
    genes[2].subtype = 9;  // 9 % 7 = 2: appearance; group 5 % 5 = head, variant 14 % 10
    genes[2].payload = {5, 14};
    genes[3].subtype = 2;  // a male-only head gene, skipped for a female
    genes[3].flags = kGeneMaleOnly;
    genes[3].payload = {0, 6};
    const ParentLook female = parent_look(genes, true);
    assert(female.genus == 2 && female.head_variant == 4);
    const ParentLook male = parent_look(genes, false);
    assert(male.head_variant == 6);
    ParentLook norn;
    norn.head_variant = 1;
    const std::vector<std::string> names = head_sprite_names(norn, true, 2);
    // Female files first (genus 0 + 4), variant 1 stages 2..0, variant 0,
    // then the male files.
    assert(names.size() == 12 && names[0] == "a421" && names[2] == "a401" && names[3] == "a420" &&
           names[6] == "a021" && names.back() == "a000");
    assert(head_sprite_names(norn, false, 0)[0] == "a001" && head_sprite_names(norn, false, 0)[2] == "a401");
}

}  // namespace

int main() {
    test_more_parents();
    test_parent_look();
    Nest nest;
    assert(parse_nest("100110", nest));
    assert(nest.eggs[0] == EggState::female && nest.eggs[1] == EggState::male);
    assert(format_nest(nest) == "100110" && nest.any_left());
    nest.eggs[0] = EggState::taken;
    assert(format_nest(nest) == "x00110");
    assert(parse_nest("xxxxxx", nest) && !nest.any_left());
    assert(!parse_nest("10011", nest) && !parse_nest("10a110", nest) && !nest.any_left());
    assert(format_nest(fresh_nest(0x2d)) == "101101");

    // A two-frame sprite file: 2 x 1 and 1 x 2, then a truncated one.
    std::vector<std::uint8_t> file = {2, 0,
                                      18, 0, 0, 0, 2, 0, 1, 0,
                                      20, 0, 0, 0, 1, 0, 2, 0,
                                      0, 7, 9, 0};
    std::vector<GameSprite> sprites;
    assert(parse_game_sprites(file, sprites) && sprites.size() == 2);
    assert(sprites[0].width == 2 && sprites[0].height == 1 && sprites[0].pixels[1] == 7);
    assert(sprites[1].height == 2 && sprites[1].pixels[0] == 9 && sprites[1].pixels[1] == 0);
    file.pop_back();
    assert(!parse_game_sprites(file, sprites) && sprites.empty());
    std::vector<std::uint8_t> palette(768, 0);
    palette[3] = 63;  // colour 1's red, 6 bits
    palette[5] = 1;
    GamePalette colours;
    assert(parse_palette_dta(palette, colours) && colours[1].red == 252 && colours[1].blue == 4);
    assert(!parse_palette_dta(std::vector<std::uint8_t>(767), colours));
    assert(egg_sprite_frame(2, kEggSpriteCracked) == 23);

    // Exactly what the 1996 kit sent for egg 1 (female).
    assert(hatch_script(0, true) ==
           "inst,sys: wtop,sys: cmra 2223 724,new: simp eggs 8 0 2000 0,pose 3,"
           "setv clas 33882624,setv attr 67,new: gene tokn mum1 tokn dad1 obv0,"
           "setv obv1 2,tick 2400,dde: hatc,mvto 2408 870");
    assert(hatch_script(5, false).find("simp eggs 8 40 ") != std::string::npos);
    assert(hatch_script(5, false).find("tokn mum6 tokn dad6") != std::string::npos);
    assert(hatch_script(5, false).find("setv obv1 1,") != std::string::npos);
    assert(hatch_script(5, false).find("mvto 2608 870") != std::string::npos);
    assert(hatch_script(2, true, 4, 1).find("tokn mum4 tokn dad1") != std::string::npos);
    assert(hatch_script(2, true, 4, 1).find("simp eggs 8 16 ") != std::string::npos);
    // Scrambled pairs: never the same number, and all 30 others reachable.
    int seen[kEggCount + 1][kEggCount + 1] = {};
    for (std::uint32_t random = 0; random < 300; ++random) {
        int mum = 0, dad = 0;
        scrambled_parents(random, mum, dad);
        assert(mum >= 1 && mum <= kEggCount && dad >= 1 && dad <= kEggCount && mum != dad);
        ++seen[mum][dad];
    }
    for (int mum = 1; mum <= kEggCount; ++mum) {
        for (int dad = 1; dad <= kEggCount; ++dad) {
            assert((seen[mum][dad] == 10) == (mum != dad));
        }
    }
    // Egg parents: own pairs by default, kept as "mm dd" pairs.
    NestParents parents;
    assert(format_parents(parents) == "11 22 33 44 55 66");
    std::uint32_t counter = 0;
    parents = scrambled_nest_parents([&counter] { return counter += 7; });
    for (int i = 0; i < kEggCount; ++i) {
        assert(parents.mum[i] != parents.dad[i]);
    }
    NestParents read_back;
    assert(parse_parents(format_parents(parents), read_back));
    for (int i = 0; i < kEggCount; ++i) {
        assert(read_back.mum[i] == parents.mum[i] && read_back.dad[i] == parents.dad[i]);
    }
    assert(!parse_parents("11 22 33 44 55 60", read_back) && read_back.mum[5] == 6);
    // Turned on with eggs on their own pairs: those get scrambled ones, the
    // scrambled ones stay.
    NestParents mixed;
    mixed.mum[1] = 4;
    mixed.dad[1] = 2;
    assert(scramble_own_pairs(mixed, [&counter] { return counter += 5; }));
    for (int i = 0; i < kEggCount; ++i) {
        assert(mixed.mum[i] != mixed.dad[i]);
    }
    assert(mixed.mum[1] == 4 && mixed.dad[1] == 2);
    assert(!scramble_own_pairs(mixed, [&counter] { return counter += 5; }));
    assert(!parse_parents("11 22 33", read_back));

    // Colourful eggs: a cross that answers the genome, then the egg made
    // with it.
    assert(cross_script(4, 1) == "new: gene tokn mum4 tokn dad1 var0,dde: putv var0,endm");
    assert(hatch_script_for_genome(2, true, 1481261369).find(
               "setv attr 67,setv obv0 1481261369,setv obv1 2,") != std::string::npos);
    assert(genome_file_for(1481261369) == "9AJX.gen");
    std::vector<std::uint8_t> genome = {'g', 'e', 'n', 'e', 2, 6, 40, 0, 0, 0x07, 1, 0x80,
                                        'g', 'e', 'n', 'e', 1, 3, 90, 0, 0, 0x01, 9, 9,
                                        'g', 'e', 'n', 'd'};
    EggColours egg_colours;
    egg_colours.tint[0] = 0x30;
    egg_colours.hue_rotation = 0xa0;
    egg_colours.colour_swap = 0x10;
    assert(append_colour_genes(genome, egg_colours));
    std::vector<Gene> genes;
    assert(parse_genome(genome, genes) && genes.size() == 2 + 3 * kColourGenesPerChannel);
    const Gene& first_added = genes[2];
    assert(first_added.family == 2 && first_added.subtype == 6 && first_added.id == 91 &&
           first_added.flags == kGeneMutable);
    const std::vector<std::uint8_t> expected = {0, 0x30, 'g', 'e', 'x', 't', 0xa0, 0x10};
    assert(first_added.payload == expected);
    assert(genes.back().payload[0] == 2 && genes.back().id == 91 + 11);
    assert(tag_at(genome, genome.size() - 4, "gend"));
    std::vector<std::uint8_t> broken = {'g', 'e', 'n', 'e', 2, 6, 1, 0, 0, 0, 1, 2};
    assert(!append_colour_genes(broken, egg_colours) && broken.size() == 12);
    std::puts("hatchery_test: all passed");
    return 0;
}
