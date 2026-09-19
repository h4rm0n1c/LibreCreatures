# Biochemistry and stimuli

Biochemistry gives a creature a changing internal state. It is a compact
chemical system, not a list of named moods. The genome defines which chemicals
exist in useful concentrations, how they decay, which receptors read them, and
which emitters and reactions change them.

The implementation is centred on [`Biochemistry`](../src/c1/biochemistry/biochemistry.hpp).

## The chemical state

Each creature has 256 chemical slots. A slot stores a byte concentration and a
half-life selector. A concentration can be increased directly, reduced by its
half-life, changed by a reaction, or changed by an emitter. The numeric value
is deliberately small and fast to update; its meaning comes from the genome
and the loci that use it.

There are three kinds of wiring:

| Record | Direction | Purpose |
| --- | --- | --- |
| Receptor | Chemical → brain or creature locus | Reads a chemical after its threshold and gain rules are applied. |
| Emitter | Brain or creature locus → chemical | Adds a configured amount at a configured period when its source is active. |
| Reaction | Chemicals → chemicals | Consumes up to two reactants and produces up to two products at a selected tick rate. |

Brain and creature loci are separate domains. A receptor may therefore feed a
brain lobe input, while another receptor reads a creature state byte. The
source and target are resolved from genome records rather than from hard-coded
chemical names.

## Stimuli join outside events to chemistry

A stimulus is a packed context used by perception and scripts. It carries an
attention activation, a target neuron index, a target-lobe activation, flags,
four chemical identifiers, four chemical amounts, and optional target-object
and source-creature references.

The default stimulus table has 36 contexts. Genome loading copies and mutates
those contexts for a particular creature; the table is not shared mutable state
between all creatures. A perceived object can therefore supply both a target
for attention and chemical information for the creature's internal state.

## A typical drive-to-action path

Consider a hungry creature near a food object. The details vary with the
genome, but the mechanism can be read as this sequence:

1. A chemical concentration changes because of decay, a reaction, an emitter,
   or a stimulus.
2. A receptor converts that concentration into a brain or creature-locus
   input.
3. The creature update copies drive and perception inputs into the relevant
   lobes. An attended food object supplies target context.
4. Brain connections activate a decision pattern. Verb and noun activity make
   an action candidate meaningful.
5. Action selection checks target requirements and emits a body action or CAOS
   event.
6. The action changes the world or adds another stimulus, closing the loop.

This is a semantic pattern, not a claim that every genome uses a “hunger” slot
or a single food receptor. The same machinery can represent fear, fatigue,
curiosity, learning, or a kit-generated stimulus.

## Timing and persistence

Chemistry advances during the creature phase of the world tick. Concentrations,
reaction rates, and half-lives therefore depend on the world clock. Chemistry
is serialised with the creature so that loading a world restores its internal
state rather than merely restoring the visible pose.

## Source map

- [`biochemistry.hpp`](../src/c1/biochemistry/biochemistry.hpp) defines
  concentrations, receptors, emitters, reactions, and update operations.
- [`stimulus.hpp`](../src/c1/creatures/stimulus.hpp) defines the stimulus
  context passed into the creature.
- [`update.hpp`](../src/c1/creatures/update.hpp) copies drive and perception
  inputs into brain state.
