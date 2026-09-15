# Native creature attachment rows and eye-frame selection

2026-09-15. Supersedes earlier claims that the Body archive fix fully restored
native attachment ordering.

## Evidence and corrections

The user's `knowngood` world was created by the original executable, with two
living norns and their matching sprite/genome files. The original directory
was kept read-only; runtime testing used a copy under `.c1-smoke-run/knowngood`
with this as the secondary world resource tree, separate from install assets.
World.sfc SHA256:
`9ece5297ca9a996da1a3ca95a68e3a3654e534e6807fd1479005148eb9dd4b7a`.

1. Body::Serialize at native `00416290` stores six chain rows, each containing
   ten interleaved X/Y view pairs. Raw instructions at `00416355..0041636c`
   increment the inner view index to 10, then advance the outer chain base
   by 10, for six rows. The prior interleaving correction used view-outer,
   chain-inner loops, still transposing the archive. Both read and write now
   use chain-outer, view-inner. Do not trust the obsolete Ghidra comment
   describing two separate matrices.

   Independent reconstruction from the native save's down-foot coordinates,
   body offsets and linked limb attachment frames:

   | Norn | Old body position | Native/corrected body position | Wrong part positions, old/new |
   | --- | --- | --- | --- |
   | 3FZK | 3623,900 | 3647,892 | 9 / 0 |
   | 1MBV | 3883,884 | 3881,885 | 9 / 0 |

2. Skeleton::RecomputeBodyPartLayout `0043b5a0` does not always add 13 to
   the head image. `0043b897` loads eyes-open from byte +0x11e;
   `0043b8ca` tests it and `0043b8d9 CMOVNZ ECX,EAX` selects the expression
   adjustment WITHOUT +13 when eyes are open. The decompiler presented an
   empty branch and the port consequently always drew closed eyes. Preserve
   the existing drive/expression adjustment, adding 13 only for closed eyes.

## Verification

- `tests/c1_body_attachment_archive_test.cpp`: real archive reader/writer
  against an independently specified native-order stream. Failed before
  correction; passes with ASan/UBSan afterward. Not just a symmetric roundtrip.
- `tests/c1_skeleton_eye_frame_test.cpp`: real Skeleton layout method,
  four facings x six head poses x three expression states x both eye states.
  Failed before correction; passes afterward as a 32-bit Windows console
  binary under the existing Wine test prefix. No sanitizer claim for this
  test (host lacks 32-bit Linux C++ headers).
- Private MSVC clean-source build passes, 139 sources; final incremental
  rebuild compiled skeleton.cpp only after the earlier body.cpp rebuild.
- Managed GUI display :99 reused. Native save reload shows two correctly
  joined, open-eyed norns. Screenshot:
  `.gui-user/screenshots/2026-09-15_21-52-25-323.png` in workspace root.
  Directory-ownership-only screenshot `21-41-27-619` still showed piles;
  body-order-only screenshot `21-45-42-661` showed joined but closed-eyed norns.
- Fresh egg hatching, long-term locomotion/brain behavior, and recovery of
  saves previously written with the transposed attachment format remain
  unverified. No heuristic conversion of old malformed port saves is added.

## Reproduce console tests

From either repository root, with a pre-existing output cache directory:

```sh
g++ -std=c++17 -g -fsanitize=address,undefined -ffunction-sections -fdata-sections \
 tests/c1_body_attachment_archive_test.cpp src/c1/creatures/body.cpp \
 src/c1/objects/entity.cpp -Wl,--gc-sections \
 -o /tmp/bm-cache/c1_body_attachment_archive_test
/tmp/bm-cache/c1_body_attachment_archive_test

i686-w64-mingw32-g++ -std=c++17 -O2 -static -ffunction-sections -fdata-sections \
 tests/c1_skeleton_eye_frame_test.cpp src/c1/creatures/skeleton.cpp \
 src/c1/creatures/body.cpp src/c1/creatures/genome.cpp \
 src/c1/objects/entity.cpp src/c1/objects/object.cpp src/c1/world/geometry.cpp \
 src/c1/display/gallery.cpp src/c1/display/image.cpp src/c1/display/palette.cpp \
 src/c1/display/sprite_cache.cpp src/c1/display/blit.cpp src/c1/sound/sound.cpp \
 src/c1/scripting/classifier_scripts.cpp src/c1/scripting/tables.cpp \
 src/c1/scripting/macro.cpp src/c1/scripting/macro_holder.cpp \
 -Wl,--gc-sections -o /tmp/bm-cache/c1_skeleton_eye_frame_test.exe
env DISPLAY=:99 WINEPREFIX=/home/harri/.c1-smoke-prefix WINEDEBUG=-all \
 timeout 15s wine /tmp/bm-cache/c1_skeleton_eye_frame_test.exe
```
