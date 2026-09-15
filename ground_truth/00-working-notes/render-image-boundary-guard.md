# Render image boundary guard — 2026-09-15

The Windows reports Creatures.exe.28100.dmp and Creatures.exe.28144.dmp
map to the current hardening build's transparent blit and Image::load_pixel_data
respectively. The latter dereferences gallery pointer 0x006c0061.
These identify invalid image state, but do not establish the first corrupting
write or prove that an out-of-range Entity index caused either crash.

Correction to the initial diagnosis: a blit's copy width can be clipped.
A 4-by-39 copy alone does not establish a malformed 4-by-39 sprite.
Likewise, a render crash does not establish whether the queued button event
had already run. Both claims require additional runtime evidence.

Gallery::create_gallery and serialize_gallery allocate exactly image_count
Image elements. WorldRenderer's visibility pass previously accepted equality
and its blit pass indexed the array without revalidation. The recovered native
renderer at 0x00412aa0 also uses the inclusive predicate, so the new exclusive
guard is intentional memory-safety hardening, not a byte-parity claim.

Both passes now use Entity::has_current_image before indexing. Invalid states
are skipped and up to 32 diagnostics per process are appended to
Creatures.render.log in the game working directory, with Entity and Gallery
addresses, index/count and world position. Entity has no owning Object pointer,
so classifier identification still requires registry reverse mapping.

This prevents the confirmed out-of-bounds access path; it does not validate
the lifetime of an arbitrary non-null Gallery pointer or repair corrupt Image
contents. A fresh Windows lift-button reproduction remains necessary.

## Verification

Both repositories compiled and linked successfully with the pinned Windows
toolchain (139 translation units each; separate build directories).
The private harness exercises the actual renderer with a valid index, equality
to count, an index above count, a null image array, and an index becoming invalid
between visibility and blitting. All five pass under ASan and UBSan:

```sh
g++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  harness/render_image_boundary_test.cpp \
  src/c1/display/rendering.cpp src/c1/objects/entity.cpp \
  src/c1/display/image.cpp src/c1/display/blit.cpp \
  src/c1/world/geometry.cpp -o /tmp/c1-render-image-boundary-test
(cd /tmp && ./c1-render-image-boundary-test)
```

Negative control: linking this test against the previous renderer fails with
an ASan heap-buffer-overflow in Image::width in the visibility pass.
Wine smoke testing did not reach the command pipe; a startup-only retry exited
early. No gameplay commands were delivered, so lift interaction is NOT verified.
