"""Stage DEF-subset composition onto a single Saturn VDP2 plane.

Composes "normal" background layers (sprite + start + horizontal
tiling) onto one WxH indexed canvas, mirroring how the example
previously drew its procedural stage. Parallax/delta are handled by
the example at runtime (VDP2 scroll); this module only builds the
static image.

Layers may use different SFF palette nodes: their palettes are merged
into one union (exact remap, no quantization loss) so the plane needs
a single CRAM bank. The SFF palette node is authoritative for colors,
matching Ikemen GO's renderer (the PNG's own PLTE is ignored).
"""
from __future__ import annotations

from dataclasses import dataclass

from . import sff
from .codecs import decode
from .palettes import materialize_index, merge


class StageError(ValueError):
    pass


@dataclass
class StageLayer:
    sprite: tuple[int, int]           # (group, number)
    start: tuple[int, int] = (0, 0)   # destination in 240-space
    tile_x: bool = False              # repeat horizontally across the plane
    tile_spacing: tuple[int, int] = (0, 0)


@dataclass
class ComposedStage:
    pixels: bytearray
    palette: list[int]
    width: int
    height: int
    warnings: list[str]


def compose(container: sff.SffContainer, layers: list[StageLayer],
            width: int = 320, height: int = 224,
            crop_top: int = 16) -> ComposedStage:
    """Blits layers (in 240-space, cropped by crop_top rows from the top)
    onto an indexed canvas with one merged palette."""
    decoded: list[tuple[StageLayer, sff.SpriteNode, bytes, list[int]]] = []
    warnings: list[str] = []
    seen_palette_indices: set[int] = set()

    for layer in layers:
        node = _find_sprite(container, layer.sprite)
        if node is None:
            raise StageError(f"stage sprite {layer.sprite} not in SFF")
        seen_palette_indices.add(node.palette_index)
        palette = materialize_index(container, node.palette_index)
        pixels = decode(node, container.sprite_data(node))
        decoded.append((layer, node, pixels, palette))

    merged, remaps = merge([entry[3] for entry in decoded])
    if len(seen_palette_indices) > 1:
        warnings.append(
            f"merged {len(seen_palette_indices)} palettes into "
            f"{sum(1 for c in merged if c != 0)} colors")

    canvas = bytearray(width * height)
    for (layer, node, pixels, _palette), remap in zip(decoded, remaps):
        _blit(canvas, width, height, pixels, node.width, node.height,
              layer, crop_top, remap)
    return ComposedStage(canvas, merged, width, height, warnings)


def _find_sprite(container: sff.SffContainer, key: tuple[int, int]):
    for node in container.sprite_nodes():
        if (node.group, node.number) == key:
            return node
    return None


def _blit(canvas, cw: int, ch: int, pixels, sw: int, sh: int,
          layer: StageLayer, crop_top: int, remap) -> None:
    dst_x = layer.start[0]
    dst_y = layer.start[1] - crop_top
    step = sw + layer.tile_spacing[0]
    xs = range(dst_x, cw, step) if layer.tile_x else (dst_x,)
    remapped = bytes(remap[v] for v in pixels)
    for x in xs:
        if x + sw <= 0 or x >= cw:
            continue
        for row in range(sh):
            y = dst_y + row
            if not 0 <= y < ch:
                continue
            base = row * sw
            for col in range(sw):
                px = x + col
                if 0 <= px < cw:
                    canvas[y * cw + px] = remapped[base + col]
