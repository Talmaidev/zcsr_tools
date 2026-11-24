#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generic offline asset baker (GIF source contract).

The source is a project-local folder:

    assets_src/<asset_id>/source.gif
    assets_src/<asset_id>/meta.txt     (optional keys below)

    id=example
    fps=12              # uniform fps; frame_ms wins when both are present
    frame_ms=80         # uniform per-frame duration
    loop=1              # 0 = one-shot, 1 = loop
    anchor=bottom       # metadata only, not baked into pixels

Output:

    GIF -> composited RGBA frames -> one grid-strip PNG
        -> one text manifest row (frame count, grid, anchor, per-frame ms)

The runtime does not decode GIFs. It only sees a plain PNG strip plus
normalized UV rectangles. Game-specific manifest headers and source labels are
provided by the consumer.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

from PIL import Image

MAX_SHEET_WIDTH = 2048
DEFAULT_FRAME_MS = 100
ONE_SHOT_DEFAULT = False


def snake(name: str) -> str:
    out = []
    for ch in name:
        if ch.isalnum():
            out.append(ch.lower())
        elif out and out[-1] != "_":
            out.append("_")
    return "".join(out).strip("_") or "asset"


def read_meta(path: Path) -> dict[str, str]:
    meta: dict[str, str] = {}
    if not path.is_file():
        return meta
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        meta[key.strip()] = value.strip()
    return meta


def load_gif(path: Path, frame_ms: int | None) -> tuple[list[Image.Image], list[int], int, int]:
    im = Image.open(path)
    n = int(getattr(im, "n_frames", 1))
    frames: list[Image.Image] = []
    durations: list[int] = []
    for i in range(n):
        im.seek(i)
        if frame_ms is not None:
            ms = frame_ms
        else:
            ms = int(im.info.get("duration", DEFAULT_FRAME_MS) or DEFAULT_FRAME_MS)
            ms = max(20, min(1000, ms))
        durations.append(ms)
        frames.append(im.convert("RGBA").copy())
    return frames, durations, im.width, im.height


def union_bbox(frames: list[Image.Image], w: int, h: int) -> tuple[int, int, int, int]:
    boxes = [f.getchannel("A").getbbox() for f in frames]
    boxes = [b for b in boxes if b and b[2] > b[0] and b[3] > b[1]]
    if not boxes:
        return 0, 0, w, h
    x0 = min(b[0] for b in boxes)
    y0 = min(b[1] for b in boxes)
    x1 = max(b[2] for b in boxes)
    y1 = max(b[3] for b in boxes)
    return x0, y0, x1, y1


def bake_gif(gif_path: Path, asset_id: str, out_dir: Path,
             frame_ms: int | None, loop: bool, anchor: str,
             source: str = "original", scale: float = 1.0,
             path_prefix: str = "assets/anim/") -> dict:
    frames, durations, canvas_w, canvas_h = load_gif(gif_path, frame_ms)
    n = len(frames)
    if n == 0:
        raise RuntimeError("no frames")

    if scale != 1.0:
        scaled_w = max(1, int(round(canvas_w * scale)))
        scaled_h = max(1, int(round(canvas_h * scale)))
        frames = [frame.resize((scaled_w, scaled_h), Image.Resampling.LANCZOS)
                  for frame in frames]
        canvas_w, canvas_h = scaled_w, scaled_h

    cx0, cy0, cx1, cy1 = union_bbox(frames, canvas_w, canvas_h)
    fw, fh = cx1 - cx0, cy1 - cy0
    if fw <= 0 or fh <= 0:
        raise RuntimeError("empty alpha bounds")

    cols = max(1, min(n, MAX_SHEET_WIDTH // fw))
    rows = math.ceil(n / cols)
    sheet = Image.new("RGBA", (cols * fw, rows * fh), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        cropped = frame.crop((cx0, cy0, cx1, cy1))
        sheet.paste(cropped, ((i % cols) * fw, (i // cols) * fh))

    out_path = out_dir / f"{asset_id}.png"
    sheet.save(out_path, "PNG", compress_level=9)

    return {
        "kind": "ANIM",
        "id": asset_id,
        "path": f"{path_prefix}{asset_id}.png",
        "frames": n,
        "cols": cols,
        "fw": fw,
        "fh": fh,
        "sheet_w": cols * fw,
        "sheet_h": rows * fh,
        "canvas_w": canvas_w,
        "canvas_h": canvas_h,
        "crop_x": cx0,
        "crop_y": cy0,
        "anchor": anchor,
        "loop": 1 if loop else 0,
        "ms": ",".join(str(v) for v in durations),
        "source": source,
    }


def bake_static(src: Path, asset_id: str, out_dir: Path,
                path_prefix: str = "assets/ui/") -> dict:
    im = Image.open(src).convert("RGBA")
    out_path = out_dir / f"{asset_id}.png"
    im.save(out_path, "PNG", compress_level=9)
    return {
        "kind": "IMG",
        "id": asset_id,
        "path": f"{path_prefix}{asset_id}.png",
        "w": im.width,
        "h": im.height,
        "source": "original",
    }


def format_row(row: dict) -> str:
    if row["kind"] == "ANIM":
        return (
            f"ANIM {row['id']} {row['path']} {row['frames']} {row['cols']} "
            f"{row['fw']} {row['fh']} {row['sheet_w']} {row['sheet_h']} "
            f"{row['canvas_w']} {row['canvas_h']} {row['crop_x']} {row['crop_y']} "
            f"{row['anchor']} {row['loop']} {row['ms']} source={row.get('source', 'unknown')}"
        )
    return (f"IMG {row['id']} {row['path']} {row['w']} {row['h']} "
            f"source={row.get('source', 'unknown')}")


def parse_uniform_ms(meta: dict[str, str]) -> int | None:
    if "frame_ms" in meta:
        value = int(meta["frame_ms"])
        return max(20, min(1000, value))
    if "fps" in meta:
        fps = float(meta["fps"])
        if fps <= 0.0:
            raise ValueError("fps must be > 0")
        return max(20, min(1000, int(round(1000.0 / fps))))
    return None


def bake_project(
    project: Path,
    source: Path | None = None,
    no_merge: bool = False,
    manifest_rel: str = "assets/manifest.txt",
    anim_rel: str = "assets/anim",
    ui_rel: str = "assets/ui",
    anim_prefix: str = "assets/anim/",
    ui_prefix: str = "assets/ui/",
    manifest_header: str | None = None,
) -> int:
    project = Path(project)
    source = Path(source) if source else project / "assets_src"
    anim_dir = project / anim_rel
    ui_dir = project / ui_rel
    manifest_path = project / manifest_rel
    anim_dir.mkdir(parents=True, exist_ok=True)
    ui_dir.mkdir(parents=True, exist_ok=True)

    # Preserve rows that are not present in this source tree by default. A
    # consumer may explicitly request a full replacement.
    old_rows: list[str] = []
    if manifest_path.is_file() and not no_merge:
        for raw in manifest_path.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if line and not line.startswith("#"):
                old_rows.append(line)

    if not source.is_dir():
        print(
            f"ERROR: source folder not found: {source}\n"
            "Expected assets_src/<asset_id>/source.gif plus optional meta.txt.\n"
            "Create that project-local source tree; a desktop GIF dump is not a build input.",
            file=sys.stderr,
        )
        return 2

    rows: list[dict] = []
    anim_pixels = 0
    seen: set[str] = set()

    for gif in sorted(source.glob("*/source.gif")):
        folder = gif.parent
        meta = read_meta(folder / "meta.txt")
        asset_id = meta.get("id") or folder.name
        if asset_id in seen:
            print(f"ERROR: duplicate asset id: {asset_id}", file=sys.stderr)
            return 2
        seen.add(asset_id)
        frame_ms = parse_uniform_ms(meta)
        loop = meta.get("loop", "1" if not ONE_SHOT_DEFAULT else "0")
        loop = loop.strip().lower() not in {"0", "false", "no", "off"}
        anchor = meta.get("anchor", "bottom")
        origin = meta.get("source", "original").strip().lower()
        if origin not in {"original", "fanmade"}:
            print(f"ERROR: invalid source={origin} for {asset_id}", file=sys.stderr)
            return 2
        try:
            scale = float(meta.get("scale", "1"))
        except ValueError:
            print(f"ERROR: invalid scale={meta.get('scale')} for {asset_id}",
                  file=sys.stderr)
            return 2
        if not 0.05 <= scale <= 4.0:
            print(f"ERROR: scale out of range for {asset_id}: {scale}", file=sys.stderr)
            return 2
        row = bake_gif(
            gif, asset_id, anim_dir, frame_ms, loop, anchor, origin, scale,
            anim_prefix,
        )
        rows.append(row)
        anim_pixels += row["sheet_w"] * row["sheet_h"]
        print(
            f"  ANIM {asset_id:24s} {row['frames']:>3d}f  "
            f"{row['fw']}x{row['fh']}  sheet {row['sheet_w']}x{row['sheet_h']}"
            + (f"  scale={scale:g}" if scale != 1.0 else "")
        )

    static_dir = source / "static"
    if static_dir.is_dir():
        for src in sorted(p for p in static_dir.iterdir()
                          if p.suffix.lower() in {".png", ".jpg", ".jpeg"}):
            asset_id = snake(src.stem)
            if asset_id in seen:
                print(f"ERROR: duplicate asset id: {asset_id}", file=sys.stderr)
                return 2
            seen.add(asset_id)
            row = bake_static(src, asset_id, ui_dir, ui_prefix)
            rows.append(row)
            print(f"  IMG  {asset_id:24s} {row['w']}x{row['h']}")

    manifest = manifest_path
    header = [
        manifest_header or "# Asset manifest v1 (generated by zcsr_asset_pipeline.strip)",
        "# Source contract: assets_src/<id>/source.gif + meta.txt (id/fps/frame_ms/loop/anchor).",
        "# ANIM id path frames cols fw fh sheet_w sheet_h canvas_w canvas_h crop_x crop_y anchor loop ms source",
        "# ms: uniform frame duration in milliseconds; a comma list is retained for per-frame GIF timing.",
        "# loop: 0 = one-shot, 1 = replay. anchor is metadata (bottom/top/center), not UV.",
        "# source: original | fanmade; runtime rejects cross-origin duplicate ids.",
        "# IMG  id path w h source",
    ]
    body = [format_row(r) for r in rows]
    preserved = 0
    for line in old_rows:
        fields = line.split()
        if len(fields) < 2:
            continue
        old_id = fields[1]
        if old_id in seen:
            continue
        seen.add(old_id)
        if not any(field.startswith("source=") for field in fields):
            line = f"{line} source=fanmade"
        body.append(line)
        preserved += 1
    manifest.write_text("\n".join(header + body) + "\n", encoding="utf-8")

    anims = sum(1 for r in rows if r["kind"] == "ANIM")
    imgs = len(rows) - anims
    print(
        f"\nOK: {anims} animations, {imgs} images -> {manifest}\n"
        f"    preserved existing manifest rows: {preserved}\n"
        f"    baked strip pixels: {anim_pixels:,} (~{anim_pixels * 4 / 1024 / 1024:.1f} MiB RGBA)"
    )
    return 0


def main(
    argv: list[str] | None = None,
    default_project: Path | None = None,
    default_manifest_header: str | None = None,
) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", default=None,
                    help="source root; default <project>/assets_src")
    ap.add_argument("--project", default=str(default_project or Path.cwd()))
    ap.add_argument(
        "--no-merge",
        action="store_true",
        help="replace the manifest instead of preserving existing rows whose ids "
             "are not present in assets_src",
    )
    ap.add_argument("--manifest", default="assets/manifest.txt",
                    help="manifest path relative to project")
    ap.add_argument("--anim-dir", default="assets/anim",
                    help="baked animation directory relative to project")
    ap.add_argument("--ui-dir", default="assets/ui",
                    help="baked static-image directory relative to project")
    ap.add_argument("--anim-prefix", default="assets/anim/",
                    help="path prefix written into ANIM manifest rows")
    ap.add_argument("--ui-prefix", default="assets/ui/",
                    help="path prefix written into IMG manifest rows")
    ap.add_argument("--manifest-header", default=default_manifest_header,
                    help="first manifest line; consumer-specific")
    args = ap.parse_args(argv)
    return bake_project(
        Path(args.project),
        Path(args.source) if args.source else None,
        args.no_merge,
        args.manifest,
        args.anim_dir,
        args.ui_dir,
        args.anim_prefix,
        args.ui_prefix,
        args.manifest_header,
    )


if __name__ == "__main__":
    raise SystemExit(main())
