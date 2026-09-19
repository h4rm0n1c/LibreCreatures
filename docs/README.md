# How Creatures 1 Works

This guide explains the game as a living system. It is for people who want to
understand what a world, a creature, a kit, or a saved world means when they
work with LibreCreatures.

The guide uses ordinary language first. Source links are provided when a reader
wants to follow a concept into the implementation. The diagrams show the
relationships between systems. They are models for reading the game, not a
promise that every genome uses the same dimensions or every object follows the
same path.

## Read the guide in this order

1. [The simulation loop](simulation.md) explains what one world update does.
2. [Creatures](creatures.md) explains the body, life stages, perception,
   attention, action, and reproduction.
3. [Brains](brain.md) explains lobes, neurons, connections, and dreaming.
4. [Biochemistry and stimuli](biochemistry.md) explains the chemical state that
   links drives, senses, brains, and actions.
5. [The world and its objects](world.md) explains rooms, ground, objects,
   classifiers, events, and vehicles.
6. [CAOS and kits](scripting.md) explains scripts and the external interfaces
   that let tools work with a running world.
7. [Persistence](persistence.md) explains what a world file stores and how the
   live object graph is rebuilt.
8. [Rendering and sound](rendering-and-sound.md) explains the visible and
   audible sides of the simulation.

## A few words with precise meanings

| Term | Meaning in this guide |
| --- | --- |
| **World tick** | One scheduled simulation update. It advances object logic, creature work, sound, rendering damage, and the world clock. |
| **Creature** | The complete simulated animal: an object identity plus body, genome, brain, biochemistry, instincts, and life-stage state. |
| **Object** | A world entity with a classifier, bounds, events, and optional rendering and sound state. A creature is one specialised kind of object. |
| **Classifier** | The family, genus, and species numbers used to select an object's CAOS scripts. |
| **Stimulus** | A small package of chemical amounts and target information. It carries meaning from perception or scripts into brain and chemistry. |
| **Lobe** | A genome-defined neural sheet. Standard roles include perception, drives, decision, attention, and concepts. |
| **Kit** | An external tool or embedded tool that communicates with the game through the automation and named-pipe boundary. |

## What is stable, and what is data-driven

The game has stable rules for ownership, event dispatch, ticking, saving, and
the platform boundary. Many details inside a creature are data-driven. A
genome chooses which lobe records load, how large their grids are, which
connections exist, and which chemical receptors and emitters are present.

That distinction matters when reading the guide. The nine standard lobe roles
are stable vocabulary. Their exact grid sizes, connection counts, and chemical
networks belong to a genome and life stage.

The clean-room implementation keeps Windows, MFC, COM, and DirectSound at the
platform edge. The simulation concepts described here live in the portable
parts of [`src/c1`](../src/c1). The original game also depends on external
resource files such as galleries, sounds, and genome data; those resources are
described where they affect a system rather than copied into this repository.

## How to use this guide when extending the project

Start with the page for the system you are changing. Then follow its source
links to the owning runtime. Keep ownership and timing rules intact: a world
object is adopted by the world runtime, a creature's brain and chemistry are
updated by the document's creature phase, and platform calls go through a host
interface. This keeps a new feature understandable both to a person and to
the game loop that must service it.
