# Hatched creatures froze the world tick through a stale sleep indicator

Date: 2026-09-15 (Australia/Perth)

## Symptom

An egg could complete incubation and produce a standing Norn or Grendel, but
the newborn appeared inactive/frozen. The symptom reproduced in a freshly
created Windows world and was initially easy to misread as a missing brain,
biochemistry, or hatch activation step.

## Root cause

The newborn was not the first failure. A previously sleeping creature had a
non-null `sleep_indicator_object_` and reached the normal creature update
path, but its indicator had already been freed.

`WindowsCreatureAttentionHost::create_sleep_indicator()` created the native
top-level `SimpleObject` in a `unique_ptr` member of
`WindowsCreatureAttentionHost`. That host is a short-lived stack object,
constructed for one action-selection pass. The creature retained the raw
indicator pointer across ticks, so destruction of the attention host left a
dangling pointer. The next `Creature::update()` reached its final
`move_to_and_redraw(*sleep_indicator_object_, ...)` call and raised an MSVC
RTTI access violation.

The document-level exception boundary recovered, but the world tick had
already been aborted. Every timer tick retried the same object walk, so later
objects—including newly hatched creatures—never received their normal update.
That is why the hatchling looked frozen even though its creation and registry
insertion had succeeded.

## Native evidence

The native `SetSleepIndicator` routine at `0x0040da80` constructs the
indicator as a world `Object`, dispatches its activation event, and leaves the
creature holding the object identity. The native creature update path later
dispatches the indicator's virtual move/redraw operation. The lifetime must
therefore extend beyond the attention/action-selection call that creates it;
the short-lived host vector was not a valid owner.

## Fix

`C1WindowsDocument::adopt_sleep_indicator()` now transfers the indicator to
`WorldRuntime::adopt_non_scenery_object()`, alongside other top-level dynamic
world objects such as speech bubbles. The attention host no longer owns a
temporary indicator vector. The world registry and owner now have the same
lifetime boundary, while the creature may safely retain its raw identity
between ticks.

## Verification

- Diagnostic live run: after Play, the world tick advanced past the previously
  repeating tick `330622`; the formerly failing creature completed its update.
- The newly hatched creatures at the end of the registry received successive
  updates with ages `73, 70, 66`, then `74, 71, 67`, proving that their normal
  ticks resumed.
- Temporary freeze probes were removed before the final build.
- Clean-source build: `139/139` compile, resource compile pass, link pass in
  `/tmp/bm-freeze-final`.
