#include "brain_recorder.hpp"

#include "brain.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
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

constexpr std::uint32_t kFormatVersion = 4;
// Frames are buffered and written as one compressed chunk this often, so a
// game that is killed loses at most this many frames.
constexpr std::uint32_t kFramesPerChunk = 32;
constexpr int kCompressionQuality = 9;
// Most dendrite changes are the decision lobe's current weights relaxing a
// little each tick.  A change that is only a fall in current weight is
// written every this many frames (accumulated); every other change is
// written in the frame it happens.
constexpr std::uint32_t kDecayFrameInterval = 8;
constexpr std::uint32_t kKeyframeInterval = 256;
constexpr std::uint16_t kNoTarget = 0xffff;

struct DendriteState {
    std::uint16_t target = kNoTarget;
    std::uint8_t current_weight = 0;
    std::uint8_t target_weight = 0;
    std::uint8_t baseline_weight = 0;
    std::uint8_t state = 0;

    bool operator==(const DendriteState& other) const {
        return target == other.target &&
               current_weight == other.current_weight &&
               target_weight == other.target_weight &&
               baseline_weight == other.baseline_weight &&
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

private:
    std::vector<std::uint8_t>& out_;
};

struct Recording {
    std::FILE* file = nullptr;
    std::uint32_t moniker = 0;
    std::uint32_t part = 0;
    std::uint32_t frames = 0;
    Snapshot previous;
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

std::map<std::uint32_t, std::uint32_t>& parts_by_moniker() {
    static std::map<std::uint32_t, std::uint32_t> value;
    return value;
}

std::uint16_t target_index(const Brain& brain, const LobeConnectionRule& rule,
                           const LobeConnection& connection) {
    if (connection.target_neuron == nullptr ||
        rule.target_lobe_index >= brain.lobe_count()) {
        return kNoTarget;
    }
    const Lobe& target_lobe = brain.lobe(rule.target_lobe_index);
    if (target_lobe.neuron_count_value() == 0) {
        return kNoTarget;
    }
    const LobeNeuron* first = &target_lobe.neuron(0);
    const std::ptrdiff_t offset = connection.target_neuron - first;
    if (offset < 0 ||
        offset >= static_cast<std::ptrdiff_t>(target_lobe.neuron_count_value()) ||
        offset >= kNoTarget) {
        return kNoTarget;
    }
    return static_cast<std::uint16_t>(offset);
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
                lobe.connection_rule(rule).target_lobe_index));
        }
        layout.push_back(
            static_cast<std::uint8_t>(lobe.perception_copy_state()));
        for (std::uint32_t index = 0; index < lobe.neuron_count_value();
             ++index) {
            const LobeNeuron& neuron = lobe.neuron(index);
            layout.push_back(neuron.rule0_connection_count);
            layout.push_back(neuron.rule1_connection_count);
            snapshot.neurons.push_back(
                {neuron.activation, neuron.firing_strength});
            for (std::uint8_t rule = 0; rule < 2; ++rule) {
                const LobeConnection* connections =
                    rule == 0 ? neuron.rule0_connections_begin
                              : neuron.rule1_connections_begin;
                const std::uint8_t count = rule == 0
                                               ? neuron.rule0_connection_count
                                               : neuron.rule1_connection_count;
                for (std::uint8_t dendrite = 0;
                     connections != nullptr && dendrite < count; ++dendrite) {
                    const LobeConnection& connection = connections[dendrite];
                    snapshot.dendrites.push_back(
                        {target_index(brain, lobe.connection_rule(rule),
                                      connection),
                         connection.current_weight, connection.target_weight,
                         connection.baseline_weight,
                         connection.dendrite_state});
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

// Writes one frame.  A decay held back from a delta is put back in
// `snapshot`, which then says what the file says.
void write_frame(Recording& recording, Snapshot& snapshot,
                 const BrainRecordContext& context) {
    Writer out(recording.pending);
    out.u8('C');
    out.u32(context.world_tick);
    out.u32(static_cast<std::uint32_t>(context.x));
    out.u32(static_cast<std::uint32_t>(context.y));
    out.u8(context.action);
    out.u8(static_cast<std::uint8_t>(context.attention_slot));
    out.u32(context.attention_classifier);

    const bool keyframe = recording.frames % kKeyframeInterval == 0;
    if (keyframe) {
        out.u8('K');
        out.u32(context.world_tick);
        for (const NeuronState& neuron : snapshot.neurons) {
            out.u8(neuron.activation);
            out.u8(neuron.firing);
        }
        for (const DendriteState& dendrite : snapshot.dendrites) {
            out.u16(dendrite.target);
            out.u8(dendrite.current_weight);
            out.u8(dendrite.target_weight);
            out.u8(dendrite.baseline_weight);
            out.u8(dendrite.state);
        }
    } else {
        // Changed entries in index order, each as its gap from the previous
        // one and a mask of the fields that changed, then those fields.
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
        changed.clear();
        const bool decay_frame = recording.frames % kDecayFrameInterval == 0;
        for (std::uint32_t i = 0; i < snapshot.dendrites.size(); ++i) {
            const DendriteState& now = snapshot.dendrites[i];
            const DendriteState& before = recording.previous.dendrites[i];
            if (now == before) {
                continue;
            }
            DendriteState decayed_only = before;
            decayed_only.current_weight = now.current_weight;
            if (!decay_frame && decayed_only == now &&
                now.current_weight < before.current_weight) {
                snapshot.dendrites[i] = before;
                continue;
            }
            changed.push_back(i);
        }
        out.varint(static_cast<std::uint32_t>(changed.size()));
        next = 0;
        for (std::uint32_t i : changed) {
            const DendriteState& now = snapshot.dendrites[i];
            const DendriteState& before = recording.previous.dendrites[i];
            const std::uint8_t mask = static_cast<std::uint8_t>(
                (now.target != before.target ? 1 : 0) |
                (now.current_weight != before.current_weight ? 2 : 0) |
                (now.target_weight != before.target_weight ? 4 : 0) |
                (now.baseline_weight != before.baseline_weight ? 8 : 0) |
                (now.state != before.state ? 16 : 0));
            out.varint(i - next);
            out.u8(mask);
            if (mask & 1) out.u16(now.target);
            if (mask & 2) out.u8(now.current_weight);
            if (mask & 4) out.u8(now.target_weight);
            if (mask & 8) out.u8(now.baseline_weight);
            if (mask & 16) out.u8(now.state);
            next = i + 1;
        }
    }
    ++recording.frames;
    if (recording.frames % kFramesPerChunk == 0) {
        write_chunk(recording);
    }
}

} // namespace

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
        snapshot.layout != recording.previous.layout) {
        recording.moniker = context.moniker;
        if (!open_part(recording, snapshot)) {
            recordings().erase(key);
            return;
        }
    }
    write_frame(recording, snapshot, context);
    recording.previous = std::move(snapshot);
}

void forget_brain_recording(const void* key) {
    auto found = recordings().find(key);
    if (found == recordings().end()) {
        return;
    }
    close_recording(found->second);
    recordings().erase(found);
}

} // namespace creatures1::brain
