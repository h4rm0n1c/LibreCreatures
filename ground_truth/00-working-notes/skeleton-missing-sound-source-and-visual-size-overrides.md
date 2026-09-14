# Every Creature reported position (0,0) and size (0,0) to CAOS/sound -- resolved

Date: 2026-09-14

## The bug

`objects::Object` declares four virtuals used throughout CAOS (`posl`/
`post`/`posr`/`posb`, `wdth`/`hght`) and sound panning/attenuation
(`Object::update_sound`): `sound_source_x()`, `sound_source_y()`,
`current_visual_width()`, `current_visual_height()`. The base-class stubs
(`objects/object.cpp`) all `return 0`.

`Skeleton` (the class every `Creature` HOLDS as its `skeleton_` member --
see the earlier Creature-tick-wiring finding: `Creature` does not inherit
`Object`, it owns a `Skeleton` value member, and `Skeleton : public
objects::Object` is what actually answers every position/size/sound
virtual for a creature) never overrode any of the four. Every Creature in
the game therefore reported `sound_source_x() == 0`,
`sound_source_y() == 0`, `current_visual_width() == 0`,
`current_visual_height() == 0` regardless of its real position -- CAOS
`posl`/`post` on a targeted creature returned `0|0` even though `gnus`/
`spcs` confirmed the target was a real, valid object.

Consequences of this one gap:
- **Invisible-ish / hit-tested-wrong**: `GetClickEventIdAtWorldPosition`
  and world-rect-based lookups that go through `sound_source_x/y` +
  `current_visual_width/height` (rather than the already-correct
  `get_bounds()`/sprite_bounds path used by rendering itself) see every
  creature as a zero-size point at the world origin.
- **Unheard**: `Object::update_sound()`'s
  `compute_sound_attenuation_and_pan()` and `sound_audibility_state()`
  both depend on `sound_source_x()`/`sound_source_y()`; with both always
  0, every creature's continuous sound computes its pan/attenuation
  relative to world origin instead of the creature's real position.

## Confirmed against native, not guessed

Found Skeleton's real vftable (installed at `0x0045ac8c` by
`Skeleton::Skeleton @ 0x0043aaf0`, `MOV dword ptr [ESI], 0x45ac8c`) and
read its slots 30-33 directly (the same four slots that, at the *base*
`Object` vtable, are named `GetSoundSourceX`/`GetSoundSourceY`/
`GetCurrentVisualWidth`/`GetCurrentVisualHeight`). Skeleton's own vtable
installs different functions at those slots -- native names them
`GetSpriteBoundsMinX`/`MinY`/`Width`/`Height`
(`0x00406d70`/`0x00406d80`/`0x00406d20`/`0x00406d30`) -- and decompiling
them confirms they are plain accessors on Skeleton's own `sprite_bounds`
member:

```
GetSpriteBoundsMinX   -> sprite_bounds.min_x
GetSpriteBoundsMinY   -> sprite_bounds.min_y
GetSpriteBoundsWidth  -> sprite_bounds.max_x - sprite_bounds.min_x
GetSpriteBoundsHeight -> sprite_bounds.max_y - sprite_bounds.min_y
```

This port's `Skeleton` already had every piece needed
(`sprite_bounds`, and even the exact same accessors under different
names: `sprite_bounds_min_x()`/`sprite_bounds_min_y()`/
`sprite_bounds_width()`/`sprite_bounds_height()`, plus the analogous,
already-correct `get_bounds()` override) -- they were simply never wired
to the base class's `sound_source_x/y`/`current_visual_width/height`
virtuals.

## Fix

Added four overrides to `Skeleton` (`creatures/skeleton.hpp`/`.cpp`) that
delegate to the existing `sprite_bounds` field exactly as native's real
vtable slots do:

```cpp
int Skeleton::sound_source_x() const { return sprite_bounds.min_x; }
int Skeleton::sound_source_y() const { return sprite_bounds.min_y; }
int Skeleton::current_visual_width() const {
    return sprite_bounds.max_x - sprite_bounds.min_x;
}
int Skeleton::current_visual_height() const {
    return sprite_bounds.max_y - sprite_bounds.min_y;
}
```

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir
  /tmp/bm-cache` -- 139/139 compile, resource compiles, link succeeds.
- Live-tested under Wine against the user's own crashed save:
  - Before the fix: `targ norn,dde: putv posl` / `putv post` returned
    `0|0` even though `gnus`/`spcs` on the same target confirmed a real
    object.
  - After the fix: `posl`/`posr` return real, distinct world coordinates
    (`4100`/`4151`, a real 51px sprite width) confirming
    `sound_source_x()`/`current_visual_width()` are now correctly wired
    through Skeleton's real `sprite_bounds`.

## Open follow-on, NOT fixed here

Two things surfaced during this investigation that are real but distinct
from this fix, and are being left open rather than folded in:

1. **`sound_source_y()`/`current_visual_height()` read 0/large for at
   least one creature in the crash save** (`post` returns 0 while `posb`
   -- `sound_source_y() + current_visual_height()` -- returns ~935).
   Since the override now correctly reflects whatever `sprite_bounds.min_y`
   / `max_y` actually hold, this looks like a genuine, separate bug in
   *how* `sprite_bounds`'s Y extent gets computed/restored for this
   creature (`down_foot_y`/`body->world_y()` ending up at literal world
   top), not a missing-virtual problem -- not yet root-caused.
2. **`totl`/`enum` return wildly unstable counts on repeated, back-to-back
   queries against a live, running world** (`totl 4 0 0` oscillating
   between two different values call to call, e.g. 268/86, 208/307,
   508/132 in different runs) even though `totl 3 1 1` stayed rock
   solid at 4. This looks like the classifier filter in the `enum`/`totl`
   family matching far more than intended (possibly all live objects,
   not just the requested family/genus/species), with the live world's
   own transient objects (particles, sounds, etc.) driving the
   count around each tick. This may well be the mechanism behind item 3
   from the user's original report ("hand cannot interact with world
   properly, in fact it caused a crash") and deserves its own dedicated
   investigation -- not attempted here.
