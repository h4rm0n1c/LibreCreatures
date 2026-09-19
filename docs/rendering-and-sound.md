# Rendering and sound

Rendering and sound make the simulation perceivable. They consume world state;
they do not define creature decisions or object ownership. Both systems use a
platform host so the portable game code can describe intent without making
every simulation type know about GDI, palettes, or DirectSound.

## Rendering the world

An entity supplies a gallery, image or image sequence, world position, and
render plane. The world renderer turns those entities into visible sprite
records, clips them to the viewport, and asks its host to draw the gallery
image.

The main renderer keeps a viewport origin, selected-creature follow state,
smooth scrolling, a background gallery, and an overlay gallery. A secondary
eye-view renderer can show a creature without applying its scroll side effects
to the shared document.

### Stable depth order

Visible records are sorted by render plane in ascending order with a stable
sort. Equal-plane records therefore retain their existing order. This matters
for compound objects and carried objects, where a small native plane offset
places a part in front of or behind its carrier without changing its world
position.

### Dirty rectangles

Object movement and animation mark old and new areas as dirty. During a world
update, dirty rectangles are deferred while the object registry and world
phases run. The renderer flushes them after the phases complete. A single
update can therefore change several objects without repainting between every
script or creature action.

The renderer keeps bounded queues for visible records and dirty rectangles. A
full redraw is used after resize, load, palette changes, or another operation
that invalidates the whole buffer.

### The platform edge

[`WorldRendererHost`](../src/c1/display/rendering.hpp) owns the palette, window,
device contexts, DIB, blits, and native drawing calls. The portable renderer
only asks for buffer changes, palette realisation, clipping, and image draws.
This keeps a renderer bug distinct from a world-object bug.

## Sound as scheduled world output

The sound manager owns 32 channels and a bounded sound cache. A cache entry
remembers the backend buffer, byte size, reference count, and last-use
generation. Channels carry attenuation, pan, and a continuous marker.

A sound request can play immediately or be queued for a later tick. Continuous
object sounds are tracked separately so a pause or object deletion can stop
them as a group. The world update services queued sound events, then considers
ambient sound when its cooldown expires.

The DirectSound adapter supplies primary format, PCM buffer creation, locking,
restore, duplication, volume, pan, play, and stop. The sound manager supplies
the game rules for cache reuse, channel choice, scheduling, and suspension.

When the document pauses, it kills the world timer, clears continuous markers,
and stops all active sound. Resuming restores the mixer as part of the document
state change rather than letting audio continue on an independent clock.

## Source map

- [`rendering.hpp`](../src/c1/display/rendering.hpp) defines the world renderer,
  visible records, plane sorting, viewport, and dirty rectangles.
- [`entity.hpp`](../src/c1/objects/entity.hpp) defines gallery and image state.
- [`sound.hpp`](../src/c1/sound/sound.hpp) defines channels, cache, queues, and
  the DirectSound host boundary.
