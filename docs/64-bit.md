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
| An object as a CAOS value (`targ`, `from`, `_it_`, `ownr`, `var0`, `obv0` and the others that can hold an object) | `objects/object_handle.hpp`: `handle_of`, `object_at_handle`; used by `scripting/macro.cpp` | Scripts (`dde: putv targ` prints it), kits, saved worlds (a variable keeps the value it held) | Done. 32-bit: the handle is the address. 64-bit: `objects/object_handle.cpp` gives each object a handle (nonzero, 4-aligned, from 0x00400000, never reused) the first time one is needed; `Object`'s destructor takes it back. Test: `tests/c1_object_handle_test.cpp`. |
| An object in the lab's records and debug readouts | `creatures/creature.cpp` (brain recorder), `platform/windows_document_host.cpp`, `platform/windows_macro_host.cpp` | The lab, the debug console | Done: uses `handle_of`. |
| A pending macro's token | `scripting/pipe_server.cpp`, `PipeServer::pending_command_token` | Kits, through the pipe | Done. 32-bit: the `MacroHolder`'s address, as in the original. 64-bit: a counter. |
| Neuron addresses in row 5's duplicate test (native mode only) | `brain/neuron.cpp`, `original_pointer_value` | Nobody; it decides a migration | Keep: the result depends on heap addresses in the original too, so it cannot be exact on any build. The value is already cut to 32 bits explicitly. |
| The event and stimulus queue records | `objects/events.hpp`, two `static_assert`s on the record sizes (24 and 20 bytes) | Nobody outside the program | Done: the sizes are the original's, and nothing depends on the byte layout, so the asserts apply to the 32-bit build only. |
| Window, DDE and process handles | `platform/windows_*` | Windows | Already pointer-sized (`uintptr_t`). |

## Where it stands

The game logic (everything outside `platform/`) compiles as a 64-bit
program with GCC and clang. (The pipe server held an unused copy of the
kit pipe's IDispatch proxy, with the Windows COM headers; the live one is
`platform/windows_pipe_dispatch_proxy`, so the copy is gone.) Clang's
`-Wshorten-64-to-32` finds one narrowing in that code, now explicit
(`brain/brain.cpp`).

c1-lab's `tools/build_source.py --arch x64` builds the whole program for
x64 with the same MSVC 14.44 toolchain (x64 compiler, MFC, CRT and SDK
libraries, `/MACHINE:X64`, no `/FIXED`). The whole program, the platform
layer too, compiles for x64 with no error. `main` links a 64-bit
Creatures.exe (2026-10-19). On `neorender` the link also needs SDL3's x64
library, which `third_party/SDL3` does not have yet.

Not run yet. To run it, a 64-bit build needs the x64 `mfc140.dll`,
`vcruntime140.dll`, `vcruntime140_1.dll` and `msvcp140.dll` beside it
(`VC/Redist/MSVC/14.44.35112/x64` in the toolchain); the lab's Wine
prefixes are already 64-bit (WoW64), so 32-bit kits can run beside it.

## Other work for a 64-bit build

- Run `-Wshorten-64-to-32` on the platform layer too, once it builds for
  x64. On Linux and macOS `long` is 64 bits; on 64-bit Windows it stays 32
  bits.
- Kits: a 32-bit kit and a 64-bit game can talk through DDE, OLE automation
  and the pipe. Done: the game looks for a kit's `CLSID\{...}\LocalServer32`
  in the 32-bit registry view too (`platform/windows_com_host.cpp`), and a
  64-bit build writes `SFC.OLE`'s `LocalServer32` in both views
  (`platform/windows_application_host.cpp`), so 32-bit kits find it.
- The kit protocol carries no pointer-sized field: `Communicate` takes a
  32-bit header and a 32-bit payload (a genome file id, a Funeral Kit state
  word or 0), and kits get object values only through CAOS replies, which
  are handles.
- The lab needs a 64-bit (WoW64) Wine prefix to run a 64-bit game with
  32-bit kits.

## Tests

The function tests, the CAOS tests and the world test must give the same
results before and after each step. For a 64-bit build, the world test must
compare object values as references, not as numbers: a saved variable that
holds an object holds an address, which is different in every program.
