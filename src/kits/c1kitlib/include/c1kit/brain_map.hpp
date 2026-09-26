#pragma once

// The creature's brain as the game reports it, and what each neuron stands
// for.  Header-only and portable.
//
// The brain is a 64 x 64 grid holding nine lobes, each a rectangle of
// neurons:
//
//   `dde: lobe` (query holder)   binary: a count byte, then per lobe grid x,
//                                grid y, width, height and winner-take-all
//                                flags, a byte each.
//   brain report (holder mode 2) three bytes per neuron that is not zero:
//                                '0' + grid x, '0' + grid y, '0' + value/16.
//                                (Brain::format_activity_report.)
//   `dde: cell L N R`            seven "%d|" values for neuron N of lobe L,
//                                over its dendrites under rule R: firing
//                                strength, activation, dendrite count, and
//                                the sums of their current, target and
//                                baseline weights and states.

#include "c1kit/protocol.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace c1kit {

constexpr int kBrainGridSize = 64;

enum StandardLobe : int {
    kLobePerception = 0,
    kLobeDrive = 1,
    kLobeStimulusSource = 2,
    kLobeVerb = 3,
    kLobeNoun = 4,
    kLobeGeneralSense = 5,
    kLobeDecision = 6,
    kLobeAttention = 7,
    kLobeConcept = 8,
    kStandardLobeCount = 9,
};

inline const char* lobe_name(int lobe) {
    switch (lobe) {
    case kLobePerception: return "Perception";
    case kLobeDrive: return "Drive";
    case kLobeStimulusSource: return "Stimulus source";
    case kLobeVerb: return "Verb";
    case kLobeNoun: return "Noun";
    case kLobeGeneralSense: return "General sense";
    case kLobeDecision: return "Decision";
    case kLobeAttention: return "Attention";
    case kLobeConcept: return "Concept";
    default: return "Lobe";
    }
}

// What each lobe does, in a line.
inline const char* lobe_description(int lobe) {
    switch (lobe) {
    case kLobePerception: return "Copies of the drive, sense, verb and noun inputs, where the concept lobe reads them";
    case kLobeDrive: return "One neuron per drive: how much the creature feels each need";
    case kLobeStimulusSource: return "Which kind of object the last stimulus came from";
    case kLobeVerb: return "Verbs the creature has heard";
    case kLobeNoun: return "Nouns: the kinds of object seen or heard of";
    case kLobeGeneralSense: return "Senses: touch, sounds, nearby creatures and walls";
    case kLobeDecision: return "One neuron per action; the strongest one is what the creature does";
    case kLobeAttention: return "Which kind of object the creature is attending to";
    case kLobeConcept: return "Learned associations between perceptions, which drive decisions";
    default: return "";
    }
}

struct LobeLayout {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int flags = 0;
    int neurons() const { return width * height; }
    bool contains(int grid_x, int grid_y) const {
        return grid_x >= x && grid_x < x + width && grid_y >= y &&
               grid_y < y + height;
    }
};

// The kit's reply loses its last byte to the terminator (the game writes a
// 0xff there to be overwritten), so a reply of count*5+1 bytes is whole.
inline bool parse_lobe_reply(const std::string& reply,
                             std::vector<LobeLayout>& out) {
    out.clear();
    if (reply.empty()) {
        return false;
    }
    const std::size_t count = static_cast<std::uint8_t>(reply[0]);
    if (reply.size() < 1 + count * 5) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto byte = [&](std::size_t offset) {
            return static_cast<int>(static_cast<std::uint8_t>(reply[1 + i * 5 + offset]));
        };
        LobeLayout lobe;
        lobe.x = byte(0);
        lobe.y = byte(1);
        lobe.width = byte(2);
        lobe.height = byte(3);
        lobe.flags = byte(4);
        out.push_back(lobe);
    }
    return true;
}

constexpr char kLobeQuery[] = "dde: lobe,endm";

// The brain grid from a brain report.  The report lists every neuron whose
// value is not zero, but as value/16, so a neuron at 1..15 comes through as
// level 0: `reported` tells it apart from a neuron at 0, which is not listed.
struct BrainActivity {
    std::uint8_t level[kBrainGridSize][kBrainGridSize] = {};
    bool reported[kBrainGridSize][kBrainGridSize] = {};
};

inline void parse_activity_report(const std::string& reply, BrainActivity& out) {
    out = BrainActivity();
    for (std::size_t at = 0; at + 3 <= reply.size(); at += 3) {
        const int x = static_cast<std::uint8_t>(reply[at]) - '0';
        const int y = static_cast<std::uint8_t>(reply[at + 1]) - '0';
        const int value = static_cast<std::uint8_t>(reply[at + 2]) - '0';
        if (x >= 0 && x < kBrainGridSize && y >= 0 && y < kBrainGridSize &&
            value >= 0) {
            out.level[x][y] = static_cast<std::uint8_t>(value > 15 ? 15 : value);
            out.reported[x][y] = true;
        }
    }
}

// The middle of the range a report can mean for a cell: 0 if not listed,
// otherwise level * 16 + 8 (so 8 for 1..15, 248 for 240..255).
inline int estimated_value(const BrainActivity& activity, int x, int y) {
    if (x < 0 || x >= kBrainGridSize || y < 0 || y >= kBrainGridSize ||
        !activity.reported[x][y]) {
        return 0;
    }
    return activity.level[x][y] * 16 + 8;
}

// What the brain report measures: the first work value (var0) of the
// holder's macro.  Rules 0 and 1 (var1) pick the dendrites for the weight
// modes.  But a kit cannot set them: LoadMacro only stores the script
// (Macro::LoadScriptText @ 0x0041a280) and RequestMacro goes straight to the
// report (CMacroHolder::DispatchFormatBrainActivityReport @ 0x00419400), so
// they stay 0 and a report is always of firing strength.  The other measures
// are had exactly from `cell` (exact_report_value), bar the strongest weight.
enum BrainReportMode : int {
    kReportFiringStrength = 0,
    kReportActivation = 1,
    kReportStrongestWeight = 2,
    kReportAverageTargetWeight = 3,
    kReportAverageDendriteState = 4,
};

inline std::string brain_report_script(int mode, int rule) {
    return "inst,setv var0 " + std::to_string(mode) + ",setv var1 " +
           std::to_string(rule) + ",endm";
}

// `cell`: one neuron.
struct NeuronValues {
    int firing_strength = 0;
    int activation = 0;
    int dendrites = 0;
    int current_weight_sum = 0;
    int target_weight_sum = 0;
    int baseline_weight_sum = 0;
    int dendrite_state_sum = 0;
};

inline std::string neuron_query(int lobe, int neuron) {
    const std::string cell =
        std::to_string(lobe) + " " + std::to_string(neuron);
    return "inst,dde: cell " + cell + " 0,dde: cell " + cell + " 1,endm";
}

// Two sets, rule 0 then rule 1.
inline bool parse_neuron_values(std::string reply, NeuronValues out[2]) {
    for (int rule = 0; rule < 2; ++rule) {
        int values[7] = {};
        for (int& value : values) {
            const std::string field = take_field(reply, '|');
            if (field.empty()) {
                return false;
            }
            value = std::atoi(field.c_str());
        }
        out[rule].firing_strength = values[0];
        out[rule].activation = values[1];
        out[rule].dendrites = values[2];
        out[rule].current_weight_sum = values[3];
        out[rule].target_weight_sum = values[4];
        out[rule].baseline_weight_sum = values[5];
        out[rule].dendrite_state_sum = values[6];
    }
    return true;
}

// `cell` for `count` neurons of a lobe from `first`, under one rule, in one
// query.  The reply comes back in the kit's command buffer (4096 bytes); a
// cell is at most about 36 characters, so 64 fit with room to spare.
constexpr int kCellsPerQuery = 64;

inline std::string lobe_cells_query(int lobe, int first, int count, int rule) {
    std::string script = "inst";
    for (int neuron = first; neuron < first + count; ++neuron) {
        script += ",dde: cell " + std::to_string(lobe) + " " + std::to_string(neuron) + " " +
                  std::to_string(rule);
    }
    return script + ",endm";
}

inline bool parse_cell_batch(std::string reply, int count, std::vector<NeuronValues>& out) {
    out.clear();
    for (int cell = 0; cell < count; ++cell) {
        int values[7] = {};
        for (int& value : values) {
            const std::string field = take_field(reply, '|');
            if (field.empty()) {
                return false;
            }
            value = std::atoi(field.c_str());
        }
        NeuronValues v;
        v.firing_strength = values[0];
        v.activation = values[1];
        v.dendrites = values[2];
        v.current_weight_sum = values[3];
        v.target_weight_sum = values[4];
        v.baseline_weight_sum = values[5];
        v.dendrite_state_sum = values[6];
        out.push_back(v);
    }
    return true;
}

// What a brain report in `mode` would have measured, exactly, from a
// neuron's `cell` values; -1 for the strongest dendrite weight, which `cell`
// sums rather than reporting the largest.
inline int exact_report_value(const NeuronValues& v, int mode) {
    switch (mode) {
    case kReportFiringStrength:
        return v.firing_strength;
    case kReportActivation:
        return v.activation;
    case kReportAverageTargetWeight:
        return v.dendrites > 0 ? v.target_weight_sum / v.dendrites : 0;
    case kReportAverageDendriteState:
        return v.dendrites > 0 ? v.dendrite_state_sum / v.dendrites : 0;
    default:
        return -1;
    }
}

// The decision lobe, one "%d|" per decision neuron plus the attention target
// and the two learning chemicals (the Decisions page;
// CDecisionPage::RunScienceExperimentCaosScript @ 0x00407840 asks for 16
// cells, rule 0).
inline std::string decisions_query(int neurons) {
    return "inst,dde: putv _it_,dde: putv chem 54,dde: putv chem 55,"
           "setv var0 0,reps " +
           std::to_string(neurons) +
           ",dde: cell 6 var0 0,addv var0 1,repe,endm";
}

// ---------------------------------------------------------------------------
// What a neuron stands for
// ---------------------------------------------------------------------------

// The game's stock vocabulary (Creature::initialize_default_vocabulary,
// the executable's tables at 00454330, 004543d0 and 00454410): sixteen
// verbs, one per verb neuron, and forty kinds of object, one per noun,
// attention and stimulus-source neuron (attention neuron n speaks noun word
// record 0x10 + n, which is kind n).
inline const char* verb_word(int neuron) {
    static const char* const kVerbs[16] = {
        "stay", "push", "pull", "stop", "come", "run", "get", "drop",
        "why", "rest", "west", "east", "aaa", "bbb", "ccc", "ddd"};
    return neuron >= 0 && neuron < 16 ? kVerbs[neuron] : "";
}

inline const char* object_kind_word(int neuron) {
    static const char* const kKinds[40] = {
        "", "hand", "button", "nature", "herb", "egg", "food",
        "drink", "dispenser", "music", "animal", "heat", "comfort", "toy",
        "bigtoy", "weed", "", "", "", "", "", "", "", "", "", "", "vehicle",
        "lift", "computer", "gadget", "cannon", "", "", "", "", "", "",
        "Norn", "Grendel", ""};
    return neuron >= 0 && neuron < 40 ? kKinds[neuron] : "";
}

// General sense neurons the game sets by number (creature.cpp and
// update.cpp); the others are set by stimulus genes, which name no sense.
inline const char* general_sense_name(int neuron) {
    switch (neuron) {
    case 12: return "Distance moved";
    case 13: return "Moved";
    case 15: return "Another creature is near";
    case 16: return "It shares a parent with me";
    case 17: return "It is my parent";
    case 18: return "It is my child";
    case 19: return "It is the opposite sex";
    default: return "";
    }
}

// Names the kit reads from the game's files.
struct NeuronNames {
    std::vector<std::string> chemicals;  // allchemicals.str
    std::vector<std::string> decisions;  // decision.str
};

// "Approach", "Hunger", "food"...; empty when the neuron has no fixed
// meaning (perception and concept neurons, most senses).
inline std::string neuron_meaning(int lobe, int neuron, const NeuronNames& names) {
    switch (lobe) {
    case kLobeDrive:
        // Drive neurons take the drive levels (update.cpp), which the drive
        // chemicals set in order from Pain (chemical 1) through the receptor
        // genes of a stock genome.
        if (neuron + 1 < static_cast<int>(names.chemicals.size())) {
            return names.chemicals[static_cast<std::size_t>(neuron + 1)];
        }
        return "";
    case kLobeDecision:
        if (neuron < static_cast<int>(names.decisions.size())) {
            return names.decisions[static_cast<std::size_t>(neuron)];
        }
        return "";
    case kLobeVerb:
        return verb_word(neuron);
    case kLobeNoun:
    case kLobeAttention:
    case kLobeStimulusSource:
        return object_kind_word(neuron);
    case kLobeGeneralSense:
        return general_sense_name(neuron);
    default:
        return "";
    }
}

// Which lobe and neuron a grid cell is, or false.  The first lobe listed
// wins where lobes overlap.
inline bool neuron_at(const std::vector<LobeLayout>& lobes, int grid_x,
                      int grid_y, int& lobe, int& neuron) {
    for (std::size_t i = 0; i < lobes.size(); ++i) {
        if (lobes[i].contains(grid_x, grid_y)) {
            lobe = static_cast<int>(i);
            neuron = (grid_y - lobes[i].y) * lobes[i].width + (grid_x - lobes[i].x);
            return true;
        }
    }
    return false;
}

} // namespace c1kit
