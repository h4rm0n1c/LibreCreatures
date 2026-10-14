#include "brain_recorder.hpp"

#include "brain.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

// The vendored stb_image_write (third_party/stb, public domain / MIT), for
// its zlib compressor only; no image writing is compiled in.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "../../../third_party/stb/stb_image_write.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace creatures1::brain {
namespace {

constexpr std::uint32_t kFormatVersion = 11;
// Frames are buffered and written as one compressed chunk this often, so a
// game that is killed loses at most this many frames.
constexpr std::uint32_t kFramesPerChunk = 32;
constexpr int kCompressionQuality = 9;
constexpr std::uint32_t kKeyframeInterval = 256;
constexpr std::uint16_t kNoTarget = 0xffff;

struct DendriteState {
    std::uint16_t target = kNoTarget;
    std::uint8_t susceptibility = 0;
    std::uint8_t stw = 0;
    std::uint8_t ltw = 0;
    std::uint8_t state = 0;

    bool operator==(const DendriteState& other) const {
        return target == other.target &&
               susceptibility == other.susceptibility &&
               stw == other.stw &&
               ltw == other.ltw &&
               state == other.state;
    }
};

struct NeuronState {
    std::uint8_t activation = 0;
    std::uint8_t firing = 0;
};

struct Snapshot {
    std::vector<std::uint8_t> layout;
    std::vector<NeuronState> neurons;
    std::vector<DendriteState> dendrites;
};

class Writer {
public:
    explicit Writer(std::vector<std::uint8_t>& out) : out_(out) {}
    void u8(std::uint8_t value) { out_.push_back(value); }
    void u16(std::uint16_t value) {
        u8(static_cast<std::uint8_t>(value));
        u8(static_cast<std::uint8_t>(value >> 8));
    }
    void u32(std::uint32_t value) {
        u16(static_cast<std::uint16_t>(value));
        u16(static_cast<std::uint16_t>(value >> 16));
    }
    void bytes(const std::vector<std::uint8_t>& data) {
        out_.insert(out_.end(), data.begin(), data.end());
    }
    // Unsigned LEB128: 7 bits a byte, low bits first.
    void varint(std::uint32_t value) {
        while (value >= 0x80) {
            u8(static_cast<std::uint8_t>(value | 0x80));
            value >>= 7;
        }
        u8(static_cast<std::uint8_t>(value));
    }

    // Zigzag: small values of either sign stay short.
    void zigzag(std::int32_t value) {
        varint(value >= 0 ? static_cast<std::uint32_t>(value) * 2u
                          : static_cast<std::uint32_t>(-(value + 1)) * 2u + 1u);
    }

private:
    std::vector<std::uint8_t>& out_;
};

// Where one dendrite sits in the file: its number in keyframe order and the
// lobe its source is in.
struct ConnectionPlace {
    std::uint32_t number = 0;
    std::uint32_t source_lobe = 0;
};

struct Recording {
    const Brain* brain = nullptr;
    std::unordered_map<const Dendrite*, ConnectionPlace> places;
    // Per lobe, the number of its first dendrite; one more entry holds the
    // dendrite count.
    std::vector<std::uint32_t> lobe_first_numbers;
    std::FILE* file = nullptr;
    std::uint32_t moniker = 0;
    std::uint32_t part = 0;
    std::uint32_t frames = 0;
    Snapshot previous;
    std::vector<std::uint8_t> previous_inputs;
    std::vector<std::uint8_t> previous_chemicals;
    std::vector<std::uint8_t> previous_chemistry;
    const void* biochemistry = nullptr;
    // Chemical changes since the last frame: (origin, number, chemical) ->
    // summed change.
    std::map<std::tuple<std::uint8_t, std::uint32_t, std::uint8_t>, int> flows;
    std::uint32_t stimuli = 0;
    // Votes of the winner-take-all lobes' last update, by lobe index:
    // per neuron the two sums, and per contributing dendrite its share.
    struct LobeVotes {
        std::vector<std::uint8_t> sums;
        std::map<std::uint32_t, std::uint8_t> contributions;
    };
    std::map<std::uint32_t, LobeVotes> votes;
    LobeVotes* votes_target = nullptr;  // the lobe being summed now
    std::vector<std::int32_t> previous_goal_matrix;
    std::vector<BrainRecordContext::Word> previous_words;
    std::uint32_t instincts = 0;
    std::uint32_t current_instinct = 0;
    // The lobe whose dendrites are updating now: its 'U' record's sets, in
    // the order the update works on each dendrite.
    struct LobeUpdate {
        bool open = false;
        std::uint8_t lobe = 0;
        std::uint8_t flags = 0;
        std::uint32_t tick = 0;
        std::uint32_t first_number = 0;  // the lobe's first dendrite
        std::array<std::uint32_t, 2> suscept_decay{};
        std::array<std::vector<std::uint32_t>, 7> sets;
        std::array<std::vector<std::uint8_t>, 7> payloads;
    };
    LobeUpdate update;
    std::vector<std::uint8_t> pending;

    Recording() = default;
    Recording(const Recording&) = delete;
    Recording& operator=(const Recording&) = delete;
    ~Recording();
};

// Writes the buffered frames as one chunk: u32 raw size, u32 compressed size,
// then the zlib stream.
void write_chunk(Recording& recording) {
    if (recording.file == nullptr || recording.pending.empty()) {
        return;
    }
    int compressed_size = 0;
    unsigned char* compressed = stbi_zlib_compress(
        recording.pending.data(), static_cast<int>(recording.pending.size()),
        &compressed_size, kCompressionQuality);
    if (compressed == nullptr) {
        recording.pending.clear();
        return;
    }
    std::vector<std::uint8_t> sizes;
    Writer header(sizes);
    header.u32(static_cast<std::uint32_t>(recording.pending.size()));
    header.u32(static_cast<std::uint32_t>(compressed_size));
    std::fwrite(sizes.data(), 1, sizes.size(), recording.file);
    std::fwrite(compressed, 1, static_cast<std::size_t>(compressed_size),
                recording.file);
    STBIW_FREE(compressed);
    std::fflush(recording.file);
    recording.pending.clear();
}

Recording::~Recording() {
    write_chunk(*this);
    if (file != nullptr) {
        std::fclose(file);
    }
}

struct Settings {
    std::string folder;
    std::set<std::uint32_t> monikers;
};

const Settings& settings() {
    static const Settings value = [] {
        Settings result;
        if (const char* folder = std::getenv("C1_BRAIN_RECORD")) {
            result.folder = folder;
            if (!result.folder.empty() && result.folder.back() != '\\' &&
                result.folder.back() != '/') {
                result.folder.push_back('\\');
            }
        }
        if (const char* list = std::getenv("C1_BRAIN_RECORD_MONIKERS")) {
            std::string text(list);
            std::size_t start = 0;
            while (start < text.size()) {
                const std::size_t comma = text.find(',', start);
                const std::string item = text.substr(
                    start, comma == std::string::npos ? std::string::npos
                                                      : comma - start);
                if (!item.empty()) {
                    result.monikers.insert(static_cast<std::uint32_t>(
                        std::strtoul(item.c_str(), nullptr, 16)));
                }
                if (comma == std::string::npos) {
                    break;
                }
                start = comma + 1;
            }
        }
        return result;
    }();
    return value;
}

std::map<const void*, Recording>& recordings() {
    static std::map<const void*, Recording> value;
    return value;
}

std::map<const Brain*, Recording*>& recordings_by_brain() {
    static std::map<const Brain*, Recording*> value;
    return value;
}

std::map<const void*, Recording*>& recordings_by_biochemistry() {
    static std::map<const void*, Recording*> value;
    return value;
}

std::map<const Brain*, bool>& dreaming_brains() {
    static std::map<const Brain*, bool> value;
    return value;
}

std::map<std::uint32_t, std::uint32_t>& parts_by_moniker() {
    static std::map<std::uint32_t, std::uint32_t> value;
    return value;
}

std::uint16_t target_index(const Brain& brain, const DendriteType& rule,
                           const Dendrite& connection) {
    if (connection.source == nullptr ||
        rule.source_lobe >= brain.lobe_count()) {
        return kNoTarget;
    }
    const Lobe& target_lobe = brain.lobe(rule.source_lobe);
    if (target_lobe.neuron_count_value() == 0) {
        return kNoTarget;
    }
    const LobeNeuron* first = &target_lobe.neuron(0);
    const std::ptrdiff_t offset = connection.source - first;
    if (offset < 0 ||
        offset >= static_cast<std::ptrdiff_t>(target_lobe.neuron_count_value()) ||
        offset >= kNoTarget) {
        return kNoTarget;
    }
    return static_cast<std::uint16_t>(offset);
}

std::uint16_t source_index(const Brain& brain, std::uint32_t lobe_index,
                           const LobeNeuron* source) {
    if (source == nullptr || lobe_index >= brain.lobe_count()) {
        return kNoTarget;
    }
    const Lobe& lobe = brain.lobe(lobe_index);
    if (lobe.neuron_count_value() == 0) {
        return kNoTarget;
    }
    const std::ptrdiff_t offset = source - &lobe.neuron(0);
    if (offset < 0 ||
        offset >= static_cast<std::ptrdiff_t>(lobe.neuron_count_value()) ||
        offset >= kNoTarget) {
        return kNoTarget;
    }
    return static_cast<std::uint16_t>(offset);
}

// Numbers every dendrite in keyframe order, for events.
void index_connections(const Brain& brain, Recording& recording) {
    recording.places.clear();
    recording.lobe_first_numbers.clear();
    std::uint32_t number = 0;
    for (std::uint32_t lobe_index = 0; lobe_index < brain.lobe_count();
         ++lobe_index) {
        recording.lobe_first_numbers.push_back(number);
        const Lobe& lobe = brain.lobe(lobe_index);
        for (std::uint32_t index = 0; index < lobe.neuron_count_value();
             ++index) {
            const LobeNeuron& neuron = lobe.neuron(index);
            for (std::uint8_t rule = 0; rule < 2; ++rule) {
                const Dendrite* connections =
                    rule == 0 ? neuron.type0_dendrites
                              : neuron.type1_dendrites;
                const std::uint8_t count = rule == 0
                                               ? neuron.type0_dendrite_count
                                               : neuron.type1_dendrite_count;
                for (std::uint8_t dendrite = 0;
                     connections != nullptr && dendrite < count; ++dendrite) {
                    recording.places[&connections[dendrite]] = {
                        number++, lobe.dendrite_type(rule).source_lobe};
                }
            }
        }
    }
    recording.lobe_first_numbers.push_back(number);
}

Snapshot take_snapshot(const Brain& brain) {
    Snapshot snapshot;
    std::vector<std::uint8_t>& layout = snapshot.layout;
    const auto push32 = [&layout](std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            layout.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    };
    push32(brain.lobe_count());
    for (std::uint32_t lobe_index = 0; lobe_index < brain.lobe_count();
         ++lobe_index) {
        const Lobe& lobe = brain.lobe(lobe_index);
        push32(lobe.neuron_count_value());
        layout.push_back(static_cast<std::uint8_t>(lobe.grid_width_value()));
        layout.push_back(static_cast<std::uint8_t>(lobe.grid_height_value()));
        for (std::size_t rule = 0; rule < 2; ++rule) {
            layout.push_back(static_cast<std::uint8_t>(
                lobe.dendrite_type(rule).source_lobe));
        }
        layout.push_back(
            static_cast<std::uint8_t>(lobe.perception_copy_state()));
        for (std::uint32_t index = 0; index < lobe.neuron_count_value();
             ++index) {
            const LobeNeuron& neuron = lobe.neuron(index);
            layout.push_back(neuron.type0_dendrite_count);
            layout.push_back(neuron.type1_dendrite_count);
            snapshot.neurons.push_back(
                {neuron.activation, neuron.firing_strength});
            for (std::uint8_t rule = 0; rule < 2; ++rule) {
                const Dendrite* connections =
                    rule == 0 ? neuron.type0_dendrites
                              : neuron.type1_dendrites;
                const std::uint8_t count = rule == 0
                                               ? neuron.type0_dendrite_count
                                               : neuron.type1_dendrite_count;
                for (std::uint8_t dendrite = 0;
                     connections != nullptr && dendrite < count; ++dendrite) {
                    const Dendrite& connection = connections[dendrite];
                    snapshot.dendrites.push_back(
                        {target_index(brain, lobe.dendrite_type(rule),
                                      connection),
                         connection.susceptibility, connection.stw,
                         connection.ltw,
                         connection.strength});
                }
            }
        }
    }
    return snapshot;
}

void close_recording(Recording& recording) {
    write_chunk(recording);
    if (recording.file != nullptr) {
        std::fclose(recording.file);
        recording.file = nullptr;
    }
}

bool open_part(Recording& recording, const Snapshot& snapshot) {
    close_recording(recording);
    std::uint32_t& part = parts_by_moniker()[recording.moniker];
    recording.part = part++;
    char name[48];
    std::snprintf(name, sizeof(name), "%08lx-%u.c1brain",
                  static_cast<unsigned long>(recording.moniker),
                  static_cast<unsigned>(recording.part));
    const std::string path = settings().folder + name;
    recording.file = std::fopen(path.c_str(), "wb");
    if (recording.file == nullptr) {
        return false;
    }
    std::vector<std::uint8_t> header;
    Writer out(header);
    out.bytes({'C', '1', 'B', 'R'});
    out.u32(kFormatVersion);
    out.u32(recording.moniker);
    out.bytes(snapshot.layout);
    std::fwrite(header.data(), 1, header.size(), recording.file);
    std::fflush(recording.file);
    recording.frames = 0;
    return true;
}

// Drives, then every lobe's chemical inputs ('I' record).
std::vector<std::uint8_t> collect_inputs(const Brain& brain,
                                         const BrainRecordContext& context) {
    std::vector<std::uint8_t> inputs(context.drives.begin(), context.drives.end());
    for (std::uint32_t index = 0; index < brain.lobe_count(); ++index) {
        const auto& chemicals = brain.lobe(index).chemical_inputs();
        inputs.insert(inputs.end(), chemicals.begin(), chemicals.end());
    }
    return inputs;
}

// A run of byte values, whole on a keyframe (u8 0, every value) or as the
// values changed since `previous` (u8 1, varint count, then per value a
// varint gap and the u8 value).
void write_values(Writer& out, const std::vector<std::uint8_t>& values,
                  std::vector<std::uint8_t>& previous, bool keyframe) {
    if (keyframe || previous.size() != values.size()) {
        out.u8(0);
        for (std::uint8_t value : values) {
            out.u8(value);
        }
    } else {
        out.u8(1);
        std::vector<std::uint32_t> changed;
        for (std::uint32_t i = 0; i < values.size(); ++i) {
            if (values[i] != previous[i]) {
                changed.push_back(i);
            }
        }
        out.varint(static_cast<std::uint32_t>(changed.size()));
        std::uint32_t next = 0;
        for (std::uint32_t i : changed) {
            out.varint(i - next);
            out.u8(values[i]);
            next = i + 1;
        }
    }
    previous = values;
}

void write_frame(Recording& recording, Snapshot& snapshot,
                 const BrainRecordContext& context,
                 const std::vector<std::uint8_t>& inputs) {
    Writer out(recording.pending);
    if (context.chemistry != recording.previous_chemistry) {
        out.u8('B');
        out.u32(context.world_tick);
        out.bytes(context.chemistry);
        recording.previous_chemistry = context.chemistry;
    }
    out.u8('C');
    out.u32(context.world_tick);
    out.u32(static_cast<std::uint32_t>(context.x));
    out.u32(static_cast<std::uint32_t>(context.y));
    out.u8(context.action);
    out.u8(static_cast<std::uint8_t>(context.attention_slot));
    out.u32(context.attention_classifier);
    out.u8(context.flags);

    const bool keyframe = recording.frames % kKeyframeInterval == 0;
    out.u8('I');
    write_values(out, inputs, recording.previous_inputs, keyframe);
    out.u8('X');
    write_values(out, std::vector<std::uint8_t>(context.chemicals.begin(),
                                                context.chemicals.end()),
                 recording.previous_chemicals, keyframe);
    out.u8('F');
    std::uint32_t flow_count = 0;
    for (const auto& flow : recording.flows) {
        flow_count += flow.second != 0 ? 1u : 0u;
    }
    out.varint(flow_count);
    for (const auto& [key, change] : recording.flows) {
        if (change == 0) {
            continue;
        }
        out.u8(std::get<0>(key));
        out.varint(std::get<1>(key));
        out.u8(std::get<2>(key));
        out.zigzag(change);
    }
    recording.flows.clear();
    out.u8('W');
    out.u32(static_cast<std::uint32_t>(context.hand_x));
    out.u32(static_cast<std::uint32_t>(context.hand_y));
    out.u32(context.attended_object);
    out.u32(static_cast<std::uint32_t>(context.attended_x));
    out.u32(static_cast<std::uint32_t>(context.attended_y));

    out.u8('V');
    out.u8(static_cast<std::uint8_t>(recording.votes.size()));
    for (const auto& [lobe, votes] : recording.votes) {
        out.u8(static_cast<std::uint8_t>(lobe));
        out.bytes(votes.sums);
        out.varint(static_cast<std::uint32_t>(votes.contributions.size()));
        std::uint32_t next = 0;
        for (const auto& [number, contribution] : votes.contributions) {
            out.varint(number - next);
            out.u8(contribution);
            next = number + 1;
        }
    }
    recording.votes.clear();
    recording.votes_target = nullptr;

    out.u8('M');
    if (keyframe || recording.previous_goal_matrix.size() != context.goal_matrix.size()) {
        out.u8(0);
        out.varint(static_cast<std::uint32_t>(context.goal_matrix.size()));
        for (std::int32_t value : context.goal_matrix) {
            out.zigzag(value);
        }
    } else {
        out.u8(1);
        std::vector<std::uint32_t> changed;
        for (std::uint32_t i = 0; i < context.goal_matrix.size(); ++i) {
            if (context.goal_matrix[i] != recording.previous_goal_matrix[i]) {
                changed.push_back(i);
            }
        }
        out.varint(static_cast<std::uint32_t>(changed.size()));
        std::uint32_t next = 0;
        for (std::uint32_t i : changed) {
            out.varint(i - next);
            out.zigzag(context.goal_matrix[i]);
            next = i + 1;
        }
    }
    recording.previous_goal_matrix = context.goal_matrix;

    std::vector<std::uint32_t> changed_words;
    for (std::uint32_t i = 0; i < context.words.size() && i < 256; ++i) {
        if (keyframe || i >= recording.previous_words.size() ||
            !(context.words[i] == recording.previous_words[i])) {
            changed_words.push_back(i);
        }
    }
    if (!changed_words.empty()) {
        out.u8('L');
        out.u8(static_cast<std::uint8_t>(changed_words.size()));
        for (std::uint32_t i : changed_words) {
            const auto& word = context.words[i];
            out.u8(static_cast<std::uint8_t>(i));
            for (const std::string* text : {&word.heard, &word.said}) {
                const std::size_t length = std::min<std::size_t>(text->size(), 255);
                out.u8(static_cast<std::uint8_t>(length));
                out.bytes(std::vector<std::uint8_t>(text->begin(), text->begin() + length));
            }
            out.u32(word.reinforcement);
        }
    }
    recording.previous_words = context.words;
    if (keyframe) {
        out.u8('K');
        out.u32(context.world_tick);
        for (const NeuronState& neuron : snapshot.neurons) {
            out.u8(neuron.activation);
            out.u8(neuron.firing);
        }
        for (const DendriteState& dendrite : snapshot.dendrites) {
            out.u16(dendrite.target);
            out.u8(dendrite.susceptibility);
            out.u8(dendrite.stw);
            out.u8(dendrite.ltw);
            out.u8(dendrite.state);
        }
    } else {
        // Changed neurons in index order, each as its gap from the previous
        // one and a mask of the fields that changed, then those fields.  The
        // 'U' and 'E' records carry every dendrite change.
        out.u8('D');
        out.u32(context.world_tick);
        std::vector<std::uint32_t> changed;
        for (std::uint32_t i = 0; i < snapshot.neurons.size(); ++i) {
            const NeuronState& now = snapshot.neurons[i];
            const NeuronState& before = recording.previous.neurons[i];
            if (now.activation != before.activation ||
                now.firing != before.firing) {
                changed.push_back(i);
            }
        }
        out.varint(static_cast<std::uint32_t>(changed.size()));
        std::uint32_t next = 0;
        for (std::uint32_t i : changed) {
            const NeuronState& now = snapshot.neurons[i];
            const NeuronState& before = recording.previous.neurons[i];
            const std::uint8_t mask = static_cast<std::uint8_t>(
                (now.activation != before.activation ? 1 : 0) |
                (now.firing != before.firing ? 2 : 0));
            out.varint(i - next);
            out.u8(mask);
            if (mask & 1) out.u8(now.activation);
            if (mask & 2) out.u8(now.firing);
            next = i + 1;
        }
    }
    ++recording.frames;
    if (recording.frames % kFramesPerChunk == 0) {
        write_chunk(recording);
    }
}

} // namespace

bool g_brain_events_wanted = false;
bool g_chemical_flows_wanted = false;
bool g_brain_votes_wanted = false;

namespace {
ChemicalOrigin g_current_origin = ChemicalOrigin::unknown;
std::uint32_t g_current_origin_number = 0;
} // namespace

ChemicalOriginScope::ChemicalOriginScope(ChemicalOrigin origin,
                                         std::uint32_t number)
    : saved_origin_(g_current_origin), saved_number_(g_current_origin_number) {
    g_current_origin = origin;
    g_current_origin_number = number;
}

ChemicalOriginScope::~ChemicalOriginScope() {
    g_current_origin = saved_origin_;
    g_current_origin_number = saved_number_;
}

ChemicalOrigin ChemicalOriginScope::current_origin() { return g_current_origin; }

std::uint32_t ChemicalOriginScope::current_number() {
    return g_current_origin_number;
}

bool brain_recording_enabled() { return !settings().folder.empty(); }

void record_brain_frame(const void* key, const Brain& brain,
                        const BrainRecordContext& context) {
    if (!brain_recording_enabled() || brain.lobe_count() == 0) {
        return;
    }
    const auto& wanted = settings().monikers;
    if (!wanted.empty() && wanted.count(context.moniker) == 0) {
        return;
    }
    Recording& recording = recordings()[key];
    Snapshot snapshot = take_snapshot(brain);
    if (recording.file == nullptr || recording.moniker != context.moniker ||
        snapshot.layout != recording.previous.layout ||
        recording.brain != &brain) {
        recording.moniker = context.moniker;
        if (recording.brain != nullptr) {
            recordings_by_brain().erase(recording.brain);
        }
        if (!open_part(recording, snapshot)) {
            recordings().erase(key);
            return;
        }
        recording.brain = &brain;
        index_connections(brain, recording);
        // The creature's own genome, so the file decodes on its own.
        std::vector<std::uint8_t> genome;
        if (context.load_genome) {
            genome = context.load_genome();
        }
        Writer out(recording.pending);
        out.u8('G');
        out.u32(static_cast<std::uint32_t>(genome.size()));
        out.bytes(genome);
        recordings_by_brain()[&brain] = &recording;
        g_brain_events_wanted = true;
        recording.flows.clear();
        recording.stimuli = 0;
        recording.previous_chemicals.clear();
        recording.previous_chemistry.clear();
        recording.votes.clear();
        recording.votes_target = nullptr;
        recording.previous_goal_matrix.clear();
        recording.previous_words.clear();
        recording.instincts = 0;
        recording.update = {};
        g_brain_votes_wanted = true;
    }
    if (recording.biochemistry != context.biochemistry) {
        if (recording.biochemistry != nullptr) {
            recordings_by_biochemistry().erase(recording.biochemistry);
        }
        recording.biochemistry = context.biochemistry;
        if (recording.biochemistry != nullptr) {
            recordings_by_biochemistry()[recording.biochemistry] = &recording;
            g_chemical_flows_wanted = true;
        }
    }
    write_frame(recording, snapshot, context, collect_inputs(brain, context));
    recording.previous = std::move(snapshot);
}

namespace {

// A 'U' record's set for an event kind, or -1 for kinds written as 'E'.
int update_set(DendriteEvent kind) {
    switch (kind) {
    case DendriteEvent::reinforce: return 1;
    case DendriteEvent::relax: return 2;
    case DendriteEvent::ltw_step: return 3;
    case DendriteEvent::strengthen: return 4;
    case DendriteEvent::weaken: return 5;
    case DendriteEvent::went_loose: return 6;
    default: return -1;
    }
}

// A dendrite set (see the 'U' record): count, runs, then the payloads.
void write_set(Writer& out, const std::vector<std::uint32_t>& numbers,
               const std::vector<std::uint8_t>& payload, std::uint32_t first) {
    out.varint(static_cast<std::uint32_t>(numbers.size()));
    std::uint32_t next = first;
    for (std::size_t i = 0; i < numbers.size();) {
        std::size_t end = i + 1;
        while (end < numbers.size() && numbers[end] == numbers[end - 1] + 1) {
            ++end;
        }
        const std::uint32_t gap = numbers[i] - next;
        const std::size_t length = end - i;
        out.varint((gap << 1) | (length > 1 ? 1u : 0u));
        if (length > 1) {
            out.varint(static_cast<std::uint32_t>(length - 2));
        }
        next = numbers[end - 1] + 1;
        i = end;
    }
    out.bytes(payload);
}

} // namespace

void record_dendrite_event(const Brain* brain, const Dendrite& connection,
                           DendriteEvent kind, const Dendrite& before,
                           std::uint8_t signal) {
    if (brain == nullptr) {
        return;
    }
    const auto found = recordings_by_brain().find(brain);
    if (found == recordings_by_brain().end() || found->second->file == nullptr) {
        return;
    }
    Recording& recording = *found->second;
    const auto place = recording.places.find(&connection);
    if (place == recording.places.end()) {
        return;
    }
    static_cast<void>(before);  // the reader knows it: its own state
    const int set = update_set(kind);
    if (set >= 0 && recording.update.open) {
        recording.update.sets[set].push_back(place->second.number);
        auto& payload = recording.update.payloads[set];
        switch (kind) {
        case DendriteEvent::reinforce: payload.push_back(signal); break;
        case DendriteEvent::relax: payload.push_back(connection.stw); break;
        case DendriteEvent::strengthen: payload.push_back(connection.strength); break;
        default: break;
        }
        return;
    }
    const auto dreaming = dreaming_brains().find(brain);
    const bool is_dreaming = dreaming != dreaming_brains().end() && dreaming->second;
    Writer out(recording.pending);
    out.u8('E');
    out.u8(static_cast<std::uint8_t>(static_cast<std::uint8_t>(kind) |
                                     (is_dreaming ? 0x80 : 0)));
    out.varint(place->second.number);
    // Only what this kind of event changed (see the header).
    switch (kind) {
    case DendriteEvent::reinforce:
    case DendriteEvent::relax:
        out.u8(connection.stw);
        break;
    case DendriteEvent::ltw_step:
        out.u8(connection.ltw);
        break;
    case DendriteEvent::strengthen:
    case DendriteEvent::weaken:
        out.u8(connection.strength);
        break;
    case DendriteEvent::attach:
    case DendriteEvent::rescue:
    case DendriteEvent::migrate:
    case DendriteEvent::migrate_failed:
        out.u16(source_index(*brain, place->second.source_lobe,
                             connection.source));
        out.u8(connection.stw);
        out.u8(connection.ltw);
        out.u8(connection.strength);
        break;
    case DendriteEvent::went_loose:
    case DendriteEvent::dream_ltw_raise:
    case DendriteEvent::reset:
        break;
    }
}

void record_chemical_flow(const void* biochemistry, ChemicalOrigin origin,
                          std::uint32_t number, std::uint8_t chemical,
                          int change) {
    if (change == 0 || biochemistry == nullptr) {
        return;
    }
    const auto found = recordings_by_biochemistry().find(biochemistry);
    if (found == recordings_by_biochemistry().end() ||
        found->second->file == nullptr) {
        return;
    }
    found->second->flows[{static_cast<std::uint8_t>(origin), number, chemical}] +=
        change;
}

std::uint32_t record_stimulus(const Brain* brain, const StimulusRecord& stimulus) {
    if (brain == nullptr) {
        return 0;
    }
    const auto found = recordings_by_brain().find(brain);
    if (found == recordings_by_brain().end() || found->second->file == nullptr) {
        return 0;
    }
    Recording& recording = *found->second;
    const std::uint32_t number = recording.stimuli++;
    Writer out(recording.pending);
    out.u8('S');
    out.varint(number);
    out.u8(stimulus.built_in);
    out.u32(stimulus.source_object);
    out.u32(stimulus.source_classifier);
    out.u32(static_cast<std::uint32_t>(stimulus.source_x));
    out.u32(static_cast<std::uint32_t>(stimulus.source_y));
    out.u8(stimulus.flags);
    out.u8(stimulus.attention_record);
    out.u8(stimulus.sense_neuron);
    out.u8(stimulus.source_activation);
    out.u8(stimulus.sense_activation);
    for (std::size_t i = 0; i < 4; ++i) {
        out.u8(stimulus.chemicals[i]);
        out.u8(stimulus.amounts[i]);
    }
    return number;
}

namespace {

Recording* recording_of(const Brain* brain) {
    if (brain == nullptr) {
        return nullptr;
    }
    const auto found = recordings_by_brain().find(brain);
    if (found == recordings_by_brain().end() || found->second->file == nullptr) {
        return nullptr;
    }
    return found->second;
}

std::uint32_t lobe_index_of(const Brain& brain, const Lobe& lobe) {
    for (std::uint32_t index = 0; index < brain.lobe_count(); ++index) {
        if (&brain.lobe(index) == &lobe) {
            return index;
        }
    }
    return 0xff;
}

} // namespace

void begin_lobe_votes(const Brain* brain, const Lobe& lobe) {
    Recording* recording = recording_of(brain);
    if (recording == nullptr) {
        return;
    }
    const std::uint32_t index = lobe_index_of(*brain, lobe);
    if (index == 0xff) {
        return;
    }
    auto& votes = recording->votes[index];
    votes.sums.assign(static_cast<std::size_t>(lobe.neuron_count_value()) * 2, 0);
    votes.contributions.clear();
    recording->votes_target = &votes;
}

void record_vote(const Brain* brain, const Dendrite& connection,
                 std::uint8_t contribution) {
    Recording* recording = recording_of(brain);
    if (recording == nullptr || contribution == 0) {
        return;
    }
    const auto place = recording->places.find(&connection);
    if (place == recording->places.end()) {
        return;
    }
    if (recording->votes_target == nullptr) {
        return;
    }
    recording->votes_target->contributions[place->second.number] = contribution;
}

void record_neuron_sums(const Brain* brain, const Lobe& lobe,
                        std::uint32_t neuron, std::uint8_t type0,
                        std::uint8_t type1) {
    Recording* recording = recording_of(brain);
    if (recording == nullptr) {
        return;
    }
    const auto found = recording->votes.find(lobe_index_of(*brain, lobe));
    if (found == recording->votes.end() || neuron * 2 + 1 >= found->second.sums.size()) {
        return;
    }
    found->second.sums[neuron * 2] = type0;
    found->second.sums[neuron * 2 + 1] = type1;
}

void begin_lobe_update(const Brain* brain, const Lobe& lobe, std::uint32_t tick,
                       const std::array<std::uint32_t, 2>& suscept_decay) {
    Recording* recording = recording_of(brain);
    if (recording == nullptr) {
        return;
    }
    const std::uint32_t index = lobe_index_of(*brain, lobe);
    if (index + 1 >= recording->lobe_first_numbers.size()) {
        return;
    }
    auto& update = recording->update;
    update.open = true;
    update.lobe = static_cast<std::uint8_t>(index);
    const auto dreaming = dreaming_brains().find(brain);
    update.flags = dreaming != dreaming_brains().end() && dreaming->second ? 1 : 0;
    update.tick = tick;
    update.first_number = recording->lobe_first_numbers[index];
    update.suscept_decay = suscept_decay;
    for (std::size_t set = 0; set < update.sets.size(); ++set) {
        update.sets[set].clear();
        update.payloads[set].clear();
    }
}

void record_eligibility(const Brain* brain, const Dendrite& connection,
                        std::uint8_t signal) {
    Recording* recording = recording_of(brain);
    if (recording == nullptr || !recording->update.open) {
        return;
    }
    const auto place = recording->places.find(&connection);
    if (place == recording->places.end()) {
        return;
    }
    recording->update.sets[0].push_back(place->second.number);
    recording->update.payloads[0].push_back(signal);
}

void end_lobe_update(const Brain* brain) {
    Recording* recording = recording_of(brain);
    if (recording == nullptr || !recording->update.open) {
        return;
    }
    auto& update = recording->update;
    update.open = false;
    // A lobe with no dendrites has nothing to record.
    if (recording->lobe_first_numbers[update.lobe + 1u] == update.first_number) {
        return;
    }
    Writer out(recording->pending);
    out.u8('U');
    out.u8(update.lobe);
    out.u8(update.flags);
    out.varint(update.tick);
    for (std::uint32_t decay : update.suscept_decay) {
        out.varint(decay);
    }
    for (std::size_t set = 0; set < update.sets.size(); ++set) {
        write_set(out, update.sets[set], update.payloads[set], update.first_number);
    }
}

void record_instinct(const Brain* brain, const InstinctRecord& instinct) {
    Recording* recording = recording_of(brain);
    if (recording == nullptr) {
        return;
    }
    recording->current_instinct = recording->instincts++;
    Writer out(recording->pending);
    out.u8('N');
    out.varint(recording->current_instinct);
    for (std::size_t i = 0; i < 3; ++i) {
        out.u8(instinct.lobes[i]);
        out.u8(instinct.neurons[i]);
    }
    out.u8(instinct.decision_neuron);
    out.u8(instinct.chemical);
    out.u8(instinct.concentration);
}

std::uint32_t current_instinct(const Brain* brain) {
    Recording* recording = recording_of(brain);
    return recording == nullptr ? 0 : recording->current_instinct;
}

void set_brain_dreaming(const Brain* brain, bool dreaming) {
    if (dreaming) {
        dreaming_brains()[brain] = true;
    } else {
        dreaming_brains().erase(brain);
    }
}

void forget_brain_recording(const void* key) {
    auto found = recordings().find(key);
    if (found == recordings().end()) {
        return;
    }
    if (found->second.brain != nullptr) {
        recordings_by_brain().erase(found->second.brain);
        dreaming_brains().erase(found->second.brain);
    }
    if (found->second.biochemistry != nullptr) {
        recordings_by_biochemistry().erase(found->second.biochemistry);
    }
    close_recording(found->second);
    recordings().erase(found);
}

} // namespace creatures1::brain
