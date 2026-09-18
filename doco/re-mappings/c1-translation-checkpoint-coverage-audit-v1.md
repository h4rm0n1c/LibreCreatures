# C1 translation checkpoint coverage audit

This is an audit-only view. Clean-source admission is not treated as semantic batch completion.

- Queue work items: 923
- Clean-source admitted: 742
- Resolved non-emitting: 181
- Pending in admission ledger: 0
- Queue addresses with a live checkpoint: 416
- Queue addresses with an admitted checkpoint: 352
- Authoritative queue evidence snapshot: `ce33b958a77ecae12ddf5b29f1156a5e02138ec6036a8639b5f5b74bffee70ae`
- Checkpoint files on current snapshot: 388
- Checkpoint files on stale snapshots: 0
- Checkpoint files missing snapshot identity: 23
- Queue addresses with a current-snapshot live checkpoint: 394
- Queue addresses with a current-snapshot admitted checkpoint: 333
- Admitted addresses missing an admitted checkpoint: 551
- Queue addresses missing any live checkpoint: 507

## Interpretation

A held checkpoint is evidence only; an admitted checkpoint is the completion status for this audit. Superseded files are excluded from live coverage. Current-snapshot counts are the authoritative freshness view; legacy checkpoint files are retained and reported as stale rather than rewritten.

## Owner summary

| Source owner | Queue | Admitted | Non-emitting | Live checkpoint | Admitted checkpoint | Admitted missing | Queue missing live |
|---|---:|---:|---:|---:|---:|---:|---:|
| application/application.cpp | 105 | 97 | 8 | 27 | 24 | 81 | 78 |
| application/commands.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| application/document.cpp | 22 | 22 | 0 | 5 | 4 | 18 | 17 |
| application/embedded_kits.cpp | 7 | 7 | 0 | 3 | 2 | 5 | 4 |
| application/file_commands.cpp | 2 | 2 | 0 | 0 | 0 | 2 | 2 |
| application/file_dialog.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| application/kit_processes.cpp | 1 | 1 | 0 | 1 | 1 | 0 | 0 |
| application/main_frame.cpp | 19 | 17 | 2 | 12 | 11 | 8 | 7 |
| application/resources.cpp | 3 | 1 | 2 | 2 | 2 | 1 | 1 |
| application/sfc_ole.cpp | 18 | 11 | 7 | 7 | 4 | 11 | 11 |
| archive/archive.cpp | 3 | 0 | 3 | 3 | 3 | 0 | 0 |
| archive/backup.cpp | 2 | 2 | 0 | 0 | 0 | 2 | 2 |
| archive/funeral_kit.cpp | 1 | 1 | 0 | 1 | 1 | 0 | 0 |
| biochemistry/biochemistry.cpp | 9 | 5 | 4 | 6 | 6 | 3 | 3 |
| brain/blackboard.cpp | 10 | 8 | 2 | 3 | 3 | 7 | 7 |
| brain/brain.cpp | 10 | 7 | 3 | 3 | 3 | 7 | 7 |
| brain/classifiers.cpp | 3 | 1 | 2 | 3 | 3 | 0 | 0 |
| brain/instinct.cpp | 5 | 2 | 3 | 3 | 3 | 2 | 2 |
| brain/lobe.cpp | 6 | 6 | 0 | 2 | 0 | 6 | 4 |
| brain/locus_resolver.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| brain/neuron.cpp | 2 | 2 | 0 | 0 | 0 | 2 | 2 |
| brain/rules.cpp | 2 | 2 | 0 | 0 | 0 | 2 | 2 |
| common/filesystem.cpp | 2 | 2 | 0 | 0 | 0 | 2 | 2 |
| common/logging.cpp | 2 | 1 | 1 | 1 | 1 | 1 | 1 |
| common/time.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| creatures/attention.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| creatures/bacterium.cpp | 8 | 4 | 4 | 6 | 5 | 3 | 2 |
| creatures/body.cpp | 15 | 9 | 6 | 11 | 6 | 9 | 4 |
| creatures/creature.cpp | 57 | 54 | 3 | 37 | 34 | 23 | 20 |
| creatures/events.cpp | 8 | 8 | 0 | 6 | 0 | 8 | 2 |
| creatures/genome.cpp | 17 | 16 | 1 | 1 | 1 | 16 | 16 |
| creatures/learned_words.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| creatures/owner.cpp | 4 | 2 | 2 | 4 | 4 | 0 | 0 |
| creatures/registry.cpp | 9 | 4 | 5 | 5 | 5 | 4 | 4 |
| creatures/skeleton.cpp | 33 | 30 | 3 | 9 | 7 | 26 | 24 |
| creatures/update.cpp | 3 | 3 | 0 | 0 | 0 | 3 | 3 |
| creatures/voice.cpp | 10 | 9 | 1 | 1 | 1 | 9 | 9 |
| display/bitmap.cpp | 4 | 1 | 3 | 3 | 3 | 1 | 1 |
| display/blit.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| display/font.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| display/gallery.cpp | 9 | 3 | 6 | 7 | 6 | 3 | 2 |
| display/image.cpp | 10 | 7 | 3 | 4 | 3 | 7 | 6 |
| display/palette.cpp | 6 | 4 | 2 | 2 | 2 | 4 | 4 |
| display/profiler.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| display/rendering.cpp | 26 | 24 | 2 | 6 | 6 | 20 | 20 |
| display/sprite_cache.cpp | 5 | 1 | 4 | 4 | 4 | 1 | 1 |
| objects/bubble.cpp | 10 | 7 | 3 | 7 | 7 | 3 | 3 |
| objects/call_button.cpp | 7 | 3 | 4 | 5 | 5 | 2 | 2 |
| objects/compound_object.cpp | 31 | 29 | 2 | 8 | 7 | 24 | 23 |
| objects/debug.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| objects/entity.cpp | 12 | 10 | 2 | 5 | 4 | 8 | 7 |
| objects/events.cpp | 4 | 4 | 0 | 3 | 2 | 2 | 1 |
| objects/lifecycle.cpp | 2 | 2 | 0 | 2 | 1 | 1 | 0 |
| objects/lift.cpp | 15 | 12 | 3 | 15 | 15 | 0 | 0 |
| objects/object.cpp | 38 | 36 | 2 | 14 | 11 | 27 | 24 |
| objects/renderable_set.cpp | 2 | 1 | 1 | 2 | 2 | 0 | 0 |
| objects/scenery.cpp | 5 | 2 | 3 | 4 | 4 | 1 | 1 |
| objects/simple_object.cpp | 37 | 34 | 3 | 22 | 21 | 16 | 15 |
| objects/vehicle.cpp | 9 | 6 | 3 | 8 | 7 | 2 | 1 |
| platform/com.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| platform/environment.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| platform/formatting.cpp | 3 | 3 | 0 | 0 | 0 | 3 | 3 |
| platform/gdi.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| platform/mfc_adapters.cpp | 2 | 2 | 0 | 1 | 1 | 1 | 1 |
| platform/registry.cpp | 3 | 3 | 0 | 0 | 0 | 3 | 3 |
| platform/security.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| scripting/classifier_scripts.cpp | 4 | 4 | 0 | 4 | 4 | 0 | 0 |
| scripting/dde.cpp | 17 | 9 | 8 | 17 | 17 | 0 | 0 |
| scripting/macro.cpp | 24 | 20 | 4 | 18 | 16 | 8 | 6 |
| scripting/macro_holder.cpp | 13 | 10 | 3 | 7 | 5 | 6 | 6 |
| scripting/pipe_server.cpp | 20 | 15 | 5 | 9 | 5 | 15 | 11 |
| scripting/tables.cpp | 1 | 1 | 0 | 1 | 1 | 0 | 0 |
| sound/cache.cpp | 1 | 0 | 1 | 1 | 1 | 0 | 0 |
| sound/sound.cpp | 17 | 17 | 0 | 1 | 0 | 17 | 16 |
| ui/caos_console.cpp | 16 | 11 | 5 | 6 | 2 | 10 | 10 |
| ui/classifier_tip.cpp | 1 | 1 | 0 | 1 | 1 | 0 | 0 |
| ui/creature_selection.cpp | 5 | 3 | 2 | 2 | 2 | 3 | 3 |
| ui/debug_console.cpp | 14 | 11 | 3 | 5 | 2 | 9 | 9 |
| ui/dialogs.cpp | 5 | 0 | 5 | 5 | 0 | 0 | 0 |
| ui/event_bar.cpp | 12 | 7 | 5 | 5 | 5 | 7 | 7 |
| ui/eye_view.cpp | 8 | 7 | 1 | 8 | 8 | 0 | 0 |
| ui/magic_profiler.cpp | 1 | 1 | 0 | 1 | 1 | 0 | 0 |
| ui/main_window.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| ui/menus.cpp | 1 | 1 | 0 | 0 | 0 | 1 | 1 |
| ui/place_dialog.cpp | 7 | 5 | 2 | 2 | 0 | 5 | 5 |
| ui/score.cpp | 4 | 2 | 2 | 2 | 2 | 2 | 2 |
| ui/system_info.cpp | 1 | 0 | 1 | 1 | 1 | 0 | 0 |
| ui/tip_dialog.cpp | 11 | 8 | 3 | 3 | 3 | 8 | 8 |
| ui/toolbars.cpp | 7 | 3 | 4 | 4 | 4 | 3 | 3 |
| ui/tools.cpp | 11 | 8 | 3 | 5 | 5 | 6 | 6 |
| ui/version_dialog.cpp | 3 | 2 | 1 | 1 | 1 | 2 | 2 |
| ui/views.cpp | 29 | 24 | 5 | 10 | 9 | 20 | 19 |
| ui/volume_dialog.cpp | 3 | 2 | 1 | 1 | 1 | 2 | 2 |
| ui/windows.cpp | 2 | 2 | 0 | 0 | 0 | 2 | 2 |
| ui/world_statistics.cpp | 6 | 5 | 1 | 1 | 0 | 5 | 5 |
| world/geometry.cpp | 3 | 3 | 0 | 1 | 1 | 2 | 2 |
| world/map.cpp | 7 | 4 | 3 | 4 | 4 | 3 | 3 |
| world/places.cpp | 5 | 2 | 3 | 4 | 4 | 1 | 1 |
| world/settings.cpp | 2 | 0 | 2 | 2 | 2 | 0 | 0 |
| world/tick.cpp | 2 | 2 | 0 | 0 | 0 | 2 | 2 |
| world/update_timer.cpp | 3 | 3 | 0 | 0 | 0 | 3 | 3 |
| world/viewport.cpp | 4 | 4 | 0 | 0 | 0 | 4 | 4 |
