#pragma once

// A readable arrangement of a brain's lobes for display, independent of
// where the genome puts them on the brain grid.  Header-only and portable.
//
// Each lobe keeps its size and neurons; only its place changes.  Lobes are
// ranked by how far they are from the inputs -- the longest chain of lobes
// feeding them, through dendrite rules and the copies into Perception -- and
// the ranks are packed into rows, left to right, as wide as the widest rank:
// for a stock norn, the five input lobes across the top, then Attention,
// Perception, Concept and Decision.  Without the genome's wiring the ranks
// are C1's standard roles.

#include "c1kit/brain_map.hpp"
#include "c1kit/genome.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace c1kit {

constexpr int kArrangedColumnGap = 4;  // cells between lobes in a row
constexpr int kArrangedRowGap = 5;     // cells between rows (room for names)

// How far each lobe is from the inputs.
inline std::vector<int> lobe_ranks(std::size_t count, const std::vector<LobeWiring>* wiring) {
    std::vector<int> rank(count, 0);
    if (wiring == nullptr || wiring->size() != count) {
        // C1's standard lobes: the senses and drives, Attention, Perception,
        // Concept, Decision.
        static const int kStandard[] = {2, 0, 0, 0, 0, 0, 4, 1, 3};
        for (std::size_t i = 0; i < count && i < 9; ++i) rank[i] = kStandard[i];
        return rank;
    }
    struct Edge {
        std::size_t from, to;
    };
    std::vector<Edge> edges;
    for (std::size_t i = 0; i < count; ++i) {
        for (const DendriteRule& rule : (*wiring)[i].rules) {
            if (rule.most > 0 && static_cast<std::size_t>(rule.source) != i &&
                static_cast<std::size_t>(rule.source) < count) {
                edges.push_back({static_cast<std::size_t>(rule.source), i});
            }
        }
        if (i > 0 && (*wiring)[i].perception_copy != 0) {
            edges.push_back({i, 0});
        }
    }
    // Longest path; a cycle stops growing after `count` passes.
    for (std::size_t pass = 0; pass < count; ++pass) {
        bool changed = false;
        for (const Edge& e : edges) {
            if (rank[e.to] < rank[e.from] + 1 && rank[e.from] + 1 <= static_cast<int>(count)) {
                rank[e.to] = rank[e.from] + 1;
                changed = true;
            }
        }
        if (!changed) break;
    }
    return rank;
}

inline std::vector<LobeLayout> arrange_lobes(const std::vector<LobeLayout>& lobes,
                                             const std::vector<LobeWiring>* wiring) {
    const std::size_t count = lobes.size();
    std::vector<LobeLayout> out = lobes;
    if (count == 0) return out;
    const std::vector<int> rank = lobe_ranks(count, wiring);
    const int deepest = *std::max_element(rank.begin(), rank.end());

    // Each rank's lobes, in lobe order, and its width.
    std::vector<std::vector<std::size_t>> groups(static_cast<std::size_t>(deepest) + 1);
    for (std::size_t i = 0; i < count; ++i) groups[static_cast<std::size_t>(rank[i])].push_back(i);
    const auto width_of = [&](const std::vector<std::size_t>& group) {
        int width = 0;
        for (std::size_t i : group) width += lobes[i].width;
        return group.empty() ? 0 : width + kArrangedColumnGap * static_cast<int>(group.size() - 1);
    };
    int widest = 0;
    for (const auto& group : groups) widest = (std::max)(widest, width_of(group));

    // Whole ranks into rows while they fit.
    std::vector<std::vector<std::size_t>> rows;
    int row_width = 0;
    for (const auto& group : groups) {
        if (group.empty()) continue;
        const int width = width_of(group);
        if (!rows.empty() && row_width + kArrangedColumnGap + width <= widest) {
            rows.back().insert(rows.back().end(), group.begin(), group.end());
            row_width += kArrangedColumnGap + width;
        } else {
            rows.push_back(group);
            row_width = width;
        }
    }

    int top = 0;
    for (const auto& row : rows) {
        int height = 0;
        for (std::size_t i : row) height = (std::max)(height, lobes[i].height);
        int x = (widest - width_of(row)) / 2;
        for (std::size_t i : row) {
            out[i].x = x;
            out[i].y = top + (height - lobes[i].height) / 2;
            x += lobes[i].width + kArrangedColumnGap;
        }
        top += height + kArrangedRowGap;
    }
    return out;
}

} // namespace c1kit
