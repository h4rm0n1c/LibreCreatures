# Creature limb render planes: the offset table was never populated

Found 2026-09-18 while checking the port's z-ordering against CE.

## The defect

`Skeleton::update_limb_frames_for_pose` read its per-chain render-plane offsets
from a Skeleton member:

```cpp
std::array<std::array<int, kLimbChainCount>, 4>
    pose_chain_render_plane_offsets{};      // declared, zero-initialised
```

Nothing in the tree ever wrote it. A grep of the whole repository returned
exactly two hits: that declaration and the read. So every limb chain started at
offset 0 and stepped +1 per limb down the chain, for every facing.

Consequences, all visible:

  - the **head never came forward**. Its chain should start three planes in
    front of the body; it drew at the body's own plane, where the stable sort
    falls back to entity-registry order.
  - **no limb ever went behind the body**. Facing east or west should push that
    side's far arm and leg one plane back; with a zero offset they drew in
    front, so a creature in profile had its far limbs painted over its body.
  - all six chains occupied the same narrow plane band instead of being
    separated, so which limb won was left to registry order.

## The native table

`Skeleton::UpdateLimbFramesForPose @0x0043b8f0`:

```
MOVZX EAX, byte ptr [EBX + 0x78]                 ; facing_direction
LEA   EAX, [EAX + EAX*0x2]                       ; facing * 3
MOVSX EDX, byte ptr [EDI + EAX*0x2 + 0x45ac70]   ; table[facing * 6 + chain]
```

so it is a 24-byte **signed** table at `0x0045ac70`, indexed
`facing * 6 + chain`. Read out of the CE image:

```
03 01 01 01  01 01 03 01  01 01 01 ff
03 ff 01 ff  01 00 03 01  ff 01 ff 00
```

which decodes, with chains in the order the native traverses them -- head,
left leg, right leg, left arm, right arm, tail -- to:

| facing | head | L leg | R leg | L arm | R arm | tail |
| --- | --- | --- | --- | --- | --- | --- |
| 0 north | +3 | +1 | +1 | +1 | +1 | +1 |
| 1 south | +3 | +1 | +1 | +1 | +1 | -1 |
| 2 east  | +3 | **-1** | +1 | **-1** | +1 | 0 |
| 3 west  | +3 | +1 | **-1** | +1 | **-1** | 0 |

Facing east puts the *left* limbs behind, facing west the *right* ones, which
is the correct far-side-behind rule for a creature in profile. The port's
`FacingDirection` enum already numbers north/south/east/west 0..3, and its
`limb_chain_heads` are already built head, left thigh, right thigh, left
humerus, right humerus, tail tip, so both indices line up with the native
without reordering.

## One semantic detail

The native computes the step direction **once**, before walking the chain:

```
chain_offset_sign_mask = chain_render_offset >> 0x1f;
... chain_render_offset += (chain_offset_sign_mask & 0xfffffffe) + 1;
```

which is +1 when the initial offset is non-negative and -1 when it is negative.
The port re-tested `render_offset < 0` on every limb. Those agree for every
value in the table -- an offset that starts negative only decreases and one
that starts non-negative only increases, so neither crosses zero -- but the fix
takes the sign once, as the native does, so the two cannot drift apart if the
table is ever edited.

## What was NOT wrong

Checked at the same time and matching CE:

  - `CWorldRenderer::RenderWorldRectToDIB @0x00412aa0`: the entity-registry
    scan, the horizontal world-wrap adjustment, the four-part visibility test,
    the 4000-record cap, the `< 0x21` insertion-sort threshold and the
    scratch-backed stable merge above it.
  - The sort key: plane only, stable, so equal planes keep entity-registry
    order in both engines.
  - `SimpleObject::GetRenderPlane @0x00426c00` (entity's plane),
    `CompoundObject::GetRenderPlane @0x0042b740` (part 0's entity's plane),
    `Skeleton::GetBodyRenderPlane @0x00406d60` (body part's plane).

So the z-ordering machinery itself was faithful; only this one table was
missing.

## The CAOS surface (`new:`) is correct

The plane is exposed as an argument to the creation commands, not as a
standalone command -- there is no `plne` token. Checked every form against
`Macro::ExecuteNewCommand @0x0041d130`:

| form | rvalues | plane |
| --- | --- | --- |
| `new: simp <spr> <count> <first> <plane> <cache>` | 4 | 3rd |
| `new: part <idx> <x> <y> <image> <plane>` | 5 | 5th |
| `new: scen <spr> <count> <image> <plane>` | 3 | 3rd |
| `new: cbtn <spr> <count> <first> <plane>` | 3 | 3rd |
| `new: comp <spr> <count> <first> <cache>` | 3 | none |
| `new: vhcl <spr> <count> <first>` | 2 | none |
| `new: lift <spr> <count> <first>` | 2 | none |

The port's parse order matches the native's argument order in every case, and
`comp`/`vhcl`/`lift` correctly take no plane -- the native constructors receive
a cache-protect bool in that slot for `comp` and nothing at all for the other
two. Only `new: simp` appears in a shipped `World.sfc`; the rest arrive through
injected COBs, which is why an argument-count slip here would have gone unseen
until an agent was injected.

## Still open

A pixel-level A/B against the 1996 binary on the same world was not completed:
it needs the camera parked on the same creature in both engines, and the
reference opens on a different viewport with no creature in frame. The fix is
evidenced by the table read directly out of the CE image and by the algorithm
matching, not by a side-by-side render. Worth doing when the camera can be
synchronised.

Harness note: `import -window <id>` on the headless display returns a stale
pixmap -- two captures five seconds apart came back byte-identical while the
world was ticking. Capture `-window root` instead, which updates.
