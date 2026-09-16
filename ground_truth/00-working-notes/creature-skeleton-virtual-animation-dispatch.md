# Creature Skeleton Virtual Animation Dispatch

## Symptom

Saved creatures were alive and their brains, biochemistry, and action
selection were ticking, but their action scripts could select `ANIM` without
the creature runtime retaining an animation sequence. This left creatures
visually frozen and made the CAOS action surface appear only partially wired.

## Evidence

The native Skeleton vtable at `0x0045ac8c` contains the creature overrides at
the Object slots used by the CAOS interpreter:

- slot offset `0x8c`: `0x0043beb0`, `Skeleton::ParseAnimationSequence`
- slot offset `0x90`: `0x0043bf00`, `Skeleton::IsAnimationSequenceComplete`
- slot offset `0x94`: `0x0043bf10`, `Skeleton::SetTargetPoseFromTableIndex`

The raw native routines ignore the part index for animation/pose dispatch,
and the native POSE routine adds `0x12` before indexing the Skeleton pose
table. The port had Skeleton helper methods for these operations, but no
virtual overrides of `Object::parse_image_sequence`,
`Object::image_sequence_is_empty`, or
`Object::set_relative_image_index`. CAOS therefore called Object defaults.

## Fix

Added the three Skeleton virtual overrides. `ANIM` now delegates to the
Skeleton animation buffer, `OVER` checks that buffer, and `POSE` maps the
relative CAOS pose through the native `+0x12` table prefix. The port bounds
checks the resulting index before accessing the table.

## Verification

Build:

```text
python3 harness/build_clean_source.py --cache --build-dir /tmp/bm-cache
cache: 107 reused, 32 compiled
RESOURCE OK
LINK OK /tmp/bm-cache/Creatures.clean.exe
```

Managed Wine run against the known-good saved world showed the existing Norn
action log selecting `ANIM`, followed by runtime rows with non-empty sequences
and advancing cursors, including `06050605` and `13141516R`. The game was
closed with `SYS: QUIT` and the Wine server was stopped afterward.
