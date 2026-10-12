# Perception filter

This page tells what a creature can perceive in Creatures 1, and what
LibreCreatures removes from it. Informal name: the perception filter fix.

"Native" means the Community Edition `Creatures.exe` (Community Release
10.3). The rules come from `Creature::CanPerceiveObject` at 00409750.

## 1. How a creature perceives

A creature perceives an object only if all of these are true:

- The object is not the creature.
- The object does not have attribute 16 ("creatures cannot see it").
- The object is in the direction that the creature faces.
- The object overlaps the creature's perception area.
- The centre of the object is less than 500 pixels from the creature,
  horizontally.

The perception area depends on where the creature is:

| Where the creature is | Perception area |
| --- | --- |
| In a room | That room. |
| In a lift (classifier 3 2) | None. The creature perceives nothing. |
| In another vehicle, with its foot in a room | That room. |
| In another vehicle, with its foot outside all rooms | The full world: x 0 to 8352, y 0 to 1200. |

The last row is intentional. A creature that rides the submarine or the
cart through an area without rooms can see things that it cannot reach.
It can learn that these things exist.

## 2. What the base game allowed

The full-world view also showed things that are not part of the world.
They come from how the simulation works.

### 2.1 New objects at the world origin

The `new:` commands make every object at the world origin, (0, 0). The
script then moves the object to its place. `new:` ends the turn of the
script, so the object stays at the origin for at least one turn.

Example: when a norn lays an egg (script 4 0 0 65), the egg stays at the
origin for one turn. The egg is visible (attribute 64). If something
stops the script during that turn, the egg stays at the origin. This is
the egg in the sky (original-bugs row 18).

A creature that rides a vehicle outside all rooms, within 500 pixels of
x 0, can perceive the object at the origin. The cart in the undersea
tube passes this point.

### 2.2 Creatures parked outside the world

Some scripts park a new creature below the world while it develops:

- The incubator egg (scripts 2 5 2 5 and 2 5 2 9) parks the new norn at
  (0, 2000).
- The Grendel mother (script 2 5 6 1) parks the new grendel at (0, 2000).
- `new: crea` makes the creature at (1000, 2000).

Other creatures cannot see a parked creature: it is outside the world
area. But the parked creature continued to perceive from where it was.

### 2.3 What was already safe

- Objects outside the world area are not perceived. This includes
  objects that `kill` parks at (1000, 4000). Row 9 stops creatures from
  remembering that position.
- Objects that a script makes invisible before it moves them. Example:
  the lemon (2 6 4) sets attribute 16 before it moves to (571, 1084), and
  clears it after it moves back.

## 3. What LibreCreatures changes

LibreCreatures adds two rules. They apply to all perception.

1. **A new object is not perceived while it is at its spawn point.**
   The new-object host marks each object that `new:` makes. A creature
   does not perceive a marked object while the object touches the world
   origin. When a creature perceives the object at a different position,
   the mark clears. After that, the object is perceived as usual.
2. **A creature outside the world perceives nothing.** If the foot of a
   creature is outside x 0 to 8352 or y 0 to 1200, the creature does not
   perceive any object.

## 4. What does not change

- The full-world view of a creature in a vehicle. It still sees the fish
  and jellyfish in the sea, the birds in the sky, and other objects that
  no room covers.
- A creature in a lift still perceives nothing.
- Perception in a room.
- Objects that scripts or COBs make and then move. The first move takes
  the object away from the origin, so the mark has no effect.
- The world file. The mark is not saved. After a world loads, no object
  has the mark.

## 5. Possible side effect

An object that a script makes and never moves stays at the origin. A
creature cannot perceive it. The origin is the top-left corner of the
sky. No room is there, and only a creature in a vehicle could see it.

## 6. Lab results

All runs use the stock world. A norn rides the cart, which is stopped in
the undersea tube at x 300. Two pieces of cheese (2 6 1) are made with
`new:`. One stays at the origin. One moves to (150, 300). No room covers
either position.

| Build | World ticks | Attention changes | Cheese at the origin | Cheese at (150, 300) | Fish and seahorses |
| --- | --- | --- | --- | --- | --- |
| Without the filter (`a274fd2`) | 9,464 | 36 | 1 | 6 | 4 |
| With the filter (`d17fed8`) | 26,112 | 552 | 0 | 59 | 113 |

The numbers count attention changes to each target. Without the filter,
the norn attended the cheese at the origin once, at a distance of 310
pixels. With the filter, in a run almost three times longer, it never
did. It still attended the cheese at (150, 300) and the animals in the
sea, so the full-world view still operates. The sample is small: one
event shows that the base game allows it.

COB check: the Albia Edible Moss COB was injected into the build with
the filter. A norn attended the moss by sight (visible=1) at three
positions.

## 7. Source

- `Creature::can_perceive` in `src/c1/creatures/creature.cpp`
- `Object::awaiting_first_placement` in `src/c1/objects/object.hpp`
- `WindowsNewObjectHost::adopt` and `create_creature` in
  `src/c1/platform/windows_macro_host.cpp`
- `WindowsCreatureFanoutHost::is_unplaced_new_object` in
  `src/c1/platform/windows_creature_hosts.cpp`
