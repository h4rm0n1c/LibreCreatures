# Changes from Creatures 1 Community Edition

This is every way LibreCreatures behaves differently from the Creatures 1
Community Edition (CE) executable and the 1996 kits: defects fixed,
features added, and deliberate changes of behaviour.  Where an item is a
defect of the original, the row number in
[original-bugs.md](original-bugs.md) gives the native function and the
evidence.

Not listed: fixes to LibreCreatures' own translation of the game, which only
make it match CE again.

In the source, each change is marked where it is made: `LibreCreatures
deviation` (a native defect changed on purpose), `Not native` (something
added), or `Fix (bug N)` in the kits (that kit's `ORIGINAL.md`).

## What does not change

- The world file format.  A world saved by LibreCreatures opens in CE, and
  the other way round.
- The kit protocol (DDE, SFC.OLE and the Wine pipe), its message codes and
  the Tools menu slots.  The 1996 kits work with LibreCreatures, and the
  LibreCreatures kits work with CE.
- CAOS.  No command was removed or given a new meaning.  Additions are
  marked below.
- Tick order, the 90 ms world tick, and what a tick does, except where an
  item below says so.

## Both builds

### Creatures: brain

- Every creature updates on a four-tick cycle.  CE stepped by four through
  five cohorts, so every fourth creature got a second brain, chemistry and
  action pass every fifth tick.  (row 1)
- Connection rules are evaluated for every rule, with fresh registers for
  each neuron.  (row 2)
- A lobe with no neurons no longer divides by zero.  (row 3)
- Dendrite migration moves the dendrites instead of zeroing the neuron's
  weights.  (rows 4, 5)
- A loose dendrite can re-attach to its own previous target.  (row 6)
- The attended object's sense reaches the brain.  CE read it as a signed
  byte, so every sense value of 128 or more arrived as 0.  (row 8)
- Optional, off by default: the **ExtendedBrainGrid** setting lets genomes
  place lobes on a 208 x 208 grid instead of 64 x 64.  Lobe sizes keep
  their limits.  Brains built before the setting changes keep their layout.
  A lobe off the standard grid is left out of brain reports to kits, and
  reported as parked in the corner of the grid, so the 1996 Science and
  Health Kits cannot write outside their arrays.

### Creatures: chemistry, genetics and disease

- A reaction adds its whole yield; CE kept only the low byte, so 1 A -> 2 B
  with 128 A made no B.  (row 50)
- A reaction with the same chemical in both reactant slots shares its
  supply between them instead of counting it twice.  (row 51)
- Mutation no longer damages the `gext` pigment tag.  (row 7)
- A mutated bacterium still makes a disease chemical (232-235), not
  chemical 0-3.  (row 42)
- Bacterial threshold mutations clamp instead of wrapping.  (row 44)
- A creature with no gestation chemical reports its pregnancy correctly.
  (row 15)
- An imported creature whose genome is renamed breeds with its own genome,
  not with whichever creature now has its old name.  (row 49)
- A creature whose generated body sprite file is missing is made visible
  again.  (row 16)

### Creatures: perception, attention and action

- Killed objects are forgotten.  `kill` parks an object off the world; CE
  left creatures remembering it there, so norns kept going "down" through
  lifts to find it (issue #11).  A killed object is dropped from every
  creature's attention, and a record keeps the last real position.  Worlds
  that already hold such a memory are repaired on load.  (row 9)
- The perception filter.  A norn riding a vehicle outside every room
  still sees the whole world, but not an object `new:` has just made,
  still waiting at the world origin for its script (the egg in the sky),
  and a creature parked outside the world sees nothing.  See
  [Perception filter](perception-filter.md).  (row 73)
- An object whose kind has no attention slot no longer overwrites other
  creature memory, and is not adopted as a fallback target.  (row 10)
- A creature whose involuntary action has no script goes back to using its
  brain.  (row 11)
- A creature only chooses a gait that has an animation.  (row 12)
- Eating no longer disappoints a norn.  Food, drink, herb and weed scripts
  start with `drop`, and CE gave a creature Disappointment for every empty
  `drop`.  The creature's own drop still disappoints.  In A/B tests with the
  same founders, starvation went down in every world.  (row 13)
- Lonely and crowded norns say "come Norn" and "run Norn", not "comeNorn"
  and "runNorn".  (row 14)
- `stm# tact` stimulates only the creatures the source touches, not every
  creature in the world.  (row 17)
- Two creatures are siblings only if they share a known parent.  (row 38)
- A command whose noun is "rest" is delivered when attention clears.
  (row 40)
- The status report names the room the creature is in where rooms are
  stacked.  (row 43)

### Creatures: speech and learning

- Every creature that hears speech hears all of it.  CE split the words in
  the shared buffer, so after the first listener "push food" became "push".
  (row 37)
- A double or trailing space in speech no longer makes an empty word that
  erases the attended object's name.  (row 39)
- Pressing Enter with nothing typed says nothing.  (row 32)

### Scripts (CAOS)

- **The egg in the sky is gone.**  `new:` no longer ends a script's turn,
  so a laid egg is classified and moved off the world origin in the same
  turn that makes it.  Before that change, three others each closed one way
  an egg could be left there: an event for a paused script waits until the
  script has finished its turn; a script's owner dying or being removed
  lets the script finish its turn first; and a script saved mid-lay keeps
  that hold when loaded.  (row 18)
- A script that ends no longer costs the next script its turn.  (row 61)
- `enum .. kill targ .. next` visits every creature.  (row 53)
- An `enum` with no match skips its whole body, nested enums included.
  (row 54)
- `doif`, `enum` and `gsub` skip bracketed text when they scan, so text
  such as `[else]` or `[subr x]` no longer steers the script.  (rows 55, 56)
- `divv` and `modv` of -2147483648 by -1 no longer stop the game.  (row 60)
- `ltcy` and `rndv` no longer divide by zero over an empty or whole range.
  (rows 20, 58)
- `evnt` with no object, and a bad value used as an object, no longer crash
  the game.  (rows 19, 21)
- A syntax error is written to the debug log and the script carries on.
  CE showed a modal "OK / Cancel" box from inside the running script, and
  Cancel on a script a kit had sent ended the game.  (row 65)
- `dde: died` also writes the time of death to `Funeral Kit Deaths` in the
  world folder, for the Funeral Kit.

### Objects, lifts and vehicles

- An object let go of above its floor falls to it instead of appearing
  there, and plays its landing sound when it lands.
- An object let go of in the upper half of a room lands on that room's
  floor, not the floor of the room above.  (row 27)
- An object in a gap between rooms no longer falls out of the world (the
  Black Hole).  (row 22)
- A vehicle no longer drags objects that a script has moved away from it
  (the Flying Lemon).  (row 23)
- Norns no longer get stuck in a lift called to its own floor.  (row 24)
- A lift moving at a fractional speed stops at its floor.  (row 41)
- A lift answers calls from more than four buttons.  (row 45)
- A deleted call button or lift no longer leaves the other pointing at
  freed memory, and a ninth button or a button outside every room is
  ignored instead of corrupting the lift.  (rows 46, 47)
- A vehicle that keeps circling the world stays on it.  (row 52)
- `mvto`, `mvby` and `mcrt` move a vehicle for good; it no longer jumps
  back.  (row 57)
- A moving vehicle shows lowercase animation frames correctly.  (row 59)
- A carried object's drawing plane no longer depends on memory addresses.
  (row 25)

### The hand

- A click goes to the creature or object under the hand, not to a lift,
  incubator or pianola drawn over it.  (row 28)
- A click acts where it was made, even right after moving the mouse.
  (row 29)
- The hand is drawn over everything, including objects made after it.
- The hand lets go of an object that is killed or deleted while carried,
  instead of freezing.  (row 31)
- A shift + right-click pickup applies to that one pickup only.

### Selection, camera and event bar

- Deleting a creature deselects it only if it was the selected creature,
  so the player's norn stays selected and its kits stay open.  (row 30)
- A loaded world with Track Creature on opens on the selected creature,
  not at the saved origin with a pan down from the sky.
- Clicking an object on the event bar scrolls to it even with Smooth
  Scrolling on.  (row 68)
- Save, Save As and a speed change no longer restart a paused world.
  (row 33)

### Sound

- The sound cache is large enough for the world's sounds; CE dropped
  sounds without an error.  (row 36)
- Looping sounds come back after Mute is turned off and after the game
  window gets focus again.  (row 62)

### World data

- Ocean Dome Sound is hidden from creatures when a world loads, so hungry
  norns no longer go into the dome looking for a vendor (issue #10).  The
  world file is only changed when it is next saved.  (row 26)
- Two creature scripts in the stock world are corrected when a world loads,
  if they are still the shipped text: Retreat now plays its plain retreat
  animation, and Say Need gives sleepiness the tired pose.  (rows 69, 70)
- Norns can play the drum: it no longer loads stuck in the active state,
  and playing it clears that state again.  (row 71)
- The island boat runs on a working timer, as the rafts and the cart do:
  the timer docks it at each end and sends it back.  (row 72)

### Files and windows

- **File > Export Held Egg... and Import Egg...** move an egg between worlds
  as a file.
- Exporting a creature deletes it only after the file is written, so a
  full disk no longer loses the norn.  (row 48)
- A failed save is reported with its reason, in a window whose text can be
  copied.  World.sfc is left as it was.
- One copy of the game per world: a world already open in another copy is
  refused with the reason, instead of the two saving over each other.
- A maximised window is restored maximised.

### Kit protocol (in the game)

- A brain report measures what the kit asks for over SFC.OLE too.  CE
  always sent firing strength.  (row 35)
- A brain report is not limited by the kit's buffer.  CE wrote past it
  with more than about 1,365 active neurons.  (row 63)
- A slow kit command no longer reads freed memory.  (row 34)
- Under Wine, a kit that crashes or is killed has its slot closed, and the
  game no longer waits on it.  (row 64)
- A snapshot (`dde: pict`, the Owner's Kit photograph) copies the right
  rows.  (row 66)
- Kit toolbar buttons show the kit's name as their tooltip.  (row 67)
- New DDE queries (LibreCreatures only; CE ignores them): `dde: dcap`
  (which additions the game has), `dde: dend` (a neuron's dendrites) and
  `dde: ecol` (rooms and creatures, for the Ecology Kit).

## The kits

All ten 1996 kits are rebuilt, under the original file names, ProgIDs and
Tools slots.  Each kit's `ORIGINAL.md` lists the 1996 kit's defects; 62
are fixed.

### All kits

- Most kit windows can be resized, and an Options menu holds Always on
  top, Mute sounds and Skin.  (The 1996 kits had Always on top only in the
  registry.)
- **Classic skin:** with the player's own 1996 kit renamed to `<kit>.old`
  beside the new one, a kit shows the 1996 pictures and layouts.  None of
  the 1996 art is copied into LibreCreatures.
- The modern look has no cover pages and no continuous sound.
- A kit that crashes shows a report and saves it, with a minidump, in
  `%LOCALAPPDATA%\LibreCreatures\Crash Reports`.
- Settings and files are saved as they change, not only when the kit
  closes.  Under Wine the game ends a kit's process when it closes it, so
  the 1996 kits lost them there.

### Each kit

- **Hatchery:** refills the nest when it is empty instead of asking for an
  Egg Disk, and stays open after an egg.  New options: Scramble Eggs (each
  egg crosses a random pair) and Make My Creatures Colourful.
- **Owner's Kit:** the register and albums are saved as they change; the
  tabs fit; errors name the file.  (9 fixes)
- **Health Kit and Breeder's Kit:** shop stock is saved at once; "no
  subject" no longer loops.  (3 fixes each)  Options > Refill Dispensary
  puts every item back to 12, the stock the game ships.
- **Science Kit:** the genome's species and the brain scanner's measure are
  right, and dendrite ranges and totals are read as the game reads them.
  (5 fixes)
- **Biochemistry Kit:** a 33rd graph channel no longer writes past the
  32, and the history keeps recording after 2048 samples.  (2 fixes)
- **Object Injector:** a failed inject does not use up the COB, a COB with
  none left cannot be injected, and long scripts are sent whole.  (3 fixes)
- **Observation Kit:** lists every creature (CE stopped at 13), sorts by
  any column, and raises alerts without blocking.  A double-click on a
  creature selects it, turns Track Creature on and runs "Which is my
  creature?".  (17 fixes)
- **Funeral Kit:** each grave is dated when the creature died, Make
  Headstone works, the life span keeps the birth year, and its files survive
  the kit being closed by the game.  (6 fixes)  In the modern look the
  Graveyard tab puts the headstone in the middle with a note on how stones
  are made, and the Deaths tab says what it shows until a creature dies.
- **Score Kit:** larger counters, more icons, labels that fit, settings
  kept.  (12 fixes)
- **Ecology Kit (new, Tools slot 10):** a map of the world, with rooms
  coloured by temperature, crowding, food, toys or disease, every creature
  and every egg.  A click on a creature selects it and tracks it.

## neorender build only

- **Art tiers:** galleries draw from `.spr`, `.s32` or `@2x.s32` files.
  Game logic always uses the `.spr` frame sizes; a tier only changes what is
  drawn.  Art that ships only as `.s32` works, and the kits read it too.
- **SDL3 drawing** in the world view and the creature's view.  The
  `Renderer` setting (or `C1_RENDERER=gdi`) goes back to the 8-bit GDI
  drawing.
- **View menu:** Zoom (1x, 2x, scale world height to window), Maximum
  detail (SPR, S32, @2x S32) and Creature's view size (normal, double).
- **Smooth motion:** the world is drawn every frame and the hand moves
  with the mouse; the world tick stays at 90 ms and is drawn one tick
  behind.  The camera's moves are eased on real time.
- **World speed:** 1x, 2x, 5x and 10x.  The world clock, ages and timers
  run that much faster.
- **Developer view > Show rooms:** room outlines, with room numbers on
  hover.
- Speech and blackboard text is drawn over S32 art instead of forcing 1x.

## New settings

In `HKCU\Software\Gameware Development\Creatures 1\1.0`.  CE ignores them.

| Value | Build | Meaning |
| --- | --- | --- |
| `Maximised` | both | The game window was maximised when it closed. |
| `ExtendedBrainGrid` | both | 1: lobes may use the 208 x 208 grid.  Off by default. |
| `Zoom` | neorender | View > Zoom. |
| `EyeViewZoom` | neorender | View > Creature's view size. |
| `MaxImageDetail` | neorender | View > Maximum detail. |
| `SmoothMotion` | neorender | View > Smooth motion. |
| `WorldSpeed` | neorender | View > World speed. |
| `ShowRooms` | neorender | View > Developer view > Show rooms. |
| `Renderer` | neorender | `gdi` for the 8-bit GDI drawing. |

## For developers

Off unless set; none changes the game's behaviour.

| Environment variable | Build | Effect |
| --- | --- | --- |
| `C1_LAB_TRACE`, `C1_LAB_TRACE_CATEGORIES` | both | An event trace for the c1-lab harness: kits, kit pipe, selection, camera, scripts, sound, lifecycle, attention, rides, actions, goals, touch, lifts, sprites, dendrites. |
| `C1_BRAIN_RECORD`, `C1_BRAIN_RECORD_MONIKERS` | both | Records creatures' brains (every neuron and dendrite, with each frame's action and attention) to compressed files in a folder, for study with `scripts/brain_record.py`. |
| `C1_TRACE_TICK` | both | World tick timing to `Creatures.tick.log`. |
| `C1_TRACE_CREATURE` | both | Per-creature placement and movement probes. |
| `C1_KIT_TRAFFIC_LOG`, `C1_SOUND_LOG` | both | Kit traffic and sound manager logs. |
| `C1_FRAME_DUMP_DIR` | neorender | `sys: cmnd 32940` dumps the rendered view; 32941 reports drawing tiers. |
