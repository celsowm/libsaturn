#!/usr/bin/env python3
"""Convert indexed assets to Saturn-friendly .tex8 + .pal files."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable


def rgb888_to_rgb555(r: int, g: int, b: int) -> int:
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return 0x8000 | (r5 << 10) | (g5 << 5) | b5


def parse_palette_txt(path: Path) -> list[int]:
    colors: list[int] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        raw = line.strip()
        if not raw or raw.startswith("#"):
            continue
        parts = raw.replace(",", " ").split()
        if len(parts) < 3:
            raise ValueError(f"Linha invalida de paleta: {raw}")
        r, g, b = (int(parts[0]), int(parts[1]), int(parts[2]))
        if not (0 <= r <= 255 and 0 <= g <= 255 and 0 <= b <= 255):
            raise ValueError(f"RGB fora do range: {raw}")
        colors.append(rgb888_to_rgb555(r, g, b))
        if len(colors) == 256:
            break
    if not colors:
        raise ValueError("Paleta vazia")
    while len(colors) < 256:
        colors.append(0)
    return colors


def parse_pgm(path: Path) -> tuple[bytes, int, int]:
    data = path.read_bytes()
    if not data.startswith(b"P5"):
        raise ValueError("Somente PGM binario P5 e suportado")
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
        raise ValueError("Header PGM invalido")
    width = int(header_tokens[1])
    height = int(header_tokens[2])
    maxv = int(header_tokens[3])
    if maxv > 255:
        raise ValueError("PGM max value > 255 nao suportado")
    pixels = b"\n".join(chunks[payload_start:])
    expected = width * height
    if len(pixels) < expected:
        raise ValueError("Arquivo PGM truncado")
    return pixels[:expected], width, height


def parse_png(path: Path) -> tuple[bytes, int, int, list[int]]:
    try:
        from PIL import Image
    except ModuleNotFoundError as exc:
        raise RuntimeError("Para PNG e necessario instalar pillow: pip install pillow") from exc

    img = Image.open(path)
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


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert indexed8 assets for Saturn VDP1")
    parser.add_argument("--input", required=True, help="Input file (.png, .pgm or .raw)")
    parser.add_argument("--out-prefix", required=True, help="Output prefix")
    parser.add_argument("--palette", help="Palette txt file (r g b por linha)")
    parser.add_argument("--width", type=int, help="Width for .raw input")
    parser.add_argument("--height", type=int, help="Height for .raw input")
    args = parser.parse_args()

    in_path = Path(args.input)
    out_prefix = Path(args.out_prefix)
    out_prefix.parent.mkdir(parents=True, exist_ok=True)

    pixels: bytes
    width: int
    height: int
    colors: list[int]

    suffix = in_path.suffix.lower()
    if suffix == ".png":
        pixels, width, height, colors = parse_png(in_path)
    elif suffix == ".pgm":
        pixels, width, height = parse_pgm(in_path)
        if not args.palette:
            raise ValueError("Entrada .pgm exige --palette")
        colors = parse_palette_txt(Path(args.palette))
    elif suffix == ".raw":
        if not args.width or not args.height:
            raise ValueError("Entrada .raw exige --width e --height")
        width = args.width
        height = args.height
        pixels = in_path.read_bytes()
        if len(pixels) != width * height:
            raise ValueError("Tamanho de .raw nao bate com width*height")
        if not args.palette:
            raise ValueError("Entrada .raw exige --palette")
        colors = parse_palette_txt(Path(args.palette))
    else:
        raise ValueError("Formato nao suportado. Use .png, .pgm ou .raw")

    if width <= 0 or height <= 0:
        raise ValueError("Dimensoes invalidas")
    if (width % 8) != 0:
        raise ValueError("Largura deve ser multipla de 8 para VDP1")

    tex_path = out_prefix.with_suffix(".tex8")
    pal_path = out_prefix.with_suffix(".pal")
    tex_path.write_bytes(pixels)
    write_palette(pal_path, colors)
    print(f"OK: {tex_path} ({len(pixels)} bytes), {pal_path} ({256 * 2} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

