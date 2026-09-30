"""CLI wiring for the Ikemen -> Saturn asset pipeline.

Modes:
  char   --sff K.sff --air K.air --actions 0,20,40 --palettes 1,4
           Emits <prefix>_frames.c/.h (metadata + palettes) and a packed
           sprite payload binary selected by --sprite-bin.
  fx     --sff fightfx.sff --air fightfx.air --actions 0,1,2,3,40
           Preserves each referenced sprite's SFF palette and AIR drawtype.
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


def _runtime_sprite_asset(container: sff_mod.SffContainer,
                          node: sff_mod.SpriteNode,
                          all_nodes: list[sff_mod.SpriteNode] | None = None,
                          node_index: int | None = None) -> emit_mod.SpriteAsset:
    # SFF v2's link field refers to an earlier sprite when data_length == 0.
    # Ikemen shares that source texture/size while retaining the destination
    # sprite's own axis and palette selection.
    source_node = node
    source_index = node_index
    seen: set[int] = set()
    while (all_nodes is not None and source_index is not None and
           source_node.data_length == 0):
        link = source_node.palette_link
        if link >= source_index or link >= len(all_nodes) or link in seen:
            break
        seen.add(link)
        source_node = all_nodes[link]
        source_index = link

    source = container.sprite_data(source_node)
    raw = decode(source_node, source)
    if len(raw) != source_node.width * source_node.height:
        raise ValueError(
            f"sprite {(node.group, node.number)} decoded size mismatch")

    if source_node.fmt == sff_mod.FORMAT_RAW or (
            source_node.fmt == sff_mod.FORMAT_LZ5 and source):
        payload = source
        runtime_format = source_node.fmt
    else:
        # Formats unsupported by the tiny Saturn decoder are lowered offline.
        # Invalid empty links keep the legacy deterministic zero-sprite path.
        payload = raw
        runtime_format = sff_mod.FORMAT_RAW

    padded_w = (source_node.width + 7) & ~7
    left = (padded_w - source_node.width) // 2
    return emit_mod.SpriteAsset(
        width=source_node.width,
        height=source_node.height,
        padded_width=padded_w,
        left_pad=left,
        format=runtime_format,
        data=bytes(payload),
    )


def _indexed_nodes(container: sff_mod.SffContainer):
    all_nodes = list(container.sprite_nodes())
    by_key: dict[tuple[int, int], tuple[int, sff_mod.SpriteNode]] = {}
    for index, node in enumerate(all_nodes):
        # Match Ikemen: the first duplicate group/number wins.
        by_key.setdefault((node.group, node.number), (index, node))
    return all_nodes, by_key


def cmd_char(args) -> int:
    container = sff_mod.load(Path(args.sff))
    actions = air_mod.parse(Path(args.air))
    all_nodes, nodes = _indexed_nodes(container)

    main_pal_idx, main_palette = _load_palette(
        container, args.palettes[0][0], args.palettes[0][1])
    alt_palettes = {}
    for i, key in enumerate(args.palettes[1:], start=1):
        _, words = _load_palette(container, key[0], key[1])
        alt_palettes[f"alt{i}"] = words

    frames: list[emit_mod.FrameAsset] = []
    sprite_assets: dict[tuple[int, int], emit_mod.SpriteAsset] = {}
    missing: list[str] = []
    for action in args.actions:
        act = actions.get(action)
        if act is None or not act.frames:
            missing.append(str(action))
            continue
        for idx, fr in enumerate(act.frames):
            entry = nodes.get((fr.group, fr.number))
            if entry is None:
                missing.append(f"{action}:{fr.group},{fr.number}")
                continue
            node_index, node = entry
            key = (fr.group, fr.number)
            if key not in sprite_assets:
                try:
                    sprite_assets[key] = _runtime_sprite_asset(
                        container, node, all_nodes, node_index)
                except UnsupportedCodec as exc:
                    raise SystemExit(f"[ikemen_sff] {key}: {exc}", 2)
            sprite = sprite_assets[key]
            frames.append(emit_mod.FrameAsset(
                action=action, index=idx,
                width=sprite.padded_width, height=sprite.height,
                ax=node.xoff + sprite.left_pad, ay=node.yoff, ticks=fr.time,
                flip_h=fr.flip_h, flip_v=fr.flip_v,
                sprite_key=key, blend_mode=fr.blend_mode,
                loop_start=(act.loop_start == idx),
                clsn1=list(fr.clsn1), clsn2=list(fr.clsn2)))
    if not frames:
        raise SystemExit("[ikemen_sff] no frames resolved", 2)

    names = {"main": main_palette, **alt_palettes}
    c_text, h_text, sprite_blob = emit_mod.emit_frames(
        args.out_prefix, args.symbol, frames, sprite_assets, names,
        Path(args.sff).name)
    out_prefix = Path(args.out_prefix)
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    (out_prefix.parent / f"{out_prefix.name}_frames.c").write_text(c_text, encoding="utf-8")
    (out_prefix.parent / f"{out_prefix.name}_frames.h").write_text(h_text, encoding="utf-8")
    sprite_bin = Path(args.sprite_bin) if args.sprite_bin else (
        out_prefix.parent / f"{out_prefix.name}_sprites.bin")
    sprite_bin.parent.mkdir(parents=True, exist_ok=True)
    sprite_bin.write_bytes(sprite_blob)

    raw_blob = sum(s.padded_width * s.height for s in sprite_assets.values())
    packed_blob = sum(len(s.data) for s in sprite_assets.values())
    max_sprite = max(s.padded_width * s.height for s in sprite_assets.values())
    manifest = {
        "symbol": args.symbol,
        "sff": args.sff,
        "air": args.air,
        "actions": {str(a): len(actions[a].frames)
                    for a in args.actions if a in actions},
        "frames": len(frames),
        "unique_sprites": len(sprite_assets),
        "pixel_bytes": raw_blob,
        "packed_pixel_bytes": packed_blob,
        "stored_pixel_bytes": len(sprite_blob),
        "sprite_blob": str(sprite_bin),
        "max_sprite_bytes": max_sprite,
        "max_sprite_source_bytes": max(len(s.data) for s in sprite_assets.values()),
        "clsn1_default": {str(a): actions[a].clsn1_default
                           for a in args.actions if a in actions},
        "clsn2_default": {str(a): actions[a].clsn2_default
                           for a in args.actions if a in actions},
        "missing": missing,
    }
    (out_prefix.parent / f"{out_prefix.name}.json").write_text(
        json.dumps(manifest, indent=2), encoding="utf-8")

    if args.png_dir:
        for key in list(sprite_assets)[:args.png_limit]:
            _, node = nodes[key]
            raw = decode(node, container.sprite_data(node))
            pixels, padded_w, _ = _pad_sprite(raw, node.width, node.height)
            _png_dump_indexed(Path(args.png_dir) / f"{key[0]}_{key[1]}.png",
                              pixels, padded_w, node.height, main_palette)
    print(f"[ikemen_sff] char {args.symbol}: frames={len(frames)} "
          f"unique={len(sprite_assets)} raw={raw_blob}B packed={packed_blob}B "
          f"missing={missing or 'none'}")
    return 0


def cmd_fx(args) -> int:
    container = sff_mod.load(Path(args.sff))
    actions = air_mod.parse(Path(args.air))
    all_nodes, nodes = _indexed_nodes(container)

    frames: list[emit_mod.FrameAsset] = []
    sprite_assets: dict[tuple[int, int], emit_mod.SpriteAsset] = {}
    palette_slots: dict[tuple[int, ...], int] = {}
    palette_words: list[list[int]] = []
    missing: list[str] = []

    for action in args.actions:
        act = actions.get(action)
        if act is None or not act.frames:
            missing.append(str(action))
            continue
        for idx, fr in enumerate(act.frames):
            entry = nodes.get((fr.group, fr.number))
            if entry is None:
                missing.append(f"{action}:{fr.group},{fr.number}")
                continue
            node_index, node = entry
            key = (fr.group, fr.number)
            if key not in sprite_assets:
                try:
                    sprite = _runtime_sprite_asset(
                        container, node, all_nodes, node_index)
                except UnsupportedCodec as exc:
                    raise SystemExit(f"[ikemen_sff] {key}: {exc}", 2)
                palette_index = (
                    node.palette_index
                    if node.palette_index < container.palette_count
                    else 0
                )
                words = pal_mod.materialize_index(container, palette_index)
                pal_key = tuple(words)
                palette_index = palette_slots.get(pal_key)
                if palette_index is None:
                    palette_index = len(palette_words)
                    palette_slots[pal_key] = palette_index
                    palette_words.append(words)
                sprite.palette_index = palette_index
                sprite_assets[key] = sprite

            sprite = sprite_assets[key]
            frames.append(emit_mod.FrameAsset(
                action=action, index=idx,
                width=sprite.padded_width, height=sprite.height,
                ax=node.xoff + sprite.left_pad, ay=node.yoff, ticks=fr.time,
                flip_h=fr.flip_h, flip_v=fr.flip_v,
                sprite_key=key, blend_mode=fr.blend_mode,
                loop_start=(act.loop_start == idx),
                clsn1=list(fr.clsn1), clsn2=list(fr.clsn2)))

    if not frames:
        raise SystemExit("[ikemen_sff] no effect frames resolved", 2)
    if len(palette_words) > 0xFFFF:
        raise SystemExit("[ikemen_sff] too many effect palettes", 2)

    names = {f"p{i}": words for i, words in enumerate(palette_words)}
    c_text, h_text, sprite_blob = emit_mod.emit_frames(
        args.out_prefix, args.symbol, frames, sprite_assets, names,
        Path(args.sff).name)

    out_prefix = Path(args.out_prefix)
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    (out_prefix.parent / f"{out_prefix.name}_frames.c").write_text(
        c_text, encoding="utf-8")
    (out_prefix.parent / f"{out_prefix.name}_frames.h").write_text(
        h_text, encoding="utf-8")
    sprite_bin = Path(args.sprite_bin) if args.sprite_bin else (
        out_prefix.parent / f"{out_prefix.name}_sprites.bin")
    sprite_bin.parent.mkdir(parents=True, exist_ok=True)
    sprite_bin.write_bytes(sprite_blob)

    manifest = {
        "symbol": args.symbol,
        "sff": args.sff,
        "air": args.air,
        "actions": {str(a): len(actions[a].frames)
                    for a in args.actions if a in actions},
        "frames": len(frames),
        "unique_sprites": len(sprite_assets),
        "palettes": len(palette_words),
        "sprite_blob": str(sprite_bin),
        "stored_pixel_bytes": len(sprite_blob),
        "max_sprite_bytes": max(
            s.padded_width * s.height for s in sprite_assets.values()),
        "max_sprite_source_bytes": max(
            len(s.data) for s in sprite_assets.values()),
        "missing": missing,
    }
    (out_prefix.parent / f"{out_prefix.name}.json").write_text(
        json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"[ikemen_sff] fx {args.symbol}: frames={len(frames)} "
          f"unique={len(sprite_assets)} palettes={len(palette_words)} "
          f"stored={len(sprite_blob)}B missing={missing or 'none'}")
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
    char.add_argument("--sprite-bin",
                      help="packed runtime sprite payload binary output")
    char.add_argument("--png-dir")
    char.add_argument("--png-limit", type=int, default=8)
    char.set_defaults(fn=cmd_char)

    fx = sub.add_parser("fx")
    fx.add_argument("--sff", required=True)
    fx.add_argument("--air", required=True)
    fx.add_argument("--actions", type=_parse_actions, required=True,
                    help="comma-separated AIR action numbers")
    fx.add_argument("--out-prefix", required=True)
    fx.add_argument("--symbol", required=True)
    fx.add_argument("--sprite-bin",
                    help="packed runtime effect sprite payload binary output")
    fx.set_defaults(fn=cmd_fx)

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
