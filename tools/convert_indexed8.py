#!/usr/bin/env python3
"""Convert indexed assets to Saturn-friendly outputs.

The converter keeps the pixel layout linear because the current VDP1 upload
path consumes a flat indexed8 stream. In addition to the legacy binary outputs,
it now emits a C header/source pair so assets can be compiled into the example
binary without runtime file loading.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Iterable, Sequence


def rgb888_to_rgb555(r: int, g: int, b: int) -> int:
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return 0x8000 | (b5 << 10) | (g5 << 5) | r5


def sanitize_identifier(value: str) -> str:
    name = re.sub(r"[^0-9A-Za-z]+", "_", value)
    name = re.sub(r"_+", "_", name).strip("_")
    if not name:
        return "asset"
    if name[0].isdigit():
        name = f"_{name}"
    return name


def asset_symbol_prefix(out_prefix: Path) -> str:
    return sanitize_identifier(out_prefix.name)


def asset_header_guard(out_prefix: Path) -> str:
    return f"{asset_symbol_prefix(out_prefix).upper()}_H"


def parse_palette_txt(path: Path) -> list[int]:
    colors: list[int] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        raw = line.strip()
        if not raw or raw.startswith("#"):
            continue
        parts = raw.replace(",", " ").split()
        if len(parts) < 3:
            raise ValueError(f"Invalid palette line: {raw}")
        r, g, b = (int(parts[0]), int(parts[1]), int(parts[2]))
        if not (0 <= r <= 255 and 0 <= g <= 255 and 0 <= b <= 255):
            raise ValueError(f"RGB out of range: {raw}")
        colors.append(rgb888_to_rgb555(r, g, b))
        if len(colors) == 256:
            break
    if not colors:
        raise ValueError("Empty palette")
    while len(colors) < 256:
        colors.append(0)
    return colors


def parse_pgm(path: Path) -> tuple[bytes, int, int]:
    data = path.read_bytes()
    if not data.startswith(b"P5"):
        raise ValueError("Only binary P5 PGM is supported")

    chunks = data.split(b"\n")
    header_tokens: list[bytes] = []
    payload_start = 0
    for i, line in enumerate(chunks):
        s = line.strip()
        if not s or s.startswith(b"#"):
            continue
        header_tokens.extend(s.split())
        if len(header_tokens) >= 4:
            payload_start = i + 1
            break
    if len(header_tokens) < 4:
        raise ValueError("Invalid PGM header")

    width = int(header_tokens[1])
    height = int(header_tokens[2])
    maxv = int(header_tokens[3])
    if maxv > 255:
        raise ValueError("PGM max value > 255 not supported")

    pixels = b"\n".join(chunks[payload_start:])
    expected = width * height
    if len(pixels) < expected:
        raise ValueError("Truncated PGM file")
    return pixels[:expected], width, height


def parse_png(
    path: Path, resize_to: tuple[int, int] | None = None
) -> tuple[bytes, int, int, list[int]]:
    try:
        from PIL import Image
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "Para PNG e necessario instalar pillow: pip install pillow"
        ) from exc

    img = Image.open(path)
    if resize_to is not None:
        resize_width, resize_height = resize_to
        if resize_width <= 0 or resize_height <= 0:
            raise ValueError("Invalid resize dimensions")
        if img.mode != "RGBA":
            img = img.convert("RGBA")
        resample = getattr(getattr(Image, "Resampling", Image), "LANCZOS")
        img = img.resize((resize_width, resize_height), resample=resample)
    if img.mode != "P":
        img = img.convert("P", palette=Image.ADAPTIVE, colors=256)
    width, height = img.size
    pixels = bytes(img.getdata())
    pal = img.getpalette() or []
    colors: list[int] = []
    for i in range(0, min(len(pal), 256 * 3), 3):
        colors.append(rgb888_to_rgb555(pal[i], pal[i + 1], pal[i + 2]))
    while len(colors) < 256:
        colors.append(0)
    return pixels, width, height, colors


def write_palette(path: Path, colors: Iterable[int]) -> None:
    out = bytearray()
    for c in colors:
        out.extend(((c >> 8) & 0xFF, c & 0xFF))
    path.write_bytes(bytes(out))


def format_byte_array(
    values: Sequence[int], indent: str = "    ", columns: int = 12
) -> str:
    if not values:
        return ""
    lines: list[str] = []
    for start in range(0, len(values), columns):
        chunk = values[start : start + columns]
        lines.append(indent + ", ".join(f"0x{value:02X}" for value in chunk))
    return ",\n".join(lines)


def format_word_array(
    values: Sequence[int], indent: str = "    ", columns: int = 8
) -> str:
    if not values:
        return ""
    lines: list[str] = []
    for start in range(0, len(values), columns):
        chunk = values[start : start + columns]
        lines.append(indent + ", ".join(f"0x{value:04X}" for value in chunk))
    return ",\n".join(lines)


def emit_asset_headers(
    out_prefix: Path,
    pixels: bytes,
    palette: Sequence[int],
    width: int,
    height: int,
    palette_index: int,
) -> tuple[Path, Path]:
    symbol_prefix = asset_symbol_prefix(out_prefix)
    header_guard = asset_header_guard(out_prefix)
    header_path = out_prefix.with_suffix(".h")
    source_path = out_prefix.with_suffix(".c")
    header_name = header_path.name

    pixel_values = list(pixels)
    palette_values = [int(value) & 0xFFFF for value in palette]

    header_path.write_text(
        "\n".join(
            [
                f"#ifndef {header_guard}",
                f"#define {header_guard}",
                "",
                "#include <stdint.h>",
                "",
                "#ifdef __cplusplus",
                'extern "C" {',
                "#endif",
                "",
                "typedef struct sat_indexed8_asset {",
                "    const uint8_t* pixels;",
                "    const uint16_t* palette;",
                "    uint16_t width;",
                "    uint16_t height;",
                "    uint16_t palette_index;",
                "    uint32_t pixel_count;",
                "    uint32_t palette_count;",
                "} sat_indexed8_asset_t;",
                "",
                f"extern const uint8_t {symbol_prefix}_pixels[{len(pixel_values)}];",
                f"extern const uint16_t {symbol_prefix}_palette[{len(palette_values)}];",
                f"extern const sat_indexed8_asset_t {symbol_prefix}_asset;",
                "",
                "#ifdef __cplusplus",
                "}",
                "#endif",
                "",
                f"#endif /* {header_guard} */",
                "",
            ]
        ),
        encoding="utf-8",
    )

    source_path.write_text(
        "\n".join(
            [
                f'#include "{header_name}"',
                "",
                f"const uint8_t {symbol_prefix}_pixels[{len(pixel_values)}] = {{",
                format_byte_array(pixel_values),
                "};",
                "",
                f"const uint16_t {symbol_prefix}_palette[{len(palette_values)}] = {{",
                format_word_array(palette_values),
                "};",
                "",
                f"const sat_indexed8_asset_t {symbol_prefix}_asset = {{",
                f"    {symbol_prefix}_pixels,",
                f"    {symbol_prefix}_palette,",
                f"    {width}u,",
                f"    {height}u,",
                f"    {palette_index}u,",
                f"    {len(pixel_values)}u,",
                f"    {len(palette_values)}u",
                "};",
                "",
            ]
        ),
        encoding="utf-8",
    )

    return header_path, source_path


def emit_legacy_outputs(
    out_prefix: Path, pixels: bytes, colors: Sequence[int]
) -> tuple[Path, Path]:
    tex_path = out_prefix.with_suffix(".tex8")
    pal_path = out_prefix.with_suffix(".pal")
    tex_path.write_bytes(pixels)
    write_palette(pal_path, colors)
    return tex_path, pal_path


def convert_asset(
    in_path: Path,
    out_prefix: Path,
    palette_path: Path | None,
    width_arg: int | None,
    height_arg: int | None,
    palette_index: int,
    resize_to: tuple[int, int] | None,
) -> dict[str, Path]:
    suffix = in_path.suffix.lower()
    pixels: bytes
    width: int
    height: int
    colors: list[int]

    if suffix in {".png", ".tga", ".bmp", ".jpg", ".jpeg"}:
        pixels, width, height, colors = parse_png(in_path, resize_to=resize_to)
    elif suffix == ".pgm":
        pixels, width, height = parse_pgm(in_path)
        if palette_path is None:
            raise ValueError(".pgm input requires --palette")
        colors = parse_palette_txt(palette_path)
    elif suffix == ".raw":
        if width_arg is None or height_arg is None:
            raise ValueError(".raw input requires --width and --height")
        width = width_arg
        height = height_arg
        pixels = in_path.read_bytes()
        if len(pixels) != width * height:
            raise ValueError(".raw size doesn't match width*height")
        if palette_path is None:
            raise ValueError(".raw input requires --palette")
        colors = parse_palette_txt(palette_path)
    else:
        raise ValueError("Unsupported format. Use .png, .tga, .bmp, .jpg, .jpeg, .pgm or .raw")

    if width <= 0 or height <= 0:
        raise ValueError("Invalid dimensions")
    if (width % 8) != 0:
        raise ValueError("Width must be multiple of 8 for VDP1")

    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    tex_path, pal_path = emit_legacy_outputs(out_prefix, pixels, colors)
    header_path, source_path = emit_asset_headers(
        out_prefix, pixels, colors, width, height, palette_index
    )
    return {
        "tex8": tex_path,
        "pal": pal_path,
        "h": header_path,
        "c": source_path,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert indexed8 assets for Saturn VDP1"
    )
    parser.add_argument(
        "--input", required=True, help="Input file (.png, .tga, .bmp, .jpg, .jpeg, .pgm or .raw)"
    )
    parser.add_argument("--out-prefix", required=True, help="Output prefix")
    parser.add_argument("--palette", help="Palette txt file (r g b por linha)")
    parser.add_argument("--width", type=int, help="Width for .raw input")
    parser.add_argument("--height", type=int, help="Height for .raw input")
    parser.add_argument(
        "--resize",
        nargs=2,
        type=int,
        metavar=("WIDTH", "HEIGHT"),
        help="Resize PNG input before conversion",
    )
    parser.add_argument(
        "--palette-index",
        type=int,
        default=0,
        help="Palette index stored in generated metadata",
    )
    args = parser.parse_args()

    in_path = Path(args.input)
    out_prefix = Path(args.out_prefix)
    palette_path = Path(args.palette) if args.palette else None
    result = convert_asset(
        in_path=in_path,
        out_prefix=out_prefix,
        palette_path=palette_path,
        width_arg=args.width,
        height_arg=args.height,
        palette_index=args.palette_index,
        resize_to=tuple(args.resize) if args.resize else None,
    )

    print(
        "OK: "
        f"{result['h']} + {result['c']} "
        f"({in_path.suffix.lower()[1:]} -> {len(result['tex8'].read_bytes())} bytes, "
        f"{len(result['pal'].read_bytes())} bytes palette)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
