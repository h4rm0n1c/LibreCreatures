# The packed classifier: one 32-bit encoding, reused everywhere

Every object in the simulation carries a classifier — the family, genus, and
species numbers that answer "what kind of thing is this?" for scripts,
events, and object-identity checks. [`world.md`](world.md) explains what a
classifier *means*. This page documents the *encoding*: a single packed
`uint32_t` layout that the game recovers from native and that this port
reuses, unchanged, everywhere a classifier needs to be stored, compared, or
matched — not just for objects, but for CAOS script dispatch too.

It is worth reading once and remembering, because it is not confined to one
module: at last count it appears in 27 files across `objects/`, `creatures/`,
`scripting/`, `application/`, and every `platform/windows_*_host.cpp` file
that touches object identity.

## The layout

```
bit:     31        24 23        16 15         8 7          0
        +------------+------------+------------+------------+
        |   family   |   genus    |  species   | event/spare|
        +------------+------------+------------+------------+
```

As a formula: `packed = (family << 24) | (genus << 16) | (species << 8) | low_byte`.

The low byte's meaning depends on which of the two classifier structs is
using it (see below) — for an `Object`'s own classifier it is largely
reserved/legacy; for a *script* classifier it is the event ID the script
fires on. Everything else about the layout — which byte is family, which is
genus, which is species — is identical between the two.

This is not a LibreCreatures invention. It is native's own in-memory
representation, confirmed against the decompiled executable — see the
`0xffff00ff`/`0xff000000`-style masks throughout the code below, each with a
comment tying it back to a specific recovered comparison.

## The two structs that share it

**`Object::Classifier`** (private, inside [`Object`](../src/c1/objects/object.hpp)):

```cpp
struct Classifier {
    std::uint8_t event = 0;
    std::uint8_t species = 0;
    std::uint8_t genus = 0;
    std::uint8_t family = 0;
};
```

Exposed through `classifier_base()` (read) and `set_classifier_base()`
(write, in [`object.hpp`](../src/c1/objects/object.hpp)), which packs/unpacks
with exactly the shifts above. This is every object's runtime identity —
what a family/genus/species check like "is this a Creature?" tests.

**`scripting::ScriptClassifier`** (in
[`scripting/tables.hpp`](../src/c1/scripting/tables.hpp)):

```cpp
struct ScriptClassifier {
    ScriptEvent event = ScriptEvent::deactivate;
    std::uint8_t species = 0;
    std::uint8_t genus = 0;
    std::uint8_t family = 0;
};
```

Field-for-field identical layout to `Object::Classifier`, down to the byte
order. This is the key used to look up which CAOS script handles a given
family/genus/species/event combination, in the fixed
`g_script_definition_entries` table (capacity 2000, see
[`tables.hpp`](../src/c1/scripting/tables.hpp)) and in the 32-entry
`kBuiltinScripts` table in
[`application/document.cpp`](../src/c1/application/document.cpp) — native's
own default behaviours (touch pain/pleasure, the sleep cycle, sayn, drop,
the death/funeral script, and so on), installed once per new world in
`Document::new_document`.

`application/document.cpp`'s `unpack_classifier()` is the canonical
pack → struct conversion for the script table:

```cpp
scripting::ScriptClassifier unpack_classifier(std::uint32_t packed) {
    return scripting::ScriptClassifier{
        static_cast<scripting::ScriptEvent>(packed & 0xffU),
        static_cast<std::uint8_t>((packed >> 8) & 0xffU),
        static_cast<std::uint8_t>((packed >> 16) & 0xffU),
        static_cast<std::uint8_t>((packed >> 24) & 0xffU),
    };
}
```

## Wildcard matching (script lookup only)

Script classifier lookup layers a wildcard convention on top of the same
encoding: **a stored `species` or `genus` byte of `0` means "any"**, and
lookup falls back through three levels of specificity, most specific first.
From [`scripting/classifier_scripts.cpp`](../src/c1/scripting/classifier_scripts.cpp):

```cpp
// Exact match: family, genus, and species all equal.
bool classifier_matches(ScriptClassifier entry, ScriptClassifier query);

// Native compares the entire stored classifier to query & 0xffff00ff.
// The cleared byte is species, not genus; only an explicit zero matches.
bool matches_species_wildcard(ScriptClassifier entry, ScriptClassifier query);

// Both species and genus cleared -- matches by family alone.
bool matches_species_genus_wildcard(ScriptClassifier entry, ScriptClassifier query);
```

This wildcard rule is specific to script classifier lookup. `Object`'s own
`classifier_base()` has no wildcard concept — an object's classifier is
always a concrete, fully-specified identity. Don't assume `0` means "any"
if you see it on an `Object`'s classifier; it usually just means family/
genus/species `0`, a real (if unusual) value.

## The most common thing this encoding is used for: "is this a Creature?"

Family `4` is the Creature family. Because of the byte layout above, that
check is a single mask-and-compare, and it appears, worded slightly
differently, all over the tree:

```cpp
(classifier_base() & 0xff000000u) == 0x04000000u        // objects/lift.cpp
((source.classifier_base() >> 24) & 0xffu) == 4u          // platform/windows_object_event_host.cpp
(packed_motion_classifier & 0xff000000u) != 0x04000000u   // creatures/creature.cpp
```

All three are the same test written three ways. If you're adding a new
family check, prefer matching the surrounding file's idiom rather than
introducing a fourth spelling.

## Combining an exact match with a flag bit

Occasionally code needs "family/genus match, and a specific flag bit is
set" in one comparison, by ORing a bit into the low byte before or after
masking. From `recipient_for_insemination` in
[`platform/windows_macro_host.cpp`](../src/c1/platform/windows_macro_host.cpp):

```cpp
const std::uint32_t expected_classifier =
    (source.skeleton().classifier_base() & 0xffff0200u) | 0x200u;
if (link->classifier_base() != expected_classifier) {
    return nullptr;
}
```

This keeps the source's own family and genus (top two bytes, `0xffff0000`),
clears species and the rest of the low byte, then forces bit `0x200` — the
object-family "creature" bit-flag convention used elsewhere in the
classifier's low word. If you need this pattern, read the comment at that
call site first; the exact mask depends on which bits the specific check
cares about, and copying a mask from an unrelated call site is an easy way
to silently check the wrong thing.

## Where to look before adding a new use

Before inventing a new packed representation for "kind of object" or
"kind of event," check whether one of the two existing structs already
fits:

- Need an object's own runtime identity, or to test what family/genus/
  species it belongs to? Use `Object::classifier_base()` /
  `set_classifier_base()`.
- Need a lookup key for "which script handles this classifier and event"?
  Use `scripting::ScriptClassifier`, and go through
  `scripting/classifier_scripts.cpp`'s existing match functions rather than
  writing a new comparison — the wildcard fallback order is easy to get
  subtly wrong by hand.
- Need to serialize a classifier? It round-trips as the same packed
  `uint32_t` in [`platform/mfc_object_archive.cpp`](../src/c1/platform/mfc_object_archive.cpp)
  and the world-save path in
  [`platform/windows_document_host.cpp`](../src/c1/platform/windows_document_host.cpp) — don't
  reinvent a byte order for a new archive field; match this one.

Grepping for `0xff000000`, `>> 24) & 0xff`, or `classifier_base()` will find
the current call sites if you need more examples than the ones quoted here.
