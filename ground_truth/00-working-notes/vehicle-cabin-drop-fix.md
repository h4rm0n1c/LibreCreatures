# Vehicle cabin drop fault

The Windows document adapter returned the whole world (0,0,8352,1200)
for every vehicle's local cabin and zero for both primary entity coordinates.
The pointer overlap adapter also returned local bounds without translation.
Consequently, release could select a distant vehicle instead of the room,
attach the object to it, and bottom-align at Y=1200 minus image height.
This explains why the previous room-floor changes did not address the report.

Both repositories now read Vehicle::creature_event_bounds_local and the
primary part Entity coordinates. The overlap adapter translates the cabin
to world coordinates; the movement adapter retains local coordinates.
Lift derives from Vehicle and shares these services.

Validation: harness/vehicle_cabin_adapter_test.py compiles the production
adapter method bodies against a geometry fixture, checking distant vehicle
exclusion, cabin floor, moving vehicle coordinates, and empty vehicles.
Passes in both repositories. Pinned Windows compile/resources/link pass.
This is adapter-level coverage, not a full interactive pickup/drop test.

Build: builds/Creatures.vehicle-cabin-drop-fix.exe (workspace root).
Windows gameplay confirmation pending.
