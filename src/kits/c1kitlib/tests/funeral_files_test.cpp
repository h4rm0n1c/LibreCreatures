// Portable tests for the Funeral Kit's graves (c1kit/funeral_files.hpp).
//   g++ -std=c++17 -I../include funeral_files_test.cpp

#include "c1kit/funeral_files.hpp"

#include <cassert>
#include <cstdio>

namespace {

using namespace c1kit;

void test_moniker_from_id() {
    // "4b5a4633" is a moniker from a real register.
    assert(moniker_from_id(0x4b5a4633u) == "4b5a4633");
    assert(moniker_from_id(0x56424d31u) == "56424d31");
}

void test_grave_from_register() {
    OwnerRecord record;
    for (std::size_t i = 0; i < kOwnerFieldCount; ++i) {
        record.fields[i] = "f" + std::to_string(i);
    }
    const Grave grave = grave_from_register(record, "00:56 Sep 26 2026");
    for (std::size_t i = 0; i <= kOwnerBirthplace; ++i) {
        assert(grave.fields[i] == record.fields[i]);
    }
    // The owner's details are not copied.
    assert(grave[kGraveDeathTime] == "00:56 Sep 26 2026");
    assert(grave[kGraveEpitaph].empty() && grave[kGraveReserved].empty());
    assert(!grave.has_headstone());
}

void test_round_trip() {
    std::vector<Grave> graves(2);
    graves[0][kGraveMoniker] = "4b5a4633";
    graves[0][kGraveCreatureName] = "Male";
    graves[0][kGraveEpitaph] = std::string(300, 'x');  // a long CString
    graves[0][kGraveHeadstone] = "1";
    graves[1][kGraveMoniker] = "56424d31";
    const std::vector<std::uint8_t> bytes = serialize_graves(graves);
    assert(bytes[0] == 2 && bytes[1] == 0);
    std::vector<Grave> read;
    assert(parse_graves(bytes, read));
    assert(read.size() == 2);
    for (std::size_t g = 0; g < 2; ++g) {
        for (std::size_t i = 0; i < kGraveFieldCount; ++i) {
            assert(read[g].fields[i] == graves[g].fields[i]);
        }
    }
    assert(read[0].has_headstone() && !read[1].has_headstone());
    assert(find_grave(read, "56424d31") == &read[1]);
    assert(find_grave(read, "nope") == nullptr);
    assert(parse_graves({}, read) && read.empty());
    std::vector<std::uint8_t> cut(bytes.begin(), bytes.begin() + 20);
    assert(!parse_graves(cut, read));
}

void test_life_span() {
    // As the 1996 kit showed it: the birth year dropped.
    assert(format_life_span("21:16 Sep 15 2026", "00:56 Sep 26 2026") ==
           "21:16 Sep 15 to 00:56 Sep 26 2026");
    // Born in another year: kept.
    assert(format_life_span("21:16 Dec 31 2025", "00:56 Jan 02 2026") ==
           "21:16 Dec 31 2025 to 00:56 Jan 02 2026");
    assert(format_life_span("", "00:56 Sep 26 2026") == "00:56 Sep 26 2026");
    assert(format_life_span("21:16 Sep 15 2026", "") == "21:16 Sep 15 2026");
}

} // namespace

int main() {
    test_moniker_from_id();
    test_grave_from_register();
    test_round_trip();
    test_life_span();
    std::puts("funeral_files_test: all passed");
    return 0;
}
