#!/usr/bin/env python3
"""Offline layered reanim compositor.

This is intentionally *not* a runtime fragment system. It parses compiled
``*.reanim.compiled`` definitions, evaluates integer frames, shifts whole track
sets by a manual offset, and composites them into one GIF + PNG sheet.

Runtime only sees the generated GIF/PNG strip; it never loads this tool or the
source definition.

Recipe example:
{
  "debug_root": "tools/_cache/original_reanim/Debug",
  "fps": 12,
  "out": "assets_src/_compose/example",
  "base": {
    "compiled": "compiled/reanim/Example.reanim.compiled",
    "layer": "anim_walk"
  },
  "layers": [
    {
      "compiled": "compiled/reanim/Example.reanim.compiled",
      "layer": "anim_walk",
      "keep_prefix": ["Example_innerarm", "Example_outerarm"],
      "offset": [0, 0],
      "frame_mode": "clamp"
    }
  ]
}
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any

from PIL import Image

from .reanim import (
    Definition,
    ImageIndex,
    Transform,
    Track,
    parse_definition,
    render_frame,
    select_frame_range,
    transform_matrix,
)


@dataclass
class AttachmentRuntime:
    name: str
    definition: Definition
    layer_name: str
    frame_start: int
    frame_count: int
    frame_mode: str
    anchor: str
    base_track: str
    insert_after: str
    offset_x: float
    offset_y: float
    scale_x: float
    scale_y: float
    base_pose_frame: int


@dataclass
class LayerRuntime:
    name: str
    definition: Definition
    layer_name: str
    frame_start: int
    frame_count: int
    offset_x: float
    offset_y: float
    alpha: float
    frame_mode: str
    hold_inactive: bool
    compiled: Path
    sync_to_base: bool
    independent_layer: bool
    anchor: str


def _csv_list(value: Any) -> list[str]:
    if value is None:
        return []
    if isinstance(value, str):
        return [item.strip() for item in value.split(",") if item.strip()]
    if isinstance(value, (list, tuple)):
        return [str(item).strip() for item in value if str(item).strip()]
    raise ValueError(f"expected string or list, got {type(value).__name__}")


def _resolve_path(value: str, recipe_dir: Path, debug_root: Path | None = None) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    candidate = (recipe_dir / path).resolve()
    if candidate.exists() or debug_root is None:
        return candidate
    # Recipes may store source paths relative to the original Debug root
    # (``compiled/reanim/...``), which keeps them portable across machines.
    return (debug_root / path).resolve()


def _filter_definition(
    definition: Definition, hide_prefix: list[str], keep_prefix: list[str]
) -> Definition:
    keep = [prefix.casefold() for prefix in keep_prefix]
    hide = [prefix.casefold() for prefix in hide_prefix]
    tracks: list[Track] = []
    for track in definition.tracks:
        folded = track.name.casefold()
        if keep and not any(folded.startswith(prefix) for prefix in keep):
            continue
        if any(folded.startswith(prefix) for prefix in hide):
            continue
        tracks.append(track)
    if not tracks:
        raise ValueError("layer filter removed every track")
    return Definition(fps=definition.fps, tracks=tracks)


def _shift_definition(
    definition: Definition, dx: float, dy: float, alpha_mul: float
) -> Definition:
    if dx == 0.0 and dy == 0.0 and alpha_mul == 1.0:
        return definition
    tracks: list[Track] = []
    for track in definition.tracks:
        transforms = [
            replace(
                transform,
                x=transform.x + dx,
                y=transform.y + dy,
                alpha=transform.alpha * alpha_mul,
            )
            for transform in track.transforms
        ]
        tracks.append(Track(name=track.name, transforms=transforms))
    return Definition(fps=definition.fps, tracks=tracks)


def _mat_mul(
    a: tuple[float, float, float, float, float, float],
    b: tuple[float, float, float, float, float, float],
) -> tuple[float, float, float, float, float, float]:
    a00, a01, a02, a10, a11, a12 = a
    b00, b01, b02, b10, b11, b12 = b
    return (
        a00 * b00 + a01 * b10,
        a00 * b01 + a01 * b11,
        a00 * b02 + a01 * b12 + a02,
        a10 * b00 + a11 * b10,
        a10 * b01 + a11 * b11,
        a10 * b02 + a11 * b12 + a12,
    )


def _mat_inverse(
    m: tuple[float, float, float, float, float, float],
) -> tuple[float, float, float, float, float, float]:
    m00, m01, m02, m10, m11, m12 = m
    det = m00 * m11 - m01 * m10
    if abs(det) < 1e-8:
        return (1.0, 0.0, 0.0, 0.0, 1.0, 0.0)
    inv00 = m11 / det
    inv01 = -m01 / det
    inv10 = -m10 / det
    inv11 = m00 / det
    return (
        inv00,
        inv01,
        -(inv00 * m02 + inv01 * m12),
        inv10,
        inv11,
        -(inv10 * m02 + inv11 * m12),
    )


def _transform_affine(transform: Transform) -> tuple[float, float, float, float, float, float]:
    m00, m01, m10, m11 = transform_matrix(transform)
    return (m00, m01, transform.x, m10, m11, transform.y)


def _track_at(
    definition: Definition, name: str, frame_index: int
) -> Transform | None:
    folded = name.casefold()
    for track in definition.tracks:
        if track.name.casefold() == folded:
            if not track.transforms:
                return None
            transform = track.transforms[min(frame_index, len(track.transforms) - 1)]
            if transform.frame < 0.0 or transform.alpha <= 0.0:
                return None
            return transform
    return None


def _attachment_runtime(
    spec: dict[str, Any], recipe_dir: Path, debug_root: Path | None = None
) -> AttachmentRuntime:
    compiled = _resolve_path(str(spec["compiled"]), recipe_dir, debug_root)
    definition = parse_definition(compiled)
    definition = _filter_definition(
        definition,
        _csv_list(spec.get("hide_prefix")),
        _csv_list(spec.get("keep_prefix")),
    )
    layer_name, frame_start, frame_count = select_frame_range(
        definition, spec.get("layer")
    )
    scale = spec.get("scale", 1.2)
    if isinstance(scale, (int, float)):
        scale_x = scale_y = float(scale)
    elif isinstance(scale, (list, tuple)) and len(scale) == 2:
        scale_x, scale_y = float(scale[0]), float(scale[1])
    else:
        raise ValueError(f"{compiled}: attachment scale must be a number or [sx, sy]")
    offset = spec.get("offset", [0.0, 0.0])
    if not isinstance(offset, (list, tuple)) or len(offset) != 2:
        raise ValueError(f"{compiled}: attachment offset must be [dx, dy]")
    frame_mode = str(spec.get("frame_mode", "loop"))
    if frame_mode not in ("clamp", "loop"):
        raise ValueError(f"{compiled}: attachment frame_mode must be clamp or loop")
    anchor = str(spec.get("anchor", "center"))
    if anchor not in ("top_left", "center"):
        raise ValueError(f"{compiled}: attachment anchor must be top_left or center")
    base_track = str(spec["base_track"]).strip()
    if not base_track:
        raise ValueError(f"{compiled}: attachment base_track is required")
    return AttachmentRuntime(
        name=str(spec.get("name", compiled.stem)),
        definition=definition,
        layer_name=layer_name,
        frame_start=frame_start,
        frame_count=frame_count,
        frame_mode=frame_mode,
        anchor=anchor,
        base_track=base_track,
        insert_after=str(spec.get("insert_after", base_track)).strip() or base_track,
        offset_x=float(offset[0]),
        offset_y=float(offset[1]),
        scale_x=scale_x,
        scale_y=scale_y,
        base_pose_frame=int(spec.get("base_pose_frame", 0)),
    )


def _attachment_frame(attachment: AttachmentRuntime, global_frame: int) -> int:
    if attachment.frame_mode == "loop" and attachment.frame_count > 0:
        return attachment.frame_start + (global_frame % attachment.frame_count)
    return attachment.frame_start + min(global_frame, max(0, attachment.frame_count - 1))


def _attachment_overlay(
    base: LayerRuntime,
    attachment: AttachmentRuntime,
    global_frame: int,
) -> tuple[float, float, float, float, float, float] | None:
    base_frame = _layer_frame(base, global_frame, base.frame_count)
    parent_transform = _track_at(base.definition, attachment.base_track, base_frame)
    if parent_transform is None:
        return None
    parent = _transform_affine(parent_transform)
    parent = (
        parent[0], parent[1], parent[2] + base.offset_x,
        parent[3], parent[4], parent[5] + base.offset_y,
    )
    base_pose_transform = _track_at(
        base.definition, attachment.base_track, attachment.base_pose_frame
    )
    if base_pose_transform is not None:
        base_pose = _transform_affine(base_pose_transform)
        base_pose = (
            base_pose[0], base_pose[1], base_pose[2] + base.offset_x,
            base_pose[3], base_pose[4], base_pose[5] + base.offset_y,
        )
    else:
        base_pose = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0)
    offset = (
        attachment.scale_x,
        0.0,
        attachment.offset_x,
        0.0,
        attachment.scale_y,
        attachment.offset_y,
    )
    return _mat_mul(_mat_mul(parent, _mat_inverse(base_pose)), offset)


def _attachment_frame_image(
    attachment: AttachmentRuntime,
    image_index: ImageIndex,
    canvas: tuple[int, int, int, int],
    global_frame: int,
    overlay: tuple[float, float, float, float, float, float],
    cache: dict[str, Image.Image],
) -> Image.Image:
    child_frame = _attachment_frame(attachment, global_frame)
    return render_frame(
        attachment.definition,
        child_frame,
        image_index,
        canvas,
        cache,
        False,
        attachment.anchor,
        overlay,
    )


def _attachment_canvas(
    base: LayerRuntime,
    attachment: AttachmentRuntime,
    image_index: ImageIndex,
    canvas: tuple[int, int, int, int],
) -> tuple[int, int, int, int]:
    """Expand the base canvas to include every attached child frame."""
    min_x, min_y, max_x, max_y = canvas
    child_count = max(1, attachment.frame_count)
    for global_frame in range(base.frame_count):
        overlay = _attachment_overlay(base, attachment, global_frame)
        if overlay is None:
            continue
        child_frame = _attachment_frame(attachment, global_frame)
        for track in attachment.definition.tracks:
            if not track.transforms:
                continue
            transform = track.transforms[min(child_frame, len(track.transforms) - 1)]
            if transform.frame < 0.0 or transform.alpha <= 0.0 or not transform.image_name:
                continue
            path = image_index.find(transform.image_name)
            if path is None:
                continue
            with Image.open(path) as image:
                width, height = image.size
            local = _transform_affine(transform)
            combined = _mat_mul(overlay, local)
            c00, c01, c02, c10, c11, c12 = combined
            if attachment.anchor == "center":
                offsets = (
                    (-width * 0.5, -height * 0.5),
                    (width * 0.5, -height * 0.5),
                    (-width * 0.5, height * 0.5),
                    (width * 0.5, height * 0.5),
                )
            else:
                offsets = ((0.0, 0.0), (width, 0.0), (0.0, height), (width, height))
            for sx, sy in offsets:
                x = c00 * sx + c01 * sy + c02
                y = c10 * sx + c11 * sy + c12
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    return (
        math.floor(min_x - 2.0),
        math.floor(min_y - 2.0),
        math.ceil(max_x + 2.0),
        math.ceil(max_y + 2.0),
    )


def _layer_runtime(
    spec: dict[str, Any], recipe_dir: Path, default_hold: bool,
    debug_root: Path | None = None
) -> LayerRuntime:
    compiled = _resolve_path(str(spec["compiled"]), recipe_dir, debug_root)
    definition = parse_definition(compiled)
    definition = _filter_definition(
        definition,
        _csv_list(spec.get("hide_prefix")),
        _csv_list(spec.get("keep_prefix")),
    )
    layer_name, frame_start, frame_count = select_frame_range(
        definition, spec.get("layer")
    )
    offset = spec.get("offset", [0.0, 0.0])
    if not isinstance(offset, (list, tuple)) or len(offset) != 2:
        raise ValueError(f"{compiled}: offset must be [dx, dy]")
    alpha = float(spec.get("alpha", 1.0))
    if not 0.0 <= alpha <= 1.0:
        raise ValueError(f"{compiled}: alpha must be 0..1")
    frame_mode = str(spec.get("frame_mode", "clamp"))
    if frame_mode not in ("clamp", "loop"):
        raise ValueError(f"{compiled}: frame_mode must be clamp or loop")
    anchor = str(spec.get("anchor", "top_left"))
    if anchor not in ("top_left", "center"):
        raise ValueError(f"{compiled}: anchor must be top_left or center")
    return LayerRuntime(
        name=str(spec.get("name", compiled.stem)),
        definition=definition,
        layer_name=layer_name,
        frame_start=frame_start,
        frame_count=frame_count,
        offset_x=float(offset[0]),
        offset_y=float(offset[1]),
        alpha=alpha,
        frame_mode=frame_mode,
        hold_inactive=bool(spec.get("hold_inactive", default_hold)),
        compiled=compiled,
        sync_to_base=bool(spec.get("sync_to_base", False)),
        independent_layer=bool(spec.get("independent_layer", False)),
        anchor=anchor,
    )


def _effective_transform(
    track: Track, frame_index: int, hold_inactive: bool
) -> Transform | None:
    if not track.transforms:
        return None
    transform = track.transforms[min(frame_index, len(track.transforms) - 1)]
    if transform.frame < 0.0 and hold_inactive:
        for previous in reversed(
            track.transforms[: min(frame_index + 1, len(track.transforms))]
        ):
            if previous.frame >= 0.0 and previous.image_name:
                return previous
    if transform.alpha <= 0.0 or transform.frame < 0.0 or not transform.image_name:
        return None
    return transform


def _layer_frame(layer: LayerRuntime, global_frame: int, base_count: int) -> int:
    if layer.frame_mode == "loop" and layer.frame_count > 0:
        return layer.frame_start + (global_frame % layer.frame_count)
    return layer.frame_start + min(global_frame, max(0, layer.frame_count - 1))


def _canvas_for_layers(
    layers: list[LayerRuntime], image_index: ImageIndex, base_count: int
) -> tuple[int, int, int, int]:
    min_x = min_y = float("inf")
    max_x = max_y = float("-inf")
    for layer in layers:
        shifted = _shift_definition(
            layer.definition, layer.offset_x, layer.offset_y, layer.alpha
        )
        for global_frame in range(base_count):
            frame_index = _layer_frame(layer, global_frame, base_count)
            for track in shifted.tracks:
                transform = _effective_transform(
                    track, frame_index, layer.hold_inactive
                )
                if transform is None:
                    continue
                path = image_index.find(transform.image_name)
                if path is None:
                    continue
                # Reuse the original transform geometry; only the image size
                # is needed for the union box.
                with Image.open(path) as image:
                    width, height = image.size
                m00, m01, m10, m11 = transform_matrix(transform)
                if layer.anchor == "center":
                    offsets = (
                        (-width * 0.5, -height * 0.5),
                        (width * 0.5, -height * 0.5),
                        (-width * 0.5, height * 0.5),
                        (width * 0.5, height * 0.5),
                    )
                else:
                    offsets = ((0.0, 0.0), (width, 0.0), (0.0, height), (width, height))
                for sx, sy in offsets:
                    x = transform.x + m00 * sx + m01 * sy
                    y = transform.y + m10 * sx + m11 * sy
                    min_x = min(min_x, x)
                    min_y = min(min_y, y)
                    max_x = max(max_x, x)
                    max_y = max(max_y, y)
    if not math.isfinite(min_x):
        raise ValueError("no drawable transforms; cannot determine canvas")
    pad = 2.0
    return (
        math.floor(min_x - pad),
        math.floor(min_y - pad),
        math.ceil(max_x + pad),
        math.ceil(max_y + pad),
    )


def _render_composite(
    layers: list[LayerRuntime],
    image_index: ImageIndex,
    canvas: tuple[int, int, int, int],
    base_count: int,
    cache: dict[str, Image.Image],
    attachment: AttachmentRuntime | None = None,
    attachment_layer: LayerRuntime | None = None,
) -> list[Image.Image]:
    frames: list[Image.Image] = []
    for global_frame in range(base_count):
        out = Image.new(
            "RGBA", (canvas[2] - canvas[0], canvas[3] - canvas[1]), (0, 0, 0, 0)
        )
        for layer in layers:
            frame_index = _layer_frame(layer, global_frame, base_count)
            shifted = _shift_definition(
                layer.definition, layer.offset_x, layer.offset_y, layer.alpha
            )
            insert_image = None
            insert_after = None
            if attachment is not None and layer is attachment_layer:
                overlay = _attachment_overlay(layer, attachment, global_frame)
                if overlay is not None:
                    insert_image = _attachment_frame_image(
                        attachment, image_index, canvas, global_frame, overlay, cache
                    )
                    insert_after = attachment.insert_after
            frame = render_frame(
                shifted, frame_index, image_index, canvas, cache, layer.hold_inactive,
                layer.anchor, insert_after_track=insert_after,
                insert_image=insert_image,
            )
            out = Image.alpha_composite(out, frame)
        frames.append(out)
    return frames


def _load_recipe(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        recipe = json.load(handle)
    if not isinstance(recipe, dict):
        raise ValueError("recipe root must be an object")
    return recipe


def _gif_frame(frame: Image.Image) -> Image.Image:
    """Convert an RGBA frame to an indexed frame with a stable transparent slot.

    Pillow's default ``transparency=0`` can make an opaque black palette entry
    transparent when the adaptive palette orders colors differently. Composite
    transparent pixels onto a sentinel color, force that sentinel to palette
    index 0, and remap the corresponding index. This preserves black outlines.
    """
    sentinel = (255, 0, 255)
    rgba = frame.convert("RGBA")
    # GIF has binary transparency. Threshold alpha before palette conversion;
    # compositing semi-transparent edge pixels onto magenta would leave a
    # visible fringe around hair and hands.
    background = Image.new("RGB", rgba.size, sentinel)
    background.paste(
        rgba.convert("RGB"),
        (0, 0),
        rgba.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0),
    )
    indexed = background.quantize(
        colors=255, method=Image.Quantize.FASTOCTREE
    )
    palette = indexed.getpalette() or []
    sentinel_index = 0
    for i in range(0, len(palette), 3):
        if tuple(palette[i : i + 3]) == sentinel:
            sentinel_index = i // 3
            break
    else:
        # Fallback: choose the nearest palette color to the sentinel.
        sentinel_index = min(
            range(len(palette) // 3),
            key=lambda i: sum(
                (palette[i * 3 + c] - sentinel[c]) ** 2 for c in range(3)
            ),
        )
    if sentinel_index != 0:
        palette[0:3], palette[sentinel_index * 3 : sentinel_index * 3 + 3] = (
            palette[sentinel_index * 3 : sentinel_index * 3 + 3],
            palette[0:3],
        )
        indexed.putpalette(palette)
        indexed = indexed.point(
            lambda value: 0 if value == sentinel_index else sentinel_index if value == 0 else value
        )
    indexed.info["transparency"] = 0
    return indexed


def compose(
    recipe: dict[str, Any],
    recipe_dir: Path,
    debug_root: Path,
    out: Path | None = None,
    fps: float | None = None,
) -> dict[str, Any]:
    debug_root = Path(debug_root).expanduser()
    if not debug_root.is_absolute():
        debug_root = (recipe_dir / debug_root).resolve()
    if not debug_root.is_dir():
        raise FileNotFoundError(f"source root not found: {debug_root}")
    fps = float(fps or recipe.get("fps", 12.0))
    if fps <= 0.0:
        raise ValueError("fps must be positive")
    out = Path(out or recipe.get("out") or "")
    if not out:
        raise ValueError("out is required")
    if not out.is_absolute():
        out = (recipe_dir / out).resolve()
    hold_default = bool(recipe.get("hold_inactive", False))

    base_spec = recipe.get("base")
    if not isinstance(base_spec, dict):
        raise ValueError("recipe.base is required")
    base = _layer_runtime(base_spec, recipe_dir, hold_default, debug_root)
    layer_specs = recipe.get("layers", [])
    if not isinstance(layer_specs, list):
        raise ValueError("recipe.layers must be a list")
    layers = [base]
    for spec in layer_specs:
        if not isinstance(spec, dict):
            raise ValueError("each layer must be an object")
        layer = _layer_runtime(spec, recipe_dir, hold_default, debug_root)
        # Same-definition layers (head/hand/duck on one body) must use the
        # exact frame range of the base body, not their own fallback range.
        if layer.sync_to_base or (
            layer.compiled == base.compiled and not layer.independent_layer
        ):
            layer.frame_start = base.frame_start
            layer.frame_count = base.frame_count
        layers.append(layer)

    attachment_specs = recipe.get("attachments", [])
    if not isinstance(attachment_specs, list):
        raise ValueError("recipe.attachments must be a list")
    attachments = [
        _attachment_runtime(spec, recipe_dir, debug_root)
        for spec in attachment_specs
        if isinstance(spec, dict)
    ]
    if len(attachments) > 1:
        raise ValueError("only one attachment is supported in this compositor build")
    attachment = attachments[0] if attachments else None
    attachment_layer = base
    if attachment is not None:
        attach_layer_name = str(recipe.get("attach_layer", "")).strip()
        if attach_layer_name:
            folded = attach_layer_name.casefold()
            attachment_layer = next(
                (layer for layer in layers if layer.name.casefold() == folded),
                None,
            )
            if attachment_layer is None:
                raise ValueError(f"attach_layer not found: {attach_layer_name}")

    image_index = ImageIndex(debug_root)
    # The base clip defines the output length; add layers clamp or loop into it.
    base_count = base.frame_count
    if base_count <= 0:
        raise ValueError("base layer has no frames")
    canvas = _canvas_for_layers(layers, image_index, base_count)
    if attachment is not None:
        canvas = _attachment_canvas(attachment_layer, attachment, image_index, canvas)
    cache: dict[str, Image.Image] = {}
    frames = _render_composite(
        layers, image_index, canvas, base_count, cache, attachment, attachment_layer
    )
    if not frames:
        raise ValueError("no frames rendered")

    out.parent.mkdir(parents=True, exist_ok=True)
    width = canvas[2] - canvas[0]
    height = canvas[3] - canvas[1]
    sheet = Image.new("RGBA", (width * len(frames), height), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        sheet.alpha_composite(frame, (index * width, 0))
    sheet_path = out.with_suffix(".png")
    gif_path = out.with_suffix(".gif")
    meta_path = out.with_suffix(".json")
    sheet.save(sheet_path)
    gif_frames = [_gif_frame(frame) for frame in frames]
    gif_frames[0].save(
        gif_path,
        save_all=True,
        append_images=gif_frames[1:],
        duration=max(1, round(1000 / fps)),
        loop=0,
        disposal=2,
        transparency=0,
        optimize=False,
    )
    # Pillow coalesces consecutive identical frames even when optimize=False.
    # The canonical source is the GIF, so report what was actually written.
    with Image.open(gif_path) as gif_image:
        stored_frames = int(getattr(gif_image, "n_frames", 1))
    metadata = {
        "source": "zcsr_asset_pipeline.compose",
        "debug_root": str(debug_root),
        "fps": fps,
        "out": str(out),
        "frame_width": width,
        "frame_height": height,
        "frames": stored_frames,
        "rendered_frames": len(frames),
        "canvas": {
            "left": canvas[0],
            "top": canvas[1],
            "right": canvas[2],
            "bottom": canvas[3],
        },
        "layers": [
            {
                "name": layer.name,
                "layer": layer.layer_name,
                "frame_start": layer.frame_start,
                "frame_count": layer.frame_count,
                "offset": [layer.offset_x, layer.offset_y],
                "alpha": layer.alpha,
                "frame_mode": layer.frame_mode,
                "hold_inactive": layer.hold_inactive,
                "independent_layer": layer.independent_layer,
                "anchor": layer.anchor,
                "tracks": [
                    {"name": track.name, "frames": len(track.transforms)}
                    for track in layer.definition.tracks
                ],
            }
            for layer in layers
        ],
        "attachments": [
            {
                "name": attachment.name,
                "layer": attachment.layer_name,
                "frame_start": attachment.frame_start,
                "frame_count": attachment.frame_count,
                "base_track": attachment.base_track,
                "insert_after": attachment.insert_after,
                "offset": [attachment.offset_x, attachment.offset_y],
                "scale": [attachment.scale_x, attachment.scale_y],
                "frame_mode": attachment.frame_mode,
                "anchor": attachment.anchor,
                "base_pose_frame": attachment.base_pose_frame,
                "attach_layer": attachment_layer.name,
            }
        ] if attachment is not None else [],
    }
    meta_path.write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return metadata


def inspect_recipe(recipe: dict[str, Any], recipe_dir: Path, debug_root: Path) -> None:
    debug_root = Path(debug_root).expanduser()
    if not debug_root.is_absolute():
        debug_root = (recipe_dir / debug_root).resolve()
    if not debug_root.is_dir():
        raise FileNotFoundError(f"source root not found: {debug_root}")
    for key in ("base", "layers"):
        value = recipe.get(key)
        specs = [value] if key == "base" else value
        if isinstance(specs, dict):
            specs = [specs]
        if not isinstance(specs, list):
            continue
        for spec in specs:
            if not isinstance(spec, dict):
                continue
            compiled = _resolve_path(str(spec.get("compiled", "")), recipe_dir, debug_root)
            definition = parse_definition(compiled)
            layer_name, frame_start, frame_count = select_frame_range(
                definition, spec.get("layer")
            )
            print(f"{compiled.name}: layer={layer_name} start={frame_start} frames={frame_count}")
            for track in definition.tracks:
                print(f"  {track.name}: {len(track.transforms)}")


# Stable public names for consumers that used the historical private helpers.
canvas_for_layers = _canvas_for_layers
filter_definition = _filter_definition
load_recipe = _load_recipe


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("recipe", type=Path, help="recipe JSON")
    parser.add_argument("--debug-root", type=Path, required=True,
                        help="source root containing reanim/ and images/")
    parser.add_argument("--out", type=Path, default=None,
                        help="override recipe.out; writes .png/.gif/.json")
    parser.add_argument("--fps", type=float, default=None, help="override recipe.fps")
    parser.add_argument("--inspect", action="store_true",
                        help="print layer/track summary instead of rendering")
    args = parser.parse_args(argv)

    recipe_path = args.recipe.resolve()
    recipe = _load_recipe(recipe_path)
    recipe_dir = recipe_path.parent
    if args.inspect:
        inspect_recipe(recipe, recipe_dir, args.debug_root)
        return 0
    metadata = compose(recipe, recipe_dir, args.debug_root, args.out, args.fps)
    print(json.dumps({
        "out": metadata["out"],
        "frames": metadata["frames"],
        "frame_width": metadata["frame_width"],
        "frame_height": metadata["frame_height"],
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"zcsr_asset_pipeline.compose: {exc}", file=sys.stderr)
        raise SystemExit(1)
