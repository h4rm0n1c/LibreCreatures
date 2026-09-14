# REPE (bounded `reps...repe` loop-back) yielded once per iteration -- resolved

Date: 2026-09-14

## Correcting an earlier wrong conclusion

An earlier pass this same day (superseded, deleted:
`scripted-object-slow-movement-and-frozen-animation-investigated.md`)
investigated the user's report of a "flying bird" (`family=2 genus=10`)
moving very slowly and animating strangely, and concluded -- wrongly --
that this was native-faithful CAOS interpreter behavior, based on a
partial hand-read of raw x86 disassembly. **The user pushed back hard
and correctly**: they could guarantee the bird flew faster than this,
and pointed out that the actual, already machine-transformed decompiled
source tree (`src/decompiled/`) should have been consulted directly
instead of re-deriving control flow from raw instruction bytes by hand.
Re-investigating with the real decompiled dump found a genuine,
confirmed bug.

## The bug

`Macro::execute_counted_repeat_command`'s handling of `REPE` (the
loop-back half of a `reps N ... repe` bounded loop,
`scripting/macro.cpp`) returned `MacroControlFlowResult::cursor_changed`
whenever the loop had iterations remaining. This project's own
`finalize_interpreter_iteration` treats anything other than
`iteration_complete` as "yield to the caller now" -- so every single
`reps...repe` iteration was spread across its own separate world tick,
instead of the whole bounded loop running to completion within the tick
it started on.

For an object script like this save's flying/drifting decoration:

```
reps var1, anim [01R], mvby 2 0, repe
```

with `var1` randomized 8-16, this meant an 8-16-unit "shuffle" burst
that should complete in one ~90ms tick was instead spread across 8-16
real ticks (720ms-1.4s). It also broke the object's own animation: `anim
[01R]` resets the image-sequence cursor to frame 0 on every call
(confirmed correct/native-faithful separately, see below), and since the
script re-issues `anim` on every `reps` iteration, spreading those
iterations across many real ticks meant the sprite's own per-tick
auto-advance (`SimpleObject::tick`'s call to `Entity::
advance_image_sequence`) kept getting reset back to frame 0 by the very
next script iteration before it could ever show a different frame --
`pose` sat at 0 almost permanently, with only rare flickers.

## Confirmed against the actual decompiled dump, not guessed

`src/decompiled/Macro.cpp`'s `Macro::ExecuteInterpreter` (native's real,
monolithic interpreter) handles `REPE` (opcode `0x65706572`) by popping
the remaining count; **both** the "loop finished" branch (count reaches
0) and the "loop continues" branch (count still `> 1`, cursor rewound,
new count pushed) fall through to the exact same `goto LAB_00420c6f;`.
That label is the interpreter's single shared "iteration complete" join:

```c
LAB_00420c6f:
  bVar30 = true;
LAB_00420c74:
  ...
  if (self_cursor->capture_output_enabled == 0 && !bVar30) {
    return;                    // yield to caller
  }
  goto LAB_0041dd00;            // keep dispatching -- same tick
```

`bVar30` is reset to `false` at the very top of the dispatch loop
(`LAB_0041dd00`) before every token fetch, and is the *only* thing this
function checks to decide whether to return (yield) or keep processing
more commands in the same call. Since `REPE` unconditionally sets it
`true` on both outcomes, **the whole `reps...repe` loop -- every
iteration -- runs within the single tick it started on**, in native.
This project's `cursor_changed` result was wrong; it should have been
`iteration_complete`.

## Fix

Changed the "loop continues" branch of `MacroCommand::repeat` (REPE) in
`Macro::execute_counted_repeat_command` to return
`MacroControlFlowResult::iteration_complete` instead of `cursor_changed`.
The cursor rewind and stack push (loop mechanics) are unchanged -- only
the yield/continue signal was wrong.

## A related change that was tried and reverted -- do not retry blindly

The same decompiled dump shows `EVER` (unconditional `loop...ever`) and
`UNTIL`'s false-condition path sharing a structurally identical rewind
sequence (`LAB_0041f237`) that also ends in `goto LAB_00420c6f` --
apparently the same "don't yield" signal REPE uses. Changing this
project's shared `repeat_saved_cursor()` helper (used by both `ever` and
`until`) to also return `iteration_complete` **hung the live game
solid** on the very next test: frozen frame, world tick stopped
advancing, CAOS pipe commands timing out. Reverted immediately; the
helper still returns `cursor_changed` (yields once per iteration),
which is what this project already had and is known not to hang.

**Do not retry this without first finding what actually differs**
between REPE's real behavior and EVER/UNTIL's real behavior -- the
apparent shared label is misleading somehow (a different flag gets set
or read between `LAB_00420c6f` and this specific opcode's dispatch,
possibly, or the address `0x0041f237` isn't really the single shared
site the decompiled text's `goto` suggests). A hypothesis worth checking
next time: real per-tick pacing for a loop with no explicit `wait`
likely comes from something *else* inside the loop body yielding on its
own (most plausibly a `doif` condition's false-branch skip-scan, which
is a separate, already-established-correct code path) -- meaning EVER
itself plausibly never needs to yield in native because in practice
every real script's loop body contains at least one `doif`/`else` that
does the yielding for it, and a script that truly has *no* conditional
anywhere in its loop body might be vanishingly rare in shipped content.
If that hypothesis is right, this project's own `cursor_changed` for
EVER/UNTIL is a compensating-but-imperfect stand-in for a per-tick pacing
mechanism that in native comes from elsewhere -- not something to "fix"
by copying REPE's exact resolution without confirming the actual
mechanism first.

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir
  /tmp/bm-cache` -- 139/139 compile (REPE fix alone; the reverted
  EVER/UNTIL change also compiled clean, which is why the hang wasn't
  caught until live-testing -- a reminder that this class of scheduling
  bug is invisible to compile/link checks and needs an actual run every
  time).
- Live-tested against the user's crashed save with the REPE fix alone
  (EVER/UNTIL reverted): the game stays fully responsive (CAOS pipe
  commands succeed, world tick keeps advancing, screenshots keep
  changing frame to frame -- confirmed NOT hung, unlike the reverted
  variant). The flying/drifting decoration's `pose` value now shows
  real variety tick to tick (`1|0|1|1`, `1|1|1|1`, `0|1|1|0`, etc.)
  instead of sitting at `0|0|0|0` almost permanently -- the animation-
  freeze symptom is confirmed fixed. Position movement is visibly
  noisier/less smooth (matches bursts completing within single ticks
  now, as intended) though a precise speed-multiplier measurement was
  not attempted given the small sample size and this object's own
  random-walk direction reversals.
