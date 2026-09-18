# C1 wiring audit — 2026-09-19

This is a progress record, not a declaration of complete behavioral fidelity.
Native evidence: live ghidra-mcp-2 Creatures.exe pointer tables and function
bodies, compared with `re_work/creatures1_decompiled/Creatures`. Raw table
snapshot: `c1-vtable-wiring-20260919.json` (Object and ten derived classes;
slots 0–44, plus PointerTool slot 45). Shared no-op addresses must remain no-ops.

## Evidence-backed changes

| Native evidence | Clean-source wiring |
|---|---|
| Vehicle slots 5/6/7 = 0042c0b0/0042c170/0042c230; Lift slot 7 instead uses 00427c60 | Retained and validated recovered Vehicle handlers and most-derived event routing. These bodies belong to Vehicle, not Lift. |
| Vehicle helper 0042c2c0, also called by Lift::Tick 0042c4f0 | Corrected four function namespaces/receiver types in Ghidra, added ownership comments, saved program, refreshed affected Vehicle.cpp/Lift.cpp export blocks. |
| CallButton slot 13 = 00429a70 | Edit finalization now reaches existing floor-registration method before SimpleObject fallback. |
| Macro::ExecuteNewCommand 0041d130, NEW:PART branch | Use Object gallery, create Entity at origin, store local offsets in CompoundPart, extend part count. Previous adapter used part 0's absent gallery and omitted offsets/count. |
| Scenery shares SimpleObject slots 20/21, 26/27, 30–33 | Connected Entity movement, bounds, plane and geometry methods. Slot 22 and slot 29 remain inherited no-ops. |
| Object slot 41 = 00426190 | Generic document tick fallback reaches existing sound update. |
| Bubble slot 16 = 00429f90 | Macro runtime-initialization/deletion adapter reaches existing destroy-and-redraw behavior. |
| UpdateViewAnchoredObjects 00413950 calls slot 21; Creature slot 21 = 0043c070 | View anchor adapter now routes Scenery movement and Skeleton foot/layout movement. |

Creature task: slots 28/29 use explicit owner bridges; event IDs 0/1/2 map
to slots 5/6/7, event 3 to slot 10, events 4/5 to slots 8/9, events 6/7 to
slots 11/12. Creature slot 10 is a native no-op. Event 8/9 dispatch must retain
Creature slot 34, including clearing its sleep indicator except event 0x29.
Existing Skeleton overrides also cover slots 35/36/37 animation/pose.

## Fresh validation

- c1-lab MSVC build: all 139 sources, resource compile and executable link pass.
- `harness/creature_click_dispatch_test.py`: pass (production routing, UBSan).
- `harness/vehicle_cabin_adapter_test.py`: pass (production adapters).
- `git diff --check`: pass.
- Isolated world `wiring_verified_20260919`, derived from knowngood; source
  worlds untouched. Vehicle classifier 3/250/2: activation ran script 1,
  ACTV=1, XVEC=256, X advanced; stop ran script 0, ACTV=0, XVEC=0;
  reverse ran script 2, ACTV=2, XVEC=-256, X decreased. Reload returned
  stopped vehicle ACTV=0, XVEC=0, X=2788.
- CallButton edit/drop: parsed saved World.sfc with existing parse_sfc.py.
  Lift classifier 66781696 has floor_count=1; button classifier 50070016
  has floor_index=0 and a Lift reference. This verifies floor registration,
  not an end-to-end passenger journey.
- Live NEW:COMP + two NEW:PART commands, followed by MVTO, returned
  POSL=3200, POST=600, WDTH=104. Live NEW:SCEN + MVTO returned
  POSL=3400, POST=650, WDTH=104, HGHT=164.
- Native `Creature::DispatchScriptEvent @0040dbc0` and `RemoveFromWorld
  @0040e0d0` were re-decompiled after the earlier wiring pass.  The clean
  source preserves the sleep-indicator exception/force-restart path and the
  removal cleanup order through explicit hosts.  Caller review found no
  remaining route that bypasses Creature slot 34.
- Caller-level review of slots 35–45 found the existing dynamic routes match
  the recovered tables.  One real gap was fixed: `C1WindowsDocument` now
  preserves the native no-op for Scenery/plain Object slots 22/23 instead of
  throwing from `mvby`/`mvto`; derived SimpleObject, CompoundObject and
  Creature routes remain unchanged.
- `selected_creature_is_edit_object` now compares the selected Creature's
  Skeleton identity with the edit object, matching the separate Creature and
  Object registries used by the port and the native follow-selection guard.

Reproduce the last live construction check:

```text
new: comp LIFT 2 0 0,new: part 0 0 0 0 4000,new: part 1 17 23 1 4001,setv clas 66716672,mvto 3200 600,dde: putv posl,dde: putv post,dde: putv wdth,new: scen LIFT 2 0 3999,mvto 3400 650,dde: putv posl,dde: putv post,dde: putv wdth,dde: putv hght
```

## Remaining checks / limitations

- Persistence was rechecked against fresh, correctly named c1-lab staging
  directories.  A `NEW: COMP` with two `NEW: PART` records saved as a 192nd
  non-scenery record; `parse_sfc.py` recovered `part_count=2`, offsets
  `(0,0)` and `(17,23)`, planes `4000/4001`, and world coordinates
  `(3200,600)` and `(3217,623)`.  Reopening that saved world and saving again
  preserved the record and all part fields.  Two `NEW: SCEN` calls saved as
  scenery records 141 and 142, including the moved `(3400,650)` entity; a
  reopen/save cycle preserved both.  The earlier absence came from the
  investigation harness inspecting the wrong/stale staged world, not from a
  serializer omission.  No source persistence patch is warranted.
- Bubble slot-16 now has a focused live path check: `SAYN` created the
  expected family-2/genus-1/species-2 Bubble records, and `ENUM 2 1 2,
  KILL` reached the Bubble-specific deletion route without a crash.  The
  harness reports deferred enumeration/count transitions while the scheduled
  world tick drains destruction, so this is a route/safety check rather than
  a claim about exact visual lifetime timing.
- Creature/Scenery mouse anchoring now has a focused live test (world
  `anchortest2`, derived from knowngood; source worlds untouched).  With the
  pointer parked by xdotool on the lab display and CAOS `EDIT` putting the
  object in the hand:
  - Simple object: moving the mouse +100/+50 moved the held object exactly
    +100/+50 (4682,943 -> 4782,993), so the edit-object branch tracks the
    mouse one-to-one.
  - Creature: the held norn tracked the mouse in X, and raising the pointer
    lifted it off the floor to bottom=782 for a mouse world Y of ~773, i.e.
    slot 21 is planting the down foot at the pointer rather than moving the
    bounding box.  Right-clicking dropped it and re-planted its feet on the
    room floor (bottom=933), which is the slot-13 finalize path.
  - Scenery: dropped at the pointer, the saved record holds (3844,883) --
    identical to a simple-object probe measured live at the same pointer
    position, confirming the Scenery movement route (`enum` cannot visit
    scenery, so this is read back from the saved World.sfc).
  Native `UpdateViewAnchoredObjects @00413950` was re-read for this: all
  three branches (first unbounded renderable, pointer tool, edit object) call
  slot 21, and the loop tests UNBOUNDED_1 specifically, which is what
  `uses_unbounded_world_position()` means in the clean source.
- The source routes and native slot ownership are closed for the previously
  open Creature 24/34 and slots 35–45 paths.
## Generated creature sprite files (2026-09-19, later pass)

A creature's body sprites are not an authored asset: `Skeleton::LoadGenome
@0043c800` composites the breed body-part sprites through a pigment-gene
palette remap and writes `<genome moniker>.spr` itself (path built at
0043cb5e..0043cb88, opened at 0043cb98 with modeCreate|modeWrite = 0x1001,
then CFile::Write).  `Skeleton::ValidateBodySprites @00408080` re-checks that
file for every creature on world open (`OnOpenDocument @004309a0`), and the
save sweep deletes it when a creature leaves the world.

Native's exact rule, from the flag register in 00408080: BL starts clear at
00408121, and is set at 004081a2 only when the file OPENED and either its
frame count differs from the gallery's, or the first frame's width/height
differ from the gallery's first image.  The landing pad at 004081ce then
rebuilds through CGenome + Skeleton::LoadGenome.  A file that will not open
leaves BL clear, so native does not rebuild a missing file.

Two defects were found and fixed:

- `Skeleton::validate_body_sprites` read the header and discarded the result:
  no comparison, no rebuild.  It now reports staleness (count, and first-frame
  width/height) and `Creature::rebuild_body_sprites` reconstructs the genome
  and reruns `Skeleton::load_genome` alone, leaving brain, biochemistry and
  voice as they are -- which is what the native landing pad does.
- The port carried TWO composite-gallery pointers where native has one
  (Object+0x40): `Object::gallery_`, which the world save restores, and a
  separate `Skeleton::gallery`, which only the genome build path ever set.
  On a world load the Skeleton copy was null, so every check keyed on it
  silently did nothing.  Skeleton now uses the inherited Object field.

Deliberate deviation: a missing `.spr` is treated as stale and rebuilt.
Native leaves it, which strands the creature with no body sprites -- verified
live: with `3FZK.SPR` deleted, that norn loaded, walked and bred while being
completely invisible, and nothing ever restored it.

Live verification (worlds `sprtest2`/`sprtest3`/`sprtest4` from knowngood):
- First frame of `3FZK.SPR` patched to 1x1 against a gallery of 40x32: the
  file was rewritten on load (93278 bytes, first frame back to 40x32) and the
  intact `1MBV.SPR` was left byte-identical.
- `3FZK.SPR` deleted: regenerated on load and the norn renders again.

Open question, NOT closed here: the regenerated file is not byte-identical to
the one the reference world shipped.  All 136 frames match in geometry, but
the pixel payloads differ (616 of 1280 bytes in frame 0) and the difference
is not a consistent palette remap (14014 of 29492 sampled pixels break a
one-to-one mapping), so the rebuild is compositing different source pixels,
not merely tinting differently.  The clean differential for this is to corrupt
the same header in a world run by the 1996 binary and byte-compare its
rebuilt output with ours; that has not been run.  Until it is, treat the
sprite compiler's output as behaviourally correct but not proven faithful.

Minor: the port writes the generated name as `<moniker>.spr` while native
writes `<moniker>.SPR` (the ".SPR" literal at 004537cc).  Harmless on
Windows/Wine, unverified on a case-sensitive mount.

- Graphify and older exports may be stale; only the five affected live
  decompilation blocks were refreshed. No wholesale export performed.

Lab caution: synchronous DDE scripts have no OWNR, so MESG WRIT there did not
queue the desired Vehicle event. Use an object-owned timer script to send
MESG WRIT OWNR. NEW:COMP takes the fourth cache-protection argument.
