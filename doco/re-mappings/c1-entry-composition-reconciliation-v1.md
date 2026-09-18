# C1 Entry Composition Reconciliation v1

- Entry unit: `src/c1/platform/executable_entry.cpp` (112 lines)
- Queue snapshot: `ce33b958a77ecae1…`
- Classes: **1** (0 relocatable, 1 co-located MFC)
- Lines to relocate: **0**

| class | lines | verdict | semantic owner | target unit |
|---|---:|---|---|---|
| `C1Application` | 106 | stay_as_forwarding_shell | `application/application.cpp` | `platform/executable_entry.cpp` |

## Checks

**PASS:** entry unit holds only forwarding MFC shells.
