# C1 source translation queue triage

This is a read-only planning artifact generated from the current Ghidra-derived manifests. It does not claim translation or byte verification is complete.

- Pending rows: **0** of queue work items (923 total)
- Assigned exactly once: **0**; unassigned: **0**; duplicate assignments: **0**
- Batches: **0**, maximum 16 rows / 80.0 estimated cost
- Graph coverage: 0 rows; graph gaps: 0
- Semantic dependency knots: 0; largest knot: 0 batches / 0 rows

## Queue profile

| Dimension | Counts |
|---|---|
| Modules |  |
| Work kinds |  |
| Translation forms |  |
| Risk |  |

## Module triage

| Module | Rows | Source files | Low | Medium | High | Estimated cost |
|---|---:|---:|---:|---:|---:|---:|

## High-risk frontier

These are the largest evidence/review-cost rows, not a claim that their semantics are solved:

| Address | Function | Owner | Bytes | Artifacts | Opaque locals | Cost |
|---|---|---|---:|---:|---:|---:|

## Batch order

## Ghidra graph gaps


Batches are grouped by source owner and work kind, split by estimated review cost, and topologically ordered for semantic work where the stored call graph permits it. Cyclic dependency knots are kept bounded and reported explicitly; no linear order can satisfy every edge inside a knot.

| Order | Batch | Kind | Owner | Rows | Cost | Risk | Layer |
|---:|---|---|---|---:|---:|---|---:|

## Dependency knots

| Component | Batches | Rows | Modules |
|---:|---:|---:|---|

## Operating rules

- Translate ordinary semantic batches into the clean owner file named on each row; do not repair generated bootstrap output in place.
- Treat high-risk rows as review-sized units even when they share a file. Preserve their Ghidra evidence and resolve types/ABI at the source, then rerun this triage.
- Keep framework metadata, owner integration, and platform adapters in their dedicated batches; they are not evidence that C1 owns a second copy of CRT/MFC/Windows code.
- Re-run the triage after each admitted batch. The completeness invariant is one pending address in one batch exactly once.

## Input hashes

- `byte_match/c1-source-translation-queue-v1.json`: `332ae9270fad50812c64df95895dbb2e9ffa48ccb8ed2a7d07777e1895d94383`
- `byte_match/clean-source-emission-manifest-v1.json`: `23c7c440247d7b26531e16b43dca06d7ff152be29671e19662018f45a19f7f59`
- `byte_match/c1-clean-source-admission-audit-v1.json`: `1c8c3496fb019097061cebcb378c5d40abc65436b27480113b729de7bd401a88`
- `re_work/creatures1_decompiled/graphify-out/graph.json`: `add71a5d68fdddc554b8268b230e8d7e606c03324276f60412192ccca6127e71`
