// Portable tests for the Science Kit's data (c1kit/science_files.hpp,
// genome.hpp, brain_map.hpp).
//   g++ -std=c++17 -I../include science_test.cpp
// With a directory argument, also reads the real allchemicals.str,
// themes.str, decision.str, injections.str and a genome file there:
//   ./a.out <install dir> <genome file>

#include "c1kit/brain_map.hpp"
#include "c1kit/genome.hpp"
#include "c1kit/science_files.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace {

using namespace c1kit;

std::vector<std::uint8_t> bytes_of(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

void test_strings_and_themes() {
    ArchiveWriter counted;
    counted.u16(2);
    counted.cstring("Quiescent");
    counted.cstring("Activate 1");
    std::vector<std::string> names;
    assert(parse_counted_strings(counted.bytes(), names));
    assert(names.size() == 2 && names[1] == "Activate 1");

    ArchiveWriter uncounted;
    uncounted.cstring("Energy");
    uncounted.cstring("Adrenaline");
    assert(parse_uncounted_strings(uncounted.bytes(), names));
    assert(names.size() == 2 && names[0] == "Energy");

    assert(!chemical_is_named("73") && !chemical_is_named("<NONE>") &&
           !chemical_is_named("") && chemical_is_named("Waste Water") &&
           !chemical_is_named("not_allocated2++"));
    std::vector<std::string> chemicals(256);
    chemicals[1] = "Pain";
    chemicals[73] = "73";
    assert(chemical_label(chemicals, 1) == "Pain");
    assert(chemical_label(chemicals, 73) == "Chemical 73");

    // themes.str: "Custom" with 2, 17, 0, 5.
    ArchiveWriter original;
    original.u16(1);
    original.cstring("Custom");
    original.u32(0x05001102u);
    std::vector<ChemicalTheme> themes;
    assert(parse_original_themes(original.bytes(), themes));
    assert(themes.size() == 1 && themes[0].name == "Custom");
    assert((themes[0].chemicals == std::vector<std::uint8_t>{2, 17, 5}));

    themes.push_back({"Many", {1, 2, 3, 4, 5, 6, 7, 8, 9}});
    std::vector<ChemicalTheme> read;
    assert(parse_themes(serialize_themes(themes), read));
    assert(read.size() == 2 && read[1].chemicals.size() == 9 && read[1].name == "Many");
    assert(parse_themes({}, read) && read.empty());

    assert(chemical_levels_query({2, 17}) ==
           "inst,dde: putv chem 2,dde: putv chem 17,endm");
    std::vector<int> values;
    assert(parse_values("36|0|8", 3, values) && values[2] == 8);
    assert(!parse_values("36|0", 3, values));
    assert(injection_script(100, 12) == "inst,chem 100 12,endm");
}

std::string gene_bytes(std::uint8_t family, std::uint8_t subtype,
                       std::uint8_t flags, const std::string& payload) {
    std::string gene = "gene";
    gene.push_back(static_cast<char>(family));
    gene.push_back(static_cast<char>(subtype));
    gene.push_back(1);  // id
    gene.push_back(2);  // generation
    gene.push_back(0);  // switch-on stage
    gene.push_back(static_cast<char>(flags));
    return gene + payload;
}

void test_genome() {
    // A genus gene (Grendel, mother "mum6", father "dad6"), a lobe gene, a
    // receptor and a male-only pigment gene.
    std::string genus(1, '\x01');
    genus += "mum6dad6";
    std::string lobe(kLobeGenePayloadBytes, '\0');
    lobe[0] = 3; lobe[1] = 4; lobe[2] = 4; lobe[3] = 4;
    lobe[kLobeRuleOffset] = 8;      // rule 0 source lobe
    lobe[kLobeRuleOffset + 1] = 2;  // min
    lobe[kLobeRuleOffset + 2] = 5;  // max
    lobe[kLobeRuleOffset + kLobeRuleBytes + 1] = 3;  // rule 1 min
    lobe[kLobeRuleOffset + kLobeRuleBytes + 2] = 1;  // max below min -> 3
    const std::string file = gene_bytes(2, 1, 0, genus) + gene_bytes(0, 0, kGeneMutable, lobe) +
                             gene_bytes(1, 0, 0, std::string(8, 'r')) +
                             gene_bytes(2, 6, kGeneMaleOnly, std::string(2, 'p')) + "gend";
    std::vector<Gene> genes;
    assert(parse_genome(bytes_of(file), genes));
    assert(genes.size() == 4);
    assert(genes[0].family == 2 && genes[0].subtype == 1 && genes[0].payload.size() == 9);
    assert(genes[3].payload.size() == 2 && genes[3].flags == kGeneMaleOnly);

    const GenomeSummary summary = summarize_genome(genes, file.size());
    assert(summary.total_genes == 4 && summary.total_bytes == static_cast<int>(file.size()));
    assert(summary.has_genus && summary.genus == 2);
    assert(std::string(species_name(summary.genus)) == "Grendel");
    assert(summary.mother == "366d756d" && summary.father == "36646164");
    assert(summary.male_only_genes == 1 && summary.mutable_genes == 1);
    assert(summary.highest_generation == 2);
    // Sorted by family then subtype: brain, biochemistry, creature 1, 6.
    assert(summary.types.size() == 4 && summary.types[0].family == 0 &&
           summary.types[1].family == 1 && summary.types[3].subtype == 6);
    assert(summary.types[0].bytes == static_cast<int>(kGeneHeaderBytes + kLobeGenePayloadBytes));
    assert(summary.lobes.size() == 1 && summary.lobes[0].x == 3 &&
           summary.lobes[0].dendrites_max[0] == 5 && summary.lobes[0].dendrites_max[1] == 3 &&
           summary.lobes[0].source_lobe[0] == 8);
    long fewest = 0, most = 0;
    dendrite_range(summary.lobes, {16}, fewest, most);
    assert(fewest == 16 * 5 && most == 16 * 8);

    assert(!parse_genome(bytes_of("junk"), genes));
    assert(!parse_genome(bytes_of(gene_bytes(2, 1, 0, genus)), genes));  // no gend
    assert(std::string(gene_type_name(1, 2)) == "Chemical reaction");
    assert(gene_type_name(2, 9) == nullptr);

    // Monikers: "4b5a4633" is kept in "3FZK.gen".
    assert(genome_file_name("4b5a4633") == "3FZK.gen");
    assert(moniker_characters("56424d31") == "1MBV");
}

void test_brain_map() {
    // Two lobes; the terminator took the last byte.
    std::string reply;
    reply.push_back(2);
    for (const int b : {0, 0, 8, 2, 1, 10, 0, 4, 4, 0}) reply.push_back(static_cast<char>(b));
    std::vector<LobeLayout> lobes;
    assert(parse_lobe_reply(reply, lobes));
    assert(lobes.size() == 2 && lobes[0].neurons() == 16 && lobes[1].x == 10 &&
           lobes[0].flags == 1);
    assert(!parse_lobe_reply(std::string(1, '\x02'), lobes));
    parse_lobe_reply(reply, lobes);

    int lobe = -1, neuron = -1;
    assert(neuron_at(lobes, 3, 1, lobe, neuron) && lobe == 0 && neuron == 11);
    assert(neuron_at(lobes, 13, 3, lobe, neuron) && lobe == 1 && neuron == 15);
    assert(!neuron_at(lobes, 30, 30, lobe, neuron));

    BrainActivity activity;
    std::string report;
    report += static_cast<char>('0' + 3);
    report += static_cast<char>('0' + 1);
    report += static_cast<char>('0' + 9);
    report += static_cast<char>('0' + 63);
    report += static_cast<char>('0' + 0);
    report += static_cast<char>('0' + 15);
    parse_activity_report(report, activity);
    assert(activity.level[3][1] == 9 && activity.level[63][0] == 15 &&
           activity.level[0][0] == 0);
    // Level 0 is still listed: a neuron at 1..15.
    report += static_cast<char>('0' + 5);
    report += static_cast<char>('0' + 6);
    report += static_cast<char>('0' + 0);
    parse_activity_report(report, activity);
    assert(activity.reported[5][6] && activity.level[5][6] == 0 && !activity.reported[0][0]);
    assert(estimated_value(activity, 5, 6) == 8 && estimated_value(activity, 0, 0) == 0 &&
           estimated_value(activity, 63, 0) == 248 && estimated_value(activity, 3, 1) == 152);

    assert(report_probe_script() == "inst,setv var0 3,setv var1 7,endm");
    assert(lobe_cells_query(8, 10, 2, 1) == "inst,dde: cell 8 10 1,dde: cell 8 11 1,endm");
    std::vector<NeuronValues> batch;
    assert(parse_cell_batch("200|7|4|40|80|120|12|0|0|0|0|0|0|0|", 2, batch));
    assert(batch.size() == 2 && batch[0].firing_strength == 200 && batch[0].activation == 7 &&
           batch[1].dendrites == 0);
    assert(!parse_cell_batch("1|2|3|", 1, batch));
    assert(exact_report_value(batch[0], kReportFiringStrength) == 200);
    assert(exact_report_value(batch[0], kReportActivation) == 7);
    assert(exact_report_value(batch[0], kReportAverageTargetWeight) == 20);
    assert(exact_report_value(batch[0], kReportAverageDendriteState) == 3);
    assert(exact_report_value(batch[0], kReportStrongestWeight) == -1);
    assert(exact_report_value(batch[1], kReportAverageTargetWeight) == 0);

    NeuronValues values[2];
    assert(parse_neuron_values("5|120|3|30|40|50|60|0|120|0|0|0|0|0", values));
    assert(values[0].activation == 120 && values[0].dendrites == 3 &&
           values[0].dendrite_state_sum == 60 && values[1].dendrites == 0);
    assert(!parse_neuron_values("5|120|3", values));
    assert(neuron_query(6, 4) == "inst,dde: cell 6 4 0,dde: cell 6 4 1,endm");

    NeuronNames names;
    names.chemicals = {"<NONE>", "Pain", "Need for Pleasure", "Hunger"};
    names.decisions = {"Quiescent", "Activate 1"};
    assert(neuron_meaning(kLobeDrive, 2, names) == "Hunger");
    assert(neuron_meaning(kLobeDecision, 1, names) == "Activate 1");
    assert(neuron_meaning(kLobeDecision, 12, names).empty());
    assert(neuron_meaning(kLobeVerb, 1, names) == "push");
    assert(neuron_meaning(kLobeAttention, 6, names) == "food");
    assert(neuron_meaning(kLobeNoun, 37, names) == "Norn");
    assert(neuron_meaning(kLobeConcept, 3, names).empty());
    assert(std::string(lobe_name(kLobeDecision)) == "Decision");
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

void test_real_files(const std::string& directory, const std::string& genome_path) {
    std::vector<std::string> names;
    assert(parse_chemical_names(read_file(directory + "/allchemicals.str"), names));
    assert(names.size() == 256 && names[1] == "Pain" && names[100] == "Energy");
    std::vector<ChemicalTheme> themes;
    assert(parse_original_themes(read_file(directory + "/themes.str"), themes));
    assert(!themes.empty() && themes[0].name == "Custom");
    std::vector<std::string> decisions;
    assert(parse_counted_strings(read_file(directory + "/decision.str"), decisions));
    assert(decisions.size() == 12 && decisions[0] == "Quiescent" && decisions[11] == "Go Right");
    std::vector<std::string> medicines;
    assert(parse_uncounted_strings(read_file(directory + "/injections.str"), medicines));
    assert(medicines.size() == 7 && medicines[0] == "Energy" && medicines[6] == "Anti-oxidant");

    const std::vector<std::uint8_t> genome = read_file(genome_path);
    std::vector<Gene> genes;
    assert(parse_genome(genome, genes));
    const GenomeSummary summary = summarize_genome(genes, genome.size());
    std::printf("genome: %d genes, %d bytes, %s, mother %s father %s, %zu lobes\n",
                summary.total_genes, summary.total_bytes, species_name(summary.genus),
                summary.mother.c_str(), summary.father.c_str(), summary.lobes.size());
    for (const GeneTypeCount& type : summary.types) {
        std::printf("  %-22s %3d genes %5d bytes\n", gene_type_name(type.family, type.subtype),
                    type.genes, type.bytes);
    }
    const std::vector<LobeWiring> wiring = brain_wiring(genes, true);
    for (std::size_t i = 0; i < wiring.size(); ++i) {
        std::printf("  wiring %zu at %2d,%2d: rule 0 from %d (%d-%d, spread %d, mode %d), "
                    "rule 1 from %d (%d-%d, spread %d, mode %d), copy %d\n",
                    i, wiring[i].x, wiring[i].y, wiring[i].rules[0].source,
                    wiring[i].rules[0].fewest, wiring[i].rules[0].most, wiring[i].rules[0].spread,
                    wiring[i].rules[0].mode, wiring[i].rules[1].source, wiring[i].rules[1].fewest,
                    wiring[i].rules[1].most, wiring[i].rules[1].spread, wiring[i].rules[1].mode,
                    wiring[i].perception_copy);
    }
    for (const LobeGene& lobe : summary.lobes) {
        std::printf("  lobe %2d,%2d %2dx%2d dendrites %d-%d + %d-%d\n", lobe.x, lobe.y,
                    lobe.width, lobe.height, lobe.dendrites_min[0], lobe.dendrites_max[0],
                    lobe.dendrites_min[1], lobe.dendrites_max[1]);
    }
}

} // namespace

void test_brain_wiring() {
    // A lobe gene: x, y, width, height, perception copy, then rules at 18.
    const auto lobe_gene = [](int x, int y, int width, int copy, int source0, int most0,
                              int generation, int flags, int stage) {
        Gene gene;
        gene.family = 3;  // % 3 == 0: brain
        gene.subtype = 7;  // any subtype
        gene.generation = static_cast<std::uint8_t>(generation);
        gene.flags = static_cast<std::uint8_t>(flags);
        gene.switch_on_stage = static_cast<std::uint8_t>(stage);
        gene.payload.assign(112, 0);
        gene.payload[0] = static_cast<std::uint8_t>(x);
        gene.payload[1] = static_cast<std::uint8_t>(y);
        gene.payload[2] = static_cast<std::uint8_t>(width);
        gene.payload[3] = 2;
        gene.payload[4] = static_cast<std::uint8_t>(copy);
        gene.payload[18] = static_cast<std::uint8_t>(source0);
        gene.payload[19] = 1;
        gene.payload[20] = static_cast<std::uint8_t>(most0);
        gene.payload[22] = 12;  // spread 12 folds to 3
        gene.payload[27] = 4;   // connection mode 4 folds to 1
        gene.payload[18 + 47] = 9;  // rule 1's source, folded into the count
        return gene;
    };
    std::vector<Gene> genes;
    genes.push_back(lobe_gene(10, 5, 8, 0, 1, 3, 1, 0, 0));       // second pass
    genes.push_back(lobe_gene(62, 1, 8, 4, 0, 0, 0, 0, 0));       // first; x clamped to 56
    genes.push_back(lobe_gene(20, 20, 4, 0, 0, 2, 0, 0x10, 0));   // female only
    genes.push_back(lobe_gene(30, 30, 4, 0, 0, 2, 0, 0, 1));      // not stage 0
    const std::vector<LobeWiring> male = brain_wiring(genes, true);
    assert(male.size() == 2);
    assert(male[0].x == 56 && male[0].y == 1 && male[0].perception_copy == 1);
    assert(male[1].x == 10 && male[1].rules[0].source == 1 && male[1].rules[0].fewest == 1 &&
           male[1].rules[0].most == 3 && male[1].rules[0].spread == 3 &&
           male[1].rules[0].mode == 1 && male[1].rules[1].mode == 0);
    assert(male[0].rules[1].source == 9 % 2);
    const std::vector<LobeWiring> female = brain_wiring(genes, false);
    assert(female.size() == 3 && female[1].x == 20);

    // Concept neuron 331 of 640 reading Perception (7 x 16): spot 331 * 112
    // / 640 = 57 = (1, 8); with 3 dendrites and spread 2, x 0..3, y 6..10.
    DendriteReach reach;
    assert(dendrite_reach(331, 640, 7, 16, 2, 3, reach));
    assert(reach.spot_x == 1 && reach.spot_y == 8 && reach.left == 0 && reach.right == 3 &&
           reach.top == 6 && reach.bottom == 10);
    assert(dendrite_reach(331, 640, 7, 16, 2, 1, reach));  // one dendrite: the spot only
    assert(reach.left == 1 && reach.right == 1 && reach.top == 8 && reach.bottom == 8);
    assert(!dendrite_reach(331, 640, 7, 16, 2, 0, reach));
    assert(dendrite_reach(639, 640, 7, 16, 8, 2, reach));  // last neuron: last cell, clamped
    assert(reach.spot_x == 6 && reach.spot_y == 15 && reach.right == 6 && reach.bottom == 15);

    struct Layout { int x, y; };
    assert(wiring_matches(male, std::vector<Layout>{{56, 1}, {10, 5}}));
    assert(!wiring_matches(male, std::vector<Layout>{{10, 5}, {56, 1}}));
    assert(!wiring_matches(male, std::vector<Layout>{{56, 1}}));
}

int main(int argc, char** argv) {
    test_strings_and_themes();
    test_genome();
    test_brain_map();
    test_brain_wiring();
    if (argc > 2) {
        test_real_files(argv[1], argv[2]);
    }
    std::puts("science_test: all passed");
    return 0;
}
