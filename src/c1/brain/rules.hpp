#pragma once

#include <cstdint>

#include "../creatures/genome.hpp"

namespace creatures1::brain {

// An SVRule token: the bytecode of C1's lobe and dendrite rules (state,
// gain, lose, suscept and reinforce rules).  Tokens 1..21 read an operand
// into the accumulator, 22..27 are operators, 0 ends the rule.  The names
// are the official ones the C1 Genetics Kit shows (spelled in
// kSVRuleTokenNames); TRUE is spelled true_ here because windows.h defines
// TRUE.  CLobeRuleExpression::LoadFromGenome reduces every genome byte
// modulo 30, so 28 and 29 can occur; they do nothing.
enum class SVRuleToken : std::uint8_t {
    end = 0,          // <end>
    zero = 1,         // 0
    one = 2,          // 1
    sixty_four = 3,   // 64
    two_five_five = 4,// 255
    chem0 = 5,        // the lobe's chemical inputs (receptor loci 13..16)
    chem1 = 6,
    chem2 = 7,
    chem3 = 8,
    state = 9,        // the neuron's state (activation)
    output = 10,      // the neuron's output (firing strength)
    thres = 11,       // the lobe's threshold
    type0 = 12,       // sum over type 0 dendrites of STW x source output
    type1 = 13,
    anded0 = 14,      // the type 0 sum, or 0 unless every source fires
    anded1 = 15,
    input = 16,       // the dendrite's source neuron output
    conduct = 17,     // never written: reads 0
    suscept = 18,     // the dendrite's susceptibility
    stw = 19,
    ltw = 20,
    strength = 21,
    true_ = 22,       // TRUE: stop, keeping the accumulator, if it is 0
    plus = 23,        // PLUS: saturating add of the next operand
    minus = 24,       // MINUS: saturating subtract
    times = 25,       // TIMES: accumulator x operand / 256
    incr = 26,        // INCR: saturating increment
    decr = 27,        // DECR: saturating decrement
    unused_28 = 28,
    unused_29 = 29,
};

// The official name of each SVRule token, indexed by its value.
constexpr const char* kSVRuleTokenNames[30] = {
    "<end>", "0", "1", "64", "255", "chem0", "chem1", "chem2", "chem3",
    "state", "output", "thres", "type0", "type1", "anded0", "anded1",
    "input", "conduct", "suscept", "STW", "LTW", "strength", "TRUE", "PLUS",
    "MINUS", "TIMES", "INCR", "DECR", "(unused)", "(unused)"};

using Genome = creatures1::creatures::Genome;

} // namespace creatures1::brain
