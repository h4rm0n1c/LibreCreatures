#include "../../../include/C1BiochemistrySelectorData.hpp"
#include "../../../include/C1GoalDirectionActionCandidateScoreTable.hpp"
#include "../../../include/C1PoseAnimationData.hpp"

// These are source-native definitions of data objects whose extents and
// bytes were read from the live C1 image.  They are deliberately kept out of
// generated/decompiled output: the clean owners consume them as ordinary
// project data, while the addresses remain Ghidra evidence rather than C++
// identifiers.

unsigned int g_biochemistry_tick_selector_masks[32] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000001, 0x00000003, 0x00000007, 0x0000000f,
    0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
    0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
    0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
    0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
    0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
};

unsigned int g_biochemistry_tick_selector_q16_multipliers[32] = {
    0x00000000, 0x000032a5, 0x000071dd, 0x0000aabb,
    0x0000d110, 0x0000e758, 0x0000f35c, 0x0000f999,
    0x0000f999, 0x0000f999, 0x0000f999, 0x0000f999,
    0x0000f999, 0x0000f999, 0x0000f999, 0x0000f999,
    0x0000f999, 0x0000f999, 0x0000f999, 0x0000f999,
    0x0000f999, 0x0000f999, 0x0000f999, 0x0000f999,
    0x0000f999, 0x0000f999, 0x0000f999, 0x0000f999,
    0x0000f999, 0x0000f999, 0x0000f999, 0x0000ffff,
};

unsigned int g_goal_direction_drive_scale_factors[16] = {
    0xff, 0x78, 0xff, 0xff, 0xff, 0xdc, 0xc8, 0xb4,
    0xc8, 0xff, 0x64, 0xb4, 0xff, 0xff, 0xff, 0xff,
};

char g_motion_pose_digit_by_vertical_bin_and_distance_bin[10][4] = {
    {'3', '3', '3', '3'}, {'3', '3', '3', '2'},
    {'3', '3', '2', '2'}, {'3', '2', '2', '2'},
    {'2', '2', '2', '2'}, {'2', '2', '2', '2'},
    {'1', '2', '2', '2'}, {'0', '1', '2', '2'},
    {'0', '1', '1', '2'}, {'0', '0', '1', '1'},
};

// 24 rows, as at 0x0045abc8: the pair index reaches ('3' - '0') + 5 * 4 = 23.
C1InteractionPosePair g_interaction_pose_pairs_ascii_0_to_5[24] = {
    {'2', '5', {0, 0}}, {'2', '4', {0, 0}},
    {'2', '1', {0, 0}}, {'2', '4', {0, 0}},
    {'0', '5', {0, 0}}, {'1', '4', {0, 0}},
    {'2', '1', {0, 0}}, {'1', '4', {0, 0}},
    {'3', '5', {0, 0}}, {'3', '4', {0, 0}},
    {'3', '4', {0, 0}}, {'3', '1', {0, 0}},
    {'0', '5', {0, 0}}, {'1', '4', {0, 0}},
    {'1', '4', {0, 0}}, {'3', '1', {0, 0}},
    {'0', '5', {0, 0}}, {'1', '4', {0, 0}},
    {'2', '1', {0, 0}}, {'3', '1', {0, 0}},
    {'0', '5', {0, 0}}, {'1', '4', {0, 0}},
    {'2', '1', {0, 0}}, {'3', '1', {0, 0}},
};

C1GoalDirectionActionCandidateScoreTable
    g_goal_direction_action_candidate_scores = {
        {27, 27, 2, 2, -1, -1},
        {0, 0},
        {1, 2, 1, 4, 10, 11},
        {0, 0, 0, 0, 0, 0},
        {{-9, -9, 0, 0, 0, 0}, {0, 0, -9, 0, 0, 0}},
        {{0, 0, -9, -9, 1, 1},
         {0, 0, 0, 0, 0, 0},
         {-9, 8, -1, -1, 0, 0}},
        {{0, 0, 0, 0, 5, -5}, {0, 0, 0, 0, -5, 5}},
        {0, 9, 5, 0, 1, 1, 9, 0, 5, 0, 1, 1},
        {{0, 2, 1, 0, 0, 0}, {0, 0, 0, 0, 2, 2}},
    };
