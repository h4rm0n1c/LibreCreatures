# Loaded creatures can tick while their body pixels are invisible

## Symptom

A living norn loaded from an existing `World.sfc` can be present in the
world, metabolising, and reported by the health/CAOS interfaces while no body
is drawn.  To the player this looks like a frozen creature with closed eyes.

## Cause

The archive stores the creature's body graph, gallery, image offsets, and the
generated sprite-file identifier.  It does not put the sprite pixel bytes in
the gallery itself.  `Image::load_pixel_data` obtains those bytes lazily from
`SpriteFileCache`, and the Windows host was passing the configured install
`Images` directory twice as its search path.

That fails for a copied save when its generated body files remain beside the
save instead of in the current installation.  In the examined save the two
living creatures use galleries backed by `9JYX.SPR` and `5QDS.SPR`; those files
were present in `140926crash/w2/Images` but absent from the normal
`World/Images` directory.  The renderer still has a valid gallery and image
index, but receives no pixel buffer and skips the blit.

The save's runtime was not dead in this case.  `dde: getb ovvd` continued to
report the creatures as alive while age/health changed, which separates this
failure from a creature scheduler failure.

## Fix

When `World.sfc` is opened, `C1WindowsDocument` remembers the archive's parent
`Images\\` directory.  Lazy image reads now search that save-local directory
before the configured install directory in `current_image_pixels`,
`preload_image`, and `blit_image_to_dib`.

The configured directory remains the source for ordinary installed assets and
for generated body sprites.  The save-local path is only an additional lazy
read fallback, so loading a save does not relocate or regenerate its archived
body data.

## Verification

Build: `/tmp/bm-cache-creature-save-fallback/Creatures.clean.exe`, SHA-256
`d063478c0675e55e28014d979090f6a3d34d3c1cb8932849b805c934674e226c`.

Under Wine/Xvfb, the staged `w2` save was run with `9JYX.SPR` absent from the
configured install `Images` directory but present beside the save.  Both
existing norns rendered normally.  A prior control run with the same save and
no accessible generated body files showed the world and scenery but no norns.

This fixes the demonstrated invisibility path.  It does not by itself claim
that every possible movement/action-selection issue is fixed; those need to be
tested after the creature body is visible.
