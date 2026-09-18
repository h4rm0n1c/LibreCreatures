# C1 source translation queue v1

This queue is derived from the address-keyed clean-source emission
manifest. It is the input contract for semantic source reconstruction;
it is not generated C++.

- Evidence snapshot: `ce33b958a77ecae12ddf5b29f1156a5e02138ec6036a8639b5f5b74bffee70ae`
- Work items: **923**
- Evidence-only holds: **1** (retained in the manifest, excluded from actionable batches)
- Free-function/startup/platform work items: **176**
- Destination source groups: **102**

## Complete accounting

The actionable queue is only one projection of the full program inventory. These counts are kept here so queue progress is not confused with compiler/runtime or SDK boundary coverage.

| Inventory slice | Count | Meaning |
|---|---:|---|
| Ghidra function entries | 2332 | all analyzed function entries |
| Import entries | 867 | separate imported-symbol inventory |
| Actionable C1-owned work items | 923 | source translation, absorption, metadata, or platform work |
| Evidence-only function entries | 1 | retained evidence; not actionable until its role is proven |
| Compiler/runtime boundary | 955 | supplied by the selected MSVC/toolchain |
| SDK/import-library boundary | 453 | supplied by Windows/MFC/ATL/import libraries |
| Definitions emitted by C1 contract | 742 | owned source or explicit project metadata |
| Definitions provided or absorbed | 1590 | no standalone clean C1 definition emitted |

## Boundary rule

The bootstrap `_free_functions.cpp` is not an input to this queue.
MSVC/STL/compiler implementations and MFC/ATL/DLL implementations
are excluded and must be supplied by the selected toolchain or SDK.
Each remaining address is assigned to a coherent C1 destination,
absorbed into its owning operation, or represented as framework
metadata. No placeholder body is admitted by this artifact.

## Work by disposition

| Disposition | Count |
|---|---:|
| `absorb_into_owner` | 66 |
| `emit_framework_metadata` | 115 |
| `emit_narrow_platform_adapter` | 3 |
| `translate_to_owned_module` | 713 |
| `translate_to_platform_adapter` | 26 |

## Work by source destination

| Destination | Module | Items |
|---|---|---:|
| `application/application.cpp` | `application` | 105 |
| `application/commands.cpp` | `application` | 1 |
| `application/document.cpp` | `application` | 22 |
| `application/embedded_kits.cpp` | `application` | 7 |
| `application/file_commands.cpp` | `application` | 2 |
| `application/file_dialog.cpp` | `application` | 1 |
| `application/kit_processes.cpp` | `application` | 1 |
| `application/main_frame.cpp` | `application` | 19 |
| `application/resources.cpp` | `application` | 3 |
| `application/sfc_ole.cpp` | `application` | 18 |
| `archive/archive.cpp` | `archive` | 3 |
| `archive/backup.cpp` | `archive` | 2 |
| `archive/funeral_kit.cpp` | `archive` | 1 |
| `biochemistry/biochemistry.cpp` | `biochemistry` | 9 |
| `brain/blackboard.cpp` | `brain` | 10 |
| `brain/brain.cpp` | `brain` | 10 |
| `brain/classifiers.cpp` | `brain` | 3 |
| `brain/instinct.cpp` | `brain` | 5 |
| `brain/lobe.cpp` | `brain` | 6 |
| `brain/locus_resolver.cpp` | `brain` | 1 |
| `brain/neuron.cpp` | `brain` | 2 |
| `brain/rules.cpp` | `brain` | 2 |
| `common/filesystem.cpp` | `common` | 2 |
| `common/logging.cpp` | `common` | 2 |
| `common/time.cpp` | `common` | 1 |
| `creatures/attention.cpp` | `creatures` | 1 |
| `creatures/bacterium.cpp` | `creatures` | 8 |
| `creatures/body.cpp` | `creatures` | 15 |
| `creatures/creature.cpp` | `creatures` | 57 |
| `creatures/events.cpp` | `creatures` | 8 |
| `creatures/genome.cpp` | `creatures` | 17 |
| `creatures/learned_words.cpp` | `creatures` | 1 |
| `creatures/owner.cpp` | `creatures` | 4 |
| `creatures/registry.cpp` | `creatures` | 9 |
| `creatures/skeleton.cpp` | `creatures` | 33 |
| `creatures/update.cpp` | `creatures` | 3 |
| `creatures/voice.cpp` | `creatures` | 10 |
| `display/bitmap.cpp` | `display` | 4 |
| `display/blit.cpp` | `display` | 1 |
| `display/font.cpp` | `display` | 1 |
| `display/gallery.cpp` | `display` | 9 |
| `display/image.cpp` | `display` | 10 |
| `display/palette.cpp` | `display` | 6 |
| `display/profiler.cpp` | `display` | 1 |
| `display/rendering.cpp` | `display` | 26 |
| `display/sprite_cache.cpp` | `display` | 5 |
| `objects/bubble.cpp` | `objects` | 10 |
| `objects/call_button.cpp` | `objects` | 7 |
| `objects/compound_object.cpp` | `objects` | 31 |
| `objects/debug.cpp` | `objects` | 1 |
| `objects/entity.cpp` | `objects` | 12 |
| `objects/events.cpp` | `objects` | 4 |
| `objects/lifecycle.cpp` | `objects` | 2 |
| `objects/lift.cpp` | `objects` | 15 |
| `objects/object.cpp` | `objects` | 38 |
| `objects/renderable_set.cpp` | `objects` | 2 |
| `objects/scenery.cpp` | `objects` | 5 |
| `objects/simple_object.cpp` | `objects` | 37 |
| `objects/vehicle.cpp` | `objects` | 9 |
| `platform/com.cpp` | `platform` | 1 |
| `platform/environment.cpp` | `platform` | 1 |
| `platform/formatting.cpp` | `platform` | 3 |
| `platform/gdi.cpp` | `platform` | 1 |
| `platform/mfc_adapters.cpp` | `platform` | 2 |
| `platform/registry.cpp` | `platform` | 3 |
| `platform/security.cpp` | `platform` | 1 |
| `scripting/classifier_scripts.cpp` | `scripting` | 4 |
| `scripting/dde.cpp` | `scripting` | 17 |
| `scripting/macro.cpp` | `scripting` | 24 |
| `scripting/macro_holder.cpp` | `scripting` | 13 |
| `scripting/pipe_server.cpp` | `scripting` | 20 |
| `scripting/tables.cpp` | `scripting` | 1 |
| `sound/cache.cpp` | `sound` | 1 |
| `sound/sound.cpp` | `sound` | 17 |
| `ui/caos_console.cpp` | `ui` | 16 |
| `ui/classifier_tip.cpp` | `ui` | 1 |
| `ui/creature_selection.cpp` | `ui` | 5 |
| `ui/debug_console.cpp` | `ui` | 14 |
| `ui/dialogs.cpp` | `ui` | 5 |
| `ui/event_bar.cpp` | `ui` | 12 |
| `ui/eye_view.cpp` | `ui` | 8 |
| `ui/magic_profiler.cpp` | `ui` | 1 |
| `ui/main_window.cpp` | `ui` | 1 |
| `ui/menus.cpp` | `ui` | 1 |
| `ui/place_dialog.cpp` | `ui` | 7 |
| `ui/score.cpp` | `ui` | 4 |
| `ui/system_info.cpp` | `ui` | 1 |
| `ui/tip_dialog.cpp` | `ui` | 11 |
| `ui/toolbars.cpp` | `ui` | 7 |
| `ui/tools.cpp` | `ui` | 11 |
| `ui/version_dialog.cpp` | `ui` | 3 |
| `ui/views.cpp` | `ui` | 29 |
| `ui/volume_dialog.cpp` | `ui` | 3 |
| `ui/windows.cpp` | `ui` | 2 |
| `ui/world_statistics.cpp` | `ui` | 6 |
| `world/geometry.cpp` | `world` | 3 |
| `world/map.cpp` | `world` | 7 |
| `world/places.cpp` | `world` | 5 |
| `world/settings.cpp` | `world` | 2 |
| `world/tick.cpp` | `world` | 2 |
| `world/update_timer.cpp` | `world` | 3 |
| `world/viewport.cpp` | `world` | 4 |

The JSON artifact contains the complete address/name/form records for
each group and the separate free-function work-item projection.
