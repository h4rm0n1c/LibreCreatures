# 64-bit builds

LibreCreatures is a 32-bit program, as Creatures 1 is. The 32-bit build is
the default and the reference: the verification tests compare it with the
original. A 64-bit build is a future option. This file lists what a 64-bit
build needs, and what is done.

## The rule

Creatures 1 keeps some addresses in 32-bit values that scripts, kits and
saved worlds can see. A 64-bit address does not fit. Each such value goes
through one abstraction. In the 32-bit build the abstraction gives the
address, so nothing changes. In a 64-bit build it gives a 32-bit handle from
a registry.

## Inventory

| Value | Where | Seen by | Status |
| --- | --- | --- | --- |
| An object as a CAOS value (`targ`, `from`, `_it_`, `ownr`, `var0`, `obv0` and the others that can hold an object) | `objects/object_handle.hpp`: `handle_of`, `object_at_handle`; used by `scripting/macro.cpp` | Scripts (`dde: putv targ` prints it), kits, saved worlds (a variable keeps the value it held) | Done for 32-bit: the handle is the address. 64-bit: the registry, not yet written; the header stops a 64-bit build. |
| An object in the lab's records and debug readouts | `creatures/creature.cpp` (brain recorder), `platform/windows_document_host.cpp`, `platform/windows_macro_host.cpp` | The lab, the debug console | Done: uses `handle_of`. |
| A pending macro's token | `scripting/pipe_server.cpp` (`MacroHolder` address) | Kits, through the pipe | To do: a token table. |
| Neuron addresses in row 5's duplicate test (native mode only) | `brain/neuron.cpp`, `original_pointer_value` | Nobody; it decides a migration | Keep: the result depends on heap addresses in the original too, so it cannot be exact on any build. The value is already cut to 32 bits explicitly. |
| The event and stimulus queue records | `objects/events.hpp`, two `static_assert`s on the record sizes (24 and 20 bytes) | Nobody outside the program | To do: the records hold pointers. Check why the sizes are pinned, then let them grow or store handles. |
| Window, DDE and process handles | `platform/windows_*` | Windows | Already pointer-sized (`uintptr_t`). |

## Other work for a 64-bit build

- Find every narrowing of a pointer-sized value. Clang's
  `-Wshorten-64-to-32` lists them. On Linux and macOS `long` is 64 bits
  too; on 64-bit Windows it stays 32 bits.
- Kits: a 32-bit kit and a 64-bit game can talk through DDE, OLE automation
  and the pipe. The game finds a kit through `CLSID\{...}\LocalServer32`,
  which a 32-bit kit writes in the 32-bit registry view: read both views.
  Write `SFC.OLE`'s registration in both views, so 32-bit kits find a
  64-bit game.
- Check the kit protocol's packed header and the `c1kitlib` structures for
  fields of pointer size.
- The lab needs a 64-bit (WoW64) Wine prefix to run a 64-bit game with
  32-bit kits.

## Tests

The function tests, the CAOS tests and the world test must give the same
results before and after each step. For a 64-bit build, the world test must
compare object values as references, not as numbers: a saved variable that
holds an object holds an address, which is different in every program.
