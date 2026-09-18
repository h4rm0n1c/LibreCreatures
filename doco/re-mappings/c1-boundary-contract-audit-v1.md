# C1 boundary contract audit v1

This report separates code authored by the game from implementations
that arrived through MSVC, STL, CRT, MFC, ATL, or Windows imports.

- Evidence snapshot: `ce33b958a77ecae12ddf5b29f1156a5e02138ec6036a8639b5f5b74bffee70ae`
- Project functions: **2332**
- Compiler/runtime rows: **955**
- SDK/import-boundary rows: **453**
- Clean queue work items: **923**

## Runtime families present in the executable

These counts describe binary evidence, not source files to recreate.

| Family | Rows | Representative evidence names |
|---|---:|---|
| `allocation` | 7 | DeleteHeapAllocation, MsvcStringAllocatorAllocate, MsvcAlignedDeallocateWithCapacity, MsvcAlignedAllocateFromSize |
| `crt_startup` | 20 | __security_check_cookie, ___scrt_acquire_startup_lock, ___scrt_initialize_crt, ___scrt_initialize_onexit_tables |
| `exception_unwind` | 703 | MsvcException_ScalarDeletingDestructor, MsvcBadAlloc_CopyConstruct, Catch@004081b8, MsvcCppEhFunclet_00481c8 |
| `iostream_file` | 28 | RegisterMsvcFacetNodeCleanup, std::basic_ofstream<char,std::char_traits<char>>::~basic_ofstream, StdBasicOfstream_CloseAndDispatch, std::basic_ofstream<char,std::char_traits<char>>::basic_ofstream |
| `msvc_containers` | 43 | MsvcSmallBuffer_ReleaseHeapIfLarge, MsvcUnwindCleanup_FreeHeapBufferAtFramePlus4, ResizeU32VectorWithFill, FillU32Range |
| `msvc_string` | 20 | MsvcBasicString_Reset, MsvcBasicString_AppendBytes, MsvcBasicString_AppendChar, MsvcBasicString_Reserve |
| `other_compiler_runtime` | 134 | InitializeEntityRegistry, InitializeNonSceneryObjectRegistry, InitializeSceneryRegistry, InitializeWorldObjectRegistry |

## Clean-source lane

This scan applies only to admitted `src/c1`, never to the legacy
`src/decompiled` bootstrap. The lane is intentionally not materialized
yet; once it exists, this becomes the emission gate for analyst names
and raw Ghidra identifiers.

- Materialized: **True**
- Source files: **269**
- Forbidden-token findings: **0**

**PASS:** no forbidden analyst/runtime or raw Ghidra tokens in the clean lane.

## Legacy bootstrap visibility

The old `src/decompiled` tree is retained for bounded compiler
experiments and is not the clean source lane. Its visible runtime
sections are reported here so they cannot be mistaken for admitted
C1 implementations.

- Bootstrap source files: **299**
- Boundary sections still visible there: **1432**
- Legacy evidence files: `API-MS-WIN-CRT-HEAP-L1-1-0.DLL.cpp`, `API-MS-WIN-CRT-LOCALE-L1-1-0.DLL.cpp`, `API-MS-WIN-CRT-MATH-L1-1-0.DLL.cpp`, `API-MS-WIN-CRT-RUNTIME-L1-1-0.DLL.cpp`, `API-MS-WIN-CRT-STDIO-L1-1-0.DLL.cpp`, `C1MsvcString.hpp`, `MFC140.DLL.cpp`, `MSVCP140.DLL.cpp`, `VCRUNTIME140.DLL.cpp`, `_free_functions.cpp`, `std.cpp`

C1-prefixed runtime-looking artifacts still visible in the bootstrap:

- `C1MfcCStringArray.cpp`: `C1MfcCString`, `C1MfcCStringArray`
- `CAOSConsoleDlg.cpp`: `C1MfcCString`
- `CBrain.cpp`: `C1MsvcVectorThiscallCallback`
- `CCreatureRegister.cpp`: `C1MsvcVectorThiscallCallback`
- `CGallery.cpp`: `C1MsvcVectorThiscallCallback`
- `CMainFrame.cpp`: `C1MsvcString`, `C1MsvcVectorThiscallCallback`
- `COwner.cpp`: `C1MsvcVectorThiscallCallback`
- `Creature.cpp`: `C1MsvcVectorThiscallCallback`
- `MapData.cpp`: `C1MsvcVectorThiscallCallback`
- `SFCApp.cpp`: `C1MsvcString`
- `SFCDoc.cpp`: `C1MsvcVectorThiscallCallback`
- `SFCView.cpp`: `C1MsvcString`
- `_free_functions.cpp`: `C1MsvcString`, `C1MsvcStringDataPointer`, `C1MsvcThiscallVoidPtr`, `C1MsvcVectorThiscallCallback`, `C1Snprintf_00449740`
- `C1MfcCString.hpp`: `C1MfcCString`
- `C1MfcCStringArray.hpp`: `C1MfcCStringArray`
- `C1MsvcBasicFilebufCharLayout.hpp`: `C1MsvcBasicFilebufCharLayout`, `C1MsvcBasicStreambufCharLayout`
- `C1MsvcBasicOfstreamCharLayout.hpp`: `C1MsvcBasicFilebufCharLayout`, `C1MsvcBasicOfstreamCharLayout`
- `C1MsvcBasicStreambufCharLayout.hpp`: `C1MsvcBasicStreambufCharLayout`
- `CAOSConsoleDlg.hpp`: `C1MfcCString`, `C1MfcCStringArray`
- `DebugConsoleDialog.hpp`: `C1MsvcBasicOfstreamCharLayout`
- `PipeServerCommandContext.hpp`: `C1MsvcString`


## Queue invariants

The clean-source queue may contain game-owned callers and narrow
platform adapters, but never the implementation rows above or SDK
DLL bodies. Runtime-looking names in the owned lane are reported as
review failures rather than silently renamed.

**PASS:** reviewed boundary guard holds for `MfcFramework::MFC_CStringArray_GetAt_inline` at `0040fcb0`.

**PASS:** all boundary invariants hold.

This audit does not claim that the queued C1 bodies have already been
translated. It only prevents common-library and DLL implementations
from being mistaken for that translation.
