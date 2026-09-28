// The .egg file check (creatures::Egg::is_well_formed_file) and its CRC.
// Build: c++ -std=c++20 -I src/c1 tests/c1_egg_format_test.cpp
//            src/c1/creatures/egg.cpp src/c1/creatures/genome.cpp

#include "creatures/egg.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace c1 = creatures1;
using c1::creatures::Egg;

namespace {

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void put(std::vector<std::uint8_t>& out, const std::string& text) {
    out.insert(out.end(), text.begin(), text.end());
}

// A version-2 egg, laid out as the game writes one.
std::vector<std::uint8_t> make_egg(const std::vector<std::uint8_t>& genes,
                                   const std::string& moniker = "0JRM",
                                   std::uint32_t sex = 1) {
    std::vector<std::uint8_t> out = {0xff, 0xff, 0x01, 0x00, 0x04, 0x00};
    put(out, "CEgg");
    put_u32(out, 2);
    put_u32(out, 0x02050200u);
    put_u32(out, sex);
    put_u32(out, static_cast<std::uint32_t>(genes.size()));
    put_u32(out, Egg::crc32(genes.data(), genes.size()));
    out.insert(out.end(), {0xff, 0xff, 0x01, 0x00, 0x07, 0x00});
    put(out, "CGenome");
    put_u32(out, static_cast<std::uint32_t>(genes.size()));
    put(out, moniker);
    put_u32(out, sex);
    out.push_back(0);
    out.insert(out.end(), genes.begin(), genes.end());
    return out;
}

bool ok(const std::vector<std::uint8_t>& bytes) {
    return Egg::is_well_formed_file(bytes.data(), bytes.size());
}

} // namespace

int main() {
    // The CRC is the standard one: the IEEE check value.
    const char* check = "123456789";
    assert(Egg::crc32(reinterpret_cast<const std::uint8_t*>(check), 9) ==
           0xcbf43926u);

    std::vector<std::uint8_t> genes;
    put(genes, "gene");
    genes.insert(genes.end(), 64, 0x5a);
    put(genes, "gend");

    const std::vector<std::uint8_t> good = make_egg(genes);
    assert(good.size() == Egg::kHeaderBytes + genes.size());
    assert(ok(good));
    assert(ok(make_egg(genes, "abc9", 2)));

    // Truncated anywhere, or with anything appended.
    for (std::size_t size = 0; size < good.size(); ++size) {
        assert(!Egg::is_well_formed_file(good.data(), size));
    }
    std::vector<std::uint8_t> longer = good;
    longer.push_back(0);
    assert(!ok(longer));
    assert(!Egg::is_well_formed_file(nullptr, 0));

    // Any one byte of the genes changed: the CRC catches it.
    for (std::size_t index = Egg::kHeaderBytes + 4; index < good.size();
         ++index) {
        std::vector<std::uint8_t> bad = good;
        bad[index] ^= 0x01;
        assert(!ok(bad));
    }

    // Header fields that must be exact.
    const auto with = [&](std::size_t offset, std::uint32_t value) {
        std::vector<std::uint8_t> bad = good;
        for (int shift = 0; shift < 32; shift += 8) {
            bad[offset + shift / 8] = static_cast<std::uint8_t>(value >> shift);
        }
        return bad;
    };
    assert(!ok(with(10, 1)));            // version 1
    assert(!ok(with(14, 0x02060200u)));  // not an egg
    assert(!ok(with(18, 3)));            // no such sex
    assert(!ok(with(22, 0xffffffffu)));  // a huge genome it does not have
    assert(!ok(with(43, 0xffffffffu)));  // CGenome disagrees
    assert(!ok(with(26, 0)));            // wrong CRC

    // Monikers that are not safe filenames.
    assert(!ok(make_egg(genes, "..\\a")));
    assert(!ok(make_egg(genes, "a/b.")));
    assert(!ok(make_egg(genes, std::string("ab\0c", 4))));

    // Genes that do not start "gene".
    std::vector<std::uint8_t> not_genes(genes.size(), 0);
    assert(!ok(make_egg(not_genes)));

    // Things people rename to .egg.
    std::vector<std::uint8_t> avi = {'R', 'I', 'F', 'F', 0, 0, 0, 0,
                                     'A', 'V', 'I', ' '};
    avi.resize(4096, 0x11);
    assert(!ok(avi));
    std::vector<std::uint8_t> mp4 = {0, 0, 0, 0x20, 'f', 't', 'y', 'p'};
    mp4.resize(4096, 0x22);
    assert(!ok(mp4));
    // A too-big file is refused on size alone.
    std::vector<std::uint8_t> huge(Egg::kHeaderBytes + Egg::kMaxGenomeBytes + 1);
    std::memcpy(huge.data(), good.data(), good.size());
    assert(!ok(huge));

    std::puts("c1_egg_format_test: ok");
    return 0;
}
