#!/usr/bin/env python3
from __future__ import annotations

import argparse
import urllib.error
import urllib.request
from pathlib import Path

from PIL import Image

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))

# When installed in libsaturn/tools this resolves directly. For standalone
# syntax tests the import can be satisfied by placing saturn_asset_common.py
# beside this script.
from saturn_asset_common import format_byte_array, format_word_array, rgb888_to_rgb555

SOURCE_URL = "https://dl.polyhaven.org/file/ph-assets/HDRIs/extra/Tonemapped%20JPG/dikhololo_night.jpg"
SOURCE_PAGE = "https://polyhaven.com/a/dikhololo_night"
WIDTH = 512
HEIGHT = 128


def download(url: str, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": "libsaturn-infinite-explorer/1.0"})
    with urllib.request.urlopen(req, timeout=60) as response:
        data = response.read()
    if len(data) < 1024:
        raise ValueError("downloaded panorama is unexpectedly small")
    path.write_bytes(data)


def crop_horizon(img: Image.Image) -> Image.Image:
    """Keep the sky plus the 360-degree horizon and discard most of the nadir.

    The source is a true equirectangular 360 panorama, so horizontal wrapping
    remains physically seamless after a vertical crop and resize.
    """
    img = img.convert("RGB")
    w, h = img.size
    top = int(h * 0.06)
    bottom = int(h * 0.61)
    cropped = img.crop((0, top, w, bottom))
    resample = getattr(getattr(Image, "Resampling", Image), "LANCZOS")
    return cropped.resize((WIDTH, HEIGHT), resample=resample)


def quantize(img: Image.Image) -> tuple[bytes, list[int]]:
    # Quantizing the complete 360 strip in one pass gives one shared palette
    # across the seam instead of two independently quantized edges.
    adaptive = getattr(getattr(Image, "Palette", Image), "ADAPTIVE")
    q = img.convert("P", palette=adaptive, colors=256)
    pixels = bytes(q.getdata())
    pal = q.getpalette() or []
    colors: list[int] = []
    for i in range(0, min(len(pal), 256 * 3), 3):
        colors.append(rgb888_to_rgb555(pal[i], pal[i + 1], pal[i + 2]))
    while len(colors) < 256:
        colors.append(0x8000)
    return pixels, colors


def fallback_panorama() -> Image.Image:
    """Deterministic fallback used only when the CC0 source cannot be fetched."""
    img = Image.new("RGB", (WIDTH, HEIGHT))
    px = img.load()
    for y in range(HEIGHT):
        t = y / max(1, HEIGHT - 1)
        for x in range(WIDTH):
            star = ((x * 1103515245 + y * 12345 + 0x51A7) >> 9) & 0x3FF
            glow = max(0.0, 1.0 - abs(t - 0.72) * 8.0)
            r = int(5 + 24 * glow)
            g = int(8 + 34 * glow)
            b = int(18 + 52 * glow)
            if star == 0 and y < HEIGHT * 0.65:
                r = g = b = 220
            px[x, y] = (r, g, b)
    return img


def emit(out_c: Path, out_h: Path, pixels: bytes, palette: list[int], real_source: bool) -> None:
    out_h.parent.mkdir(parents=True, exist_ok=True)
    out_h.write_text(
        "\n".join([
            "#ifndef INFINITE_EXPLORER_HORIZON_H",
            "#define INFINITE_EXPLORER_HORIZON_H",
            "",
            "#include <stdint.h>",
            "",
            f"#define EXPLORER_HORIZON_WIDTH {WIDTH}u",
            f"#define EXPLORER_HORIZON_HEIGHT {HEIGHT}u",
            f"#define EXPLORER_HORIZON_REAL_SOURCE {1 if real_source else 0}u",
            "extern const uint8_t explorer_horizon_pixels[];",
            "extern const uint16_t explorer_horizon_palette[256];",
            "",
            "#endif",
            "",
        ]), encoding="utf-8"
    )
    out_c.write_text(
        "\n".join([
            '#include "horizon.h"',
            "",
            f"const uint8_t explorer_horizon_pixels[{len(pixels)}] = {{",
            format_byte_array(list(pixels)),
            "};",
            "",
            "const uint16_t explorer_horizon_palette[256] = {",
            format_word_array(palette),
            "};",
            "",
        ]), encoding="utf-8"
    )


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-c", required=True)
    ap.add_argument("--out-h", required=True)
    ap.add_argument("--cache-dir")
    ap.add_argument("--require-real", action="store_true")
    args = ap.parse_args()

    out_c = Path(args.out_c)
    out_h = Path(args.out_h)
    cache_dir = Path(args.cache_dir) if args.cache_dir else out_c.parent / "cc0_cache"
    source = cache_dir / "dikhololo_night.jpg"
    real_source = True

    try:
        if not source.exists():
            download(SOURCE_URL, source)
            print(f"[infinite-explorer] downloaded CC0 360 panorama: {SOURCE_PAGE}")
        img = crop_horizon(Image.open(source))
    except (OSError, ValueError, urllib.error.URLError) as exc:
        if args.require_real:
            raise SystemExit(f"failed to fetch/decode CC0 horizon: {exc}") from exc
        print(f"[infinite-explorer] warning: CC0 horizon unavailable: {exc}; using deterministic fallback")
        img = fallback_panorama()
        real_source = False

    pixels, palette = quantize(img)
    emit(out_c, out_h, pixels, palette, real_source)


if __name__ == "__main__":
    main()
