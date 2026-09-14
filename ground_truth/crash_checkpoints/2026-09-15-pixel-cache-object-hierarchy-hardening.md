# Pixel-cache and in-game object-hierarchy hardening

Date: 2026-09-15

## Scope

This pass follows the runtime handoff and the CallButton left-click crash
report. It covers two recurring port failure classes:

1. shared pixel-cache LRU links surviving image/gallery lifetime changes;
2. derived in-game object types bypassing inherited behavior or indexing
   derived storage without validating the base-owned state.

## Pixel-cache result

PixelCacheState::lru_entries is now the canonical LRU order. The legacy
Image::cache_prev_/cache_next_ fields remain maintained as a native-layout
and diagnostic view, but no cache operation follows them as its source of
truth.

The cache now:

- unlinks an image through its canonical list position;
- refuses to relink an image into a cache that did not load it;
- removes stale non-resident list entries so eviction cannot retry forever;
- detaches resident data before Image::configure or archive load overwrites
  image state;
- scans the canonical list for eviction, skipping protected images;
- avoids byte and entry-counter underflow.

This directly addresses the previously observed invalid predecessor write in
Image::get_pixel_data. It still needs a fresh Windows runtime reproduction
to prove the CallButton/redraw crash no longer occurs.

## Object-hierarchy result

The runtime hierarchy was audited in leaf-first order:

- CallButton, PointerTool, and Bubble use SimpleObject behavior;
- Vehicle, Lift, and Blackboard use CompoundObject behavior;
- Skeleton and Scenery remain intentional direct-Object special cases;
- Creature remains the document's reverse-mapped owner of its Skeleton.

Concrete fixes:

- prld now dispatches to the host-aware preload implementation for
  SimpleObject and CompoundObject descendants;
- archive reference compatibility accepts the real base/derived relations:
  CallButton/PointerTool/Bubble -> SimpleObject,
  Vehicle/Lift/Blackboard -> CompoundObject, and Lift -> Vehicle;
- image-sequence and pose operations validate object parts, entities,
  galleries, and image ranges before dereferencing them;
- current-image bounds use a null-safe, exclusive upper-bound check.

The existing tick, event, archive payload, image-setting, and redraw dispatch
paths were checked for inheritance order. No additional missing override was
confirmed there.

## Verification

- The clean-source MSVC/Wine build passed for 139 source files.
- Resource compilation passed.
- Full link passed and produced a clean executable/map in the disposable
  build directory.
- Linux -Wall -Wextra -Werror syntax checks passed for the image/entity
  slice in both repositories.
- The full object slice cannot be checked with the host Linux compiler because
  the project intentionally asserts 32-bit event layouts; the available
  32-bit headers are not installed.
- The changed source files are byte-for-byte identical between
  byte_match and LibreCreatures.

## Next runtime check

Build/install this exact clean executable on Windows, reproduce:

1. fresh world, hand right-click pickup then drop;
2. left-click the CallButton call button;
3. normal redraw/scrolling through objects with different derived types.

If it crashes again, retain the new .dmp and the exact .exe/.map
pair so the fault can be mapped without ambiguity.
