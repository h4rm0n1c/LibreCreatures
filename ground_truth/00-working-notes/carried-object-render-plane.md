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

## Not verified end to end

The fix is evidenced by the disassembly, the table contents, and the world
file's part counts -- not by watching an egg. Driving an egg into the incubator
from the CAOS pipe needs the *incubator* to be the event source, and
`mesg writ`'s source is the script owner, so the pipe attaches the egg to the
selected creature instead. Dragging an egg in through the UI is the direct
check.

Related: [limb-render-plane-offsets.md](limb-render-plane-offsets.md), the other
z-ordering defect found in the same pass.
