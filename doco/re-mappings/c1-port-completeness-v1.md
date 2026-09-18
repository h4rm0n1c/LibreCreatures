# C1 Port Completeness

| dimension | resolved | total | outstanding |
|---|---:|---:|---|
| function inventory | 2331 | 2332 | - |
| function translation | 377 | 923 | verify and checkpoint |
| composition liveness | 101 | 101 | none |
| menu command routing | 51 | 51 | none |
| absent shell classes | 0 | 11 | implement the MFC class |

## Notes

- **function inventory** (c1-boundary-contract-audit-v1.json): every recovered function is classified as toolchain, SDK/import or C1-owned
- **function translation** (queue + admissions ledger): 546 of the 546 outstanding are already admitted translations; 0 are unwritten
- **composition liveness** (c1-composition-liveness-audit-v1.json + composition holds): 1 composition holds remain: translated behaviour that the executable cannot yet reach
- **menu command routing** (c1-ui-surface-audit-v1.json): concrete_document=1; concrete_frame=44; framework_default=2; framework_shell_only=3; placeholder=1
- **absent shell classes** (held framework-metadata rows): modules holding rows because the port has no counterpart class at all

## Work queue

| kind | module | rows |
|---|---|---:|
| composition_hold | `application/main_frame.cpp` | 1 |
| verify_and_checkpoint | `application/application.cpp` | 81 |
| verify_and_checkpoint | `objects/object.cpp` | 27 |
| verify_and_checkpoint | `creatures/skeleton.cpp` | 27 |
| verify_and_checkpoint | `objects/compound_object.cpp` | 24 |
| verify_and_checkpoint | `creatures/creature.cpp` | 23 |
| verify_and_checkpoint | `display/rendering.cpp` | 21 |
| verify_and_checkpoint | `ui/views.cpp` | 20 |
| verify_and_checkpoint | `objects/simple_object.cpp` | 19 |
| verify_and_checkpoint | `sound/sound.cpp` | 17 |
| verify_and_checkpoint | `creatures/genome.cpp` | 16 |
| verify_and_checkpoint | `scripting/pipe_server.cpp` | 15 |
| verify_and_checkpoint | `application/document.cpp` | 14 |
| verify_and_checkpoint | `application/sfc_ole.cpp` | 11 |
| verify_and_checkpoint | `ui/caos_console.cpp` | 10 |
| verify_and_checkpoint | `creatures/body.cpp` | 9 |
| verify_and_checkpoint | `creatures/voice.cpp` | 9 |
| verify_and_checkpoint | `application/main_frame.cpp` | 9 |
| verify_and_checkpoint | `ui/debug_console.cpp` | 9 |
| verify_and_checkpoint | `ui/tip_dialog.cpp` | 8 |
| verify_and_checkpoint | `objects/entity.cpp` | 8 |
| verify_and_checkpoint | `brain/brain.cpp` | 7 |
| verify_and_checkpoint | `ui/event_bar.cpp` | 7 |
| verify_and_checkpoint | `display/image.cpp` | 7 |
| verify_and_checkpoint | `brain/blackboard.cpp` | 7 |
| verify_and_checkpoint | `ui/tools.cpp` | 6 |
| verify_and_checkpoint | `scripting/macro.cpp` | 6 |
| verify_and_checkpoint | `scripting/macro_holder.cpp` | 6 |
| verify_and_checkpoint | `application/embedded_kits.cpp` | 5 |
| verify_and_checkpoint | `ui/place_dialog.cpp` | 5 |
| verify_and_checkpoint | `ui/world_statistics.cpp` | 5 |
| verify_and_checkpoint | `world/viewport.cpp` | 4 |
| verify_and_checkpoint | `brain/lobe.cpp` | 4 |
| verify_and_checkpoint | `creatures/registry.cpp` | 4 |
| verify_and_checkpoint | `display/palette.cpp` | 4 |
| verify_and_checkpoint | `ui/toolbars.cpp` | 3 |
| verify_and_checkpoint | `biochemistry/biochemistry.cpp` | 3 |
| verify_and_checkpoint | `creatures/bacterium.cpp` | 3 |
| verify_and_checkpoint | `platform/formatting.cpp` | 3 |
| verify_and_checkpoint | `display/gallery.cpp` | 3 |
| verify_and_checkpoint | `creatures/update.cpp` | 3 |
| verify_and_checkpoint | `platform/registry.cpp` | 3 |
| verify_and_checkpoint | `creatures/events.cpp` | 3 |
| verify_and_checkpoint | `ui/creature_selection.cpp` | 3 |
| verify_and_checkpoint | `world/update_timer.cpp` | 3 |
| verify_and_checkpoint | `world/map.cpp` | 3 |
| verify_and_checkpoint | `world/geometry.cpp` | 3 |
| verify_and_checkpoint | `objects/bubble.cpp` | 3 |
| verify_and_checkpoint | `world/tick.cpp` | 2 |
| verify_and_checkpoint | `objects/call_button.cpp` | 2 |
| verify_and_checkpoint | `ui/version_dialog.cpp` | 2 |
| verify_and_checkpoint | `brain/neuron.cpp` | 2 |
| verify_and_checkpoint | `objects/vehicle.cpp` | 2 |
| verify_and_checkpoint | `ui/windows.cpp` | 2 |
| verify_and_checkpoint | `ui/score.cpp` | 2 |
| verify_and_checkpoint | `ui/volume_dialog.cpp` | 2 |
| verify_and_checkpoint | `common/filesystem.cpp` | 2 |
| verify_and_checkpoint | `brain/rules.cpp` | 2 |
| verify_and_checkpoint | `brain/instinct.cpp` | 2 |
| verify_and_checkpoint | `application/file_commands.cpp` | 2 |
| verify_and_checkpoint | `platform/mfc_adapters.cpp` | 2 |
| verify_and_checkpoint | `archive/backup.cpp` | 2 |
| verify_and_checkpoint | `platform/environment.cpp` | 1 |
| verify_and_checkpoint | `platform/security.cpp` | 1 |
| verify_and_checkpoint | `objects/debug.cpp` | 1 |
| verify_and_checkpoint | `display/blit.cpp` | 1 |
| verify_and_checkpoint | `application/commands.cpp` | 1 |
| verify_and_checkpoint | `objects/events.cpp` | 1 |
| verify_and_checkpoint | `world/places.cpp` | 1 |
| verify_and_checkpoint | `platform/com.cpp` | 1 |
| verify_and_checkpoint | `application/resources.cpp` | 1 |
| verify_and_checkpoint | `display/font.cpp` | 1 |
| verify_and_checkpoint | `platform/gdi.cpp` | 1 |
| verify_and_checkpoint | `display/bitmap.cpp` | 1 |
| verify_and_checkpoint | `creatures/attention.cpp` | 1 |
| verify_and_checkpoint | `brain/locus_resolver.cpp` | 1 |
| verify_and_checkpoint | `common/logging.cpp` | 1 |
| verify_and_checkpoint | `scripting/classifier_scripts.cpp` | 1 |
| verify_and_checkpoint | `display/sprite_cache.cpp` | 1 |
| verify_and_checkpoint | `objects/scenery.cpp` | 1 |
| verify_and_checkpoint | `objects/lifecycle.cpp` | 1 |
| verify_and_checkpoint | `display/profiler.cpp` | 1 |
| verify_and_checkpoint | `ui/menus.cpp` | 1 |
| verify_and_checkpoint | `creatures/learned_words.cpp` | 1 |
| verify_and_checkpoint | `ui/main_window.cpp` | 1 |
| verify_and_checkpoint | `common/time.cpp` | 1 |
| verify_and_checkpoint | `application/file_dialog.cpp` | 1 |
