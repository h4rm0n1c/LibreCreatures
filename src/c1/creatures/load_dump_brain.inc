// Not native: the brain fields of the load equivalence test (load_dump.hpp),
// for every creature's brain (Creature +0x2c18).
//
//   LOAD_DUMP_LOBE_FIELD(name, offset, kind, count, stride, proven_by, lc)
//   LOAD_DUMP_NEURON_FIELD(name, offset, kind, count, stride, proven_by, lc)
//
// The original: lobe n at brain + 8 + 200 * n, the lobe count at brain +
// 6408; a lobe's neurons at the pointer at lobe +156, 16 bytes each, their
// count at lobe +164 (tests/equivalence/brain_native.hpp).
//
// Where the offsets come from: the brain test builds the original's brain
// at these offsets from LibreCreatures' saved brain (build_native), runs both
// for 1 to 6 ticks over 200,000 cases and compares every field (read_native,
// plus the active and loose-dendrite counts directly), with no difference.
// That proves each offset holds what LibreCreatures' Brain::serialize
// writes there, and the world round trip proves the load reads it back into
// the field the save writes it from.  So pairing these offsets with the
// fields Brain::serialize writes is not circular here, as it would be with
// no behavioural test of the save.  Rows marked "runtime" are not saved:
// the load builds them, which is what this dump checks.
//
// lc: an expression of `l` (the Lobe) or `nr` (the LobeNeuron), `i` and `B`
// (the test's friend accessor for private Lobe state).

LOAD_DUMP_LOBE_FIELD(grid_x, 4, U32{}, 1, 0, "brain", l.grid_x_offset_value())
LOAD_DUMP_LOBE_FIELD(grid_y, 8, U32{}, 1, 0, "brain", l.grid_y_offset_value())
LOAD_DUMP_LOBE_FIELD(width, 12, U32{}, 1, 0, "brain", l.grid_width_value())
LOAD_DUMP_LOBE_FIELD(height, 16, U32{}, 1, 0, "brain", l.grid_height_value())
LOAD_DUMP_LOBE_FIELD(threshold, 20, U8{}, 1, 0, "brain", B::threshold(l))
LOAD_DUMP_LOBE_FIELD(relaxation, 21, U8{}, 1, 0, "brain", B::relaxation(l))
LOAD_DUMP_LOBE_FIELD(baseline, 22, U8{}, 1, 0, "brain", B::baseline(l))
LOAD_DUMP_LOBE_FIELD(gain, 23, U8{}, 1, 0, "brain", B::gain(l))
LOAD_DUMP_LOBE_FIELD(state_rule, 24, Bytes{10}, 1, 0, "brain", B::state_rule(l))
LOAD_DUMP_LOBE_FIELD(winner_take_all, 34, U8{}, 1, 0, "brain", l.winner_take_all_flags_value())
LOAD_DUMP_LOBE_FIELD(rule_source_lobe, 36, U32{}, 2, 60, "brain", l.dendrite_type(i).source_lobe)
LOAD_DUMP_LOBE_FIELD(neuron_count, 164, U32{}, 1, 0, "brain", l.neuron_count_value())
LOAD_DUMP_LOBE_FIELD(connection_count, 168, U32{}, 1, 0, "brain", B::connection_count(l))
LOAD_DUMP_LOBE_FIELD(active_count, 176, U32{}, 1, 0, "brain (runtime; compared directly)", l.active_neuron_count())
LOAD_DUMP_LOBE_FIELD(percept, 180, U32{}, 1, 0, "brain", static_cast<unsigned>(l.perception_copy_state()))
LOAD_DUMP_LOBE_FIELD(migration_candidate, 184, U32{}, 2, 4, "brain (runtime)", B::migration_candidate(l, i))
LOAD_DUMP_LOBE_FIELD(active_fraction, 192, U8{}, 1, 0, "brain", B::active_fraction(l))
LOAD_DUMP_LOBE_FIELD(loose_dendrites, 193, U8{}, 2, 1, "brain (runtime; compared directly)", l.late_phase_state().per_type_loose_dendrite_count[i])
LOAD_DUMP_LOBE_FIELD(token_state, 195, U8{}, 4, 1, "brain", l.late_phase_state().token_evaluation_state[i])

LOAD_DUMP_NEURON_FIELD(grid_x, 0, U8{}, 1, 0, "brain", nr.grid_x)
LOAD_DUMP_NEURON_FIELD(grid_y, 1, U8{}, 1, 0, "brain", nr.grid_y)
LOAD_DUMP_NEURON_FIELD(firing, 2, U8{}, 1, 0, "brain", nr.firing_strength)
LOAD_DUMP_NEURON_FIELD(activation, 3, U8{}, 1, 0, "brain", nr.activation)
LOAD_DUMP_NEURON_FIELD(dendrite_count, 12, U8{}, 2, 1, "brain", i == 0 ? nr.type0_dendrite_count : nr.type1_dendrite_count)
LOAD_DUMP_NEURON_FIELD(excluded, 14, U8{}, 1, 0, "brain", nr.winner_take_all_excluded)
LOAD_DUMP_NEURON_FIELD(source_lobe, 15, U8{}, 1, 0, "brain", nr.source_lobe_index)
