# C1 clean-source emission manifest v1

This is an address-keyed emission contract derived from the saved
Ghidra project map. It is not generated C++ and does not modify the
decompiled/bootstrap corpus.

- Evidence snapshot: `ce33b958a77ecae12ddf5b29f1156a5e02138ec6036a8639b5f5b74bffee70ae`
- Function entries: **2332**
- Imported boundary entries: **867**

## Contract

The emitter must make the disposition decision before writing a source
definition. Runtime/STL/DLL bodies are not copied into C1 modules.
A clean source function is written only after semantic translation; an
absorbed helper has no standalone source definition.

## Disposition summary

| Disposition | Count | Clean-source result |
|---|---:|---|
| `absorb_into_owner` | 66 | fold into the owning operation or static lifetime; no standalone helper |
| `emit_framework_metadata` | 115 | class declaration/framework metadata rather than a recovered body |
| `emit_narrow_platform_adapter` | 3 | normal adapter only; no analyst-labelled CRT body |
| `evidence_only` | 1 | retain Ghidra evidence; no clean definition until source identity/signature is proven |
| `provided_by_sdk_or_import_library` | 453 | no C1 definition; link against the SDK/import library |
| `provided_by_toolchain` | 955 | no C1 definition; use compatible runtime/container facilities |
| `translate_to_owned_module` | 713 | semantic rewrite required in the mapped module |
| `translate_to_platform_adapter` | 26 | normal MFC/Win32-facing adapter |

## Translation forms

| Form | Count |
|---|---:|
| `absorb_into_archive_reader` | 2 |
| `absorb_into_archive_reader_raii` | 1 |
| `absorb_into_classifier_map_owner` | 1 |
| `absorb_into_dialog_owner` | 11 |
| `absorb_into_owner` | 12 |
| `absorb_into_pipe_response_builder` | 3 |
| `absorb_into_startup_registration` | 32 |
| `absorb_into_toolbar_owner` | 2 |
| `absorb_into_window_lifetime` | 2 |
| `clean_semantic_helper` | 3 |
| `crt_formatting_boundary` | 3 |
| `external_sdk_boundary` | 453 |
| `framework_metadata` | 115 |
| `ordinary_source_translation` | 710 |
| `platform_api_adapter` | 26 |
| `representation_hold` | 1 |
| `toolchain_supplied_implementation` | 955 |

## Common-library boundary slice

The `toolchain_supplied_implementation` rows include the analyst
`MsvcBasicString_*`, byte-vector, string-map, classifier-tree, and
pipe-tree storage routines, plus compiler/EH/iostream support. Their
callers remain game-owned, but those implementation bodies are not.
The CRT formatting rows remain only as a narrow
ABI boundary, while XML escaping, classifier-name lookup, profiler
ordering, and point-in-rectangle behavior remain semantic translations.

The manifest intentionally retains the evidence-side name and address
for traceability. Those names must never be used as clean-source
identifiers.
