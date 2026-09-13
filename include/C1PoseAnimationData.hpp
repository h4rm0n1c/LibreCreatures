#pragma once

// Exact source-side view of the pose data recovered in Ghidra from the C1
// executable.  The interaction table is a four-byte record because the
// native code loads the two pose characters as one little-endian word and
// preserves the two trailing zero bytes.
#pragma pack(push, 1)
struct C1InteractionPosePair {
    char primary_pose_digit;
    char secondary_pose_digit;
    unsigned char padding[2];
};
#pragma pack(pop)

static_assert(sizeof(C1InteractionPosePair) == 4,
              "C1 interaction pose pairs must remain four bytes");

// 0x0045a9fc: exact SIMD fill source used before the bounded pose copy.
static const char kC1PoseUnknownFill[16] = {
    'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X',
    'X', 'X', 'X', 'X', 'X', 'X', 'X', '\0',
};

// 0x00454690: [vertical_bin][distance_bin].
extern char g_motion_pose_digit_by_vertical_bin_and_distance_bin[10][4];

// 0x0045abc8: the complete 22-record source table.  AdvancePoseAnimation
// consumes the indexed records beginning at logical record 2; other C1
// pose code owns the preceding entries in the same table.
extern C1InteractionPosePair g_interaction_pose_pairs_ascii_0_to_5[22];
