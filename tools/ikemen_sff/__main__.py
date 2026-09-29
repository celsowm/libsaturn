"""CLI wiring for the Ikemen -> Saturn asset pipeline.

Modes:
  char   --sff K.sff --air K.air --actions 0,20,40 --palettes 1,4
           Emits <prefix>_frames.c/.h (pixels + frame table + palettes,
           palette names: main / alt1 / alt2 ... from --palettes).
  stage  --sff S.sff --layers "0,0:start0,0:tile;0,1:start0,185:tile"
           Emits <prefix>_plane.c/.h (320x224 indexed canvas + palette).

Both modes also write <prefix>.json (manifest: stats, warnings) and,
with --png-dir, debug PNG renders for visual inspection.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import air as air_mod
from . import emit as emit_mod
from . import palettes as pal_mod
from . import sff as sff_mod
from . import stage as stage_mod
from .codecs import decode, UnsupportedCodec

PALETTE_NODE_MAIN = 0  # sprite palidx 0 -> palette node 0


def _load_palette(container: sff_mod.SffContainer, group: int, number: int):
    for i, node in enumerate(container.palette_nodes()):
        if node.group == group and node.number == number:
            return i, pal_mod.materialize(container, node)
    raise SystemExit(f"[ikemen_sff] palette ({group},{number}) not found", 2)


def _png_dump_indexed(path: Path, pixels, width: int, height: int,
                      palette: list[int]) -> None:
    from PIL import Image
    img = Image.frombytes("P", (width, height), bytes(pixels))
    rgb = bytearray()
    for w in palette:
        b = (w >> 10) & 0x1F
        g = (w >> 5) & 0x1F
        r = w & 0x1F
        rgb += bytes((r << 3, g << 3, b << 3))
    img.putpalette(bytes(rgb[:768]))
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def _pad_sprite(pixels: bytes, width: int, height: int):
    """VDP1 textures need a width that is a multiple of 8: the sprite is
    centered inside the padded box and the transparent pads are index 0.
    Returns (pixels, padded_width, left_pad); adding left_pad to the
    sprite axis keeps horizontal flip math exact (symmetric box)."""
    padded = (width + 7) & ~7
    if padded == width:
        return bytes(pixels), width, 0
    left = (padded - width) // 2
    out = bytearray(padded * height)
    for row in range(height):
        out[row * padded + left:row * padded + left + width] = \
            pixels[row * width:row * width + width]
    return bytes(out), padded, left


def cmd_char(args) -> int:
    container = sff_mod.load(Path(args.sff))
    actions = air_mod.parse(Path(args.air))
    nodes = {(n.group, n.number): n for n in container.sprite_nodes()}

    main_pal_idx, main_palette = _load_palette(
        container, args.palettes[0][0], args.palettes[0][1])
    alt_palettes = {}
    for i, key in enumerate(args.palettes[1:], start=1):
        _, words = _load_palette(container, key[0], key[1])
        alt_palettes[f"alt{i}"] = words

    frames: list[emit_mod.FrameAsset] = []
    sprite_pixels: dict[tuple[int, int], bytes] = {}
    missing: list[str] = []
    for action in args.actions:
        act = actions.get(action)
        if act is None or not act.frames:
            missing.append(str(action))
            continue
        for idx, fr in enumerate(act.frames):
            node = nodes.get((fr.group, fr.number))
            if node is None:
                missing.append(f"{action}:{fr.group},{fr.number}")
                continue
            key = (fr.group, fr.number)
            if key not in sprite_pixels:
                try:
                    raw = decode(node, container.sprite_data(node))
                except UnsupportedCodec as exc:
                    raise SystemExit(f"[ikemen_sff] {key}: {exc}", 2)
                padded, padded_w, left = _pad_sprite(raw, node.width, node.height)
                sprite_pixels[key] = padded
            else:
                padded_w = (node.width + 7) & ~7
                left = (padded_w - node.width) // 2
            frames.append(emit_mod.FrameAsset(
                action=action, index=idx, width=padded_w, height=node.height,
                ax=node.xoff + left, ay=node.yoff, ticks=fr.time,
                flip_h=fr.flip_h, flip_v=fr.flip_v,
                sprite_key=key, clsn2=list(fr.clsn2) or list(act.clsn2_default)))
    if not frames:
        raise SystemExit("[ikemen_sff] no frames resolved", 2)

    names = {"main": main_palette, **alt_palettes}
    c_text, h_text = emit_mod.emit_frames(
        args.out_prefix, args.symbol, frames, sprite_pixels, names,
        Path(args.sff).name)
    out_prefix = Path(args.out_prefix)
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    (out_prefix.parent / f"{out_prefix.name}_frames.c").write_text(c_text, encoding="utf-8")
    (out_prefix.parent / f"{out_prefix.name}_frames.h").write_text(h_text, encoding="utf-8")

    blob = sum(len(p) for p in sprite_pixels.values())
    manifest = {
        "symbol": args.symbol,
        "sff": args.sff,
        "air": args.air,
        "actions": {str(a): len(actions[a].frames)
                    for a in args.actions if a in actions},
        "frames": len(frames),
        "unique_sprites": len(sprite_pixels),
        "pixel_bytes": blob,
        "clsn_default": {str(a): actions[a].clsn2_default
                          for a in args.actions if a in actions},
        "missing": missing,
    }
    (out_prefix.parent / f"{out_prefix.name}.json").write_text(
        json.dumps(manifest, indent=2), encoding="utf-8")

    if args.png_dir:
        for key, pixels in list(sprite_pixels.items())[:args.png_limit]:
            node = nodes[key]
            padded_w = (node.width + 7) & ~7
            _png_dump_indexed(Path(args.png_dir) / f"{key[0]}_{key[1]}.png",
                              pixels, padded_w, node.height, main_palette)
    print(f"[ikemen_sff] char {args.symbol}: frames={len(frames)} "
          f"unique={len(sprite_pixels)} pixels={blob}B missing={missing or 'none'}")
    return 0


def cmd_stage(args) -> int:
    container = sff_mod.load(Path(args.sff))
    layers = []
    for spec in args.layers.split(";"):
        spec = spec.strip()
        if not spec:
            continue
        parts = spec.split(":")
        group, number = (int(v) for v in parts[0].split(","))
        layer = stage_mod.StageLayer(sprite=(group, number))
        for opt in parts[1:]:
            if opt.startswith("start"):
                sx, sy = (int(v) for v in opt.split("=")[1].split(","))
                layer.start = (sx, sy)
            elif opt == "tile":
                layer.tile_x = True
        layers.append(layer)
    stage = stage_mod.compose(container, layers)

    c_text, h_text = emit_mod.emit_stage(
        args.out_prefix, args.symbol, stage, Path(args.sff).name)
    out_prefix = Path(args.out_prefix)
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    (out_prefix.parent / f"{out_prefix.name}_plane.c").write_text(c_text, encoding="utf-8")
    (out_prefix.parent / f"{out_prefix.name}_plane.h").write_text(h_text, encoding="utf-8")

    (out_prefix.parent / f"{out_prefix.name}.json").write_text(json.dumps({
        "symbol": args.symbol,
        "sff": args.sff,
        "layers": [str(l.sprite) for l in layers],
        "width": stage.width,
        "height": stage.height,
        "warnings": stage.warnings,
    }, indent=2), encoding="utf-8")

    if args.png_dir:
        _png_dump_indexed(Path(args.png_dir) / f"{args.symbol}.png",
                          stage.pixels, stage.width, stage.height, stage.palette)
    print(f"[ikemen_sff] stage {args.symbol}: {stage.width}x{stage.height} "
          f"pixels={len(stage.pixels)}B warnings={stage.warnings or 'none'}")
    return 0


def _parse_actions(text: str) -> list[int]:
    return [int(v) for v in text.split(",") if v.strip()]


def _parse_palettes(text: str) -> list[tuple[int, int]]:
    return [tuple(int(v) for v in entry.split(",")) for entry in text.split(";")]


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="ikemen_sff")
    sub = ap.add_subparsers(dest="mode", required=True)

    char = sub.add_parser("char")
    char.add_argument("--sff", required=True)
    char.add_argument("--air", required=True)
    char.add_argument("--actions", type=_parse_actions, required=True,
                      help="comma-separated AIR action numbers")
    char.add_argument("--palettes", type=_parse_palettes, required=True,
                      help="palette (group,number) list, ';' separated; "
                           "first is main, others alt1..altN")
    char.add_argument("--out-prefix", required=True)
    char.add_argument("--symbol", required=True)
    char.add_argument("--png-dir")
    char.add_argument("--png-limit", type=int, default=8)
    char.set_defaults(fn=cmd_char)

    st = sub.add_parser("stage")
    st.add_argument("--sff", required=True)
    st.add_argument("--layers", required=True)
    st.add_argument("--out-prefix", required=True)
    st.add_argument("--symbol", required=True)
    st.add_argument("--png-dir")
    st.set_defaults(fn=cmd_stage)

    args = ap.parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
