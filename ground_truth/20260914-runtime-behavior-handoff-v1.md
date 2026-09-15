# Creatures 1 cleanroom port — runtime-behavior handoff

Date: 2026-09-14 (Australia/Perth)

## Current 2026-09-15 update

Latest native-reference retest: see `00-working-notes/creature-sprite-directory-ownership.md`
and `00-working-notes/native-creature-attachments-and-eye-frames.md`. Resolved
directory strings now have owned lifetimes, Body archive rows match native
chain/view order, and head frames respect the native eyes-open CMOV. Both
native norns render correctly joined with open eyes using a disposable copy
of the user's `knowngood` world. Fresh hatching remains unverified. Earlier
claims below that the first Body archive correction closed the issue were
premature. Use the managed GUI display, not the old ad-hoc Xvfb recipe below.

The selected Windows world is a real secondary resource tree under the user's
Documents directory: its root owns `World.sfc`, `Images`, `Genetics`,
`Backup`, and `TempBu`.  The port's save-local creature sprite read fallback
was correct but several generated-resource and backup paths still pointed at
the primary install tree.  That routing fix is documented in
`ground_truth/00-working-notes/secondary-world-resource-root.md` and is now
present in both repositories.  The clean-source build passes.  The current
Wine smoke prefix stayed alive but did not publish the CAOS pipe, so do not
interpret that run as a gameplay visibility verdict; investigate the separate
automation/startup path before claiming live behavior.

Written for a fresh agent (Codex) picking this up because the current
session is out of budget. This is NOT about the structural
decomposition described in `20260907-cleanroom-port-structural-handoff-v1.md`
(that file covers `executable_entry.cpp` splitting and is largely
superseded — the project is now in a **live runtime-behavior bug-fixing
phase**, not a structural-extraction phase). Read this file fully before
touching anything; it exists specifically so you don't re-derive facts
that cost real time to find, or waste time on paths already proven
wrong.

## The user

Not a casual requester. Deep C1 domain knowledge (knows the real CAOS
language, the real object taxonomy, the real Grendel Mother mechanic,
submarine coordinate history "going back 20 years" across other C1
reimplementation projects). Corrects wrong technical claims immediately
and expects the correction to be taken seriously and re-investigated,
not argued with. Standing philosophy, stated directly: **"byte match is
a tool, not the goal — I want a complete, accurate clean-room port that
works like the real game."** Do not present "native does this too" or
"the save's own script is buggy" as a satisfying stopping point without
first proving it beyond doubt — twice this session an initial
"not a port bug" conclusion was wrong and had to be walked back after
the user pushed back. **Never add Claude/AI attribution to git commits**
— standing instruction, no exceptions, even in commit bodies.

## Repo layout and the two-repo workflow (mandatory, every fix)

- **`/home/harri/creatures/byte_match`** — private research repo, no
  git remote. All development, investigation, and disassembly-cross-
  referencing happens here first.
- **`/home/harri/creatures/LibreCreatures`** — public repo, remote
  `h4rm0n1c/LibreCreatures` on GitHub. This is what actually ships.
- **`/home/harri/creatures/builds/Creatures.clean.exe`** +
  `BUILD_INFO.txt` — the durable, user-facing build artifact and its
  changelog. Update both after every push.

For every fix, in this exact order:
1. Investigate and edit in `byte_match`.
2. `python3 harness/build_clean_source.py --cache --build-dir /tmp/bm-cache`
   from `byte_match` root — expect `139/139` compile, resource compile
   pass, link pass. (File count may drift as the port grows; check the
   printed total, don't hardcode 139 forever.)
3. Live-test under Wine if the fix is behavior-visible (see harness
   section below).
4. Write a `ground_truth/00-working-notes/<slug>.md` doc: the bug, how
   it was found, disassembly evidence (address + real behavior), the
   fix, and verification results. This project's docs are how the next
   session (or agent) avoids re-deriving the same investigation — write
   one for every fix, no exceptions.
5. Commit in `byte_match` (long, evidence-carrying commit body — no
   Claude attribution).
6. Copy the exact same changed files + the same doc into
   `LibreCreatures` (same relative paths under `src/c1/`,
   `ground_truth/00-working-notes/`).
7. In `LibreCreatures`: temp-symlink the toolchain
   (`ln -s /home/harri/creatures/byte_match/toolchain toolchain`), copy
   `harness/build_clean_source.py` in, run the **same** build command
   with a **different** `--build-dir` (independent verification, not
   reusing byte_match's cache), confirm compile/link pass, then
   `rm -rf harness toolchain` before committing (LibreCreatures ships
   source only, not the toolchain or harness).
8. Commit (same message, no Claude attribution) and `git push origin main`.
9. Copy the freshly built exe to
   `/home/harri/creatures/builds/Creatures.clean.exe`, and rewrite
   `BUILD_INFO.txt` with the new commit hash, md5, and a running
   changelog of every fix so far (see the existing file for the format
   — keep appending, don't replace history).

### Build and test hygiene (mandatory)

Reuse stable cache directories (`/tmp/bm-cache` for the private lane and
`/tmp/lc-cache` for the public lane); do not create a new cache directory for
each retry. Long builds and smoke tests must run in a tracked foreground tool
session. The smoke harness owns Xvfb and Wine process groups and must tear them
down on success, timeout, exception, or interruption. Before handing off,
check for orphaned `Xvfb`, `Creatures`, Wine, and compiler processes and verify
the public repo's `git status`. If the public lane temporarily receives the
build script or a toolchain symlink, register cleanup before launching and
remove only those exact temporary entries afterward; preserve existing
untracked harness files.

Long commit bodies are expected and valuable here — they're the primary
place disassembly evidence and "why" get recorded permanently. Use a
here-doc or `-F <file>` for commit messages with backticks; the shell
will otherwise interpret them as command substitution (this bit me once
this session).

## Standing technical discipline (from the user's accumulated corrections)

- **Ghidra field names and decompiled argument lists are guesses, not
  facts.** Offsets and pushed constants are real evidence; the names
  Ghidra/the decompiler assigns are inference and are sometimes wrong.
  When something looks suspicious, **read the raw disassembly**, not the
  decompiler's prose. This exact discipline is what found both the
  xvec/yvec bug and the `NEW: CREA` bug this session — the decompiler's
  synthesized comments/types would have led to the wrong conclusion in
  both cases.
- **Byte-match translator discipline**: if something in the generated/
  translated source looks wrong, fix the *pipeline* or the Ghidra DB,
  never hand-edit generated output or fabricate a placeholder value.
- **Search before declaring absence**: grep by *method name* across the
  whole tree, never by class name — nested/out-of-directory/.cpp-local
  implementers hide from class-scoped greps. Verified ~10x necessary
  this project.
- **No API-tis**: ~225 host interfaces already exist, only ~7%
  polymorphic. Never mint a new one-off interface for a single function;
  extend an existing interface (e.g. `MacroRuntimeHost`,
  `MacroNewObjectHost`) with a virtual method instead.
- **Reference binaries are read-only, absolute.** The original C1 exes
  and the Ghidra database must never be written to. A prior session
  accidentally trashed the reference `Creatures.exe` with a careless
  build step; the user had to manually replace it. Triple-check any
  path before a build/copy touches anything under the reference-binary
  directories.
- **CAOS command-lookup technique** (used successfully ~4 times this
  session — this is the fastest reliable way to find native's real
  behavior for a specific CAOS token): pack the 4-character token as
  little-endian bytes in Python
  (`bytes([ord(a),ord(b),ord(c),ord(d)])`), then
  `mcp__ghidra-mcp__search_bytes` with that byte pattern to find the
  literal comparison instruction in the disassembly (usually inside
  `Macro::ParseRValue`, `Macro::AssignLValue`, or
  `Macro::ExecuteNewCommand`), then `disassemble_function` or
  `decompile_function_by_address` at the enclosing function's start
  address. This reliably finds the ground truth even when the
  decompiler's synthesized argument types/names are misleading.
- **`caos_token(a,b,c,d)`** in `src/c1/scripting/macro.cpp` packs a
  4-char CAOS command as `a | (b<<8) | (c<<16) | (d<<24)` — matches
  native's own packing exactly (`new_command_subtype == 0x61657263` for
  `'crea'` is `caos_token('c','r','e','a')`).

## Ghidra MCP server

A custom PyGhidra-based MCP server is available (tools prefixed
`mcp__ghidra-mcp__*`). Two known instances exist across this project's
history (an older FlexTalk-project instance and this C1 project's own
instance) — if tools don't respond, the server may need restarting; see
memory file `project_ghidra_mcp_2.md` from the parent session's memory
store if available, otherwise just retry — the tools were working
throughout this session without intervention.

Useful entry points used this session: `search_functions_by_name`,
`search_bytes`, `disassemble_function` (needs `address`, not `name`),
`decompile_function_by_address`, `read_pointer_array` (for reading a
vtable's slots + resolved symbol names directly), `get_struct_layout`.

## The live-test harness (Xvfb + Wine + CAOS pipe)

This is the pattern used successfully throughout this session to verify
fixes against real, running behavior rather than just compile success.

```python
import sys; sys.path.insert(0, 'harness')  # from byte_match root
from run_c1_smoke import stage_run_directory, install_registry, DEFAULT_INSTALL, DEFAULT_PREFIX
from pathlib import Path
run_dir = Path.home() / ".c1-smoke-prefix" / "drive_c" / "Program Files" / "Creatures"
stage_run_directory(DEFAULT_INSTALL, run_dir, True)   # 3 positional args: install, run_dir, force
install_registry(DEFAULT_PREFIX, run_dir)
```

Then, in bash:
```bash
# Copy in whatever save you want to test against (defaults to a fresh
# install save otherwise); copy the freshly built exe over; caos_client.exe
# sometimes gets wiped by the restage step above and needs re-copying
# from /tmp/c1-cache/caos_client.exe.
RUN_DIR="/home/harri/.c1-smoke-prefix/drive_c/Program Files/Creatures"
cp <your-save>/World.sfc "$RUN_DIR/World.sfc"
cp /tmp/bm-cache/Creatures.clean.exe "$RUN_DIR/Creatures.clean.exe"
cp /tmp/c1-cache/caos_client.exe "$RUN_DIR/caos_client.exe" 2>/dev/null

# Use a FRESH, never-before-used Xvfb display number each time — stale
# lock files from prior abrupt kills (`/tmp/.X<N>-lock`) will block reuse.
Xvfb :NN -screen 0 1280x900x24 &   # run_in_background: true
sleep 2

export DISPLAY=:NN WINEPREFIX=/home/harri/.c1-smoke-prefix
cd "$RUN_DIR"
(wine Creatures.clean.exe /Automation > /tmp/wine.log 2>&1 &)
# poll `pgrep -x "Creatures.clea+"` until it appears (up to ~30s)

# Dismiss the "Tip of the Day" dialog that blocks the pipe server:
WID=$(DISPLAY=:NN xdotool search --name "Tip of the Day" | head -1)
DISPLAY=:NN xdotool windowfocus "$WID"; DISPLAY=:NN xdotool key --window "$WID" Return
```

Then drive CAOS via the pipe:
```python
import subprocess, os
env = dict(os.environ); env["WINEPREFIX"] = "..."; env["DISPLAY"] = ":NN"
subprocess.run(["wine", "caos_client.exe", "--hex", "FIRECOMMAND", "1", script],
                capture_output=True, text=True, env=env, timeout=20, cwd=RUN_DIR)
```
The reply is hex-dumped; parse the `OK\n<len>\n<payload>` text form
(the "len" field is off by one from the payload's real byte length —
harmless quirk, ignore it, trust the payload text).

**CAOS syntax gotcha learned this session**: to print an expression's
value over the pipe, use `dde: putv <expr>` (NOT `putv <var>,<expr>` —
that's an assignment, backwards from what it looks like). One `dde:`
per `FIRECOMMAND` call. Example: `dde: putv totl 4 0 0`,
`enum 4 0 0,dde: putv posl,next,endm` (enumerates and prints one value
per matched object, comma-joined by `|` in the reply).

**Environment hygiene**: each `Xvfb`/`wine` pair leaks if killed
abruptly (`pkill` on this project's shell wrapper reliably returns a
nonzero/144 exit code even on success — expected, not a real failure).
Increment the display number every relaunch rather than fighting stale
locks. `pgrep -af Creatures.clean.exe` / `pgrep -af Xvfb` to check
what's actually still running before assuming a clean slate.

## `harness/parse_minidump.py` — crash dump triage

`python3 harness/parse_minidump.py <path-to-.dmp>` prints loaded
modules, the exception code/address, and faulting register state. To
resolve a crash address to a real function in the CURRENT build (not
native's original addresses — this port's link layout differs from
native's), cross-reference against that build's own `.map` file
(`/tmp/bm-cache/Creatures.clean.map` or wherever `--build-dir` pointed):
find the highest-addressed symbol whose address is `<=` the fault
address — that's the containing function. A quick Python loop over the
map file's `0001:xxxxxxxx <mangled-name> <address> f <objfile>` lines,
sorted, works fine (done ad hoc this session, no dedicated script exists
yet — writing one would be a good, cheap contribution).

**Important**: the user's crash dumps show the module as
`C:\GOG Games\Creatures\Creatures.exe` — this is NOT the original,
unmodified game. The user replaces their real install's `Creatures.exe`
with this port's freshly built exe (keeping the original filename) for
day-to-day testing, so these dumps are crashes **in this port**,
resolved against **this port's own `.map` file**, not native's Ghidra
addresses. Don't confuse the two address spaces.

## Fixes made this session, in order (all pushed, all in both repos)

Each has a full writeup in `ground_truth/00-working-notes/`. Read the
relevant one before touching anything nearby — don't re-derive.

1. **`ab0c40a`/`b2348a7`** — Wired up `Creature`'s never-called per-tick
   update; fixed a real ~7x tick-rate bug. (No note filename captured
   here from this session's memory — check `git log` around this hash
   in `ground_truth/00-working-notes/` for the doc; likely named for
   "creature-tick" or similar. This predates the summarized portion of
   this session slightly.)
2. **`d67212d`/`afca2d6`** — Fixed a divide-by-zero crash in
   `Lobe::update_early_phase` (brain simulation). **Likely already
   fixes crash dump `Creatures.exe.28020.dmp`** (exception
   `0xc0000094` STATUS_INTEGER_DIVIDE_BY_ZERO at offset `0x1d469`,
   which resolves in this build's map to
   `Lobe::update_early_phase`) — that dump's timestamp (08:43) may
   predate this fix; **verify by checking dump timestamp vs. commit
   time before assuming still-open**.
3. **`bfc44c6`/`0f36d48`** — `xvec-yvec-write-corrupted-classifier.md`.
   CAOS `setv xvec`/`setv yvec` (a vehicle's own movement-vector write)
   called the wrong setters entirely (`set_classifier_base`/
   `set_packed_bounds_state_for_script` instead of the vehicle's real
   velocity fields), corrupting the target's classifier on every write.
   This is why the submarine never stopped and vanished from `enum`/
   `totl`. Confirmed against native `Macro::AssignLValue @ 0x0041b9f0`.
4. **`3980070`/`1d46184`** — `skeleton-missing-sound-source-and-visual-
   size-overrides.md`. `Skeleton` (held by every `Creature` as a value
   member — **`Creature` does NOT inherit `Object`; it owns a
   `Skeleton skeleton_{}` member, and `Skeleton : public objects::Object`
   is what actually answers every position/size/sound virtual for a
   creature** — this is a load-bearing architectural fact, get it wrong
   and you'll look in the wrong class) never overrode `Object`'s
   `sound_source_x/y()`/`current_visual_width/height()`, so every
   creature reported position (0,0) and size (0,0) to CAOS and to sound
   panning. Fixed by wiring these to the already-correct
   `sprite_bounds` member (confirmed against native's real Skeleton
   vftable slots 30-33 @ `0x0045ac8c`).
5. **`78a1a6b`/`23fd0ba`** and its follow-up **`987b263`/`e2eb597`**
   (the follow-up commit contains a **correction of a wrong
   conclusion** in the first — read both) —
   `skeleton-and-compound-object-unregister-wrong-container.md`.
   **CRITICAL ARCHITECTURE FACT**: `WorldRuntime`
   (`src/c1/world/runtime.hpp`/`.cpp`) keeps *multiple, distinct*
   parallel object containers that are easy to confuse:
   - `objects_` — the registry `totl`/`enum`/every CAOS enumeration
     command actually iterates.
   - `world_objects_` — a separate registry, different purpose.
   - `creatures_` — the creature-selection registry.
   - `owned_objects_`/`owned_creatures_` — ownership (`unique_ptr`)
     storage, separate again from the raw-pointer registries above.
   - `renderable_objects_` — yet another list, for rendering.

   Two cleanup functions (`WindowsSkeletonLifetimeHost::
   unregister_from_object_registry`, `WindowsNewObjectHost::
   unregister_from_object_registry`) searched `world_objects_` to find
   an index, then deleted that index from `creatures_` — three
   different containers touched incoherently in one buggy function.
   Net effect: destroying a `Creature`/`Vehicle`/`Lift`/`Blackboard`/
   `CompoundObject` never removed it from `objects_`, leaving permanent
   dangling pointers that `totl`/`enum` dereference forever after.
   Fixed by mirroring the one correct sibling implementation
   (`Object::unregister_from_non_scenery_object_registry`).

   The follow-up commit's correction: initial diagnosis of *remaining*
   Creature-family population growth blamed a "restock population"
   world script for checking the wrong genus — **the user corrected
   this immediately and correctly**: that script is a Grendel Mother,
   and checking for genus=2 survivors is exactly right. This led
   directly to finding fix #6.
6. **`a91d6d6`/`bf141d7`** — `new-crea-genome-argument-was-bracket-
   text-not-rvalue.md`. **This is probably the most consequential fix
   of the session.** `NEW: CREA <genome-id> <sex>`'s first argument was
   parsed with `read_bracketed_text_argument()` (requires a literal
   `[...]`). The standard, correct CAOS pattern —
   `new: gene tokn gren 0 var0` then `new: crea var0 1` — passes the
   genome id as a bare variable, never bracketed. The bracket-reader
   aborted silently and fell back to genome id 0 (empty genome), so
   **every creature ever spawned via a variable-held genome id** (any
   Grendel Mother, egg layer, breeding machine, incubator — this is the
   general pattern, not save-specific) silently became a broken,
   geneless creature defaulting to genus=1/species=0 regardless of what
   was actually requested. Confirmed against native
   `Macro::ExecuteNewCommand @ 0x0041d130`'s real `'crea'` case: both
   operands are plain `ParseRValue` calls, matching every other `new:`
   subcommand — there was never a bracket-text form for this specific
   argument. Live-verified: a previously-observed runaway population
   growth (~1 broken creature every 2-4s, forever) is completely gone,
   and the exact Grendel Mother script pattern now correctly produces
   `gnus==2` creatures.

7. **`d072f41`/`74f2dfa`** — `pixel-cache-skeleton-lifetime-host.md`.
   Removed a per-call Windows Skeleton lifetime adapter that was retained
   after its stack lifetime ended. Skeleton lifetime operations now route
   through the stable document-owned host. This was a real dangling-host
   bug, but it did **not** close the separate shared pixel-cache crash;
   dump `15388` reproduced that crash afterward.
8. **`08813cf`/`537bf35`** — fixed the creature-rendering and pianola
   interaction lifetime wiring. The Windows document now supplies stable
   creature/pickup host state, removing one use-after-scope path associated
   with creature interaction.
9. **`95e54e2`/`0cac059`** — corrected the saved body attachment stream
   order in `Body::read`. This is why the creature sprites are now correct
   and the Norns look substantially better. The previously reported mangled
   body/attachment issue is now considered **resolved** after visual review;
   no further Skeleton attachment investigation is currently outstanding.
10. **`087651e`/`39b000a`** — restored the Tip of the Day artwork painting
    path on Windows (bitmap resource 103 and the native artwork-control
    behavior). The earlier control-color fix is also complete. The tip
    dialog icon/artwork is no longer an outstanding item.

## Current status after the subsequent fixes (2026-09-14)

The original report below remains useful as the symptom record, but its
triage state has changed. The active queue is now: pixel-cache crash;
Vehicle/CompoundObject position/sound geometry; then the interaction,
eye-view, and pause-state clusters. The recent fixes above are closed fixes,
with behavior verification still required where explicitly noted.

## Runtime bug reports from the user (2026-09-14)

Verbatim, then triage notes:

> norns are still invisible, submarine's buttons aren't clickable for
> the hand. creature's view window is a tiny black box that doesn't
> function. hand's interaction with the world and creatures appears to
> be inconsistent, I found the "scratch/pat" animation playing while
> interacting with objects with the hand. got a few more crash dumps in
> the 140926 folder. pausing and unpausing the game seems to produce
> all kinds of side effects that can include freezing the hand. we
> really still have a lot of wiring faults, I also watched the
> submarine stay in place while the sounds got made like it was moving
> on this test.

Individually:

1. **Creature body attachment/rendering — resolved.** Fix #9 corrected the
   saved attachment stream order, and visual review now finds the Norns and
   their bodies correct. Treat this as closed unless a fresh regression
   appears. Do not conflate this with fix #4: its
   `sound_source_x/y`/`current_visual_width/height` wiring corrected
   CAOS/sound-facing accessors, not actual drawing.
2. **"Submarine's buttons aren't clickable for the hand"** — likely a
   `CallButton`-related interaction or hit-testing bug specific to
   objects with `kIsVehicle`/vehicle-bounds semantics (see
   `Object::kIsVehicle`, and the `has_special_vehicle_footprint` /
   `(classifier & 0xffff0000U) == 0x03010000U || ...0x03020000U` special
   case seen this session in `object.cpp`'s topmost-object-under-cursor
   logic — vehicles get bespoke interaction-bounds handling that plain
   objects don't; a submarine's own call-button-like UI elements may be
   falling through a gap in that special case).
3. **"Creature's view window is a tiny black box that doesn't
   function"** — this is the "follow selected creature" / creature's-
   eye-view feature. Almost certainly a viewport-sizing or renderer-
   target bug in `C1EyeView` (seen in `list_classes` earlier this
   session: `CreateWindow`, `OnPaletteChanged`, `OnQueryNewPalette`,
   `OnSize`, `UpdateSelectedCreatureFollowViewport`,
   `UpdateWindowTitleForSelectedCreature` — native class name
   `CEyeView`). Search this port for its equivalent and check whatever
   sizes/blits into it.
4. **"Hand's interaction with the world and creatures appears
   inconsistent"** — broad; likely several distinct bugs under one
   report. `PointerTool::process_pending_input`
   (`src/c1/ui/tools.cpp`) is the central hand-click dispatcher; this
   session confirmed (via disassembly of native's real
   `PointerTool::ProcessPendingInput @ 0x00428860`) that this port's
   `finish_pending_input()` call is correctly, unconditionally wired —
   so the *flag-clearing* mechanism is NOT the bug (already checked,
   don't re-check it). The actual inconsistency is more likely in the
   hit-testing (`find_topmost_overlapping_object`, `GetBounds` per
   object kind) or in per-object-kind click-event mapping
   (`click_event_id_at_world_position` overrides differ across
   `Object`/`SimpleObject`/`CompoundObject`/`Creature` — worth a
   systematic diff of all four against native's real vtable slot 28
   for each class, the same technique used for Skeleton's slots 30-33
   in fix #4).
5. **"'scratch/pat' animation playing while interacting with objects
   with the hand"** — sounds like a **creature** reaction script/event
   being incorrectly triggered by hand-on-*object* interaction (should
   only fire for hand-on-*creature* interaction, i.e. petting). Check
   `queue_creature_context_zero`/`find_creature_source` in
   `src/c1/objects/simple_object.cpp` (read this session, real code —
   this is the "attribute this interaction stimulus to a nearby
   creature" path) and whatever dispatches the pat/scratch touch
   stimulus specifically — likely a stimulus-context or event-id
   mismatch causing the wrong creature-reaction script to fire for
   object interactions.
6. **Crash dumps** — see the Crash Dump Inventory section below. The new
   `15388` dump is now cross-referenced and confirms that the pixel-cache
   crash remains live after the Skeleton lifetime-host fix.
7. **"Pausing and unpausing the game seems to produce all kinds of side
   effects that can include freezing the hand"** — not investigated at
   all. Search for the pause/unpause command handler (likely a CAOS
   `PAUS`/menu "Pause" toggle) and whatever per-tick update loop it
   gates — a frozen hand strongly suggests the pointer tool's own
   `tick()`/`update_unbounded_position_and_redraw()` (see fix-adjacent
   code in `simple_object.cpp`, read this session) is gated by the same
   pause flag as world simulation, when it should stay live even while
   paused (real C1 lets you still move/click the hand while paused).
8. **"Submarine stay[ed] in place while the sounds got made like it was
   moving"** — position/sound desync. Given fix #3 (xvec/yvec) already
   corrected the vehicle's real velocity write, and fix #4 corrected
   `sound_source_x/y` for creatures specifically (NOT vehicles — double
   check whether `Vehicle`/`CompoundObject` have their own correct
   `sound_source_x/y` overrides already, or whether they ALSO silently
   fall through to `Object`'s always-0 base stubs the same way
   `Skeleton` did before fix #4). **This is a very promising, concrete,
   probably-fast lead**: check `objects/vehicle.cpp` and
   `objects/compound_object.cpp` for `sound_source_x`/`sound_source_y`/
   `current_visual_width`/`current_visual_height` overrides — if
   `Vehicle` is missing them the same way `Skeleton` was, sound would
   compute from position (0,0) or from `CompoundObject`'s part-0
   position while the actual physical position (driven by the now-
   correctly-updating `velocity_x_8_8`/`velocity_y_8_8` fields) moves
   independently, exactly matching "sound moved but the sprite didn't."
   grep for these four method names by *method name*, not by class
   name, per the search-before-declaring-absence rule.

## Crash dump inventory (`/home/harri/creatures/140926crash/*.dmp`)

Resolved via `harness/parse_minidump.py` + cross-reference against a
freshly built `.map` file (method in the harness section above). All
paths relative to `/home/harri/creatures/140926crash/`.

| Dump | Exception | Resolves to (this build's own symbols) | Status |
|---|---|---|---|
| `Creatures.exe.28020.dmp` (08:43) | `0xc0000094` DIVIDE_BY_ZERO @ offset `0x1d469` | `Lobe::update_early_phase` | **Likely already fixed** by commit `d67212d`/`afca2d6` — verify dump timestamp vs. fix commit time before treating as open |
| `Creatures.exe.17648.dmp` (09:06) | `0xc0000005` ACCESS_VIOLATION @ offset `0x377bf` | `Image::configure` (or the gap right after it) | Same crash family as the two below — see the pre-existing `crash_checkpoints/2026-09-14-submarine-pixel-cache-dangling-pointer.md` doc, still **open** |
| `Creatures.exe.23544.dmp` (09:10) | `0xc0000005` @ offset `0x37884` | `Image::load_pixel_data` | Same crash family, still **open** |
| `Creatures.exe.26328.dmp` (10:39) | `0xc0000005` @ offset `0x366b4` | `blit_zero_transparent_pixels` | **Not yet cross-referenced against the pixel-cache doc — check whether this is the same dangling-LRU-pointer bug reached via a different call path, or a genuinely separate rendering crash.** Possibly directly relevant to the "Norns are invisible" report — a blit-time crash in the transparent-pixel path is exactly the kind of thing that would make a sprite silently fail to draw if the crash is being caught/suppressed somewhere, or corrupt state for later draws if it partially executes. |
| `Creatures.exe.27700.dmp` (18:26, NEW) | `0xc0000005` @ offset `0x377ff` | `Image::get_pixel_data` | **Exact same function as the already-documented, still-open pixel-cache dangling-pointer bug** (`crash_checkpoints/2026-09-14-submarine-pixel-cache-dangling-pointer.md`). Confirms it was still live and reproducing after the earlier fixes — none of those fixes touched this bug. **This is the single highest-confidence, most-reproduced open crash** (four dump entries now hit the same `Image`/pixel-cache LRU region: `configure`, `load_pixel_data`, and `get_pixel_data` are all part of the same subsystem). |
| `Creatures.exe.15388.dmp` (20:25, NEW) | `0xc0000005` @ `0x004378c4` | `Image::load_pixel_data` | **Same pixel-cache dangling-pointer family.** Cross-referenced against the current build map (`Image::load_pixel_data` begins at `0x00437870`), so the fault is inside that function. It occurred after the Skeleton lifetime-host fix and confirms that fix was related but not sufficient. |
| `Creatures.exe.3640.dmp` (18:31, NEW) | `0xc0000005` @ ntdll+`0x87b71` | (in `ntdll.dll`, not this port's code directly) | **Not yet analyzed.** A crash inside `ntdll` almost always means heap corruption detected by Windows' own heap manager, or a stack overflow — i.e. a downstream *consequence* of earlier corruption, not the root cause itself. Given the timestamp is 5 minutes after `27700`'s pixel-cache crash, **strongly consider these are the same play session and this ntdll crash is fallout from the same dangling-pointer bug corrupting the heap**, not an independent third bug. Get the full register/stack state and check what's on the stack near the fault. |
| `Creatures.exe(1).3640.dmp` (18:31, NEW) | `0xc000041d` FATAL_USER_CALLBACK_EXCEPTION @ same ntdll address | same | Paired with the dump above (same PID, same address) — likely two views of the *same* crash captured by two different exception filters, not two separate crashes. |

**Recommended next investigation, in priority order**:
1. The `Image` pixel-cache dangling-pointer bug (now 4 confirmed hits,
   including `15388` after the Skeleton lifetime-host fix — this is the
   single most-reproduced open crash and plausibly explains part of the
   creature invisibility/general instability reports). Full
   existing investigation, ruled-out hypotheses, and exact fault-site
   code are already written up in
   `ground_truth/crash_checkpoints/2026-09-14-submarine-pixel-cache-
   dangling-pointer.md` — **read it in full before starting**, it
   already rules out several plausible-looking causes (Gallery/Image
   array reallocation, double-release paths, dead code). The doc's own
   suggested next steps: get a `MiniDumpWithFullMemory` dump next time
   (current dumps lack the freed object's prior heap contents), or
   trace live with a debugger/instrumented build.
2. Check `Vehicle`/`CompoundObject` for the same missing
   `sound_source_x/y`/`current_visual_width/height` overrides pattern
   as fix #4 (Skeleton) — cheap to check, plausibly explains the
   submarine sound/position desync report directly.
3. The remaining interaction, eye-view, pause/unpause, and dump-`3640`
   investigations in the bug list above.

## Known lower-priority behavior gap

The embedded-kit protocol still has one documented mismatch in `cell`
field 6: the decision-lobe dendrite-state sum differs. The earlier claims
that this was an out-of-bounds read or a missing decrement were retracted;
the current hypothesis is a lobe-weight/life-stage timing difference. It is
not currently a crash, rendering, or interaction blocker and should remain
behind the runtime queue above.

## Addendum (same day, later): what Codex did with this handoff

Codex picked this up and landed four more commits, all pushed to both
repos (`byte_match` and `LibreCreatures`, verified in sync as of
`087651e`/`39b000a`). In order:

7. **`d072f41`/`74f2dfa`** —
   `ground_truth/00-working-notes/pixel-cache-skeleton-lifetime-host.md`.
   **This resolves the #1-priority item from this doc** (the
   `Image::get_pixel_data` dangling-pointer crash, 3+ confirmed dump
   hits). Root cause: `Skeleton` stored a `SkeletonLifetimeHost*
   lifetime_host_` pointing at a **stack-scoped adapter** supplied by
   `WindowsCreatureEnvironmentHost::initialize_from_genome`/
   `WindowsCreatureConstructionHost` — both destroyed when their call
   returned, while the `Skeleton` itself (owned by the world-lived
   `Creature`) survived. Later `Skeleton` cleanup dispatched through
   the dead adapter while releasing gallery-owned images, corrupting
   the shared pixel-cache LRU. Fix: `C1WindowsDocument` now implements
   `SkeletonLifetimeHost` directly (document lifetime covers all
   world-owned creatures); no `Skeleton` stores a per-call host anymore.
   Verified with a 120-second populated crashed-save Wine soak, no
   crash. **This is exactly the "same lifetime-host-pointer" bug class
   as fix #5's `unregister_from_object_registry` mixup — worth
   remembering as a recurring pattern in this codebase**: several
   `*Host` adapters in `windows_macro_host.cpp`/
   `windows_creature_hosts.cpp` are constructed as short-lived stack
   objects per-call, and it's easy to accidentally stash a pointer to
   one somewhere longer-lived. Audit for this pattern before assuming a
   new crash is something exotic.
8. **`08813cf`/`537bf35`** — "Fix creature rendering and pianola
   interaction lifetime" (no commit body written — check the diff
   directly if the summary here isn't enough). Extends fix #7's pattern:
   `Vehicle`/`Blackboard`/`Lift`/plain `CompoundObject` construction
   never called `set_lifetime_host(&document_)` at all, so
   `CompoundObjectLifetimeHost`'s unregister/renderable-set cleanup was
   a **silent no-op** for all four kinds until now (`C1WindowsDocument`
   now implements `CompoundObjectLifetimeHost` too). Also fixed
   `selected_creature_is_bounded()` (was hardcoded `false` — relevant to
   hand/creature interaction). Also fixed `harness/run_c1_smoke.py` to
   stage a save's own `.SPR` creature sprite files beside `World.sfc`
   (previously a test run silently combined save metadata with stale
   generated sprites from an earlier run, causing "horizontal
   corruption" — a **test-harness fidelity bug**, not a game-code bug,
   but it means earlier this-session visual verification could have
   been looking at corrupted-by-harness rendering rather than the
   port's real output; re-verify anything visual with the fixed
   harness).
9. **`95e54e2`/`0cac059`** — "Fix saved body attachment stream order".
   **Very likely a direct contributor to "Norns are invisible."**
   `Body::serialize`'s `join_x`/`join_y` limb-attachment table
   (consumed by `Skeleton::recompute_body_part_layout`'s
   `join_x[leg_index][body_view]`/`join_y[...]` — this is the exact
   code this session traced through while investigating
   `sound_source_x/y` for fix #4) was archived as an X matrix followed
   by a Y matrix; native's real format interleaves each view as an
   `(X,Y)` pair. Every loaded save had every limb's attachment offset
   scrambled. No live-verification numbers recorded in the commit body
   — **worth confirming with the same `posl`/`posb`/`enum` CAOS-pipe
   technique used for fix #4**, now that both this and fix #7's harness
   correction are in, to see if creature bounding boxes are now sane.
10. **`087651e`/`39b000a`** — "Restore tip dialog artwork painting".
    Cosmetic only (the startup Tip-of-the-Day dialog's bitmap/header
    text were never painted). Unrelated to the bug list.

**Housekeeping done in this addendum pass** (by the session reading this
note, after Codex's work): LibreCreatures' `origin/main` was 3 commits
behind its local `HEAD` (`537bf35`/`0cac059`/`39b000a` existed locally
but were never `git push`ed) — pushed now, confirmed in sync. The
durable build artifact (`/home/harri/creatures/builds/Creatures.clean.exe`
+ `BUILD_INFO.txt`) was also stale (still reflecting fix #7's
predecessor, `d072f41`) — rebuilt from current `HEAD` (139/139, clean)
and updated. **Lesson for whoever picks this up next: check
`git log origin/main..HEAD` in `LibreCreatures` and diff
`BUILD_INFO.txt`'s recorded commit hash against actual `HEAD` early —
both silently drifted here.**

None of Codex's four fixes have been independently live-verified by
the agent writing this addendum (time went to sync/build housekeeping
instead). The bug-list triage table above (items 1-8, "NEW bug reports
from the user") should be re-walked against a build made from current
`HEAD`, since 3 of those items (invisible Norns, general "wiring
faults," the pixel-cache crash implicitly underlying instability) may
now be partially or fully resolved by fixes #7-9. Don't assume they're
fixed without testing — but don't re-investigate from scratch either;
start by testing, and only dig deeper into whichever items are still
reproducing.

## Addendum 2 (same day, later still): live-verification results + a new lead

Re-tested the current `HEAD` build (`087651e`/`39b000a`) against the
user's crashed save, both by reviewing screenshots Codex left in `/tmp`
from its own session and by live-driving a fresh Wine session directly.

**Confirmed working, with real evidence**:
- Creatures render correctly and visibly (Codex's `/tmp/c1-body-attach-
  fixed*.png` screenshots, and this session's own live screenshots, both
  show fully-formed, correctly-posed creature sprites — not invisible,
  not corrupted). Fix #9 (body attachment stream order) appears to have
  resolved the "Norns are invisible" report, at least for the cases
  checked.
- Hand hover-interaction works: hovering the hand over a creature shows
  its name ("Female didi"), and hovering a `pianola`-like object shows
  its verb tooltip ("push") plus in-progress word-learning state
  ("Female push bab" / "Male dis[appear]" — a creature mid-way through
  associating a verb/noun pair with the interaction). This is real,
  live evidence against the "hand interaction inconsistent" report, at
  least for hover/tooltip behavior — click-through behavior (actually
  triggering an action) was not conclusively tested (see below).
- **The `NEW: CREA` fix (fix #6 from the original session) was
  confirmed organically, not just by direct CAOS testing**: left a real
  game session running for ~2 minutes with no interaction, and the
  save's own Grendel Mother timer fired on its own and produced a real,
  correctly-classified `gnus==2` creature (`totl 4 0 0` went from 2 to
  3, `totl 4 2 0` went from 0 to 1, matching a real, natural gameplay
  event rather than a manual test script). This is about as strong a
  confirmation as this fix could get.

**Inconclusive / not confirmed either way**:
- Could not get a clean, confident test of the submarine's own call
  buttons specifically — locating the submarine visually required
  scrolling a large distance and repeated attempts didn't reliably land
  the viewport there (see the CMRA finding below, which may be why).
  Clicked a different vehicle's control-button-like sprites (a "cable
  car") with no visible reaction, but this is weak evidence — could be
  a missed click target as easily as a real non-functional button.
  **Still needs a proper repro.**
- Opened a menu (`Creatures`) and its popup rendered as a solid black
  rectangle with no visible item text. This COULD be evidence relevant
  to the "eye view is a tiny black box" report (a shared owner-draw/
  paint issue affecting more than one window), but it could equally be
  a **screenshot-capture artifact**: `xwd -root` (used for all
  screenshots this pass) is known to sometimes fail to correctly
  composite popup/override-redirect windows under Xvfb+Wine, showing
  them as black even when they're actually painting fine. **Do not
  treat this as a confirmed bug without a capture method proven to
  render popups correctly** (try a window-specific capture instead of
  `-root`, or a real X compositor, before concluding anything here).
  The eye-view window itself was never actually opened and tested this
  pass — `create_eye_view_window`'s default size (128x96, cross-
  referenced in-code against a native `g_eye_default_rect` global and
  the follow-viewport's own 0x40/0x30 half-extents) looks intentionally
  small and native-faithful, so "tiny" may be correct-and-expected
  while "doesn't function" (if true) would be a separate rendering
  problem inside that window, not a sizing one. Trace
  `C1EyeViewWindow::publish_follow_viewport`/
  `update_selected_creature_follow_viewport` live with an actually-
  opened eye view before concluding anything further.
- "Scratch/pat animation playing during object interaction",
  "pause/unpause side effects including a frozen hand", and "submarine
  stayed in place while sound indicated movement" were **not
  investigated at all** this pass — no code reading, no live testing.

**Investigated, turned out NOT to be a bug**: CAOS `CMRA <x> <y>` (set
camera/viewport origin) appeared to have no effect — `CMRX`/`CMRY`
never changed after issuing it. **User's explanation, confirmed
correct**: "creature smooth tracking" (`ViewportNavigationMode::
follow_selected_creature`, the default mode per `ui/views.hpp`) was on
and had a creature selected, so the main viewport was being
re-centered on that creature every tick — any manual `CMRA` write gets
silently overwritten by the very next per-tick follow-camera update
before the next query can observe it. This is expected, intentional
behavior, not a bug: same mechanism as the eye view's own
`update_selected_creature_follow_viewport`, just for the main view.
**No fix needed here.** (The manual scrollbar drag "worked" earlier in
this same testing because dragging a scrollbar is exactly the kind of
input that's supposed to switch navigation to manual mode and disable
follow — consistent with `enable_viewport_navigation()`'s own comment,
seen earlier this session, "SetViewportNavigationMode(1) is the manual
mode.") If testing camera-dependent behavior again, either deselect the
creature first or explicitly disable smooth tracking before relying on
`CMRA`/`CMRX`/`CMRY`.

**Housekeeping**: pushed all pending commits to `LibreCreatures`'
`origin/main` (was 3 commits behind), rebuilt the durable artifact from
current `HEAD` (139/139 clean) and updated `BUILD_INFO.txt`.

## General reminders for continuing this work

- Always live-test against `/home/harri/creatures/140926crash/w2/World.sfc`
  (the user's real crashed save, 2 real creatures — a Norn pair,
  species 1/2 — plus 23 Vehicle/Lift/Blackboard/CompoundObject and 165
  SimpleObject/CallButton/PointerTool objects, confirmed exact via
  `parse_sfc.py` this session) when a fix is creature- or vehicle-
  related; it's the closest thing to a real repro environment available.
- `/home/harri/creatures/re_work/tools/parse_sfc.py` is this project's
  own byte-exact `World.sfc` parser (confirmed end-to-end against a
  real specimen) — use it to get ground-truth object/classifier/script
  data straight from a save file rather than guessing at save contents.
  Note: `Creature` objects store their `Object`-base fields under a
  `skeleton_object_base` key in the parsed dict, not a flat `base` key
  like every other object kind — a one-off shape quirk, not a bug in
  the parser.
- Don't trust a single short observation window for anything population-
  or timer-driven — this session's Grendel Mother investigation needed
  a continuous 30-110 second window to distinguish "stable" from "still
  slowly growing."
- When in doubt about whether a symptom is a port bug or a bug in the
  *save file's own CAOS script content*: assume port bug until you have
  concrete disassembly evidence of native doing the same wrong thing.
  Getting this backwards cost real time this session (see fix #5's
  follow-up commit).
