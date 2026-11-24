#!/usr/bin/env python3
"""Convert compiled reanim definitions into 12 fps frame sheets.

The format stores ``*.reanim.compiled`` as:
  4 bytes magic 0xDEADFED4
  4 bytes uncompressed_size
  zlib stream containing:
    4 bytes schema hash
    ReanimatorDefinition raw struct
    deep-copied strings / arrays / transforms

This generic core does not resample video. It evaluates transform tracks at
integer animation frames, composites source PNG parts, and returns a horizontal
PNG sheet plus a GIF preview. Game-specific presets and source-root lookup live
in the consumer repository.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from PIL import Image

MAGIC = 0xDEADFED4
DEFINITION_SIZE = 16
TRACK_SIZE = 12
TRANSFORM_SIZE = 44


@dataclass
class Transform:
    x: float
    y: float
    skew_x: float
    skew_y: float
    scale_x: float
    scale_y: float
    frame: float
    alpha: float
    image_name: str


@dataclass
class Track:
    name: str
    transforms: list[Transform]


@dataclass
class Definition:
    fps: float
    tracks: list[Track]


class Cursor:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.pos = 0

    def read(self, size: int) -> bytes:
        if self.pos + size > len(self.data):
            raise ValueError(f"unexpected end of reanim data at {self.pos}")
        chunk = self.data[self.pos : self.pos + size]
        self.pos += size
        return chunk

    def u32(self) -> int:
        return struct.unpack("<I", self.read(4))[0]

    def i32(self) -> int:
        return struct.unpack("<i", self.read(4))[0]

    def f32(self) -> float:
        return struct.unpack("<f", self.read(4))[0]

    def string(self) -> str:
        length = self.i32()
        if length < 0 or length > 100000:
            raise ValueError(f"invalid string length {length} at {self.pos - 4}")
        raw = self.read(length)
        return raw.decode("utf-8", errors="replace")

    def skip(self, size: int) -> None:
        self.read(size)


def decompress_compiled(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) < 8:
        raise ValueError(f"{path}: too small")
    magic, size = struct.unpack_from("<II", data, 0)
    if magic != MAGIC:
        raise ValueError(f"{path}: bad magic {magic:#x}")
    raw = zlib.decompress(data[8:])
    if len(raw) != size:
        raise ValueError(f"{path}: uncompressed size {len(raw)} != header {size}")
    return raw


def read_transform(raw: bytes, cursor: Cursor) -> Transform:
    x, y, skew_x, skew_y, scale_x, scale_y, frame, alpha = struct.unpack(
        "<8f", raw[:32]
    )
    image_name = cursor.string()
    cursor.string()  # font field is unused by this format
    cursor.string()  # text field is unused by this format
    return Transform(
        x=x,
        y=y,
        skew_x=skew_x,
        skew_y=skew_y,
        scale_x=scale_x,
        scale_y=scale_y,
        frame=frame,
        alpha=alpha,
        image_name=image_name,
    )


PLACEHOLDER = -10000.0


def fill_missing_transforms(definition: Definition) -> None:
    """Forward-fill the unset fields exactly like ReanimationLoadDefinition."""
    for track in definition.tracks:
        prev_x = 0.0
        prev_y = 0.0
        prev_skew_x = 0.0
        prev_skew_y = 0.0
        prev_scale_x = 1.0
        prev_scale_y = 1.0
        prev_frame = 0.0
        prev_alpha = 1.0
        prev_image = ""
        for transform in track.transforms:
            for field in (
                "x",
                "y",
                "skew_x",
                "skew_y",
                "scale_x",
                "scale_y",
                "frame",
                "alpha",
            ):
                value = getattr(transform, field)
                if value == PLACEHOLDER:
                    value = {
                        "x": prev_x,
                        "y": prev_y,
                        "skew_x": prev_skew_x,
                        "skew_y": prev_skew_y,
                        "scale_x": prev_scale_x,
                        "scale_y": prev_scale_y,
                        "frame": prev_frame,
                        "alpha": prev_alpha,
                    }[field]
                    setattr(transform, field, value)
                else:
                    if field == "x":
                        prev_x = value
                    elif field == "y":
                        prev_y = value
                    elif field == "skew_x":
                        prev_skew_x = value
                    elif field == "skew_y":
                        prev_skew_y = value
                    elif field == "scale_x":
                        prev_scale_x = value
                    elif field == "scale_y":
                        prev_scale_y = value
                    elif field == "frame":
                        prev_frame = value
                    elif field == "alpha":
                        prev_alpha = value
            if not transform.image_name:
                transform.image_name = prev_image
            else:
                prev_image = transform.image_name


def parse_definition(path: Path) -> Definition:
    cursor = Cursor(decompress_compiled(path))
    cursor.u32()  # schema hash
    raw_definition = cursor.read(DEFINITION_SIZE)
    track_count = struct.unpack_from("<I", raw_definition, 4)[0]
    fps = struct.unpack_from("<f", raw_definition, 8)[0]
    if track_count > 10000:
        raise ValueError(f"{path}: unreasonable track count {track_count}")

    raw_track_size = cursor.u32()
    if raw_track_size != TRACK_SIZE:
        raise ValueError(f"{path}: track size {raw_track_size} != {TRACK_SIZE}")
    raw_tracks = cursor.read(TRACK_SIZE * track_count)

    tracks: list[Track] = []
    for index in range(track_count):
        name = cursor.string()
        raw_count = struct.unpack_from("<I", raw_tracks, index * TRACK_SIZE + 8)[0]
        raw_transform_size = cursor.u32()
        if raw_transform_size != TRANSFORM_SIZE:
            raise ValueError(
                f"{path}: transform size {raw_transform_size} != {TRANSFORM_SIZE}"
            )
        raw_transforms = cursor.read(TRANSFORM_SIZE * raw_count)
        transforms = [
            read_transform(raw_transforms[offset : offset + TRANSFORM_SIZE], cursor)
            for offset in range(0, len(raw_transforms), TRANSFORM_SIZE)
        ]
        tracks.append(Track(name=name, transforms=transforms))
    definition = Definition(fps=fps, tracks=tracks)
    fill_missing_transforms(definition)
    return definition


class ImageIndex:
    def __init__(self, debug_root: Path) -> None:
        self.debug_root = debug_root
        self.by_key: dict[str, Path] = {}
        for directory in ("reanim", "images"):
            root = debug_root / directory
            if not root.is_dir():
                continue
            for path in root.rglob("*.png"):
                relative = path.relative_to(debug_root).with_suffix("")
                keys = {
                    str(relative).replace("\\", "/").lower(),
                    path.stem.lower(),
                    path.stem.replace(" ", "").lower(),
                }
                for key in keys:
                    self.by_key.setdefault(key, path)

    def find(self, image_name: str) -> Path | None:
        if not image_name:
            return None
        name = image_name.lower()
        candidates: list[str] = []
        for prefix, directories in (
            ("image_reanim_", ("reanim/", "images/")),
            ("image_", ("images/", "reanim/", "")),
        ):
            if name.startswith(prefix):
                tail = name[len(prefix) :]
                candidates.extend(directory + tail for directory in directories)
        candidates.extend((name, name.replace(" ", "")))
        for candidate in candidates:
            hit = self.by_key.get(candidate)
            if hit is not None:
                return hit
        # Last chance: match a normalized file stem after stripping prefix.
        tail = name
        for prefix in ("image_reanim_", "image_"):
            if tail.startswith(prefix):
                tail = tail[len(prefix) :]
                break
        normalized = tail.replace(" ", "").replace("_", "")
        for key, path in self.by_key.items():
            if key.replace("_", "").replace(" ", "") == normalized:
                return path
        return None


def transform_matrix(transform: Transform) -> tuple[float, float, float, float]:
    skew_x = math.radians(-transform.skew_x)
    skew_y = math.radians(-transform.skew_y)
    m00 = math.cos(skew_x) * transform.scale_x
    m10 = -math.sin(skew_x) * transform.scale_x
    m01 = math.sin(skew_y) * transform.scale_y
    m11 = math.cos(skew_y) * transform.scale_y
    return m00, m01, m10, m11


def transformed_corners(
    transform: Transform, image: Image.Image, anchor: str = "top_left"
) -> Iterable[tuple[float, float]]:
    width, height = image.size
    m00, m01, m10, m11 = transform_matrix(transform)
    if anchor == "center":
        offsets = (
            (-width * 0.5, -height * 0.5),
            (width * 0.5, -height * 0.5),
            (-width * 0.5, height * 0.5),
            (width * 0.5, height * 0.5),
        )
    else:
        offsets = ((0.0, 0.0), (width, 0.0), (0.0, height), (width, height))
    for sx, sy in offsets:
        yield (
            transform.x + m00 * sx + m01 * sy,
            transform.y + m10 * sx + m11 * sy,
        )


def render_frame(
    definition: Definition,
    frame_index: int,
    image_index: ImageIndex,
    canvas: tuple[int, int, int, int],
    cache: dict[str, Image.Image],
    hold_inactive: bool = False,
    anchor: str = "top_left",
    overlay: tuple[float, float, float, float, float, float] = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0),
    insert_after_track: str | None = None,
    insert_image: Image.Image | None = None,
) -> Image.Image:
    left, top, right, bottom = canvas
    width = right - left
    height = bottom - top
    out = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for track in definition.tracks:
        if not track.transforms:
            continue
        transform = track.transforms[min(frame_index, len(track.transforms) - 1)]
        if transform.frame < 0.0 and hold_inactive:
            # A layered clip (idle body + blink overlay) keeps the last active
            # pose of tracks that are not changing in this frame.
            for previous in reversed(track.transforms[: min(frame_index + 1, len(track.transforms))]):
                if previous.frame >= 0.0 and previous.image_name:
                    transform = previous
                    break
        if transform.alpha <= 0.0 or transform.frame < 0.0 or not transform.image_name:
            continue
        path = image_index.find(transform.image_name)
        if path is None:
            continue
        key = str(path)
        part = cache.get(key)
        if part is None:
            part = Image.open(path).convert("RGBA")
            cache[key] = part
        if transform.alpha < 1.0:
            alpha = part.getchannel("A").point(
                lambda value, a=transform.alpha: int(value * a)
            )
            part = part.copy()
            part.putalpha(alpha)

        t00, t01, t10, t11 = transform_matrix(transform)
        # Child reanims are drawn with an overlay matrix (attachment anchor).
        # Destination = overlay * local_transform * source.
        o00, o01, o02, o10, o11, o12 = overlay
        tx, ty = transform.x, transform.y
        m02 = o00 * tx + o01 * ty + o02
        m12 = o10 * tx + o11 * ty + o12
        m00 = o00 * t00 + o01 * t10
        m01 = o00 * t01 + o01 * t11
        m10 = o10 * t00 + o11 * t10
        m11 = o10 * t01 + o11 * t11
        det = m00 * m11 - m01 * m10
        if abs(det) < 1e-6:
            continue
        inv00 = m11 / det
        inv01 = -m01 / det
        inv10 = -m10 / det
        inv11 = m00 / det
        # Pillow maps destination coordinates back to source coordinates.
        # Reanimator treats mTransX/mTransY as the image center (see
        # ReanimBltMatrix: pos = center - scaled_size * 0.5).
        half_x = part.width * 0.5 if anchor == "center" else 0.0
        half_y = part.height * 0.5 if anchor == "center" else 0.0
        a = inv00
        b = inv01
        c = inv00 * (left - m02) + inv01 * (top - m12) + half_x
        d = inv10
        e = inv11
        f = inv10 * (left - m02) + inv11 * (top - m12) + half_y
        layer = part.transform(
            (width, height),
            Image.AFFINE,
            (a, b, c, d, e, f),
            resample=Image.BICUBIC,
            fillcolor=(0, 0, 0, 0),
        )
        out = Image.alpha_composite(out, layer)
        if insert_image is not None and insert_after_track is not None:
            if track.name.casefold() == insert_after_track.casefold():
                out = Image.alpha_composite(out, insert_image)
    return out


def select_frame_range(
    definition: Definition, layer_name: str | None
) -> tuple[str, int, int]:
    """Mirror Reanimation::GetFramesForLayer for one animation layer."""
    candidates: list[str] = []
    if layer_name:
        candidates = [layer_name]
    else:
        # A full idle layer is the useful default for a whole-unit preview.
        candidates = ["anim_full_idle", "anim_idle", "_ground"]
    track_by_name = {track.name.casefold(): track for track in definition.tracks}
    chosen: Track | None = None
    chosen_name = ""
    for candidate in candidates:
        track = track_by_name.get(candidate.casefold())
        if track is not None:
            chosen = track
            chosen_name = track.name
            break
    if chosen is None:
        for track in definition.tracks:
            if any(transform.image_name for transform in track.transforms):
                chosen = track
                chosen_name = track.name
                break
    if chosen is None:
        raise ValueError("no animation layer found")

    start = 0
    count = 1
    first = None
    last = None
    for index, transform in enumerate(chosen.transforms):
        if transform.frame >= 0.0:
            if first is None:
                first = index
            last = index
    if first is not None and last is not None:
        start = first
        count = last - first + 1
    return chosen_name, start, count


def build_canvas(
    definition: Definition,
    image_index: ImageIndex,
    frame_start: int,
    frame_count: int,
    hold_inactive: bool = False,
    anchor: str = "top_left",
) -> tuple[tuple[int, int, int, int], int]:
    min_x = min_y = float("inf")
    max_x = max_y = float("-inf")
    cache: dict[str, Image.Image] = {}
    for frame in range(frame_start, frame_start + frame_count):
        for track in definition.tracks:
            if not track.transforms:
                continue
            transform = track.transforms[min(frame, len(track.transforms) - 1)]
            if transform.frame < 0.0 and hold_inactive:
                for previous in reversed(track.transforms[: min(frame + 1, len(track.transforms))]):
                    if previous.frame >= 0.0 and previous.image_name:
                        transform = previous
                        break
            if transform.alpha <= 0.0 or transform.frame < 0.0 or not transform.image_name:
                continue
            path = image_index.find(transform.image_name)
            if path is None:
                continue
            part = cache.get(str(path))
            if part is None:
                part = Image.open(path).convert("RGBA")
                cache[str(path)] = part
            for x, y in transformed_corners(transform, part, anchor):
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if not math.isfinite(min_x):
        raise ValueError("no drawable transforms; cannot determine canvas")
    pad = 2.0
    return (
        (
            math.floor(min_x - pad),
            math.floor(min_y - pad),
            math.ceil(max_x + pad),
            math.ceil(max_y + pad),
        ),
        len(cache),
    )


def run(
    compiled: Path,
    debug_root: Path,
    out_dir: Path,
    fps: float = 12.0,
    layer: str | None = None,
    hide_prefix: tuple[str, ...] | list[str] = (),
    anchor: str = "top_left",
    hold_inactive: bool = False,
    preset: str | None = None,
) -> dict:
    """Render one compiled reanim clip into a sheet + GIF + metadata."""
    definition = parse_definition(compiled)
    hidden_prefixes = sorted(set(hide_prefix), key=str.casefold)
    hidden = [prefix.casefold() for prefix in hidden_prefixes]
    if hidden:
        definition = Definition(
            fps=definition.fps,
            tracks=[
                track
                for track in definition.tracks
                if not any(track.name.casefold().startswith(prefix) for prefix in hidden)
            ],
        )
    image_index = ImageIndex(debug_root)
    selected_layer, frame_start, frame_count = select_frame_range(definition, layer)
    canvas, image_count = build_canvas(
        definition, image_index, frame_start, frame_count, hold_inactive, anchor
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    cache: dict[str, Image.Image] = {}
    frames: list[Image.Image] = [
        render_frame(
            definition, frame_start + frame, image_index, canvas, cache,
            hold_inactive, anchor,
        )
        for frame in range(frame_count)
    ]
    if not frames:
        raise ValueError(f"{compiled}: no frames")

    width = canvas[2] - canvas[0]
    height = canvas[3] - canvas[1]
    sheet = Image.new("RGBA", (width * frame_count, height), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        sheet.alpha_composite(frame, (index * width, 0))
    stem = compiled.name.removesuffix(".compiled").removesuffix(".reanim")
    sheet_path = out_dir / f"{stem}_{selected_layer}_12fps.png"
    gif_path = out_dir / f"{stem}_{selected_layer}_12fps.gif"
    sheet.save(sheet_path)
    frames[0].save(
        gif_path,
        save_all=True,
        append_images=frames[1:],
        duration=max(1, round(1000 / fps)),
        loop=0,
        disposal=2,
        transparency=0,
    )
    metadata = {
        "source": str(compiled),
        "fps": fps,
        "layer": selected_layer,
        "preset": preset,
        "hidden_prefixes": hidden_prefixes,
        "frame_start": frame_start,
        "frames": frame_count,
        "frame_width": width,
        "frame_height": height,
        "canvas": {"left": canvas[0], "top": canvas[1], "right": canvas[2], "bottom": canvas[3]},
        "tracks": [{"name": track.name, "frames": len(track.transforms)} for track in definition.tracks],
        "images": image_count,
        "sheet": sheet_path.name,
        "gif": gif_path.name,
    }
    (out_dir / f"{stem}_{selected_layer}_metadata.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return metadata


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiled", type=Path, help="*.reanim.compiled")
    parser.add_argument("--debug-root", type=Path, required=True,
                        help="source root containing reanim/ and images/")
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--fps", type=float, default=12.0)
    parser.add_argument(
        "--layer",
        default=None,
        help="animation layer name, e.g. anim_full_idle or anim_shooting",
    )
    parser.add_argument(
        "--hide-prefix",
        action="append",
        default=[],
        help="hide track names starting with this prefix; may be repeated",
    )
    parser.add_argument(
        "--anchor",
        choices=("top_left", "center"),
        default="top_left",
        help="image coordinate anchor; center matches the engine matrix convention",
    )
    parser.add_argument(
        "--hold-inactive",
        action="store_true",
        help="hold the last active pose of tracks that are off in this frame",
    )
    args = parser.parse_args()
    metadata = run(
        args.compiled, args.debug_root, args.out_dir, args.fps, args.layer,
        tuple(args.hide_prefix), args.anchor, args.hold_inactive,
    )
    print(json.dumps(metadata, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
