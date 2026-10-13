#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace creatures1::brain {

class Brain;

// Not native.  A lab tool that records creatures' brains while the world
// runs, for offline study (scripts/brain_record.py reads the files).  Off
// unless C1_BRAIN_RECORD names a folder.  C1_BRAIN_RECORD_MONIKERS limits it
// to some creatures: genome monikers in hex as the lab trace prints them,
// comma separated (all creatures when it is unset).
//
// One file per creature brain, <moniker>-<part>.c1brain, little-endian:
//
//   header   "C1BR", u32 version (10), u32 moniker, u32 lobe count, then per
//            lobe: u32 neuron count, u8 grid width, u8 grid height,
//            u8 rule 0 target lobe, u8 rule 1 target lobe, u8 perception
//            copy (0 none, 1 copied, 2 copied mutually exclusive), and per
//            neuron: u8 rule 0 dendrite count, u8 rule 1 dendrite count.
//            The perception lobe (lobe 0) holds the copied lobes' neurons
//            one after another, in lobe order.
//   chunks   the rest of the file: u32 raw size, u32 compressed size, then
//            a zlib stream holding whole records; one chunk per 32 frames.
//   records  'G' (format 8) comes first in each part: u32 length, then the
//            creature's genome file (.gen) as it was when the part opened,
//            or length 0 if it could not be read.  The other records each
//            start with a u8 type; 'C', 'K', 'D' and 'B' then a u32 world
//            tick:
//     'C'  context: i32 x, i32 y (the creature's foot), u8 chosen action,
//          i8 attention slot (-1 none), u32 attended object's classifier,
//          u8 flags (1 asleep, 2 dreaming: instincts waiting to be
//          processed; bits 2..4 the life stage and bits 5..6 the sex,
//          1 male 2 female, format 8).
//     'I'  inputs, right after each 'C' (format 7): the creature's 16 drive
//          levels, then each lobe's four chemical inputs (chem 0..3) in
//          lobe order -- 16 + 4 x lobe count values in all.  u8 0 then
//          every value as u8 (on a keyframe), or u8 1, a varint count of
//          changed values, then per value a varint gap from the previous
//          one's number + 1 and the u8 value.
//     'K'  keyframe: every neuron (u8 activation, u8 firing strength) in lobe
//          order, then every dendrite (u16 target neuron index, 0xffff when
//          none; u8 current weight, target weight, baseline weight, state)
//          in lobe, neuron, rule, dendrite order.
//     'D'  delta from the previous frame.  Neurons, then dendrites, each
//          numbered in keyframe order: a varint count of changed entries,
//          then per entry a varint gap from the previous entry's number + 1
//          (the first counts from 0), a u8 mask of the fields that changed
//          and those fields in keyframe order.  Neuron mask: 1 activation,
//          2 firing strength.  Dendrite mask: 1 target (u16), 2 current
//          weight, 4 target weight, 8 baseline weight, 16 state.  Varints
//          are unsigned LEB128.  A dendrite change that is only a fall in
//          current weight is written every 8th frame (the fall so far), not
//          in each frame: those are most of a brain's changes.
//
//     'E'  event: something the brain code did to one dendrite, written when
//          it happens, so events come between frames in the order they
//          happened.  u8 kind (DendriteEvent, low 7 bits; 0x80 set while
//          dreaming), varint dendrite number (keyframe order), then only
//          what that kind changed:
//            reinforce, relax         u8 STW
//            ltw step                 u8 LTW
//            strengthen, weaken       u8 strength
//            attach, rescue, migrate,
//            migrate failed           u16 source (0xffff none), u8 STW,
//                                     u8 LTW, u8 strength
//            went loose               nothing: current weight, STW, LTW and
//                                     strength become 0
//            dream LTW raise          nothing: LTW becomes STW
//            reset                    nothing: current weight becomes 0 and
//                                     STW becomes LTW
//          A reader knows each dendrite's state before the event from the
//          last keyframe, the deltas and the events since.  Events start
//          after the part's first keyframe.  (Format 5 wrote u8 kind, u8
//          flags, the number, then the whole dendrite before and after.)
//     'S'  stimulus (format 9): written when a stimulus reaches the creature,
//          between frames like 'E'.  varint stimulus number (counts from 0
//          in each part), u8 built-in stimulus (0..35; 255 a scripted
//          `stim`), u32 source object (its CAOS handle), u32 its classifier,
//          i32 x, i32 y (where it was), u8 flags (1 the source is this
//          creature, 2 ignored: asleep and the stimulus does not wake,
//          4 halved: asleep, 8 fed the general sense neuron), u8 attention
//          record (255 none), u8 general sense neuron, u8 stimulus-source
//          activation, u8 general sense activation, then 4 x (u8 chemical,
//          u8 amount) as applied.  The chemicals' own changes are 'F' flows
//          with origin stimulus and this number.
//     'B'  chemistry tables (format 9), before a frame's 'C' whenever they
//          differ from the last ones written (always in a part's first
//          frame): the creature's biochemistry as loaded for its life stage.
//          u8 receptor count, per receptor u8 domain, tissue, locus,
//          chemical, threshold, nominal, gain, flags; u8 emitter count, per
//          emitter u8 domain, tissue, locus, chemical, threshold, period,
//          amount, flags; u8 reaction count, per reaction u8 amount and
//          chemical of reactant 1, reactant 2, product 1, product 2, then
//          u8 rate.  'F' flows number emitters and reactions by their place
//          in these tables.
//     'X'  chemicals (format 9), after 'I': the creature's 256 chemical
//          concentrations, in the 'I' record's layout (u8 0 then every
//          value on a keyframe, else u8 1 and the changed values).
//     'F'  chemical flows (format 9), after 'X': every change to a chemical
//          since the previous frame, summed by origin.  varint count, then
//          per flow u8 origin (ChemicalOrigin), varint origin number (the
//          emitter or reaction gene's index among its kind, the stimulus
//          number, else 0), u8 chemical and a zigzag varint change (the
//          change actually made, after saturation).  Each chemical's change
//          in 'X' is the sum of its flows; brain_record.py checks that.  A
//          reaction's reactants show as falls and its products as rises.
//     'W'  world (format 9), after 'F': i32 x, i32 y of the hand, u32 the
//          attended object's handle (0 none), i32 x, i32 y of its centre.
//     'V'  votes (format 10), after 'W': how the winner-take-all lobes
//          (decision, attention) summed their inputs in the last update
//          before this frame.  u8 lobe count, then per lobe: u8 lobe index,
//          per neuron u8 type 0 sum and u8 type 1 sum (after clamping and
//          the lobe's input gain: the SVRule's type0 and type1), then a
//          varint count of contributing dendrites and per dendrite a varint
//          gap in keyframe order (as the 'D' record) and u8 contribution,
//          (STW x source firing) >> 8, before clamping and gain.
//     'M'  goal-direction matrix (format 10), after 'V': for each of the 40
//          attention records, 16 drive weights (how much that kind of
//          object has relieved each drive; the creature's second learner,
//          outside the brain).  u8 0 then 640 zigzag varints, or u8 1, a
//          varint count of changed weights, then per weight a varint gap
//          and the zigzag varint value.
//     'L'  learned words (format 10), after 'M', only when one changed: u8
//          count, then per changed record u8 index, u8 length and the heard
//          word, u8 length and the word it says back, u32 reinforcement.
//     'N'  instinct (format 10), between frames, when the creature starts
//          dreaming one: varint instinct number (counts from 0 in each
//          part), 3 x (u8 lobe, u8 neuron) it replays, u8 decision neuron,
//          u8 chemical, u8 concentration.  The instinct's chemical flows
//          have origin instinct and this number, and its dendrite events
//          (dreaming) come after this record and before the next.
//
// A frame is a context record followed by a keyframe or a delta.  A
// keyframe comes first and then every 256 frames.  A brain whose layout
// changes (a new genome loaded) starts a new part.  Every change to a
// dendrite's source, STW, LTW or strength in a delta is the result of the
// events since the last frame; brain_record.py checks that.
struct BrainRecordContext {
    std::uint32_t world_tick = 0;
    std::uint32_t moniker = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint8_t action = 0;
    std::int8_t attention_slot = -1;
    std::uint32_t attention_classifier = 0;
    std::uint8_t flags = 0;  // 1 asleep, 2 dreaming, life stage << 2, sex << 5
    std::array<std::uint8_t, 16> drives{};  // the creature's drive levels
    // The creature's biochemistry (identifies its chemical flows; null when
    // it has none) and its 256 concentrations.
    const void* biochemistry = nullptr;
    std::array<std::uint8_t, 256> chemicals{};
    // The chemistry tables, laid out as the 'B' record's body.
    std::vector<std::uint8_t> chemistry;
    // The goal-direction matrix, 40 x 16 in attention record order.
    std::vector<std::int32_t> goal_matrix;
    // Learned words: per record its heard word, said word, reinforcement.
    struct Word {
        std::string heard;
        std::string said;
        std::uint32_t reinforcement = 0;
        bool operator==(const Word& other) const {
            return heard == other.heard && said == other.said &&
                   reinforcement == other.reinforcement;
        }
    };
    std::vector<Word> words;
    std::int32_t hand_x = 0;
    std::int32_t hand_y = 0;
    std::uint32_t attended_object = 0;
    std::int32_t attended_x = 0;
    std::int32_t attended_y = 0;
    // Reads the creature's genome file; called once when a part opens.
    std::function<std::vector<std::uint8_t>()> load_genome;
};

// What the brain code did to a dendrite (the 'E' record's kind).
enum class DendriteEvent : std::uint8_t {
    reinforce = 1,       // STW raised by the rule's reinforcement signal
    relax = 2,           // STW moved back towards LTW
    ltw_step = 3,        // LTW stepped one towards STW
    strengthen = 4,      // strength raised by the growth rule
    weaken = 5,          // strength decayed by one, still attached
    went_loose = 6,      // strength decayed to 0; weights zeroed
    attach = 7,          // a loose dendrite re-attached to a new source
    rescue = 8,          // a loose dendrite re-attached to its old source
    migrate = 9,         // migration gave the dendrite a source and weights
    migrate_failed = 10, // migration found no set; dendrite zeroed
    dream_ltw_raise = 11,// after a dream, LTW raised to STW
    reset = 12,          // activity reset: current weight 0, STW back to LTW
};

// Where a change to a chemical came from (the 'F' record's origin).
enum class ChemicalOrigin : std::uint8_t {
    unknown = 0,    // a write with no origin set
    emitter = 1,    // number: the emitter gene's index
    reaction = 2,   // number: the reaction gene's index
    decay = 3,      // half-life decay
    stimulus = 4,   // number: the 'S' record's stimulus number
    script = 5,     // CAOS `chem` (scripts, kits)
    bacterium = 6,  // a bacterium's intake or output
    instinct = 7,   // a dream step setting an instinct's chemical
};

// What a stimulus did on arrival (the 'S' record).
struct StimulusRecord {
    std::uint8_t built_in = 0xff;
    std::uint32_t source_object = 0;
    std::uint32_t source_classifier = 0;
    std::int32_t source_x = 0;
    std::int32_t source_y = 0;
    std::uint8_t flags = 0;  // 1 self, 2 ignored asleep, 4 halved, 8 fed sense
    std::uint8_t attention_record = 0xff;
    std::uint8_t sense_neuron = 0xff;
    std::uint8_t source_activation = 0;
    std::uint8_t sense_activation = 0;
    std::array<std::uint8_t, 4> chemicals{};
    std::array<std::uint8_t, 4> amounts{};
};

struct Dendrite;

bool brain_recording_enabled();

// True once any brain is being recorded; the brain code tests this before
// copying a dendrite for an event.
extern bool g_brain_events_wanted;

// Records an event for `connection`, a dendrite of `brain`, whose fields
// were `before` and are now as `connection` holds them.  Nothing happens
// unless `brain` is being recorded.
void record_dendrite_event(const Brain* brain, const Dendrite& connection,
                           DendriteEvent kind, const Dendrite& before);

// True once any creature's chemicals are being recorded; biochemistry tests
// this before working out a change for record_chemical_flow.
extern bool g_chemical_flows_wanted;

// Records that `chemical` of `biochemistry` changed by `change` because of
// `origin` (number: see ChemicalOrigin).  Nothing happens unless that
// creature is being recorded.
void record_chemical_flow(const void* biochemistry, ChemicalOrigin origin,
                          std::uint32_t number, std::uint8_t chemical,
                          int change);

// The origin of chemical changes made while one is alive: writes that do not
// name their own origin (Biochemistry::add_chemical_moles) take it.
class ChemicalOriginScope {
public:
    ChemicalOriginScope(ChemicalOrigin origin, std::uint32_t number);
    ~ChemicalOriginScope();
    ChemicalOriginScope(const ChemicalOriginScope&) = delete;
    ChemicalOriginScope& operator=(const ChemicalOriginScope&) = delete;

    static ChemicalOrigin current_origin();
    static std::uint32_t current_number();

private:
    ChemicalOrigin saved_origin_;
    std::uint32_t saved_number_;
};

// True once any brain is being recorded; lobes test this before reporting
// their votes.
extern bool g_brain_votes_wanted;

class Lobe;

// A winner-take-all lobe of `brain` is summing its inputs: forget the votes
// of its last update.
void begin_lobe_votes(const Brain* brain, const Lobe& lobe);

// One dendrite's contribution to its neuron's input sum.
void record_vote(const Brain* brain, const Dendrite& connection,
                 std::uint8_t contribution);

// A neuron's two input sums (the SVRule's type0 and type1).
void record_neuron_sums(const Brain* brain, const Lobe& lobe,
                        std::uint32_t neuron, std::uint8_t type0,
                        std::uint8_t type1);

// What an instinct replays (the 'N' record).
struct InstinctRecord {
    std::array<std::uint8_t, 3> lobes{};
    std::array<std::uint8_t, 3> neurons{};
    std::uint8_t decision_neuron = 0;
    std::uint8_t chemical = 0;
    std::uint8_t concentration = 0;
};

// Records that `brain`'s creature started dreaming an instinct.
void record_instinct(const Brain* brain, const InstinctRecord& instinct);

// The number of the instinct `brain` is dreaming (the last 'N'), for its
// chemical flows.
std::uint32_t current_instinct(const Brain* brain);

// Records a stimulus reaching `brain`'s creature and returns its number for
// the chemicals' flows (0 when the creature is not recorded).
std::uint32_t record_stimulus(const Brain* brain, const StimulusRecord& stimulus);

// Marks the events of `brain` as made while it dreams (instinct processing).
void set_brain_dreaming(const Brain* brain, bool dreaming);

// Records one frame of `brain`, after its update.  `key` identifies the
// creature for the life of the recording.
void record_brain_frame(const void* key, const Brain& brain,
                        const BrainRecordContext& context);

// The creature is gone: close its file.
void forget_brain_recording(const void* key);

} // namespace creatures1::brain
