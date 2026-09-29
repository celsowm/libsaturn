"""Palette materialization: SFFv2 RGBA quads -> Saturn BGR555 words.

Index 0 is always transparent for sprite pixels (MUGEN semantics; the
value stored in CRAM for it is irrelevant to transparency but is kept
as pure black for deterministic output).
"""
from __future__ import annotations

from . import sff

BLACK = 0x0000
ENTRY_COUNT = 256


class PaletteError(ValueError):
    pass


def materialize(container: sff.SffContainer, node: sff.PaletteNode) -> list[int]:
    """Builds a 256-entry BGR555 list from a palette node's RGBA data."""
    raw = container.palette_data(node)
    quads = len(raw) // 4
    if quads == 0:
        if node.link != 0 or quads == 0 and node.data_size == 0:
            # Linked palette: resolve through the link target.
            if 0 <= node.link < container.palette_count:
                return materialize(container, container.palette_by_index(node.link))
        raise PaletteError("palette node has no data and no link")
    words = [BLACK] * ENTRY_COUNT
    for i in range(min(quads, ENTRY_COUNT)):
        r, g, b, _a = raw[i * 4:i * 4 + 4]
        words[i] = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)
    words[0] = BLACK
    return words


def materialize_index(container: sff.SffContainer, index: int) -> list[int]:
    return materialize(container, container.palette_by_index(index))


def png_embedded(data: bytes) -> list[int] | None:
    """Returns the PNG's own PLTE as BGR555, for mismatch diagnostics."""
    import io
    from PIL import Image
    img = Image.open(io.BytesIO(data))
    if img.mode != "P" or img.palette is None:
        return None
    flat = img.palette.palette
    words = [BLACK] * ENTRY_COUNT
    for i in range(min(len(flat) // 3, ENTRY_COUNT)):
        r, g, b = flat[i * 3:i * 3 + 3]
        words[i] = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)
    return words


def pack_c_array(words: list[int]) -> str:
    return ",".join(f"0x{w:04X}u" for w in words)


def merge(words_list: list[list[int]]) -> tuple[list[int], list[list[int]]]:
    """Merges palettes into one union (up to 256 distinct colors, index 0
    reserved transparent) and returns (merged, remap_tables); remap[i][old]
    gives the merged index for palette i's old index. Raises PaletteError
    when the union exceeds 256 entries."""
    merged = [BLACK]
    index_of = {BLACK: 0}
    remaps: list[list[int]] = []
    for words in words_list:
        table = [0] * ENTRY_COUNT
        for old in range(ENTRY_COUNT):
            color = words[old] & 0x7FFF  # strip VDP2 semi-transparent bit
            if old == 0:
                table[old] = 0
                continue
            slot = index_of.get(color)
            if slot is None:
                if len(merged) >= ENTRY_COUNT:
                    raise PaletteError(
                        f"palette union exceeds {ENTRY_COUNT} colors")
                slot = len(merged)
                merged.append(color)
                index_of[color] = slot
            table[old] = slot
        remaps.append(table)
    while len(merged) < ENTRY_COUNT:
        merged.append(BLACK)
    return merged, remaps
