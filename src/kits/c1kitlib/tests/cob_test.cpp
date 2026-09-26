// Portable tests for COB files (c1kit/cob.hpp).
//   g++ -std=c++17 -I../include cob_test.cpp
// With a directory argument, also reads every .cob and .rcb there:
//   ./a.out <install dir>

#include "c1kit/cob.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <iterator>

namespace {

using namespace c1kit;

std::vector<std::uint8_t> sample_cob() {
    ArchiveWriter w;
    w.u16(1);
    w.u16(20);
    w.u32(12); w.u32(31); w.u32(98);  // expires 31 Dec 1998
    w.u16(2); w.u16(1); w.u16(0); w.u16(0);
    w.cstring("scrp 2 10 12 2,stim writ from 10 255 0 0 34 60 43 60 17 60 12 20,endm");
    w.cstring("scrp 2 10 12 9,chem 57 20,fire 3 4,endm");
    w.cstring("inst,new: simp bugs 34 0 400 0,setv clas 34212864,kill norn,endm");
    w.u32(2); w.u32(1); w.u16(4);
    w.bytes({1, 2, 0, 0});
    w.cstring("Beetle");
    w.cstring("A bug");
    return w.bytes();
}

void test_sample() {
    std::vector<Cob> cobs;
    assert(parse_cob_file(sample_cob(), cobs) && cobs.size() == 1);
    const Cob& cob = cobs[0];
    assert(cob.quantity == 20 && !cob.unlimited() && cob.has_expiry());
    assert(cob.install_scripts.size() == 2 && cob.inject_scripts.size() == 1);
    assert(cob.sprite.width == 2 && cob.sprite.stride == 4 && cob.sprite.pixels[1] == 2);
    assert(cob.name == "Beetle" && cob.description == "A bug");
    assert(cob_expired(cob, 2026, 1, 1) && !cob_expired(cob, 1998, 12, 31) &&
           cob_expired(cob, 1999, 1, 1));

    InstalledScript s;
    assert(installed_script(cob.install_scripts[1], s) && s.classifier.genus == 10 && s.event == 9);
    const std::vector<Classifier> made = created_classifiers(cob.inject_scripts);
    assert(made.size() == 1 && made[0].family == 2 && made[0].genus == 10 && made[0].species == 12);
    const std::vector<int> chemicals = affected_chemicals(cob);
    assert((chemicals == std::vector<int>{34, 43, 17, 12, 57}));
    assert(needs_creature(cob));
    const std::vector<std::string> warnings = cob_warnings(cob, true);
    assert(warnings.size() == 3 && warnings[0].find("kill norn") != std::string::npos &&
           warnings[2] == "This agent is past its use-by date (31/12/98)");
    assert(generated_removal(cob) ==
           "inst,enum 2 10 12,kill targ,next,scrx 2 10 12 2,scrx 2 10 12 9,endm");
    assert(event_name(17) == "Activate 1 (Extraspective)" && event_name(12) == "Event 12" &&
           event_name(72) == "Die");
    assert(caos_tokens("dde: puts [hello, world],endm").size() == 4);

    const std::vector<ClassifierName> names = parse_classifier_names(
        "2|||Simple Objects|\n2|10||Bugs|\n2|10|12|Albian Carrot Beetle|\n");
    assert(classifier_name(names, {2, 10, 12}) == "Albian Carrot Beetle");
    assert(classifier_name(names, {2, 10, 13}) == "Bugs");
    assert(classifier_name(names, {2, 11, 1}) == "Simple Objects");
    assert(classifier_name(names, {9, 1, 1}).empty());
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

void test_real(const std::string& dir) {
    DIR* d = opendir(dir.c_str());
    assert(d != nullptr);
    int read = 0;
    while (dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name.size() < 4) continue;
        std::string ext = name.substr(name.size() - 4);
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".cob" && ext != ".rcb") continue;
        std::vector<Cob> cobs;
        const bool ok = parse_cob_file(read_file(dir + "/" + name), cobs);
        std::printf("  %-34s %s", name.c_str(), ok ? "" : "UNREADABLE\n");
        if (ok) {
            std::printf("%-26s qty %3d  %zu+%zu scripts  %dx%d\n", cobs[0].name.c_str(),
                        cobs[0].quantity, cobs[0].install_scripts.size(),
                        cobs[0].inject_scripts.size(), cobs[0].sprite.width, cobs[0].sprite.height);
            ++read;
        }
    }
    closedir(d);
    // The generated removal for the beetle is what its .rcb says.
    std::vector<Cob> beetle, rcb;
    assert(parse_cob_file(read_file(dir + "/Albian Carrot Beetle.COB"), beetle));
    assert(parse_cob_file(read_file(dir + "/Albian Carrot Beetle.rcb"), rcb));
    std::printf("generated: %s\nrcb:       %s\n", generated_removal(beetle[0]).c_str(),
                rcb[0].inject_scripts[0].c_str());
    // The same commands (the author put two scrx the other way round).
    const auto commands = [](const std::string& script) {
        std::vector<std::string> parts;
        std::size_t from = 0;
        while (from <= script.size()) {
            const std::size_t comma = script.find(',', from);
            parts.push_back(script.substr(from, comma == std::string::npos ? std::string::npos : comma - from));
            if (comma == std::string::npos) break;
            from = comma + 1;
        }
        std::sort(parts.begin(), parts.end());
        return parts;
    };
    assert(commands(generated_removal(beetle[0])) == commands(rcb[0].inject_scripts[0]));
    assert(read > 10);
}

} // namespace

int main(int argc, char** argv) {
    test_sample();
    if (argc > 1) test_real(argv[1]);
    std::puts("cob_test: all passed");
    return 0;
}
