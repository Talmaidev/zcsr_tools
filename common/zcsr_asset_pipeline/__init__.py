"""Game-agnostic offline asset pipeline.

Public modules:
- reanim: compiled reanim parsing, frame evaluation, strip + GIF rendering.
- compose: layered source composition from recipe data.
- strip: GIF source -> packed RGBA strip + manifest rows.
- atlas: already-baked strips -> atlas pages + atlas manifest.

The package is intentionally free of game instance names and game-specific
source-root policies. Consumers provide recipe data, header text and paths.
"""

__version__ = "1.0.0"
