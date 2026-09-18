# C1 library-edge audit v1

This report distinguishes a game-owned caller from the runtime or SDK
implementation it invokes. A CRT/STL/MFC call in a game function is a
dependency, not evidence that the callee belongs in C1 source.

- Evidence snapshot: `ce33b958a77ecae12ddf5b29f1156a5e02138ec6036a8639b5f5b74bffee70ae`
- Game-owned callers with boundary dependencies: **506**
- Boundary call edges: **3985**
- Unique boundary targets: **110**

## Dependency roles

| Role | Edges | Clean-source treatment |
|---|---:|---|
| `toolchain_implementation` | 719 | caller emits normal C++ and the toolchain supplies the callee |
| `sdk_or_framework_boundary` | 71 | caller emits a normal SDK/MFC-facing operation |
| `game_policy_at_platform_boundary` | 58 | translate the game policy; keep API mechanics at the boundary |
| `sdk_or_import_boundary` | 3137 | link against the imported SDK/runtime library |

## Boundary implementations with game-owned callers

| Address | Evidence name | Category | Translation form | Callers |
|---|---|---|---|---:|
| `0x004019d0` | `guard_check_icall` | `compiler_runtime` | `toolchain_supplied_implementation` | 22 |
| `0x00401ee0` | `CArchiveRuntimeState::ReadByte` | `external_boundary` | `external_sdk_boundary` | 1 |
| `0x004020f0` | `MsvcThrowBadArrayNewLength` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00402190` | `MsvcThrowStringLengthError` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x00406420` | `MsvcBasicString_Reset` | `compiler_runtime` | `toolchain_supplied_implementation` | 4 |
| `0x00406480` | `MsvcStringAllocatorAllocate` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x004064e0` | `MsvcBasicString_AppendBytes` | `compiler_runtime` | `toolchain_supplied_implementation` | 8 |
| `0x00406680` | `MsvcAlignedAllocateFromSize` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00406710` | `CArchiveRuntimeState::WriteUInt32` | `external_boundary` | `external_sdk_boundary` | 1 |
| `0x00406790` | `MsvcCrt_Snprintf` | `platform_or_application_glue` | `crt_formatting_boundary` | 9 |
| `0x004067b0` | `MsvcLocalStdioPrintfOptions` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00406ac0` | `CArchiveRuntimeState::ReadString` | `external_boundary` | `external_sdk_boundary` | 8 |
| `0x00406d00` | `MfcThrowStatusException` | `external_boundary` | `external_sdk_boundary` | 3 |
| `0x004081ce` | `Catch@004081ce` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0040ee20` | `PointInRectXY` | `platform_or_application_glue` | `platform_api_adapter` | 2 |
| `0x0040f996` | `MsvcCppEhFunclet_004f996` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0040fcb0` | `MfcFramework::MFC_CStringArray_GetAt_inline` | `external_boundary` | `external_sdk_boundary` | 1 |
| `0x0040fcd0` | `PlatformEditControl::SetEditSelectionAndScroll` | `platform_or_application_glue` | `platform_api_adapter` | 1 |
| `0x0041019a` | `MsvcCppEhFunclet_004019a` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x004104a0` | `std::basic_ofstream<char,std::char_traits<char>>::~basic_ofstream` | `compiler_runtime` | `toolchain_supplied_implementation` | 3 |
| `0x00410c20` | `StdOstreamInsertCString` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x00410e10` | `std::basic_ofstream<char,std::char_traits<char>>::basic_ofstream` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x00411dc0` | `MsvcBasicString_AppendChar` | `compiler_runtime` | `toolchain_supplied_implementation` | 3 |
| `0x00411fc0` | `std::basic_filebuf<char,std::char_traits<char>>::close` | `compiler_runtime` | `toolchain_supplied_implementation` | 3 |
| `0x00412050` | `std::basic_filebuf<char,std::char_traits<char>>::open` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x00412210` | `StdThrowInvalidStringPosition` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00414a60` | `MfcCGdiObject_Destructor` | `external_boundary` | `external_sdk_boundary` | 4 |
| `0x004171d0` | `MeasureCStringTextExtent` | `platform_or_application_glue` | `platform_api_adapter` | 3 |
| `0x0042d0c0` | `RemoveObjectFromRenderableSet` | `compiler_runtime` | `toolchain_supplied_implementation` | 6 |
| `0x0042d180` | `FindRenderableObjectSetNode` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x0042d210` | `GrowRenderableObjectSetForInsert` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x0042d2a0` | `InsertRenderableObjectSetNode` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x0042d560` | `FillU32Range` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x0042d580` | `FillU32RangeOptimized` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0042d5e0` | `FreeU32ArrayAllocation` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x0042d630` | `RenderableObjectSet_Clear` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0042f360` | `OpenCreaturesRegistryKeys` | `platform_or_application_glue` | `platform_api_adapter` | 2 |
| `0x0042f470` | `ReadOrInitializeRegistryDwordPair` | `platform_or_application_glue` | `platform_api_adapter` | 1 |
| `0x0042fe18` | `FUN_0042fe18` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00435618` | `MsvcCppEhFunclet_0045618` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00435f20` | `MsvcVectorStorage_Release` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x00436237` | `MsvcCppEhFunclet_0046237` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00436280` | `StdThrowMapSetTooLong` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x004395f0` | `MsvcVector_ConstructFromClassifierProfilerTreeRange` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00439760` | `MsvcTreeStorage_Release` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x004397a0` | `MsvcClassifierNameMap_GetOrInsert` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x004398d0` | `MsvcBasicString_Reserve` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00439a20` | `MsvcBasicString_MoveConstruct` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00439a60` | `MsvcBasicString_AssignCString` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00439aa0` | `MsvcBasicString_CopyConstruct` | `compiler_runtime` | `toolchain_supplied_implementation` | 3 |
| `0x00439b50` | `StdSort16ByteRecordsByDwordOffset0xC` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00439e40` | `StdBasicOstream_WritePaddedBuffer` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00439fe0` | `MsvcBasicString_AssignBytes` | `compiler_runtime` | `toolchain_supplied_implementation` | 5 |
| `0x0043a070` | `MsvcVectorStorage_Release` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0043a3b0` | `MsvcClassifierRegistryTree_FindLowerBound` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0043a460` | `MsvcStringTree_LowerBound` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0043a560` | `MsvcStringTree_InsertAndRebalance` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x0043a8a0` | `MsvcBasicString_Less` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0043c610` | `DestroyBasicIfstream` | `compiler_runtime` | `toolchain_supplied_implementation` | 3 |
| `0x0043dd40` | `ConstructAndOpenBasicIfstream` | `compiler_runtime` | `toolchain_supplied_implementation` | 3 |
| `0x0043de70` | `VirtualDispatchSlot0x34WithZeros` | `external_boundary` | `external_sdk_boundary` | 1 |
| `0x0043ff10` | `MsvcBasicIstream_Getline` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00440080` | `MsvcBasicString_AssignBytesReuseStorage` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x00440170` | `MsvcBasicStringbuf_DestroyStorage` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x004402b0` | `MsvcStringPairTreeNode_DestroySubtree` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00443010` | `SpriteFileCacheIndex_InsertIfAbsent` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00443220` | `SpriteFileCacheIndex_EraseNodeRange` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x004433d0` | `SpriteFileCacheIndex_Find` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00443dd0` | `IsRunningUnderWine` | `platform_or_application_glue` | `platform_api_adapter` | 1 |
| `0x00443e20` | `ResolveComProgIdLocalServerPath` | `platform_or_application_glue` | `platform_api_adapter` | 1 |
| `0x004440e0` | `PopulateEmbeddedKitMenuAndToolbarFromRegistry` | `platform_or_application_glue` | `platform_api_adapter` | 2 |
| `0x004444e0` | `ExecuteEmbeddedKitTool` | `platform_or_application_glue` | `platform_api_adapter` | 3 |
| `0x004449e0` | `BroadcastEmbeddedControlState` | `platform_or_application_glue` | `platform_api_adapter` | 12 |
| `0x00444a70` | `ShutdownEmbeddedKitTool` | `platform_or_application_glue` | `platform_api_adapter` | 4 |
| `0x00444c30` | `CVolumeDialog::~CVolumeDialog` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00445b30` | `Win32BuildCurrentUserSecurityDescriptor` | `platform_or_application_glue` | `platform_api_adapter` | 1 |
| `0x00448d30` | `MsvcBasicString_EqualsCString` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00448dd0` | `MsvcByteVector_InitFromRange` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00448e30` | `MsvcPendingCommandMap_FindLowerBound` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00448e80` | `MsvcPendingCommandMap_DestroyNodesAndHeader` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00448ed0` | `MsvcBasicString_Substring` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00448f30` | `MsvcBasicString_FindByteBeforeLimit` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00448f70` | `MsvcBasicString_GetEndPointer` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00448f90` | `MsvcBasicString_ResetThenMoveAssign` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00448fd0` | `MsvcPendingCommandMap_DestroyNodeSubtree` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00449010` | `MsvcByteVector_InsertByte` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x00449150` | `std::_Tree_unchecked_const_iterator<std::_Tree_val<std::_Tree_simple_types<unsigned_int>_>,std::_Iterator_base0>::operator++` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x004491b0` | `MsvcByteVector_DestroyDuplicate` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x00449210` | `MsvcPendingCommandMap_FindInsertPosition` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00449260` | `MsvcPendingCommandMap_EraseNode` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x00449580` | `MsvcByteVector_AllocateStorage` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x004496e0` | `MsvcByteVector_ReplaceStorage` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0044a0f5` | `MFC140.DLL::CWnd::CreateEx` | `external_boundary` | `external_sdk_boundary` | 1 |
| `0x0044a71f` | `__security_check_cookie` | `compiler_runtime` | `toolchain_supplied_implementation` | 118 |
| `0x0044a750` | `MsvcOperatorDeleteWrapper` | `compiler_runtime` | `toolchain_supplied_implementation` | 31 |
| `0x0044a75e` | `MsvcOperatorDeleteAdapter` | `compiler_runtime` | `toolchain_supplied_implementation` | 2 |
| `0x0044a76c` | ``eh_vector_constructor_iterator'` | `compiler_runtime` | `toolchain_supplied_implementation` | 10 |
| `0x0044a7e0` | `__ehvec_dtor` | `compiler_runtime` | `toolchain_supplied_implementation` | 4 |
| `0x0044aa1b` | `MsvcInvokeRuntimeFatalHandler` | `compiler_runtime` | `toolchain_supplied_implementation` | 4 |
| `0x0044ad36` | `_atexit` | `compiler_runtime` | `toolchain_supplied_implementation` | 18 |
| `0x0044ad79` | `__Init_thread_footer` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0044adca` | `__Init_thread_header` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0044ae31` | `MfcOperatorNewWithSEH` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0044ba70` | `MsvcMemchrAvx2Wrapper` | `compiler_runtime` | `toolchain_supplied_implementation` | 1 |
| `0x0044bb94` | `VCRUNTIME140.DLL::memset` | `external_boundary` | `external_sdk_boundary` | 11 |
| `0x0044bbac` | `VCRUNTIME140.DLL::memmove` | `external_boundary` | `external_sdk_boundary` | 8 |
| `0x0044bbb2` | `VCRUNTIME140.DLL::memcpy` | `external_boundary` | `external_sdk_boundary` | 5 |
| `0x0044bcf0` | `__alloca_probe` | `compiler_runtime` | `toolchain_supplied_implementation` | 6 |
| `0x0044bd8f` | `API-MS-WIN-CRT-MATH-L1-1-0.DLL::_libm_sse2_log10_precise` | `external_boundary` | `external_sdk_boundary` | 1 |
| `0x0044bd95` | `API-MS-WIN-CRT-MATH-L1-1-0.DLL::libm_sse2_pow_precise` | `external_boundary` | `external_sdk_boundary` | 1 |

The clean emitter must use this report together with the address-keyed
emission manifest. It must not copy a callee body into the caller's
module merely because the caller uses strings, maps, sorting, memory
copying, formatting, or MFC controls.
