// Portable tests for the Health Kit's shop file (c1kit/health_files.hpp).
//   g++ -std=c++17 -I../include health_files_test.cpp
// With a path argument, also reads a real "Health" file and checks it
// writes back byte for byte:  ./a.out <install dir>/Health

#include "c1kit/health_files.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace {

using namespace c1kit;

void test_round_trip() {
    ShopItem item;
    item.quantity = 12;
    item.command = "inst,new: simp herb 3 0 3500 0,edit";
    item.picture.width = 3;
    item.picture.height = 2;
    item.picture.stride = 4;
    item.picture.pixels = {1, 2, 3, 0, 4, 5, 6, 0};
    item.name = "Feverfew";
    item.description = "Reduces sleepiness and exhaustion";
    const std::vector<std::uint8_t> bytes = serialize_shop({item, item});
    std::vector<ShopItem> read;
    assert(parse_shop(bytes, read));
    assert(read.size() == 2 && read[1].quantity == 12 && read[1].name == "Feverfew" &&
           read[1].picture.width == 3 && read[1].picture.pixels[4] == 4 &&
           read[1].description == item.description && read[0].command == item.command);
    assert(serialize_shop(read) == bytes);
    std::vector<std::uint8_t> cut(bytes.begin(), bytes.begin() + 20);
    assert(!parse_shop(cut, read));
}

void test_real_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    std::vector<ShopItem> items;
    assert(parse_shop(bytes, items));
    for (const ShopItem& item : items) {
        std::printf("  %-14s x%-3d %dx%d  %s\n", item.name.c_str(), item.quantity,
                    item.picture.width, item.picture.height, item.description.c_str());
    }
    assert(serialize_shop(items) == bytes);
}

} // namespace

int main(int argc, char** argv) {
    test_round_trip();
    if (argc > 1) {
        test_real_file(argv[1]);
    }
    std::puts("health_files_test: all passed");
    return 0;
}
