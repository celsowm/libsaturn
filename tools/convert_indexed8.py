#!/usr/bin/env python3
"""Convert indexed assets to Saturn-friendly outputs.

The converter keeps the pixel layout linear because the current VDP1 upload
path consumes a flat indexed8 stream. In addition to the legacy binary outputs,
it now emits a C header/source pair so assets can be compiled into the example
binary without runtime file loading.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Iterable, Sequence

sys.path.insert(0, str(Path(__file__).parent))
from saturn_asset_common import (
    asset_header_guard,
    asset_symbol_prefix,
    format_byte_array,
    format_word_array,
    rgb888_to_rgb555,
    sanitize_identifier,
)


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
    path: Path,
    resize_to: tuple[int, int] | None = None,
    reserve_index0: bool = False,
    alpha_threshold: int | None = None,
    luma_threshold: int | None = None,
    gain: float = 1.0,
) -> tuple[bytes, int, int, list[int]]:
    """Quantize an image to an indexed8 Saturn asset.

    Quantization runs on RGB, never on RGBA.  Pillow's adaptive quantizer
    collapses an RGBA source to a fraction of the colours it is asked for --
    the 128x128 terrain of examples/infinite_explorer came out with 58 of the
    requested 256, which is visible as banding once the Mode-7 floor magnifies
    it.  The same image converted to RGB first quantizes to the full 256.

    reserve_index0 keeps palette entry 0 out of the quantized set, because
    both the VDP1 (sprites) and the VDP2 (bitmap backgrounds with transparent
    code enabled) read colour index 0 as TRANSPARENT.  Without it a quantizer
    is free to assign index 0 to an ordinary opaque colour -- and then every
    pixel of that colour punches a hole in the image.
    """
    try:
        from PIL import Image
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "Para PNG e necessario instalar pillow: pip install pillow"
        ) from exc

    img = Image.open(path)
    if img.mode != "RGBA":
        img = img.convert("RGBA")
    if resize_to is not None:
        resize_width, resize_height = resize_to
        if resize_width <= 0 or resize_height <= 0:
            raise ValueError("Invalid resize dimensions")
        resample = getattr(getattr(Image, "Resampling", Image), "LANCZOS")
        img = img.resize((resize_width, resize_height), resample=resample)

    width, height = img.size
    alpha = img.getchannel("A")
    rgb = img.convert("RGB")

    # Which source pixels must end up transparent (index 0). Measured BEFORE
    # the gain, so raising the gain to make a dark subject readable does not
    # also drag its backdrop above the cutout threshold.
    transparent = bytearray(width * height)
    if alpha_threshold is not None:
        for i, a in enumerate(alpha.tobytes()):
            if a < alpha_threshold:
                transparent[i] = 1
    if luma_threshold is not None:
        for i, l in enumerate(rgb.convert("L").tobytes()):
            if l <= luma_threshold:
                transparent[i] = 1

    if gain != 1.0:
        rgb = apply_gain(rgb, gain)
    has_transparent = any(transparent)
    reserve = reserve_index0 or has_transparent

    # Quantize without dithering: a 128x128 texture magnified by the VDP2
    # turns Floyd-Steinberg noise into visible speckle.
    dither = getattr(getattr(Image, "Dither", Image), "NONE")
    quantized = rgb.quantize(colors=255 if reserve else 256, dither=dither)

    raw = quantized.tobytes()
    pal = quantized.getpalette() or []
    colors: list[int] = []
    for i in range(0, min(len(pal), 256 * 3), 3):
        colors.append(rgb888_to_rgb555(pal[i], pal[i + 1], pal[i + 2]))

    if reserve:
        # Shift every index up by one so nothing lands on 0, then let 0 be
        # the transparent entry.
        pixels = bytes(
            0 if transparent[i] else min(255, value + 1) for i, value in enumerate(raw)
        )
        colors = [0x0000] + colors[:255]
    else:
        pixels = raw

    while len(colors) < 256:
        colors.append(0)
    return pixels, width, height, colors[:256]


def apply_gain(img, gain: float):
    """Scale RGB brightness, clamped.

    NASA release renders are exposed for a black background and quantize to a
    near-black sprite once they are shrunk to 64x64; a gain makes the subject
    readable on a TV instead of a dark smudge.
    """
    from PIL import Image

    lut = [min(255, int(value * gain + 0.5)) for value in range(256)]
    return Image.merge("RGB", [chan.point(lut) for chan in img.split()])


def make_darkest_palette_entry_transparent(
    pixels: bytes, colors: list[int]
) -> tuple[bytes, list[int]]:
    """Move the darkest quantized colour to index zero for VDP1 sprites.

    NASA concept art normally uses a black backdrop.  VDP1 treats colour index
    zero as transparent, so swapping that palette entry lets the artwork be
    used as a proper sprite instead of as an opaque black rectangle.
    """
    def brightness(color: int) -> int:
        return (color & 0x1F) + ((color >> 5) & 0x1F) + ((color >> 10) & 0x1F)

    darkest = min(range(len(colors)), key=lambda index: brightness(colors[index]))
    if darkest == 0:
        colors[0] = 0
        return pixels, colors

    swapped = bytearray(pixels)
    for index, value in enumerate(swapped):
        if value == darkest:
            swapped[index] = 0
        elif value == 0:
            swapped[index] = darkest
    colors[darkest] = colors[0]
    colors[0] = 0
    return bytes(swapped), colors


def write_palette(path: Path, colors: Iterable[int]) -> None:
    out = bytearray()
    for c in colors:
        out.extend(((c >> 8) & 0xFF, c & 0xFF))
    path.write_bytes(bytes(out))


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
                "#ifndef SAT_INDEXED8_ASSET_T_DEFINED",
                "#define SAT_INDEXED8_ASSET_T_DEFINED",
                "typedef struct sat_indexed8_asset {",
                "    const uint8_t* pixels;",
                "    const uint16_t* palette;",
                "    uint16_t width;",
                "    uint16_t height;",
                "    uint16_t palette_index;",
                "    uint32_t pixel_count;",
                "    uint32_t palette_count;",
                "} sat_indexed8_asset_t;",
                "#endif",
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
    transparent_dark: bool,
    reserve_index0: bool = False,
    alpha_threshold: int | None = None,
    luma_threshold: int | None = None,
    gain: float = 1.0,
) -> dict[str, Path]:
    suffix = in_path.suffix.lower()
    pixels: bytes
    width: int
    height: int
    colors: list[int]

    if suffix in {".png", ".tga", ".bmp", ".jpg", ".jpeg"}:
        pixels, width, height, colors = parse_png(
            in_path,
            resize_to=resize_to,
            reserve_index0=reserve_index0,
            alpha_threshold=alpha_threshold,
            luma_threshold=luma_threshold,
            gain=gain,
        )
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

    if transparent_dark:
        pixels, colors = make_darkest_palette_entry_transparent(pixels, colors)

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
    parser.add_argument(
        "--transparent-dark",
        action="store_true",
        help="Make the darkest quantized colour index zero (VDP1 transparency)",
    )
    parser.add_argument(
        "--reserve-index0",
        action="store_true",
        help=(
            "Quantize to 255 colours and keep index 0 unused, so the hardware's "
            "transparent colour index never collides with an opaque colour"
        ),
    )
    parser.add_argument(
        "--transparent-alpha",
        type=int,
        metavar="N",
        help="Source pixels with alpha < N become index 0 (implies --reserve-index0)",
    )
    parser.add_argument(
        "--transparent-luma",
        type=int,
        metavar="N",
        help=(
            "Source pixels with luma <= N become index 0 (implies --reserve-index0). "
            "Use for artwork on a black backdrop, where --transparent-dark only "
            "catches the single darkest palette entry and leaves the rest opaque."
        ),
    )
    parser.add_argument(
        "--gain",
        type=float,
        default=1.0,
        help="Multiply RGB brightness before quantizing (1.0 = unchanged)",
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
        transparent_dark=args.transparent_dark,
        reserve_index0=args.reserve_index0,
        alpha_threshold=args.transparent_alpha,
        luma_threshold=args.transparent_luma,
        gain=args.gain,
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
