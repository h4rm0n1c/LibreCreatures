# Click fallback and lift phase — 2026-09-15

User's Windows renderer log repeats index=9 count=9 for one moving entity.
Stock Eden.sfc confirms the mechanism: a call-button feedback lookup for
2/2/0 event 50 incorrectly accepted ANY family-2 event-50 definition. Its last
candidate is 2/4/8 event 50: `anim [1234567890R],over,snde drip,...`.
Executed on the nine-frame hand, that loops through invalid index 9 and waits
on OVER indefinitely. The correct failed lookup lets PointerTool fall back
to its own 2/1/1 event 50 animation instead.

Native ExecuteScriptForClassifier @00419e00 compares full packed classifiers
against query & 0xffff00ff, then query & 0xff0000ff. These require explicit
zero species, then zero species AND genus. The port omitted the zero checks
and also cleared the wrong semantic field in its first fallback. Both fixed.

Separately, Lift::select_nearest_call_button_and_start_move had no caller.
Reference executable re_work/windows/Creatures/Creatures.exe has Lift vtable
base 0x0045760c: slot 41 = 0x0042c4f0 (Tick), slot 42 = 0x0042c590 (call
selection). Native phase 7 @00432ce0 invokes slot 42 on tick-enabled non-scenery
objects. WindowsDriveThresholdObject wrongly assumed only Creature overrides;
it now dispatches Lift call selection too, retaining the existing phase timing.

Save-on-close failure is NOT diagnosed yet. OnSaveDocument previously caught
everything silently; Creatures.save.log now records standard/MFC exceptions
and the destination path. No save-format or save-deletion policy changed.

Verification: classifier test exercises actual script lookup/loading, rejects
unrelated feedback, checks both explicit wildcard levels, exact precedence,
and family/event isolation. ASan/UBSan (vptr disabled to isolate this link)
passes; previous classifier resolver fails its first assertion. Build and
Windows reproduction results should be distinguished: gameplay is not yet
verified on Windows for these changes.
Both repositories independently compiled and linked all 139 translation units
(136 cache hits, three rebuilt) with the pinned Windows toolchain.

Test command (private repo):
```
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize=vptr \
 -ffunction-sections -fdata-sections -Wl,--gc-sections \
 tests/c1_classifier_fallback_test.cpp src/c1/scripting/classifier_scripts.cpp \
 src/c1/scripting/macro.cpp src/c1/scripting/tables.cpp \
 -o /tmp/c1-classifier-fallback-test
/tmp/c1-classifier-fallback-test
```
