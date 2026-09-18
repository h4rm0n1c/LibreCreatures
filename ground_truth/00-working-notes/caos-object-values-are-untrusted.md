# A CAOS object value is untrusted input

Recorded 2026-09-18, after crashing the running game from the CAOS pipe with

    setv var0 21994464,targ var0,dde: putv pose

21994464 was a pointer copied out of a debug log and handed back as an object.
`targ` stored it, `putv pose` dereferenced it, and the process died with Wine's
"serious problem" dialog, taking a live world with it.

## Why this is a port defect even though the original does the same

A CAOS value is an untyped 32-bit word, so `targ var0` is legal CAOS whatever
`var0` holds, and every command taking an object rvalue has the same shape.
The original stores the value and faults on first use; there is no validation
anywhere in its interpreter.

Faithfulness is the port's rule for *behaviour a program can observe*, not for
how it fails on input no sane script produces.  A reimplementation that loses
the user's world to a typo is worse than the original in the way that matters,
and nothing about the game's semantics depends on the fault.  So this is one of
the places the port is deliberately better: same results for every valid
program, no process death for an invalid one.

## The guard

`MacroRuntimeHost::is_live_object` answers whether a pointer is one of the
world's live objects, checked against the same registries everything else
iterates (non-scenery `objects_`, then `scenery_`) and never dereferencing the
candidate.  `Macro::object_from_value` is the single conversion point from a
CAOS value to an `Object*`; it returns null for a pointer the world does not
own.  Every command that takes an object rvalue goes through it: `targ`, `mesg
writ`, `stim writ`, `stm# writ`, `spas`, `kill`, `evnt`, `rmev`, `norn`, `from`,
`touc`'s two operands and the object-pointer lvalue.

`targ` clears the target rather than keeping the previous one: a script that
retargets and is refused should do nothing next, not act on the object it
happened to be holding.

Every rejection is logged to `Creatures.object.log`.  That is deliberate.  A
rejection is either a bogus pointer -- which the original would have followed
into a fault -- or the check disagreeing with the registries, which would be a
defect in the check itself.  The second kind must be visible, because a false
negative silently drops a legitimate object and breaks the script instead of
crashing, which is harder to notice and worse to debug.

## Verified

Against a freshly staged world:

    setv var0 21994464,targ var0,dde: putv pose   -> "0", game alive
    setv var0 1,targ var0,dde: putv posl          -> "0", game alive
    setv var0 4294967295,targ var0,dde: putv posl -> "0", game alive
    mesg writ <bogus> 0, kill <bogus>             -> no effect, game alive

all four logged as rejections, where every one of them killed the process
before.  Normal play was then run with the log truncated, to confirm the guard
never refuses an object the world really owns.
