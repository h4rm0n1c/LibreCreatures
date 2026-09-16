# Conditional branch skips incorrectly consumed world ticks

Date: 2026-09-15

## Report and scope

Flying bird (2,10,5), hummingbird (2,10,4), jellyfish (2,10,7),
beachball (2,13,2), and helicopter (2,14,1) translate much too slowly.
The latter two have slow/stuttering scripted airborne motion. These are
family-2 reports, but the fault is in shared CAOS control flow, not a
family-specific speed constant or a missing SimpleObject tick dispatch.

## Evidence

Read the runtime handoff, both repository histories, the REPE rollback
note, current tick/movement adapters, and scripts parsed directly from
`140926crash/w2/World.sfc` using `re_work/tools/parse_sfc.py`.

The bird's event-7 script contains two DOIF boundary checks before MVBY.
Hummingbird and jellyfish have nested region/boundary checks before MVBY.
Ball and helicopter implement airborne movement with MVBY, conditionals,
and arithmetic inside loops; this motion is scripted, not an independent
gravity integrator. Delaying branch skips therefore delays displacement
and scripted acceleration together.

Raw Ghidra disassembly of native Macro::ExecuteInterpreter (0041dc40):

- 0041ecd2: false-DOIF branch scan finishes with JMP 00420c6f.
- 0041eaea: ELSE branch scan finishes with JMP 00420c6f.
- 00420c6f: MOV ESI, 1 (continue flag).
- 00420cce: OR EAX, ESI; 00420cd0: JNZ 0041dd00 (dispatch entry).

Thus both successful scans continue immediately. The port returned
`cursor_changed`, mapped by finalize_interpreter_iteration(false) to a
scheduler return when not in instant/output-capture mode. Every such
branch introduced an extra world tick. This is an instruction-level
mismatch, independently of the earlier REPE decompiler interpretation.

## Change

Return iteration_complete after successful DOIF-false and ELSE scans.
Preserve scan error handling, nested-branch cursor selection, and all
REPE/EVER/UNTL behavior from the rollback. Update the stale scheduler
comment that incorrectly described false DOIF as a legitimate yield.
No global timing, displacement, object hierarchy, or animation changes.

## Verification

- `python3 harness/conditional_pacing_test.py`: failed on the old code,
  passed after correction. Compiles the actual production branch, loop,
  and finalization bodies against the real Macro declaration. Checks
  nested branches, repeated boundary checks, malformed branches, and
  preserved REPE/EVER/UNTL yields. This is not a full interpreter/world test.
- Windows build: 139 translation units, 137 cached, 2 compiled; resources
  and link passed using `/tmp/drop-room-fix-20260915`.
- Live verification NOT achieved: old vehicle-cabin build exits code 0
  immediately in automation mode; new build also exits code 0 in ordinary
  mode after restaging. No gameplay observations obtained. The smoke
  harness labels ordinary-mode exit 0 a pass, but that is not evidence
  that a world ran. Do not mark the user's symptoms resolved yet.
- Existing interpreter trace test cannot currently compile unchanged:
  its NullRuntime fixture predates newer pure virtual methods and the
  teleport signature change. Not modified as part of this fix.

Candidate build: `builds/Creatures.conditional-motion-pacing-fix.exe`,
with matching `.map`. Includes the previous cabin/drop and horizontal
scroll fixes. Both repositories carry identical changed source/harness.

## Follow-up

Windows user feedback (2026-09-15): movement looks "far more normal".
This confirms improvement, not separate verification of every listed object.
The bee report was subsequently reconsidered by the user: the apparent
missing Y movement may have been a mistaken expectation of bee movement,
with the frozen-looking animation causing the confusion. No confirmed bee
Y-axis or animation bug remains; do not alter bee scheduling without a
reproducible original-versus-port mismatch.

Verify bird speed, hummingbird/jellyfish positional variation, ball bounce
and helicopter flight in a fresh Windows world. Also check bees, fish,
boat, and blackboard for script-pacing regressions. Do not reapply the
REPE/EVER/UNTL changes as part of this test.

Bee (2,10,3) event 7 in the inspected save explicitly uses MVBY +/-2 0:
horizontal back-and-forth is present in the script. The user's later
reassessment means this is not currently an actionable defect.
Fast flags and the general global timing question remain unaudited here.
