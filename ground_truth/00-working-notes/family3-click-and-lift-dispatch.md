# Family-3 click mapping and Lift dispatch — 2026-09-15

The shared failure across teleporter, cannon, and Lift controls was the
CompoundObject click-hotspot mapping. Native `Macro::ExecuteInterpreter`
`knob` at `0041e2df` writes six contiguous words at CompoundObject `+0x12c`.
Entries 0..2 are creature-event bounds; entries 3..5 are the three hand
hotspots. The clean port represented those as a six-entry creature mapping
plus a separate three-entry click mapping, but only updated the former.
Consequently every family-3 click remained unconfigured and returned the
invalid event id.

`CompoundObject::set_knob_function` now keeps both typed views synchronized,
and deserialization rebuilds the click view from serialized entries 3..5.
The macro host routes through that object-owned mutation.

The native Lift vtable at `0045760c` also overrides slots 5 and 6 with
`RequestMoveUp` (`0042c5c0`) and `RequestMoveDown` (`0042c680`). The event
runtime now selects Lift before the generic CompoundObject branch for queued
events 0 and 1. Lift slot 7 is inherited `Object::HandleQueuedEvent3`, so
queued event 2 is routed to that generic handler instead of the compound
event-2 interaction path.

Verification:

- pinned clean-source build: 139 translation units, compile and link pass;
- classifier fallback regression: ASan/UBSan pass;
- renderer image-boundary regression: ASan/UBSan pass;
- Windows gameplay click verification is still pending.
