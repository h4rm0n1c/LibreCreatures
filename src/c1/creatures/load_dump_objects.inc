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
LOAD_DUMP_OBJECT_FIELD(Object, interaction_event, 0x24, U8{}, 1, 0, "vehicle, goal", x.current_interaction_event_id())
LOAD_DUMP_OBJECT_FIELD(Object, object_variable0, 0x34, U32{}, 1, 0, "vehicle", x.object_variable(0))
LOAD_DUMP_OBJECT_FIELD(Object, timer_period, 0x44, I32{}, 1, 0, "vehicle", O::timer_period(x))
LOAD_DUMP_OBJECT_FIELD(Object, timer_countdown, 0x48, I32{}, 1, 0, "vehicle", x.timer_countdown_for_script())

// SimpleObject
LOAD_DUMP_OBJECT_FIELD(SimpleObject, entity, 0x50, Ptr{}, 1, 0, "simple_object, vehicle", x.entity())
LOAD_DUMP_OBJECT_FIELD(SimpleObject, saved_plane, 0x54, I32{}, 1, 0, "simple_object", x.saved_entity_render_plane)
LOAD_DUMP_OBJECT_FIELD(SimpleObject, interaction_flags, 0x5b, U8{}, 1, 0, "vehicle", x.interaction_event_flags)

// CallButton
LOAD_DUMP_OBJECT_FIELD(CallButton, lift, 0x5c, Ptr{}, 1, 0, "vehicle", x.lift())
LOAD_DUMP_OBJECT_FIELD(CallButton, floor, 0x60, U8{}, 1, 0, "vehicle", x.floor_index)

// CompoundObject
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_count, 0x50, I32{}, 1, 0, "vehicle, simple_object", x.part_count())
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_entity, 0x54, Ptr{}, 10, 0xc, "vehicle, simple_object", x.part(i).entity.get())
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_x, 0x58, I32{}, 10, 0xc, "vehicle, simple_object", x.part(i).local_x_offset)
LOAD_DUMP_OBJECT_FIELD(CompoundObject, part_y, 0x5c, I32{}, 10, 0xc, "vehicle", x.part(i).local_y_offset)
LOAD_DUMP_OBJECT_FIELD(CompoundObject, event_config, 0x12c, I32{}, 3, 4, "vehicle", x.creature_event_config.event_config_value[i])

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
