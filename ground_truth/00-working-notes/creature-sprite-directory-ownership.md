# Own resolved creature sprite directories

Date: 2026-09-15

## Proven defect

`C1WindowsDocument::secondary_resource_directory()` returns `std::string` by
value. Both `sprite_file_search_paths()` and `skeleton_services()` put that
temporary into a returned aggregate's `std::string_view`. The string dies at
the end of the return expression, before the caller can use the view.

This affects lazy sprite pixel reads as well as generated creature sprites.
The creature initialization host retains the service bundle as `services_`,
so making its factory call synchronously does not rescue the dangling view.
The selected-world routing change exposed this lifetime error; rebuilding
resource hosts at document boundaries does not address it.

## Native comparison and scope

Ghidra `Skeleton::LoadGenome` at `0043c800` copies the application's persistent
secondary Images directory into the path buffer before creating the generated
SPR and resolves source body sprites using resource slot 13 (primary fallback
4). The port's temporary-string ownership is a C++ adapter defect, not a
reason to change native directory selection, archive layout, or poses.

`SpriteFileSearchPaths` and `SkeletonSpriteBuildServices` now own both directory
strings. The path pair accepts string views but copies them on construction.
Copying/replacing bundles cannot invalidate a previous bundle's directories.
The neighboring backup views are safe: their backing local strings remain
alive until the synchronous backup function returns, so those were unchanged.

## Reference and verification

The user's `knowngood` directory is a read-only native reference containing two
living garden norns. Tests use `/home/harri/.c1-smoke-run/knowngood`, a copy,
with the staged install as primary and this separate world as secondary.

- Native `World.sfc` SHA256:
  `9ece5297ca9a996da1a3ca95a68e3a3654e534e6807fd1479005148eb9dd4b7a`.
- The archive parser consumes all 345565 bytes without alignment errors.
- `1MBV.SPR` and `3FZK.SPR` each contain 136 frames; every frame's pixel range
  fits its file (87204 and 93280 bytes respectively).
- `tests/c1_sprite_path_lifetime_test.cpp` passes under ASan/UBSan using:
  `g++ -std=c++17 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/c1_sprite_path_lifetime_test.cpp -o /tmp/bm-cache/c1_sprite_path_lifetime_test`.
  Run the resulting executable. It exercises returned short/long paths,
  bundle copies/reassignment, allocator churn, and mutation of view sources;
  compile-time checks also require owned paths in the skeleton service bundle.

- Private clean-source build: 139/139 covered (84 cached, 55 compiled),
  resource compilation and link pass.
- Managed GUI display `:99`, existing smoke prefix, `/Automation`: loads the
  separate copied world and answers `dde: putv totl 4 0 0,endm` with 2.
  Screenshot `.gui-user/screenshots/2026-09-15_21-41-27-619.png` in the parent
  workspace shows both norns' pixels, but both remain collapsed into piles.
  This is NOT a complete creature-rendering fix. Body layout/pose correctness
  remains open, and fresh egg hatching was not verified by this test.
