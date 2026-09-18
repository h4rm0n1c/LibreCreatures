# A carried object's render plane came from a heap address

Found 2026-09-18, from the report that eggs placed in the incubator draw in
front of its doors.

## The path

An object dropped into a machine gets `handle_queued_event_4`, which sets
`BoundsMode::explicit_rectangle` with the machine as its bounds reference.
From then on `SimpleObject::Tick` routes it to
`update_entity_for_explicit_rect_bounds_and_redraw` every tick, which
repositions it inside the machine and recomputes its plane as the machine's
plane plus an offset.

## The offset was chosen by a pointer

`UpdateEntityForExplicitRectBoundsAndRedraw @0x00428d30`:

```
00428e05  MOV   ECX, dword ptr [EDI + 0x1c]          ; bounds_reference_object
00428e17  MOVZX EAX, byte ptr [ECX + 0x78]           ; low byte of parts[3].entity
00428e1b  MOV   ESI, dword ptr [EAX*0x4 + 0x45abb8]  ; table[that byte]
00428e22  MOV   EAX, dword ptr [ECX]
00428e24  MOV   EAX, dword ptr [EAX + 0x6c]          ; slot 27, GetRenderPlane
00428e27  CALL  EAX
00428e29  ADD   EAX, ESI                             ; plane + offset
```

`+0x78` is `parts[3].entity` -- `parts` is `CompoundPart[10]` at +0x54, twelve
bytes each, so `84 + 3*12 = 120 = 0x78`, and its first field is the Entity
pointer. The table at `0x0045abb8` is exactly four `int32`s, `{-1, 1, 1, -1}`;
the next bytes are unrelated pose-string data (`0x3532`, `0x3432`, ...).

So the index is the **low byte of a heap pointer**, and the read is in bounds
only when that pointer is null. It looks like a missing dereference in the
original source -- some small 0..3 field of `parts[3]` was surely intended.

## Why the incubator is the case that shows it

Parsing `knowngood`'s `World.sfc`: of its compound objects exactly **one** has
`part_count == 4`, and its `parts[3]` is a live Entity at world (1929, 680),
image index 12, plane 2. That object's bounds, `1889,653 - 2025,786`, identify
it as classifier **3.4.1**, the incubator -- whose scripts animate `part 1` as
the door and drive `part 3` through poses 0..9 as the dial.

Every other carrier has three parts or fewer, so `parts[3].entity` is null,
the index is 0, and the offset is `-1`: the carried object draws one plane
behind its carrier. That is the intended behaviour and what the original does
everywhere it is well defined.

The incubator is the one place where the pointer is non-null, and it is exactly
where an egg is placed.

## What the port was doing, and what it does now

The port had translated the quirk literally but masked the index to stay inside
the table:

```cpp
static constexpr int kRenderPlaneOffsets[4] = {-1, 1, 1, -1};
return kRenderPlaneOffsets[reinterpret_cast<std::uintptr_t>(
    reference.part(3).entity.get()) & 3u];
```

For a null pointer that agrees with the native (index 0, `-1`). For the
incubator it picked from `{-1, 1, 1, -1}` by heap address -- **`+1` for about
half of all addresses**, drawing the egg in front of the doors, and varying
between runs.

It now returns `-1` unconditionally, with the derivation in a comment. That is
the table's only defined entry, it matches the native everywhere the native is
defined, and it is the visually correct result.

This is a deliberate deviation, of the kind that cannot be avoided: the
native's behaviour here depends on its own heap addresses, so there is nothing
stable to reproduce. Reproducing the *shape* of the bug faithfully would mean
reproducing an out-of-bounds read, and would still not reproduce its result.

## Creatures in vehicles lost their +/-5 offset

Found from the report that norns in lifts z-order wrongly.

`Creature::HandlePickupEvent @0x004098e0`, the vehicle branch:

```
MOV    EAX, [ECX + 0x54]      ; vehicle parts[0].entity
MOV    EDX, [EAX + 0xc]       ; its render_plane            -- the base
MOV    EAX, [ECX + 0x60]      ; vehicle parts[1].entity
MOV    ECX, 0x5
CMP    EDX, [EAX + 0xc]       ; against parts[1] render_plane
CMOVGE ECX, [ESP + 0x10]      ; -5 when base >= parts[1]
ADD    ECX, EDX
MOV    [EAX + 0xc], ECX       ; body->render_plane = base +/- 5
```

`+0x54` and `+0x60` are `parts[0].entity` and `parts[1].entity` (`parts` is
`CompoundPart[10]` at `+0x54`, twelve bytes each), and `+0xc` on an Entity is
its render plane. Vehicle extends CompoundObject, so both are valid.

The creature is placed five planes off the car's own plane, on whichever side
keeps it between the car and its facing panel. The port's
`vehicle_attachment_render_plane` returned the vehicle's plane **unchanged**,
so a creature in a lift sat at exactly the car's plane -- a tie, which a stable
sort keyed only on plane resolves by entity-registry order. Whether the norn
drew inside the lift or behind it was arbitrary.

## Correction: the carried-object offset is +1, not -1

The first revision of this note concluded the offset should be `-1`, on the
grounds that index 0 is the table's only in-bounds entry. That was wrong, and
it regressed every carried object: cheese dropped into the incubator vanished
*behind* it.

The incubator's own part planes settle it. From its record in a shipped
`World.sfc`:

```
part[0] plane    0   body, and what CompoundObject::GetRenderPlane returns
part[1] plane    1   the door, animated by its event 1/2 scripts
part[2] plane 4000   the front cover
part[3] plane    2   the dial
```

A carried object's plane is therefore `0 + offset`:

  - `-1` puts it behind the body -- invisible. This is what the bad revision did.
  - `+1` puts it in front of the body and far behind the front cover, which is
    where an egg in an incubator belongs.

`+1` is a real entry in the native table (two of its four values are `+1`),
deterministic, and the only one of the two that leaves the object visible. The
lesson recorded: picking a constant from the table's *index* semantics without
checking what the carrier's planes actually are produced a worse bug than the
one being fixed.

Residual ambiguity worth knowing: at plane 1 a carried object ties the door
part, so their relative order falls to registry order. Nothing in the table
offers a value that separates them, and the front cover at 4000 is what
actually occludes the slot, so this is left alone rather than invented.

## Not verified end to end

The fix is evidenced by the disassembly, the table contents, and the world
file's part counts -- not by watching an egg. Driving an egg into the incubator
from the CAOS pipe needs the *incubator* to be the event source, and
`mesg writ`'s source is the script owner, so the pipe attaches the egg to the
selected creature instead. Dragging an egg in through the UI is the direct
check.

Related: [limb-render-plane-offsets.md](limb-render-plane-offsets.md), the other
z-ordering defect found in the same pass.
