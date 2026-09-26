// Portable tests for the Biochemistry Kit's files (c1kit/biochem_files.hpp).
//   g++ -std=c++17 -I../include biochem_files_test.cpp

#include "c1kit/biochem_files.hpp"
#include "c1kit/science_files.hpp"

#include <cassert>
#include <cstdio>

using namespace c1kit;

int main() {
    const std::string text = "# notes\r\nDrives = 1, 3,6\r\nhormones=63,64\r\nbroken\r\n";
    const std::vector<SavedChemicalSet> sets = parse_saved_sets(text);
    assert(sets.size() == 2 && sets[0].name == "Drives" &&
           (sets[0].chemicals == std::vector<int>{1, 3, 6}) && sets[1].chemicals.size() == 2);

    // Replacing keeps the other lines and the place; any case matches.
    const std::string replaced = update_saved_sets(text, {"DRIVES", {2, 4}});
    assert(replaced == "# notes\r\nDRIVES=2,4\r\nhormones=63,64\r\nbroken\r\n");
    const std::string added = update_saved_sets(text, {"new", {100}});
    assert(added.find("new=100\r\n") == added.size() - 9);
    const std::string removed = update_saved_sets(text, {"hormones", {}}, true);
    assert(removed == "# notes\r\nDrives = 1, 3,6\r\nbroken\r\n");
    assert(update_saved_sets("", {"a", {1}}) == "a=1\r\n");

    std::vector<std::string> names(256);
    names[1] = "Pain";
    std::vector<std::string> read;
    assert(parse_chemical_names(serialize_chemical_names(names), read) && read[1] == "Pain" &&
           read.size() == 256);
    std::puts("biochem_files_test: all passed");
    return 0;
}
