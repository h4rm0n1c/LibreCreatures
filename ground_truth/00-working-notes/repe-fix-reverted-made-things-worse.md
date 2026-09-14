# REPE "iteration_complete" fix reverted -- made things worse, didn't fix the target bug

Date: 2026-09-15

## What happened

A prior pass this session (superseded, deleted:
`repe-loop-continuation-yielded-when-it-should-not.md`) changed
`MacroCommand::repeat`'s (REPE) loop-continuation branch from
`MacroControlFlowResult::cursor_changed` to `iteration_complete`,
reasoning from `src/decompiled/Macro.cpp`'s apparent shared
`LAB_00420c6f` join between REPE's "loop finished" and "loop continues"
branches. It was shipped on decompiled-reading confidence alone, without
a live test that actually exercised it (the harness's CAOS-pipe
injection wasn't landing reliably that pass).

The user then live-tested it directly and reported, in order:
- Bees jumping around chaotically.
- All fish types jumping around chaotically.
- The island boat's (family 3, genus 1, species 7) animation broken.
- The blackboard's animation not playing.
- And, decisively: the original target bug -- the flying bird (family 2,
  genus 10, species 5) -- **still** moves too slowly ("crawling across
  the sky"), i.e. the change did not even fix the bug it was written
  for.

Net effect: strictly worse than before the change, with zero of the
intended benefit realized. Reverted immediately back to
`cursor_changed`.

## Why the decompiled reading was misleading

The `LAB_00420c6f` join being shared between REPE's two branches is real
in the decompiled text, but evidently insufficient to explain real
per-tick pacing on its own -- otherwise the fix would have sped up the
bird as expected. The most likely explanation, not yet confirmed: some
other piece of native per-tick state (a global tick/frame counter, a
budget on how many commands or how much "work" `ExecuteInterpreter` may
do before its caller forces a yield regardless of `bVar30`, or an
entirely different code path being responsible for the bird's actual
motion) is what really governs pacing, and this project's `bVar30`-only
model of "yield vs continue" is incomplete. The EVER/UNTIL hang from
earlier in this session (see the same class of caution) is consistent
with this: this project's simplified single-flag model keeps producing
plausible-looking native evidence that doesn't hold up live.

## Standing rule going forward for this family of bugs

Do not change REPE/EVER/UNTIL/loop yield semantics again based on
decompiled reading alone. Any future attempt must:
1. Live-test against a fresh, verified save (md5-checked immediately
   before the test, per this session's other hard-learned lesson) before
   claiming anything.
2. Check multiple object categories known to use loop-driven ambient
   animation/movement (bees, fish, boat, blackboard, the original flying
   bird), not just the one object that prompted the change.
3. Confirm improvement on the original target bug specifically, not just
   "looks native-faithful" from static reading.

## Current state

`MacroCommand::repeat`'s loop-continuation branch is back to
`cursor_changed` (yield once per iteration) -- the same behavior this
project has had all along, before this session touched it. The flying
bird's slow-motion bug remains open and unexplained; do not re-attempt a
fix without live verification.

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir
  /tmp/bm-cache` -- 139/139 compile, resource compiles, link succeeds.
- Not yet re-verified live in this pass beyond the revert itself; the
  next step before claiming this resolved anything is to confirm bees,
  fish, boat, and blackboard animation are all back to their
  pre-session baseline behavior with a fresh save.
