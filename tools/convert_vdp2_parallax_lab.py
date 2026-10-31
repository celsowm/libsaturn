#!/usr/bin/env python3
"""Convert the four Parallax Lab layers into VDP2-ready C arrays.

The source art is 512x256 RGBA.  Two layers become 8bpp bitmaps (NBG0 and
NBG1); the other two become 8x8 cell maps:

  foreground   NBG0  8bpp bitmap, 256-colour CRAM bank 0, transparent dot 0
  sea          NBG1  8bpp bitmap, 256-colour CRAM bank 1, transparent dot 0
  mountains    NBG2  4bpp cell map, 16-colour CRAM block 4, transparent dot 0
  sky          NBG3  8bpp cell map, 256-colour CRAM bank 2, opaque

Cell layers use two-word pattern names so all 2048 tiles of a 512x256 layer
can be addressed without the 1-word auxiliary character-number window.  The
VRAM bases match main.c: sky chars 0x60000, mountain chars 0x48000, maps at
0x40000/0x44000.  The tool emits big-endian VDP2 words, ready for
sat_vdp2_vram_write_words.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

from PIL import Image

from saturn_asset_common import format_word_array, rgb888_to_rgb555

WIDTH = 512
HEIGHT = 256
CELLS_X = WIDTH // 8
CELLS_Y = HEIGHT // 8
MAP_CELLS = 64 * 64

SKY_CHAR_BASE = 0x3000       # 0x60000 / 0x20
MOUNTAIN_CHAR_BASE = 0x2400  # 0x48000 / 0x20
# 256-colour banks are selected by bits 6-4 of the 7-bit palette field.
SKY_PALETTE = 2 << 4
# 16-colour palettes use the 7-bit field as the block index.
MOUNTAIN_PALETTE = 4


def _quantize(path: Path, colors: int, reserve_index0: bool, dither) -> tuple[bytes, list[int]]:
    image = Image.open(path)
    if image.size != (WIDTH, HEIGHT):
        raise ValueError(f"{path.name}: expected {WIDTH}x{HEIGHT}, got {image.size}")
    image = image.convert("RGBA")
    alpha = image.getchannel("A").tobytes()
    rgb = image.convert("RGB")
    quantized = rgb.quantize(colors=colors - 1 if reserve_index0 else colors, dither=dither)
    raw = quantized.tobytes()
    palette = quantized.getpalette() or []
    entries: list[int] = []
    for i in range(0, min(len(palette), 256 * 3), 3):
        entries.append(rgb888_to_rgb555(palette[i], palette[i + 1], palette[i + 2]))
    if reserve_index0:
        pixels = bytes(0 if a < 128 else min(255, value + 1)
                       for value, a in zip(raw, alpha))
        entries = [0] + entries[: colors - 1]
    else:
        if any(a < 128 for a in alpha):
            raise ValueError(f"{path.name}: opaque layer has transparent pixels")
        pixels = raw
        entries = entries[:colors]
    while len(entries) < colors:
        entries.append(0)
    return pixels, entries


def _pack_8bpp_bitmap(pixels: bytes) -> list[int]:
    words: list[int] = []
    for y in range(HEIGHT):
        row = pixels[y * WIDTH : (y + 1) * WIDTH]
        for x in range(0, WIDTH, 2):
            words.append((row[x] << 8) | row[x + 1])
    return words


def _pack_8bpp_cells(pixels: bytes) -> list[int]:
    words: list[int] = []
    for ty in range(CELLS_Y):
        for tx in range(CELLS_X):
            for y in range(8):
                row = (ty * 8 + y) * WIDTH + tx * 8
                for x in range(0, 8, 2):
                    words.append((pixels[row + x] << 8) | pixels[row + x + 1])
    return words


def _pack_4bpp_cells(pixels: bytes) -> list[int]:
    words: list[int] = []
    for ty in range(CELLS_Y):
        for tx in range(CELLS_X):
            for y in range(8):
                row = (ty * 8 + y) * WIDTH + tx * 8
                for x in range(0, 8, 4):
                    words.append((pixels[row + x] << 12) |
                                 (pixels[row + x + 1] << 8) |
                                 (pixels[row + x + 2] << 4) |
                                 pixels[row + x + 3])
    return words


def _cell_map(char_base: int, palette: int, char_step: int, transparent_tile: bool) -> list[int]:
    words = [0] * (MAP_CELLS * 2)
    for y in range(CELLS_Y):
        for x in range(CELLS_X):
            cell = y * 64 + x
            words[cell * 2] = palette
            tile = y * CELLS_X + x
            words[cell * 2 + 1] = char_base + (tile + (1 if transparent_tile else 0)) * char_step
    # The plane is 64x64 cells; only the top 32 rows carry the 512x256 image.
    # The sky is opaque and repeats its first tile outside the image.  The
    # mountain layer leaves the rest transparent (name 0).
    if not transparent_tile:
        for y in range(CELLS_Y, 64):
            for x in range(64):
                cell = y * 64 + x
                words[cell * 2] = palette
                words[cell * 2 + 1] = char_base
    return words


def _emit(header: Path, source: Path, arrays: Sequence[tuple[str, Sequence[int]]]) -> None:
    guard = "VDP2_PARALLAX_LAB_ASSETS_H"
    header_lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <stdint.h>",
        "",
        f"#define PARALLAX_LAB_WIDTH {WIDTH}u",
        f"#define PARALLAX_LAB_HEIGHT {HEIGHT}u",
        f"#define PARALLAX_LAB_MAP_WORDS {MAP_CELLS * 2}u",
        "",
    ]
    for name, values in arrays:
        header_lines.append(f"extern const uint16_t {name}[{len(values)}];")
    header_lines += ["", f"#endif /* {guard} */", ""]
    header.write_text("\n".join(header_lines), encoding="utf-8")

    source_lines = [f'#include "{header.name}"', ""]
    for name, values in arrays:
        source_lines.append(f"const uint16_t {name}[{len(values)}] = {{")
        source_lines.append(format_word_array(values))
        source_lines.append("};")
        source_lines.append("")
    source.write_text("\n".join(source_lines), encoding="utf-8")


def convert(assets: Path, out_prefix: Path) -> dict:
    sky_pixels, sky_palette = _quantize(
        assets / "sky.png", 256, False, Image.Dither.NONE)
    mountain_pixels, mountain_palette = _quantize(
        assets / "mountains.png", 16, True, Image.Dither.FLOYDSTEINBERG)
    sea_pixels, sea_palette = _quantize(
        assets / "sea.png", 256, True, Image.Dither.NONE)
    foreground_pixels, foreground_palette = _quantize(
        assets / "foreground.png", 256, True, Image.Dither.NONE)

    arrays = [
        ("plab_foreground_palette", foreground_palette),
        ("plab_foreground_words", _pack_8bpp_bitmap(foreground_pixels)),
        ("plab_sea_palette", sea_palette),
        ("plab_sea_words", _pack_8bpp_bitmap(sea_pixels)),
        ("plab_sky_palette", sky_palette),
        ("plab_sky_chars", _pack_8bpp_cells(sky_pixels)),
        ("plab_sky_map", _cell_map(SKY_CHAR_BASE, SKY_PALETTE, 2, False)),
        ("plab_mountain_palette", mountain_palette),
        ("plab_mountain_chars", [0] * 16 + _pack_4bpp_cells(mountain_pixels)),
        ("plab_mountain_map", _cell_map(
            MOUNTAIN_CHAR_BASE, MOUNTAIN_PALETTE, 1, True)),
    ]

    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    _emit(out_prefix.with_suffix(".h"), out_prefix.with_suffix(".c"), arrays)
    report = {
        "width": WIDTH,
        "height": HEIGHT,
        "foreground_colors": len(foreground_palette),
        "sea_colors": len(sea_palette),
        "sky_colors": len(sky_palette),
        "mountain_colors": len(mountain_palette),
        "foreground_words": len(arrays[1][1]),
        "sea_words": len(arrays[3][1]),
        "sky_char_words": len(arrays[5][1]),
        "mountain_char_words": len(arrays[8][1]),
    }
    out_prefix.with_suffix(".report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--out-prefix", type=Path, required=True)
    args = parser.parse_args()
    report = convert(args.assets, args.out_prefix)
    print(json.dumps(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
