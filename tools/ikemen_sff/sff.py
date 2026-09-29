"""SFF v2 container parsing (no pixel decoding here; see codecs.py).

Layout follows Ikemen GO's reader (src/image.go, SffHeader.Read /
readHeaderV2 / loadPalettes), which is the authoritative consumer of the
screenpack files this pipeline targets.

Header (64 bytes):
  0-11   "ElecbyteSpr\\0"
  12-15  version bytes (lo3..., verhi last; verhi==2 selects v2)
  16-19  reserved
  20-35  reserved x4
  36-39  first sprite node offset
  40-43  number of sprites
  44-47  first palette node offset
  48-51  number of palettes
  52-55  lofs (sprite/palette data base offset)
  56-59  reserved
  60-63  tofs (second data base, selected by node flag bit 0)

Sprite node (28 bytes, sequential): group u16, number u16, w u16, h u16,
xoff i16, yoff i16, palette link u16, format u8, color depth u8,
data offset u32, data length u32, palette index u16, flags u16.

Palette node (16 bytes): group u16, number u16, colors u16, link u16,
data offset u32, data size u32 (RGBA quads, 4 bytes per entry).
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct

SIGNATURE = b"ElecbyteSpr\x00"
HEADER_SIZE = 64
SPRITE_NODE_SIZE = 28
PALETTE_NODE_SIZE = 16

FORMAT_RAW = 0
FORMAT_RLE8 = 2
FORMAT_RLE5 = 3
FORMAT_LZ5 = 4
FORMAT_PNG_INDEXED = 10
FORMAT_PNG_RGBA = 11
FORMAT_PNG_RGBA2 = 12


class SffFormatError(ValueError):
    """Raised when a file is not a supported SFF v2 container."""


@dataclass(frozen=True)
class SpriteNode:
    group: int
    number: int
    width: int
    height: int
    xoff: int
    yoff: int
    palette_link: int
    fmt: int
    coldepth: int
    data_offset: int
    data_length: int
    palette_index: int
    flags: int


@dataclass(frozen=True)
class PaletteNode:
    group: int
    number: int
    colors: int
    link: int
    data_offset: int
    data_size: int


@dataclass
class SffContainer:
    data: bytes
    first_sprite_offset: int
    sprite_count: int
    first_palette_offset: int
    palette_count: int
    lofs: int
    tofs: int

    def sprite_nodes(self):
        for i in range(self.sprite_count):
            base = self.first_sprite_offset + i * SPRITE_NODE_SIZE
            (group, number, w, h, xoff, yoff, link, fmt, depth,
             ofs, length, palidx, flags) = struct.unpack_from(
                "<HHHHhhHBBIIHH", self.data, base)
            yield SpriteNode(group, number, w, h, xoff, yoff, link, fmt,
                             depth, ofs, length, palidx, flags)

    def sprite_data(self, node: SpriteNode) -> bytes:
        base = self.tofs if (node.flags & 1) else self.lofs
        start = base + node.data_offset
        if node.data_length == 0:
            return b""
        return self.data[start:start + node.data_length]

    def palette_nodes(self):
        for i in range(self.palette_count):
            base = self.first_palette_offset + i * PALETTE_NODE_SIZE
            group, number, colors, link, ofs, size = struct.unpack_from(
                "<HHHHII", self.data, base)
            yield PaletteNode(group, number, colors, link, ofs, size)

    def palette_data(self, node: PaletteNode) -> bytes:
        if node.data_size == 0:
            return b""
        start = self.lofs + node.data_offset
        return self.data[start:start + node.data_size]

    def palette_by_index(self, index: int) -> PaletteNode:
        for i, node in enumerate(self.palette_nodes()):
            if i == index:
                return node
        raise SffFormatError(f"palette index {index} out of range")


def load(path: Path) -> SffContainer:
    data = Path(path).read_bytes()
    if len(data) < HEADER_SIZE or not data.startswith(SIGNATURE):
        raise SffFormatError(f"{path}: not an SFF file")
    verhi = data[15]
    if verhi != 2:
        raise SffFormatError(
            f"{path}: SFF v{verhi} not supported (v2 only)")
    (first_sprite, sprite_count, first_palette, palette_count,
     lofs, _reserved, tofs) = struct.unpack_from("<7I", data, 36)
    return SffContainer(data, first_sprite, sprite_count,
                         first_palette, palette_count, lofs, tofs)
