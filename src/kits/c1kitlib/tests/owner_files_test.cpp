// Portable tests for the Owner's Kit file formats (c1kit/owner_files.hpp).
//   g++ -std=c++17 -I../include owner_files_test.cpp

#include "c1kit/owner_files.hpp"

#include <cassert>
#include <cstdio>

namespace {

using namespace c1kit;

// "The Register" from a real world: two creatures registered by the 1996
// Owner's Kit.
const std::vector<std::uint8_t> kRealRegister = {0x02, 0x00, 0x08, 0x34, 0x62, 0x35, 0x61, 0x34, 0x36, 0x33, 0x33, 0x04, 0x4d, 0x61, 0x6c, 0x65, 0x08, 0x33, 0x36, 0x36, 0x34, 0x36, 0x31, 0x36, 0x34, 0x08, 0x33, 0x36, 0x36, 0x64, 0x37, 0x35, 0x36, 0x64, 0x11, 0x32, 0x31, 0x3a, 0x31, 0x36, 0x20, 0x53, 0x65, 0x70, 0x20, 0x31, 0x35, 0x20, 0x32, 0x30, 0x32, 0x36, 0x0e, 0x54, 0x68, 0x65, 0x20, 0x62, 0x69, 0x72, 0x74, 0x68, 0x70, 0x6c, 0x61, 0x63, 0x65, 0x00, 0x00, 0x00, 0x00, 0x08, 0x35, 0x36, 0x34, 0x32, 0x34, 0x64, 0x33, 0x31, 0x06, 0x46, 0x65, 0x6d, 0x61, 0x6c, 0x65, 0x08, 0x33, 0x35, 0x36, 0x34, 0x36, 0x31, 0x36, 0x34, 0x08, 0x33, 0x35, 0x36, 0x64, 0x37, 0x35, 0x36, 0x64, 0x11, 0x32, 0x31, 0x3a, 0x31, 0x36, 0x20, 0x53, 0x65, 0x70, 0x20, 0x31, 0x35, 0x20, 0x32, 0x30, 0x32, 0x36, 0x0e, 0x54, 0x68, 0x65, 0x20, 0x62, 0x69, 0x72, 0x74, 0x68, 0x70, 0x6c, 0x61, 0x63, 0x65, 0x00, 0x00, 0x00, 0x00};

void test_real_register() {
    std::vector<OwnerRecord> records;
    assert(parse_register(kRealRegister, records));
    assert(records.size() == 2);
    assert(records[0][kOwnerMoniker] == "4b5a4633");
    assert(records[0][kOwnerCreatureName] == "Male");
    assert(records[0][kOwnerFatherMoniker] == "36646164");
    assert(records[0][kOwnerMotherMoniker] == "366d756d");
    assert(records[0][kOwnerBirthTime] == "21:16 Sep 15 2026");
    assert(records[0][kOwnerBirthplace] == "The birthplace");
    assert(records[0][kOwnerName].empty() && records[0][kOwnerEmail].empty());
    assert(records[1][kOwnerCreatureName] == "Female");
    // Writing it back gives the same bytes.
    assert(serialize_register(records) == kRealRegister);
    assert(find_record(records, "56424d31") == &records[1]);
    assert(find_record(records, "nope") == nullptr);
    // An empty file is an empty register; a truncated one is an error.
    assert(parse_register({}, records) && records.empty());
    std::vector<std::uint8_t> cut(kRealRegister.begin(), kRealRegister.begin() + 40);
    assert(!parse_register(cut, records));
}

void test_owner_data() {
    // A reply captured from the game.
    const std::string reply =
        "4b5a4633|Male|36646164|366d756d|21:16 Sep 15 2026|The birthplace|||||";
    OwnerRecord record;
    assert(parse_owner_data(reply, record));
    assert(record[kOwnerBirthplace] == "The birthplace");
    assert(format_owner_data(record) == reply);
    record[kOwnerName] = "Ann [the] | owner";
    assert(format_owner_data(record).find("Ann the  owner|") != std::string::npos);
    assert(!parse_owner_data("", record));
    assert(!parse_owner_data("a|b|", record));
}

void test_long_strings() {
    OwnerRecord record;
    record[kOwnerAddress] = std::string(300, 'x');
    const std::vector<std::uint8_t> bytes = serialize_register({record});
    std::vector<OwnerRecord> back;
    assert(parse_register(bytes, back) && back.size() == 1);
    assert(back[0][kOwnerAddress] == record[kOwnerAddress]);
}

void test_album() {
    Photo photo;
    photo.taken = "22:01 25 August 2026";
    photo.bitmap.width = 3;
    photo.bitmap.height = 2;
    photo.bitmap.stride = 4;
    photo.bitmap.pixels = {1, 2, 3, 0, 4, 5, 6, 0};  // bottom row first
    photo.caption = "First steps";
    const std::vector<std::uint8_t> bytes = serialize_album({photo, photo});
    std::vector<Photo> back;
    assert(parse_album(bytes, back) && back.size() == 2);
    assert(back[1].taken == photo.taken && back[1].caption == "First steps");
    assert(back[0].bitmap.at(0, 0) == 4);  // top-left is the last stored row
    assert(back[0].bitmap.at(2, 1) == 3);
    assert(serialize_album(back) == bytes);
    // An album with no photos, as the 1996 kit leaves for new creatures.
    assert(parse_album({0, 0}, back) && back.empty());
    // The shape of a real album: one photo, 120 x 140.
    ArchiveWriter real;
    real.u16(1);
    real.cstring("22:01 25 August 2026");
    real.u32(0x78); real.u32(0x8c); real.u16(0x78);
    std::vector<std::uint8_t> bytes2 = real.bytes();
    bytes2.resize(bytes2.size() + 0x78 * 0x8c, 7);
    bytes2.push_back(0); bytes2.push_back(0);
    assert(parse_album(bytes2, back) && back.size() == 1 &&
           back[0].bitmap.width == 120 && back[0].bitmap.height == 140);
}

void test_photo_query() {
    // The bytes the 1996 kit sends.
    const std::string expected =
        std::string("inst,dde: panc,dde: pict x|") + static_cast<char>(0x8c) + ",endm";
    assert(photo_query() == expected);
}

} // namespace

int main() {
    test_real_register();
    test_owner_data();
    test_long_strings();
    test_album();
    test_photo_query();
    std::puts("owner file tests passed");
    return 0;
}
