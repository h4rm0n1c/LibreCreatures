# Stateful carrot drop height

## Symptom

Picking up a carrot and releasing it over a room left the carrot floating
above the floor, usually in its sideways dropped pose. This was reported
alongside broader concerns about hand interaction rectangles.

## Evidence

The C1 save parser identifies carrot objects as classifier `2,6,3`. Their
installed scripts include:

- `2,6,3,5` (drop): when the pose is carried past the active states, it sets
  `pose 6` and clears `actv`.
- `2,6,3,9` (timer): it advances poses and changes the attributes from `0x40`
  to `0x43` after the dropped state.

Native `SimpleObject::EndInteractionWithSource` at `0x00428bb0` dispatches
`EVENT_5` first, then calls `MoveToAndRedraw` with
`movement_bounds.max_y - gallery[current_image_index].height`. The image
height is therefore read after the drop script has changed the pose.

The port calculated the target Y before dispatching `EVENT_5`, so it used the
pre-drop image height. The final pose was correct, but its bottom edge was at
the wrong Y coordinate.

Native `Object::FindTopmostOverlappingObject` at `0x00425c30` also uses a
one-pixel rectangle at the pointer entity plus the serialized cursor hotspot
offsets. The port had been using the full hand sprite bounds for this special
case, which could select objects outside the actual cursor point.

## Fix

`SimpleObject::end_interaction_with_source` now defers the final image-height
subtraction until after `EVENT_5`. The existing no-room safety fallback is
retained for the port's previously observed bottom-of-world failure.

`WindowsPointerToolRuntimeHost::pointer_tool_bounds` now returns the native
one-pixel hotspot rectangle, with a full-bounds fallback only if the runtime
pointer object cannot expose its entity.

The identical source changes are maintained in both `byte_match` and
`LibreCreatures`.

## Verification

- Parsed the installed `World/World.sfc` and confirmed the live carrot scripts,
  poses, attributes, and room-bound records.
- Confirmed the native instruction/decompiler order at `0x00428bb0`.
- `python3 harness/build_clean_source.py --cache --build-dir /tmp/bm-cache`:
  139/139 source files, resource compilation, and link passed.
- Full interactive Windows gameplay verification remains to be performed
  with a visible carrot in the current viewport.
