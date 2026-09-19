# Persistence: what a world file means

A saved world is a snapshot of the document, map, objects, scripts, and live
simulation state. It is not only a collection of creature files. Loading must
rebuild the same ownership graph that the running world uses, then repair the
transient registries that make rendering and ticking efficient.

The document-level archive code is in [`document.cpp`](../src/c1/application/document.cpp).
World ownership is in [`runtime.hpp`](../src/c1/world/runtime.hpp).

## Save order in human terms

The world archive records these groups in a stable order:

1. Map data.
2. Non-scenery objects and their count.
3. Scenery objects and their count.
4. Classifier scripts.
5. Viewport origin, selected creature, favourite places, and toolbar state.
6. Running macros.
7. World objects.
8. Event-bar state and score.
9. The world tick count and document state words.

The order is part of the file contract. A reader must consume the groups in the
same order even when a group is empty.

## Loading is reconstruction

Loading first creates or resets the map and object owners, then reads the saved
records. Loaded world objects do not immediately resume their ordinary ticks.
The document rebuilds bounds and unbounded renderable entries, restores the
viewport and selection, validates required sprite data, and only then arms the
world timer.

This staged repair prevents a half-loaded object from appearing in a renderer
or receiving an event before its dependencies exist. It also explains why a
loaded object may be present in a registry before its first post-load tick.

## Ownership and deletion during a save cycle

The runtime keeps owned containers and borrowed indexes. A save walks the
owned, serialisable sets; a render or attention query may use a borrowed
index. Deletion removes the object from every borrowed index before its owned
storage is released. The archive therefore never needs to preserve a raw
pointer identity.

Document cleanup follows the same ownership direction. It closes the eye view,
deletes non-scenery before scenery, deletes the map, clears sprite and charset
caches, releases galleries, removes running macros, clears registries, and
finally releases framework state.

## External resources

The world file refers to resource data that is supplied outside the executable,
including sprite galleries, character sets, sounds, and genome data. The clean
repository documents and loads those resources through host interfaces rather
than treating their binary formats as interchangeable with the world archive.

That boundary is practical: a world save preserves simulation state, while a
resource pack supplies the artwork, audio, text, and genetic records needed to
interpret that state.

## Time and autosave

The persistent tick count increases at the end of a coordinated world update.
Autosave is considered at that boundary and only when the document has the
required privilege. Score and event-bar notifications are also handled there,
so a save records a complete tick rather than a state halfway through object,
creature, or sound work.

## Source map

- [`document.hpp`](../src/c1/application/document.hpp) describes document
  ownership, serialization, and world-update hosts.
- [`document.cpp`](../src/c1/application/document.cpp) contains the archive
  order and load/save sequencing.
- [`runtime.hpp`](../src/c1/world/runtime.hpp) defines owned objects and the
  borrowed registries rebuilt around them.
