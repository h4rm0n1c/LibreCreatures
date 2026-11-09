// Not native: the fields of the load equivalence test (load_dump.hpp).
//
//   LOAD_DUMP_FIELD(name, offset, kind, count, stride, proven_by, lc)
//
// offset: where the original keeps the field in its Creature object (the
// Skeleton and Object it starts with included).  Every offset here is one
// that a function-level test in tests/equivalence (proven_by) already uses:
// that test puts LibreCreatures' value at this offset, runs the original's
// code on it and gets the same result as LibreCreatures, so the offset holds
// that field.  Never take an offset from a struct name or from the order of
// Serialize's reads: pairing the i-th read on each side makes the test
// circular and it can no longer see a swapped field.
//
// lc: the same field in LibreCreatures, as an expression of `c` (the
// Creature), `s` (its Skeleton), `i` (the index, for count > 1) and `P` (the
// test's friend accessor for private state).  The native harness ignores it.
//
// count / stride: an array of `count` values, `stride` bytes apart.

// Object and Skeleton, at the start of the original's Creature
LOAD_DUMP_FIELD(classifier, 0x4, U32{}, 1, 0, "words", s.classifier_base())
LOAD_DUMP_FIELD(bounds_flags, 0x9, U8{}, 1, 0, "skeleton", s.script_bounds_flags())
LOAD_DUMP_FIELD(movement_bounds, 0xc, Rect{}, 1, 0, "skeleton", s.movement_bounds())
LOAD_DUMP_FIELD(bounds_reference, 0x1c, Ptr{}, 1, 0, "goal", s.bounds_reference_object())
LOAD_DUMP_FIELD(genome, 0x50, U32{}, 1, 0, "perception", s.genome_source_filename)
LOAD_DUMP_FIELD(mother, 0x54, U32{}, 1, 0, "perception", s.mother_moniker)
LOAD_DUMP_FIELD(father, 0x58, U32{}, 1, 0, "perception", s.father_moniker)
LOAD_DUMP_FIELD(body, 0x5c, Ptr{}, 1, 0, "skeleton", s.body.get())
LOAD_DUMP_FIELD(limb_chain, 0x60, Ptr{}, 6, 4, "skeleton", s.limb_chain_heads[i])
LOAD_DUMP_FIELD(facing, 0x78, U8{}, 1, 0, "skeleton", static_cast<unsigned>(s.facing_direction))
LOAD_DUMP_FIELD(down_foot, 0x79, U8{}, 1, 0, "skeleton", static_cast<unsigned>(s.down_foot))
LOAD_DUMP_FIELD(foot_x, 0x7c, I32{}, 1, 0, "skeleton", s.down_foot_x)
LOAD_DUMP_FIELD(foot_y, 0x80, I32{}, 1, 0, "skeleton", s.down_foot_y)
LOAD_DUMP_FIELD(normal_plane, 0x84, I32{}, 1, 0, "skelgenes", s.normal_render_plane)
LOAD_DUMP_FIELD(current_pose, 0x88, Text{16}, 1, 0, "skeleton", s.current_pose.characters.data())
LOAD_DUMP_FIELD(target_pose, 0x98, Text{16}, 1, 0, "skeleton", s.target_pose.characters.data())
LOAD_DUMP_FIELD(animation_sequence, 0xa8, Text{32}, 1, 0, "skeleton", s.animation_sequence.data())
LOAD_DUMP_FIELD(animation_cursor, 0xc8, CursorFrom{0xa8}, 1, 0, "skeleton", s.animation_cursor)
LOAD_DUMP_FIELD(chain_end_x, 0xcc, I32{}, 6, 4, "skeleton", s.limb_chain_end_x[i])
LOAD_DUMP_FIELD(chain_end_y, 0xe4, I32{}, 6, 4, "skeleton", s.limb_chain_end_y[i])
LOAD_DUMP_FIELD(sprite_bounds, 0xfc, Rect{}, 1, 0, "skeleton", s.sprite_bounds)
LOAD_DUMP_FIELD(previous_sprite_bounds, 0x10c, Rect{}, 1, 0, "skeleton", s.previous_sprite_bounds)
LOAD_DUMP_FIELD(boundary_correction, 0x11c, Bool{}, 1, 0, "skeleton", s.boundary_correction_pending)
LOAD_DUMP_FIELD(expression, 0x11d, U8{}, 1, 0, "skeleton", s.drive_threshold_state)
LOAD_DUMP_FIELD(eyes_open, 0x11e, Bool{}, 1, 0, "skeleton", s.eyes_open)
LOAD_DUMP_FIELD(asleep, 0x11f, Bool{}, 1, 0, "goal, words", s.sleep_indicator_active)
LOAD_DUMP_FIELD(pose_table, 0x120, Text{16}, 100, 16, "express", s.pose_string_table[i].characters.data())
LOAD_DUMP_FIELD(gait_table, 0x760, Text{18}, 8, 18, "express", s.gait_animation_table[i].data())
LOAD_DUMP_FIELD(motion_link, 0x7f0, Ptr{}, 1, 0, "goal, attention", s.motion_link)
LOAD_DUMP_FIELD(motion_x, 0x7f4, I32{}, 1, 0, "attention", s.motion_target_x)
LOAD_DUMP_FIELD(motion_y, 0x7f8, I32{}, 1, 0, "attention", s.motion_target_y)
LOAD_DUMP_FIELD(motion_part, 0x7fc, I32{}, 1, 0, "attention", s.motion_target_part_index)
LOAD_DUMP_FIELD(pose_transitions, 0x800, U8{}, 1, 0, "skeleton, locus", s.pose_transition_component_count)
LOAD_DUMP_FIELD(genome_moniker, 0x810, CStr{}, 1, 0, "status", c.register_state().history().genome_moniker)
LOAD_DUMP_FIELD(display_name, 0x814, CStr{}, 1, 0, "status", c.register_state().history().display_name)
LOAD_DUMP_FIELD(age, 0x838, U32{}, 1, 0, "status", c.register_state().age_ticks())

// Creature
LOAD_DUMP_FIELD(selected_action, 0x840, U8{}, 1, 0, "action", c.selected_action_id() & 0xffu)
LOAD_DUMP_FIELD(action_boost, 0x841, U8{}, 1, 0, "action", c.action_activation_boost())
LOAD_DUMP_FIELD(active_involuntary, 0x842, U8{}, 1, 0, "action", P::active_involuntary(c))
LOAD_DUMP_FIELD(attention_target, 0x844, Ptr{}, 40, 20, "perception, stimulus", c.attention_records()[i].target)
LOAD_DUMP_FIELD(attention_neuron, 0x848, U8{}, 40, 20, "perception, stimulus", c.attention_records()[i].target_neuron_index)
LOAD_DUMP_FIELD(attention_activation, 0x849, U8{}, 40, 20, "perception, stimulus", c.attention_records()[i].lobe_activation)
LOAD_DUMP_FIELD(attention_x, 0x84c, I32{}, 40, 20, "perception, stimulus", c.attention_records()[i].world_x)
LOAD_DUMP_FIELD(attention_y, 0x850, I32{}, 40, 20, "perception, stimulus", c.attention_records()[i].world_y)
LOAD_DUMP_FIELD(attention_visible, 0x854, Bool{}, 40, 20, "perception, stimulus", c.attention_records()[i].visible)
LOAD_DUMP_FIELD(attention_seen, 0x855, Bool{}, 40, 20, "perception", c.attention_records()[i].stimulus_seen)
LOAD_DUMP_FIELD(word_recognized, 0xb64, Text{25}, 80, 0x38, "words, heard", c.learned_word_record(i).recognized_word)
LOAD_DUMP_FIELD(word_response, 0xb7d, Text{25}, 80, 0x38, "words, heard", c.learned_word_record(i).response_word)
LOAD_DUMP_FIELD(word_reinforcement, 0xb98, U32{}, 80, 0x38, "words, heard", c.learned_word_record(i).reinforcement)
LOAD_DUMP_FIELD(goal_record, 0x1ce4, I32{}, 1, 0, "goal, attention", P::goal(c).attention_record_index)
LOAD_DUMP_FIELD(goal_verb, 0x1ce8, I32{}, 1, 0, "goal, attention", P::goal(c).action_lobe_neuron_index)
LOAD_DUMP_FIELD(goal_commitment, 0x1cec, U8{}, 1, 0, "goal, attention", P::goal(c).commitment)
LOAD_DUMP_FIELD(goal_countdown, 0x1ced, U8{}, 1, 0, "goal, attention", P::goal(c).delivery_countdown)
LOAD_DUMP_FIELD(goal_matrix, 0x1cf0, I32{}, 640, 4, "heard, goal", c.goal_direction_weight_matrix()[i / 16][i % 16])
LOAD_DUMP_FIELD(context_descriptor, 0x26f8, Bytes{4}, 36, 20, "perception, express", &c.built_in_stimulus_contexts()[i].descriptor)
LOAD_DUMP_FIELD(context_chemicals, 0x26fc, Bytes{4}, 36, 20, "perception", &c.built_in_stimulus_contexts()[i].chemical_ids)
LOAD_DUMP_FIELD(context_amounts, 0x2700, Bytes{4}, 36, 20, "perception", &c.built_in_stimulus_contexts()[i].chemical_amounts)
LOAD_DUMP_FIELD(voice_mask, 0x29c4, U32{}, 81, 4, "voice", c.voice().trigram_selection_masks()[i])
LOAD_DUMP_FIELD(voice_sound, 0x2b08, U32{}, 32, 4, "voice", c.voice().sound_ids()[i])
LOAD_DUMP_FIELD(voice_duration, 0x2b88, I32{}, 32, 4, "voice", c.voice().durations()[i])
LOAD_DUMP_FIELD(voice_delay, 0x2c10, I32{}, 1, 0, "voice", c.voice().accumulated_sound_delay_ticks())
LOAD_DUMP_FIELD(sleep_indicator_object, 0x2c14, Ptr{}, 1, 0, "action, attention", P::sleep_indicator(c))
LOAD_DUMP_FIELD(brain, 0x2c18, Ptr{}, 1, 0, "dream, goal", c.brain())
LOAD_DUMP_FIELD(biochemistry, 0x2c1c, Ptr{}, 1, 0, "dream, perception", c.biochemistry())
LOAD_DUMP_FIELD(biochemistry_tick, 0x2c20, U32{}, 1, 0, "dream", c.biochemistry_tick())
LOAD_DUMP_FIELD(control_state, 0x2c24, U8{}, 34, 1, "life, locus", reinterpret_cast<const std::uint8_t*>(&c.control_state())[i])
LOAD_DUMP_FIELD(floating_loci, 0x2c46, U8{}, 8, 1, "locus", c.control_state().floating_loci[i])
LOAD_DUMP_FIELD(gait_loci, 0x2c4e, U8{}, 8, 1, "locus", c.control_state().gait_locus_levels[i])
LOAD_DUMP_FIELD(drives, 0x2c56, U8{}, 16, 1, "goal, words, locus", c.control_state().goal_direction_drive_levels[i])
LOAD_DUMP_FIELD(instinct_count, 0x2cb8, U32{}, 1, 0, "dream, express", c.instinct_runtime_state().instinct_count)
LOAD_DUMP_FIELD(dream_countdown, 0x2cbc, U32{}, 1, 0, "dream, goal", c.instinct_runtime_state().dream_countdown)
LOAD_DUMP_FIELD(life_stage, 0x2cc0, U8{}, 1, 0, "life, dream", static_cast<unsigned>(c.genome_life_stage()))
LOAD_DUMP_FIELD(sex, 0x2cc1, U8{}, 1, 0, "perception", static_cast<unsigned>(c.genome_sex()))
LOAD_DUMP_FIELD(gamete, 0x2cc4, U32{}, 1, 0, "life", c.gamete_genome_source_filename())
LOAD_DUMP_FIELD(child, 0x2cc8, U32{}, 1, 0, "life", c.child_genome_source_filename())
LOAD_DUMP_FIELD(death_state, 0x2ccc, U8{}, 1, 0, "life, goal, locus", c.death_state())
LOAD_DUMP_FIELD(bacterium_state, 0x2cd4, U8{}, 1, 0, "bacterium", static_cast<unsigned>(c.bacterium().activity_state()))
LOAD_DUMP_FIELD(bacterium_input, 0x2cd5, U8{}, 1, 0, "bacterium", static_cast<std::uint8_t>(c.bacterium().input_chemical_id()))
LOAD_DUMP_FIELD(bacterium_kill, 0x2cd6, U8{}, 1, 0, "bacterium", c.bacterium().kill_threshold())
LOAD_DUMP_FIELD(bacterium_activation, 0x2cd7, U8{}, 1, 0, "bacterium", c.bacterium().activation_threshold())
LOAD_DUMP_FIELD(bacterium_output, 0x2cd8, U8{}, 4, 1, "bacterium", static_cast<std::uint8_t>(c.bacterium().output_chemical_ids()[i]))
