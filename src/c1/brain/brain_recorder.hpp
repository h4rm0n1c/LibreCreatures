#pragma once

#include <cstdint>

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
//   header   "C1BR", u32 version (3), u32 moniker, u32 lobe count, then per
//            lobe: u32 neuron count, u8 grid width, u8 grid height,
//            u8 rule 0 target lobe, u8 rule 1 target lobe, and per neuron:
//            u8 rule 0 dendrite count, u8 rule 1 dendrite count.
//   chunks   the rest of the file: u32 raw size, u32 compressed size, then
//            a zlib stream holding whole records; one chunk per 32 frames.
//   records  each starts with a u8 type and a u32 world tick:
//     'C'  context: i32 x, i32 y (the creature's foot), u8 chosen action,
//          i8 attention slot (-1 none), u32 attended object's classifier.
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
// A frame is a context record followed by a keyframe or a delta.  A
// keyframe comes first and then every 256 frames.  A brain whose layout
// changes (a new genome loaded) starts a new part.
struct BrainRecordContext {
    std::uint32_t world_tick = 0;
    std::uint32_t moniker = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint8_t action = 0;
    std::int8_t attention_slot = -1;
    std::uint32_t attention_classifier = 0;
};

bool brain_recording_enabled();

// Records one frame of `brain`, after its update.  `key` identifies the
// creature for the life of the recording.
void record_brain_frame(const void* key, const Brain& brain,
                        const BrainRecordContext& context);

// The creature is gone: close its file.
void forget_brain_recording(const void* key);

} // namespace creatures1::brain
