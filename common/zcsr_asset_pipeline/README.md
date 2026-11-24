# zcsr_asset_pipeline

Game-agnostic offline asset pipeline. This is a tool-layer module, not a
runtime renderer and not a game asset table.

## Modules

- `reanim.py`: parse compiled reanim definitions, evaluate integer frames,
  render a PNG sheet + GIF preview. It receives an explicit source root; it
  does not read a game-specific environment variable or cache path.
- `compose.py`: compose multiple track sets from a recipe. It supports layer
  filtering, offsets, alpha, frame modes, attachments and a source root passed
  by the caller.
- `strip.py`: turn `assets_src/<id>/source.gif + meta.txt` into one RGBA strip
  and manifest rows. The consumer supplies manifest path, header and prefixes.
- `atlas.py`: pack ANIM strips into atlas pages and write an atlas manifest.
  The consumer supplies manifest paths and header.

## Boundary

- No game instance names or genre-specific nouns in this directory.
- Recipes, asset IDs, palette values and source-root policies belong to the
  consuming game repository.
- Runtime never imports this package. It consumes baked PNG strips, atlas pages
  and manifests.
