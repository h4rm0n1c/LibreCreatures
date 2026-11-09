// Not native: the object fields of the load equivalence test (load_dump.hpp).
//
//   LOAD_DUMP_OBJECT_FIELD(class, name, offset, kind, count, stride, proven_by, lc)
//
// A row applies to every object in the object registry whose class is
// `class` or derives from it (the original: its MFC CRuntimeClass chain;
// LibreCreatures: dynamic_cast to objects::<class>).  offset is where the
// original keeps the field in that object; every offset here is one a
// function-level test in tests/equivalence (proven_by) puts LibreCreatures'
// value at and runs the original's code on, with the same result.  Never
// take an offset from a struct name or from the order of Serialize's reads.
//
// lc: the same field in LibreCreatures, as an expression of `x` (the object,
// as objects::<class>), `i` (the index, for count > 1) and `O` (the test's
// friend accessor for protected Object state).  The native
// harness ignores it.

// Object
LOAD_DUMP_OBJECT_FIELD(Object, classifier, 0x4, U32{}, 1, 0, "vehicle, simple_object", x.classifier_base())
LOAD_DUMP_OBJECT_FIELD(Object, bounds_mode, 0x8, U8{}, 1, 0, "simple_object, vehicle", static_cast<unsigned>(x.bounds_mode()))
LOAD_DUMP_OBJECT_FIELD(Object, bounds_flags, 0x9, U8{}, 1, 0, "vehicle, simple_object", x.script_bounds_flags())
LOAD_DUMP_OBJECT_FIELD(Object, movement_bounds, 0xc, Rect{}, 1, 0, "vehicle, simple_object", x.movement_bounds())
LOAD_DUMP_OBJECT_FIELD(Object, bounds_reference, 0x1c, Ptr{}, 1, 0, "simple_object, vehicle", x.bounds_reference_object())
LOAD_DUMP_OBJECT_FIELD(Object, caos_pointer, 0x20, Ptr{}, 1, 0, "vehicle, object", x.caos_object_pointer())
LOAD_DUMP_OBJECT_FIELD(Object, interaction_event, 0x24, U8{}, 1, 0, "vehicle, goal", x.current_interaction_event_id())
LOAD_DUMP_OBJECT_FIELD(Object, sound_handle, 0x28, I32{}, 1, 0, "object", x.continuous_sound_handle())
LOAD_DUMP_OBJECT_FIELD(Object, sound_id, 0x2c, U32{}, 1, 0, "object", x.continuous_sound_descriptor())
LOAD_DUMP_OBJECT_FIELD(Object, sound_persist, 0x30, U32{}, 1, 0, "object", x.continuous_sound_persists() ? 1u : 0u)
LOAD_DUMP_OBJECT_FIELD(Object, object_variable0, 0x34, U32{}, 1, 0, "vehicle, rvalue", x.object_variable(0))
LOAD_DUMP_OBJECT_FIELD(Object, object_variable1, 0x38, U32{}, 1, 0, "rvalue", x.object_variable(1))
LOAD_DUMP_OBJECT_FIELD(Object, object_variable2, 0x3c, U32{}, 1, 0, "rvalue", x.object_variable(2))
LOAD_DUMP_OBJECT_FIELD(Object, gallery, 0x40, Ptr{}, 1, 0, "object", x.gallery())
LOAD_DUMP_OBJECT_FIELD(Object, timer_period, 0x44, I32{}, 1, 0, "vehicle", O::timer_period(x))
LOAD_DUMP_OBJECT_FIELD(Object, timer_countdown, 0x48, I32{}, 1, 0, "vehicle", x.timer_countdown_for_script())
LOAD_DUMP_OBJECT_FIELD(Object, tick_enabled, 0x4c, U8{}, 1, 0, "object", x.tick_enabled() ? 1u : 0u)

// SimpleObject
LOAD_DUMP_OBJECT_FIELD(SimpleObject, entity, 0x50, Ptr{}, 1, 0, "simple_object, vehicle", x.entity())
LOAD_DUMP_OBJECT_FIELD(SimpleObject, saved_plane, 0x54, I32{}, 1, 0, "simple_object", x.saved_entity_render_plane)
LOAD_DUMP_OBJECT_FIELD(SimpleObject, click_selector, 0x58, U8{}, 3, 1, "object", static_cast<std::uint8_t>(x.click_event_for_interaction_state[i]))
LOAD_DUMP_OBJECT_FIELD(SimpleObject, interaction_flags, 0x5b, U8{}, 1, 0, "vehicle", x.interaction_event_flags)

// CallButton
LOAD_DUMP_OBJECT_FIELD(CallButton, lift, 0x5c, Ptr{}, 1, 0, "vehicle", x.lift())
LOAD_DUMP_OBJECT_FIELD(CallButton, floor, 0x60, U8{}, 1, 0, "vehicle", x.floor_index)

// PointerTool
LOAD_DUMP_OBJECT_FIELD(PointerTool, bubble, 0x64, Ptr{}, 1, 0, "pointer_tool", x.persistent_bubble)
LOAD_DUMP_OBJECT_FIELD(PointerTool, text, 0x68, Text{25}, 1, 0, "pointer_tool", x.text_buffer.data())

// CompoundObject
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_count, 0x50, I32{}, 1, 0, "vehicle, simple_object", x.part_count())
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_entity, 0x54, Ptr{}, 10, 0xc, "vehicle, simple_object", x.part(i).entity.get())
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_x, 0x58, I32{}, 10, 0xc, "vehicle, simple_object", x.part(i).local_x_offset)
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_y, 0x5c, I32{}, 10, 0xc, "vehicle", x.part(i).local_y_offset)
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_bounds, 0xcc, Rect{}, 6, 16, "vehicle", x.part_bounds(i))
LOAD_DUMP_OBJECT_FIELD(CompoundObject, event_config, 0x12c, I32{}, 6, 4, "vehicle", x.creature_event_config.event_config_value[i])

// Blackboard
LOAD_DUMP_OBJECT_FIELD(Blackboard, word, 0x144, Text{11}, 16, 11, "blackboard", x.word(i).text.data())
LOAD_DUMP_OBJECT_FIELD(Blackboard, word_value, 0x1f4, U32{}, 16, 4, "blackboard", x.word_value(i))
LOAD_DUMP_OBJECT_FIELD(Blackboard, fill_palette, 0x234, U8{}, 1, 0, "blackboard", x.display_config().fill_palette_index)
LOAD_DUMP_OBJECT_FIELD(Blackboard, text_config1, 0x235, U8{}, 1, 0, "blackboard", x.display_config().text_render_config_1)
LOAD_DUMP_OBJECT_FIELD(Blackboard, text_config2, 0x236, U8{}, 1, 0, "blackboard", x.display_config().text_render_config_2)
LOAD_DUMP_OBJECT_FIELD(Blackboard, tile_x, 0x237, U8{}, 1, 0, "blackboard", x.display_config().tile_x)
LOAD_DUMP_OBJECT_FIELD(Blackboard, tile_y, 0x238, U8{}, 1, 0, "blackboard", x.display_config().tile_y)
LOAD_DUMP_OBJECT_FIELD(Blackboard, edit_mode, 0x239, U8{}, 1, 0, "blackboard", x.edit_mode_enabled() ? 1u : 0u)

// Vehicle
LOAD_DUMP_OBJECT_FIELD(Vehicle, velocity_x, 0x144, I32{}, 1, 0, "vehicle", x.velocity_x_8_8)
LOAD_DUMP_OBJECT_FIELD(Vehicle, velocity_y, 0x148, I32{}, 1, 0, "vehicle", x.velocity_y_8_8)
LOAD_DUMP_OBJECT_FIELD(Vehicle, position_x, 0x14c, I32{}, 1, 0, "vehicle", x.position_x_8_8)
LOAD_DUMP_OBJECT_FIELD(Vehicle, position_y, 0x150, I32{}, 1, 0, "vehicle", x.position_y_8_8)
LOAD_DUMP_OBJECT_FIELD(Vehicle, creature_event_bounds, 0x154, Rect{}, 1, 0, "vehicle, simple_object", x.creature_event_bounds_local)
LOAD_DUMP_OBJECT_FIELD(Vehicle, collision_sides, 0x164, U32{}, 1, 0, "vehicle", static_cast<unsigned>(x.collision_side_flags))

// Lift
LOAD_DUMP_OBJECT_FIELD(Lift, floor_y, 0x168, I32{}, 8, 4, "vehicle", x.floor_y_by_index[i])
LOAD_DUMP_OBJECT_FIELD(Lift, floor_count, 0x188, I32{}, 1, 0, "vehicle", x.floor_count)
LOAD_DUMP_OBJECT_FIELD(Lift, current_floor, 0x18c, I32{}, 1, 0, "vehicle", x.current_floor_index)
LOAD_DUMP_OBJECT_FIELD(Lift, call_button, 0x190, Ptr{}, 8, 4, "vehicle", x.call_button_refs[i])
LOAD_DUMP_OBJECT_FIELD(Lift, selected_call_button, 0x1b0, I32{}, 1, 0, "vehicle", x.selected_call_button_index)
LOAD_DUMP_OBJECT_FIELD(Lift, call_cooldown, 0x1b4, U8{}, 1, 0, "vehicle", x.call_selection_cooldown)
