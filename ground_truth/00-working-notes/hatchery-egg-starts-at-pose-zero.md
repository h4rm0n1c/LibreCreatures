# Hatchery egg started at the small frame, 2026-09-15

## Symptom

An egg spawned from the Hatchery kit appeared at the small frame instead of
the ready/full-size frame used by the original kit.

## Cause

The recovered Hatchery command creates `eggs.spr` as an eight-frame
`SimpleObject` and immediately sends `pose 3`.  `WindowsMacroHost` routed
`pose` through `Object::set_relative_image_index`, whose compatibility
implementation returns success without changing the image.  The concrete
`SimpleObject` and `CompoundObject` implementations require the redraw host,
so the command was consumed while the object stayed at pose 0.

## Fix

Dispatch `pose` to the concrete redraw-aware implementation, using the same
`WindowsEntityImageSequenceRenderHost` already used by `base`.  Plain Object
targets retain the existing virtual fallback.

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir /tmp/bm-cache`
  passed: 139 source files compiled and linked.
- Live CAOS: `new: simp eggs 8 0 2000 0,pose 3,dde: putv pose` returned `3`.
- `python3 harness/caos_conformance.py --exe /tmp/bm-cache/Creatures.clean.exe --family effects`
  passed: 8/8, including the new Hatchery egg assertion.
