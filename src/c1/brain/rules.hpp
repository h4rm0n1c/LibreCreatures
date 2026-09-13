#pragma once

#include <cstdint>

#include "../creatures/genome.hpp"

namespace creatures1::brain {

// The bytecode used by C1 lobe expressions. Values 1..21 address the
// evaluator register file; 22..27 are operators and 30 terminates a
// program. Values 28 and 29 are retained as reserved/no-op opcodes.
enum class RuleToken : std::uint8_t {
    stop_if_zero = 22,
    saturating_add = 23,
    saturating_subtract = 24,
    q8_multiply = 25,
    saturating_increment = 26,
    saturating_decrement = 27,
    reserved_28 = 28,
    reserved_29 = 29,
    // Token zero terminates an expression.  CLobeRuleExpression::LoadFromGenome
    // normalizes every token modulo 30, so 30 itself can never appear in a
    // loaded expression, and the native evaluator's walk is unbounded -- it
    // relies on reaching this terminator in the genome's zero padding.
    end = 0,
};

using Genome = creatures1::creatures::Genome;

} // namespace creatures1::brain
