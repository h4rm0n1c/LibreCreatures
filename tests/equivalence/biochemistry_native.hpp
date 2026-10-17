#pragma once

// The biochemistry harness's random cases and its native and LC builders,
// shared by the biochemistry and dreaming equivalence tests.

#include "native_image.hpp"

#include "biochemistry/biochemistry.hpp"
#include "common/compat.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace bio = creatures1::biochemistry;

namespace biochemistry_case {

// CBiochemistry, as Ghidra lays it out (size 0x1294).
constexpr std::size_t kNativeSize = 4756;
constexpr std::size_t kChemicals = 8;
constexpr std::size_t kReceptors = 520;
constexpr std::size_t kEmitters = 2056;
constexpr std::size_t kReactions = 3592;
constexpr std::size_t kReceptorCount = 4744;
constexpr std::size_t kEmitterCount = 4748;
constexpr std::size_t kReactionCount = 4752;
constexpr std::uint32_t kNativeUpdate = 0x0042ee10;

using NativeUpdate = void(__attribute__((thiscall)) *)(void* self, std::uint32_t tick);

struct Random {
    std::uint64_t state;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next()); }
    std::uint32_t below(std::uint32_t n) { return next() % n; }
};

constexpr std::size_t kLocusPool = 24;

struct Emitter {
    std::uint8_t domain, tissue, locus, chemical, threshold, period, amount, flags;
    int pool;  // -1 none
};
struct Receptor {
    std::uint8_t domain, tissue, locus, chemical, threshold, nominal, gain, flags;
    int pool;
};
struct Reaction {
    std::uint8_t r1_amount, r1, r2_amount, r2, selector, p1_amount, p1, p2_amount, p2;
};

struct Case {
    std::array<std::uint8_t, 256> concentration{};
    std::array<std::uint8_t, 256> half_life{};
    std::vector<Emitter> emitters;
    std::vector<Receptor> receptors;
    std::vector<Reaction> reactions;
    std::array<std::uint8_t, kLocusPool> loci{};
    std::vector<std::uint32_t> ticks;
};

// A chemical: mostly from a small set, so reactions share chemicals and
// the same chemical appears in both reactant slots now and then.
std::uint8_t chemical(Random& random) {
    switch (random.below(4)) {
    case 0: return 0;
    case 1: return random.byte();
    default: return static_cast<std::uint8_t>(1 + random.below(12));
    }
}

// Reaction amounts are 1..16: the genome load makes them so, and LC's world
// load maps any other byte into that range.
std::uint8_t amount(Random& random) {
    return static_cast<std::uint8_t>(1 + random.below(16));
}

// Only the brain (0) and creature (1) domains resolve a locus.
std::uint8_t domain(Random& random) {
    return static_cast<std::uint8_t>(random.below(2));
}

// EQUIVALENCE_ONLY=decay|emitters|receptors|reactions narrows the cases to
// the decay pass plus one kind of record, to find where a difference comes
// from.
std::string g_only;

Case make_case(Random& random) {
    Case c;
    for (std::size_t i = 0; i < 256; ++i) {
        c.concentration[i] = random.below(3) == 0 ? 0 : random.byte();
        c.half_life[i] = random.byte();
    }
    for (std::uint8_t& locus : c.loci) {
        locus = random.byte();
    }
    const auto count = [&random]() {
        return random.below(10) == 0 ? random.below(129) : random.below(24);
    };
    const auto wanted = [](const char* kind) {
        return g_only.empty() || g_only == kind;
    };
    for (std::uint32_t i = wanted("emitters") ? count() : 0; i > 0; --i) {
        c.emitters.push_back({domain(random), random.byte(), random.byte(), chemical(random),
                              random.byte(),
                              static_cast<std::uint8_t>(1 + random.below(random.below(2) ? 4 : 255)),
                              random.byte(), static_cast<std::uint8_t>(random.below(8)),
                              random.below(8) == 0 ? -1 : static_cast<int>(random.below(kLocusPool))});
    }
    for (std::uint32_t i = wanted("receptors") ? count() : 0; i > 0; --i) {
        c.receptors.push_back({domain(random), random.byte(), random.byte(), chemical(random),
                               random.byte(), random.byte(), random.byte(),
                               static_cast<std::uint8_t>(random.below(4)),
                               random.below(8) == 0 ? -1 : static_cast<int>(random.below(kLocusPool))});
    }
    for (std::uint32_t i = wanted("reactions") ? count() : 0; i > 0; --i) {
        Reaction r{amount(random), chemical(random), amount(random), chemical(random),
                   random.byte(), amount(random), chemical(random), amount(random),
                   chemical(random)};
        if (random.below(6) == 0) {
            r.r2 = r.r1;  // the row 51 shape
        }
        c.reactions.push_back(r);
    }
    for (std::uint32_t i = 1 + random.below(8), tick = random.next(); i > 0; --i, ++tick) {
        c.ticks.push_back(random.below(3) == 0 ? random.next() : tick);
    }
    return c;
}

// ---- native ----

struct NativeState {
    std::vector<std::uint8_t> self = std::vector<std::uint8_t>(kNativeSize, 0);
    std::array<std::uint8_t, kLocusPool> loci{};
};

void put32(std::uint8_t* at, std::uint32_t value) { std::memcpy(at, &value, 4); }

void build_native(const Case& c, NativeState& n) {
    n.loci = c.loci;
    std::uint8_t* s = n.self.data();
    for (std::size_t i = 0; i < 256; ++i) {
        s[kChemicals + 2 * i] = c.concentration[i];
        s[kChemicals + 2 * i + 1] = c.half_life[i];
    }
    const auto locus = [&n](int pool) {
        return pool < 0 ? 0u
                        : static_cast<std::uint32_t>(
                              reinterpret_cast<std::uintptr_t>(&n.loci[static_cast<std::size_t>(pool)]));
    };
    for (std::size_t i = 0; i < c.emitters.size(); ++i) {
        const Emitter& e = c.emitters[i];
        std::uint8_t* r = s + kEmitters + 12 * i;
        const std::uint8_t fields[8] = {e.domain, e.tissue, e.locus, e.chemical,
                                        e.threshold, e.period, e.amount, e.flags};
        std::memcpy(r, fields, 8);
        put32(r + 8, locus(e.pool));
    }
    for (std::size_t i = 0; i < c.receptors.size(); ++i) {
        const Receptor& e = c.receptors[i];
        std::uint8_t* r = s + kReceptors + 12 * i;
        const std::uint8_t fields[8] = {e.domain, e.tissue, e.locus, e.chemical,
                                        e.threshold, e.nominal, e.gain, e.flags};
        std::memcpy(r, fields, 8);
        put32(r + 8, locus(e.pool));
    }
    for (std::size_t i = 0; i < c.reactions.size(); ++i) {
        const Reaction& e = c.reactions[i];
        const std::uint8_t fields[9] = {e.r1_amount, e.r1, e.r2_amount, e.r2, e.selector,
                                        e.p1_amount, e.p1, e.p2_amount, e.p2};
        std::memcpy(s + kReactions + 9 * i, fields, 9);
    }
    put32(s + kReceptorCount, static_cast<std::uint32_t>(c.receptors.size()));
    put32(s + kEmitterCount, static_cast<std::uint32_t>(c.emitters.size()));
    put32(s + kReactionCount, static_cast<std::uint32_t>(c.reactions.size()));
}

// ---- LibreCreatures, loaded through its world archive ----

class CaseArchive final : public bio::BiochemistryArchive {
public:
    explicit CaseArchive(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}
    bool is_loading() const override { return true; }
    creatures1::creatures::Creature* read_creature_reference() override { return nullptr; }
    void write_creature_reference(creatures1::creatures::Creature*) override {}
    std::uint32_t read_uint32() override {
        std::uint32_t value = 0;
        std::memcpy(&value, take(4), 4);
        return value;
    }
    void write_uint32(std::uint32_t) override {}
    std::uint8_t read_byte() override { return *take(1); }
    void write_byte(std::uint8_t) override {}
    bool consumed_all() const { return position_ == bytes_.size(); }

private:
    const std::uint8_t* take(std::size_t n) {
        if (position_ + n > bytes_.size()) {
            std::fprintf(stderr, "archive read past its end\n");
            std::exit(4);
        }
        const std::uint8_t* at = bytes_.data() + position_;
        position_ += n;
        return at;
    }
    std::vector<std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

// Hands out the case's loci in the order the archive resolves them:
// emitters, then receptors.
class CaseLoci final : public bio::BiochemistryLocusHost {
public:
    explicit CaseLoci(std::vector<std::uint8_t*> order) : order_(std::move(order)) {}
    std::uint8_t* resolve_brain_locus(creatures1::creatures::GenomeLocusKind, std::uint8_t,
                                      std::uint8_t) override {
        return next();
    }
    std::uint8_t* resolve_creature_locus(creatures1::creatures::GenomeLocusKind, std::uint8_t,
                                         std::uint8_t) override {
        return next();
    }

private:
    std::uint8_t* next() {
        return position_ < order_.size() ? order_[position_++] : nullptr;
    }
    std::vector<std::uint8_t*> order_;
    std::size_t position_ = 0;
};

struct LcState {
    bio::Biochemistry biochemistry;
    std::array<std::uint8_t, kLocusPool> loci{};
};

// Loads the case into `biochemistry`; `loci` must outlive it.
inline void build_lc_into(const Case& c, bio::Biochemistry& biochemistry,
                          std::array<std::uint8_t, kLocusPool>& loci) {
    std::vector<std::uint8_t> bytes;
    const auto u32 = [&bytes](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    u32(static_cast<std::uint32_t>(c.emitters.size()));
    u32(static_cast<std::uint32_t>(c.receptors.size()));
    u32(static_cast<std::uint32_t>(c.reactions.size()));
    for (std::size_t i = 0; i < 256; ++i) {
        bytes.push_back(c.concentration[i]);
        bytes.push_back(c.half_life[i]);
    }
    std::vector<std::uint8_t*> order;
    const auto locus = [&loci](int pool) {
        return pool < 0 ? nullptr : &loci[static_cast<std::size_t>(pool)];
    };
    for (const Emitter& e : c.emitters) {
        bytes.insert(bytes.end(), {e.domain, e.tissue, e.locus, e.chemical, e.threshold,
                                   e.period, e.amount, e.flags});
        order.push_back(locus(e.pool));
    }
    for (const Receptor& e : c.receptors) {
        bytes.insert(bytes.end(), {e.domain, e.tissue, e.locus, e.chemical, e.threshold,
                                   e.nominal, e.gain, e.flags});
        order.push_back(locus(e.pool));
    }
    for (const Reaction& e : c.reactions) {
        bytes.insert(bytes.end(), {e.r1_amount, e.r1, e.r2_amount, e.r2, e.selector,
                                   e.p1_amount, e.p1, e.p2_amount, e.p2});
    }
    CaseArchive archive(bytes);
    CaseLoci case_loci(order);
    biochemistry.serialize(archive, case_loci);
    if (!archive.consumed_all()) {
        std::fprintf(stderr, "the LC archive layout does not match the case\n");
        std::exit(4);
    }
    // Loading sets each receptor's locus to its nominal value; the case's
    // loci start where the native ones do.
    loci = c.loci;
}

inline void build_lc(const Case& c, LcState& l) { build_lc_into(c, l.biochemistry, l.loci); }

} // namespace biochemistry_case
