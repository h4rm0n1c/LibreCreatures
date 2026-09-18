# A newly constructed creature starts UNBOUNDED_2 and cannot walk until `slim`

`Creature::Creature @0x0040d580` ends its construction with

```
Object::SetBoundsMode((Object *)this, UNBOUNDED_2);
Object::UpdateMovementBounds((Object *)this);
Skeleton::SetDownFootPositionAndRecomputeLayout(&this->skeleton_base, 1000, 2000);
Skeleton::ApplyPoseString(&this->skeleton_base, "211111111111111");
```

so every creature -- from `new: crea`, from the Tools menu factories
(`CreateAndSelectGeneratedMaleCreature @0x004347a0`), from anywhere -- is born
with `movement_bounds = {0, 0, 0x7fff, 0x7fff}`, parked off-map at (1000, 2000).
Nothing in the constructor restores DEFAULT_WORLD. That is deliberate: the
caller has to be free to place the creature before the map clamps it.

**The port matches this exactly** (`Creature::Creature` in
`src/c1/creatures/creature.cpp`), and it is not a bug in either engine.

## What restores it

The world's own egg hatch script, read out of `World.sfc`:

```
... new: crea obv0 obv1, setv objp targ, mvto 0 2000, drea 1, targ ownr,
    ... setv var1 posl, addv var1 posr, divv var1 2, setv var2 posb,
    targ objp, mvto var1 var2, slim, rmev ownr, evnt targ, ...
```

`slim` is the step that matters: `SetBoundsMode(DEFAULT_WORLD)` +
`UpdateMovementBounds`, which -- with the creature's `0x40`
(`kUseCurrentMapRoom`) bounds flag -- resolves the room it is standing in and
writes the real walkable strip, e.g. `2242,757,6335,927`.

The grendel egg and the Eve-summoning script in the same world do the same
thing, and so does `mcrt`. `mvto` on its own does **not**.

## Why an unbounded creature animates but never moves

`UpdateAnchorAndBounds`'s foot-swap test is

```
bounds.max_y < limb_chain_end_y[opposite]
```

With `bounds.max_y = 0x7fff` (32767) that can never be true, so the down foot
never swaps. The creature picks actions, plays its gait animation, cycles poses
and can even eat something adjacent -- it simply never commits a step. Its
position is frozen at wherever it was placed.

## Harness consequence

A test norn created by hand **must** be created the way the game does it:

```
new: crea tokn TSTA 1, mvto <x> <y>, slim, drea 1
```

Two test norns in the `stagetest` world sat inert at 4400,927 for an hour
because the `slim` was missing. Issuing `slim` on them after the fact
(`setv var0 <handle>, targ var0, slim`) started both walking within seconds,
and the newborn's life force began climbing (44% -> 49%) as it reached food.

Related: [caos-object-values-are-untrusted.md](caos-object-values-are-untrusted.md)
(how to target a creature by handle safely),
[norns-do-not-feed-themselves.md](norns-do-not-feed-themselves.md).
