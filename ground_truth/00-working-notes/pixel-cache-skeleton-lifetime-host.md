# Pixel-cache crash: Skeleton retained a dead per-call lifetime host

Date: 2026-09-14

## Symptom and native evidence

The submarine-world crash dumps faulted in the shared intrusive pixel LRU,
not in sprite decoding.  In the clean build, `Creatures.exe+0x377ff` is the
`Image::get_pixel_data` relink instruction:

```asm
mov DWORD PTR [esi+0x20],eax
```

`ESI` is `this->cache_prev_`.  Dump `.27700` reached this through
`Image::blit_to_dib` (`+0x43770a` return path), with an invalid small
`cache_prev_` value.  The surrounding `Image` bytes had already been reused
as unrelated data, proving that a freed `Image` was still reachable from the
cache list.  The same LRU instruction family appears in `.17648` and
`.23544`.

Ghidra confirms the native cache also uses an intrusive global LRU, so the
cache algorithm itself was not changed.  Native `Skeleton::ClearBodyPartsAndGallery`
at `0x0043dc50` releases the gallery retained by the Object base, and native
`Skeleton::LoadGenome` at `0x0043c800` stores the final gallery in that stable
Object-owned slot.

## Port defect

The port's `Skeleton` retained `SkeletonLifetimeHost* lifetime_host_`.
`WindowsCreatureEnvironmentHost::initialize_from_genome` supplied a local
stack adapter, and `WindowsCreatureConstructionHost` supplied a lifetime
adapter belonging to another short-lived construction host.  Both adapters
were destroyed when their calls returned, while the Skeleton survived.  Its
later destructor/body-gallery cleanup therefore dispatched virtual calls
through dead stack state.  This was the ownership boundary capable of
corrupting gallery teardown and leaving the observed stale Image in the LRU.

## Fix

`C1WindowsDocument` now implements `SkeletonLifetimeHost`.  Its document
lifetime covers the world-owned creatures, and its implementations preserve
the existing operations: delete a limb, stop continuous sound, remove the
Skeleton from renderables and registries, and release its gallery through the
active `WorldRuntime`.

Both construction paths now build Skeleton sprite services with the document
as `gallery_lifetime_host`; no Skeleton stores an address of a per-call host.
The pixel-cache/LRU code was intentionally left unchanged.

## Verification

- `python3 harness/build_clean_source.py --cache --build-dir /tmp/bm-cache`
  passed: 139/139 source objects, resource compile, and link.
- `python3 harness/run_c1_smoke.py --exe /tmp/bm-cache/Creatures.clean.exe
  --timeout 45` passed for the normal world.
- The same harness against
  `/home/harri/creatures/140926crash/w2/World.sfc` passed the 45-second
  populated-world run and a 120-second render/update soak, with no startup
  log, Wine debugger, or early exit.
- The CAOS-pipe variant was attempted against both world forms, but the game
  never published its pipe in this environment; it is recorded as a harness
  limitation, not as a pixel-cache failure verdict.
