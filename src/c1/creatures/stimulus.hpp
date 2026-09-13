#pragma once

#include <array>
#include <cstdint>

namespace creatures1::objects {
class Object;
}

namespace creatures1::creatures {

class Creature;

// Four packed bytes are used by the original stimulus pipeline.  These names
// describe their roles without exposing the decompiler's byte-field syntax.
struct StimulusDescriptor {
    std::uint8_t attention_activation = 0;
    std::uint8_t target_neuron_index = 0;
    std::uint8_t target_lobe_activation = 0;
    std::uint8_t flags = 0;
};

struct StimulusChemicalIds {
    std::uint8_t first = 0;
    std::uint8_t second = 0;
    std::uint8_t third = 0;
    std::uint8_t fourth = 0;
};

struct StimulusChemicalAmounts {
    std::uint8_t first = 0;
    std::uint8_t second = 0;
    std::uint8_t third = 0;
    std::uint8_t fourth = 0;
};

struct StimulusContext {
    objects::Object* target = nullptr;
    Creature* source_creature = nullptr;
    StimulusDescriptor descriptor{};
    StimulusChemicalIds chemical_ids{};
    StimulusChemicalAmounts chemical_amounts{};
};

static_assert(sizeof(StimulusDescriptor) == 4);
static_assert(sizeof(StimulusChemicalIds) == 4);
static_assert(sizeof(StimulusChemicalAmounts) == 4);

// Exact native defaults referenced by the 36-entry pointer table at
// 00454460.  The pointers and chemical payloads are null/zero in the shipped
// image; only the four-byte descriptors carry initial data.  Genome loading
// mutates copies of these records, so this table remains immutable source
// data rather than shared runtime state.
inline constexpr std::array<StimulusContext, 36>
    kDefaultStimulusContexts = {{
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x20, 0x00, 0xff, 0x04}, {}, {}},
        {nullptr, nullptr, {0x50, 0x00, 0xff, 0x04}, {}, {}},
        {nullptr, nullptr, {0x20, 0x01, 0xff, 0x04}, {}, {}},
        {nullptr, nullptr, {0x78, 0x01, 0xff, 0x04}, {}, {}},
        {nullptr, nullptr, {0x5a, 0x0a, 0xff, 0x01}, {}, {}},
        {nullptr, nullptr, {0x1e, 0x0b, 0xff, 0x01}, {}, {}},
        {nullptr, nullptr, {0x00, 0x02, 0xff, 0x00}, {}, {}},
        {nullptr, nullptr, {0xff, 0xff, 0x00, 0x01}, {}, {}},
        {nullptr, nullptr, {0x00, 0x08, 0x80, 0x04}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0xff, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0xb4, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
        {nullptr, nullptr, {0x00, 0xff, 0x00, 0x00}, {}, {}},
    }};

} // namespace creatures1::creatures
