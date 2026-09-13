#pragma once

// Exact source-side view of the two GoalDirection data objects recovered in
// Ghidra at 0045424c and 004542b0. This header is deliberately independent
// of the generated Windows/MFC prelude so clean C1 sources can consume the
// verified data without importing the decompiler bridge.
#pragma pack(push, 1)
typedef signed char C1SignedAttentionRecordSlot;
typedef signed char C1SignedGoalActionLobeNeuronSlot;
typedef signed char C1SignedGoalDirectionCandidateScore;

struct C1GoalDirectionActionCandidateScoreTable {
    C1SignedAttentionRecordSlot attention_record_index_by_candidate[6];
    unsigned char unreferenced_zero_06_07[2];
    C1SignedGoalActionLobeNeuronSlot action_lobe_neuron_index_by_candidate[6];
    unsigned char unreferenced_zero_0e_13[6];
    C1SignedGoalDirectionCandidateScore selected_goal_presence_score_by_candidate[2][6];
    C1SignedGoalDirectionCandidateScore interaction_context_score_by_candidate[3][6];
    C1SignedGoalDirectionCandidateScore goal_horizontal_relation_score_by_candidate[2][6];
    unsigned char unreferenced_score_slice_3e_49[12];
    C1SignedGoalDirectionCandidateScore creature_room_class_score_by_candidate[2][6];
};
#pragma pack(pop)

static_assert(sizeof(C1GoalDirectionActionCandidateScoreTable) == 86,
              "C1 GoalDirection score table must remain 86 bytes");

extern unsigned int g_goal_direction_drive_scale_factors[16];
extern C1GoalDirectionActionCandidateScoreTable
    g_goal_direction_action_candidate_scores;
