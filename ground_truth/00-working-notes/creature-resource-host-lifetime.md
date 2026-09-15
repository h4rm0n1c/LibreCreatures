# Creature resource hosts must follow the selected world

Date: 2026-09-15 (Australia/Perth)

## Finding

The native creature loader resolves body sprites through the process's current
secondary resource tree on every `Skeleton::LoadGenome` call.  Ghidra's
`ResolveExistingBodyPartFilenameWithFallback` confirms that directory slot 13
is the secondary image slot, and native `Skeleton::LoadGenome` writes generated
creature SPR files to that same secondary `Images` directory.

The port's `C1ResourceHost` stores its directory table by value.  Opening a
world calls the MFC `CDocument::OnOpenDocument` path, which first drains the old
world through `Document::delete_contents`; the host then left `resources_`,
`creature_resources_`, and the palette hosts alive.  A later world open could
therefore keep using the previous world's `Images` and `Genetics` paths.  The
same stale save path could also leak into a newly created world.

## Fix

`delete_framework_contents()` now destroys the resource and palette hosts
after the old world and gallery registries have been drained, allowing the
next document operation to rebuild them from the current secondary tree.
`OnNewDocument()` clears the previous save-local world and image paths before
MFC drains the old document.

## Verification

- Ghidra helper/decompiled source checked: native slot 13 resolves against the
  secondary `Images` tree.
- `git diff --check`: passed.
- `python3 harness/build_clean_source.py --cache --build-dir /tmp/bm-cache`:
  139 source files, 138 reused and 1 compiled; resources and link passed.
- The existing Wine automation smoke launch exited early with code 0 because
  its OLE automation server did not remain live; it did not provide a
  gameplay-level creature visibility verdict.  No Wine or Xvfb processes were
  left running.
