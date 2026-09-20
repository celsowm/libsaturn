#!/usr/bin/env python3
"""Solid per-face colors with baked flat lighting (host-only).

Palette-swatch models -- every triangle's UVs sample one texel of a color
atlas, as low-poly exports usually do -- need no textures on the Saturn:
each face is one solid color, drawable as a VDP1 polygon. That also frees
the vertex copies exporters split only because of UVs, so the mesh welds
down to its real surface points (fewer vertices to decode and project), and
the face color can carry lighting.

Lighting is Lambert under one fixed light, evaluated for every baked
animation frame. Albedo is lit in linear space and re-encoded as sRGB, which
is what a glTF viewer shows; scaling the gamma-encoded swatch directly
darkens every mid-tone. Each (base color, light level) pair is one entry of
a shade palette of at most 256. Entry 0 is reserved: a color-bank code of 0
is transparent on the 8bpp high-resolution frame buffer, so the same indices
serve as RGB555 lookups now and as color-bank codes there.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

from .gltf import GltfError, linear_to_srgb, srgb_to_linear
from .model import SourceModel

AUTO_UNIFORM_FRACTION = 0.99
MAX_LEVELS = 32
MIN_LEVELS = 3
PALETTE_SIZE = 256


def _texel(tex, u: float, v: float) -> tuple[int, int, int]:
    """Nearest texel, addressed exactly as import_model.bake_face_rgba does."""
    u = min(max(u, 0.0), 1.0)
    v = min(max(v, 0.0), 1.0)
    ix = min(max(int(math.floor(u * (tex.width - 1) + 0.5)), 0), tex.width - 1)
    iy = min(max(int(math.floor((1.0 - v) * (tex.height - 1) + 0.5)), 0), tex.height - 1)
    r, g, b, _a = tex.pixels_rgba[iy * tex.width + ix]
    return (int(r), int(g), int(b))


@dataclass
class FaceColorAnalysis:
    colors: list[tuple[int, int, int]] = field(default_factory=list)  # per triangle
    uniform: int = 0
    non_uniform: int = 0
    untextured: int = 0

    @property
    def uniform_fraction(self) -> float:
        return self.uniform / max(1, len(self.colors))

    @property
    def distinct_colors(self) -> int:
        return len(set(self.colors))


def analyze(model: SourceModel) -> FaceColorAnalysis:
    """One color per triangle: its texel when the three corners and the
    centroid all sample the same one (uniform), their mean otherwise."""
    out = FaceColorAnalysis()
    for tri, mt in zip(model.triangles, model.tri_materials):
        mat = model.materials[mt] if mt < len(model.materials) else {}
        if "texture" not in mat or not model.uvs:
            if "rgb" in mat:
                out.uniform += 1
                out.colors.append(tuple(int(v) for v in mat["rgb"]))
                continue
            out.untextured += 1
            out.colors.append((255, 255, 255))
            continue
        tex = model.textures[mat["texture"]]
        uvs = [model.uvs[i] for i in tri]
        cu = sum(u for u, _ in uvs) / 3.0
        cv = sum(v for _, v in uvs) / 3.0
        samples = [_texel(tex, u, v) for (u, v) in uvs] + [_texel(tex, cu, cv)]
        if all(s == samples[0] for s in samples):
            out.uniform += 1
            out.colors.append(samples[0])
        else:
            out.non_uniform += 1
            out.colors.append(tuple(
                int(round(sum(s[k] for s in samples) / len(samples))) for k in range(3)))
    return out


def levels_for(color_count: int) -> int:
    """Light levels per base color that fit the reserved-zero palette."""
    if color_count < 1:
        return MAX_LEVELS
    return min(MAX_LEVELS, (PALETTE_SIZE - 1) // color_count)


def decide(mode: str, analysis: FaceColorAnalysis) -> tuple[bool, str]:
    """Whether solid-color mode applies, and why not when it does not."""
    if mode == "off":
        return False, "disabled"
    if analysis.untextured:
        return False, f"{analysis.untextured} faces have no baseColorTexture to sample"
    if levels_for(analysis.distinct_colors) < MIN_LEVELS:
        return False, (f"{analysis.distinct_colors} distinct face colors leave fewer than "
                       f"{MIN_LEVELS} light levels in a {PALETTE_SIZE}-entry palette")
    if mode == "auto" and analysis.uniform_fraction < AUTO_UNIFORM_FRACTION:
        return False, (f"only {analysis.uniform_fraction:.1%} of faces sample one texel "
                       f"(auto needs {AUTO_UNIFORM_FRACTION:.0%})")
    return True, "ok"


def flatten(model: SourceModel, analysis: FaceColorAnalysis):
    """Solid-color source model: one material per distinct color, UVs and
    normals dropped, vertex copies welded by position + skin.

    Returns ``(welded_model, base_colors, welded_vertex_count)``. Colors are
    numbered by first appearance, so output is deterministic. Material ids
    carry the colors, so the simplifier's material locks keep color regions
    apart exactly as they kept textured materials apart.
    """
    from .simplification import _weld_exact_duplicates

    index: dict[tuple[int, int, int], int] = {}
    tri_color: list[int] = []
    for color in analysis.colors:
        if color not in index:
            index[color] = len(index)
        tri_color.append(index[color])
    base_colors = list(index)

    flat = SourceModel()
    flat.vertices = list(model.vertices)
    flat.normals = None
    flat.uvs = [(0.0, 0.0)] * len(model.vertices)
    flat.triangles = list(model.triangles)
    flat.tri_materials = tri_color
    flat.materials = [{"name": f"color_{i}", "rgb": c} for i, c in enumerate(base_colors)]
    flat.textures = []
    flat.joints = list(model.joints)
    flat.weights = list(model.weights)
    flat.nodes = model.nodes
    flat.skins = model.skins
    flat.clips = model.clips
    flat.mesh_node = model.mesh_node
    flat.skin_index = model.skin_index
    welded, _remap, welded_count, _dropped = _weld_exact_duplicates(flat)
    welded.glb = model.glb
    welded.json_doc = model.json_doc
    return welded, base_colors, welded_count


def parse_light_dir(text) -> tuple[float, float, float]:
    if isinstance(text, (tuple, list)):
        parts = [float(v) for v in text]
    else:
        parts = [float(v) for v in str(text).split(",")]
    if len(parts) != 3:
        raise GltfError(f"light direction needs 3 components (got {text!r})")
    length = math.sqrt(sum(v * v for v in parts))
    if length <= 0.0:
        raise GltfError("light direction must not be zero")
    return (parts[0] / length, parts[1] / length, parts[2] / length)


def bake_shades(
    frames: list[list[tuple[float, float, float]]],
    triangles: list[tuple[int, int, int]],
    tri_colors: list[int],
    levels: int,
    light_dir: tuple[float, float, float],
    clockwise_front: bool,
) -> list[int]:
    """Shade index per face per frame (frame-major).

    ``frames`` are asset-space poses (after flips). Source triangles face
    front counter-clockwise unless ``clockwise_front`` -- the importer's
    final winding decision, which already accounts for mirror flips.
    """
    lx, ly, lz = light_dir
    sign = -1.0 if clockwise_front else 1.0
    top = levels - 1
    out: list[int] = []
    for frame in frames:
        for (a, b, c), color in zip(triangles, tri_colors):
            ax, ay, az = frame[a]
            bx, by, bz = frame[b]
            cx, cy, cz = frame[c]
            ux, uy, uz = bx - ax, by - ay, bz - az
            vx, vy, vz = cx - ax, cy - ay, cz - az
            nx = (uy * vz - uz * vy) * sign
            ny = (uz * vx - ux * vz) * sign
            nz = (ux * vy - uy * vx) * sign
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            d = (nx * lx + ny * ly + nz * lz) / length if length > 0.0 else 0.0
            k = int(round(max(0.0, min(1.0, d)) * top)) if top > 0 else 0
            out.append(1 + color * levels + k)
    return out


GOURAUD_REFERENCE_ALBEDO = 0.75


def bake_vertex_gouraud(
    frames: list[list[tuple[float, float, float]]],
    triangles: list[tuple[int, int, int]],
    vertex_count: int,
    light_dir: tuple[float, float, float],
    ambient: float,
    diffuse: float,
    clockwise_front: bool,
) -> list[int]:
    """White-Gouraud level (0..31, 16 = no change) per vertex per frame.

    Vertex normals are the area-weighted sum of the adjacent face normals in
    each pose, which blends light across every edge -- the Gouraud look. The
    runtime draws each face at its base shade (the middle light level, see
    face_base_shades) and the VDP1 adds the interpolated corner corrections.
    A correction is the sRGB step, in 5-bit levels, between a reference
    albedo lit at the vertex and lit at the middle level, so one grey table
    serves every face color."""
    lx, ly, lz = light_dir
    sign = -1.0 if clockwise_front else 1.0
    ref = GOURAUD_REFERENCE_ALBEDO
    mid = linear_to_srgb(ref * (ambient + diffuse * 0.5))
    out: list[int] = []
    for frame in frames:
        acc = [[0.0, 0.0, 0.0] for _ in range(vertex_count)]
        for (a, b, c) in triangles:
            ax, ay, az = frame[a]
            bx, by, bz = frame[b]
            cx, cy, cz = frame[c]
            ux, uy, uz = bx - ax, by - ay, bz - az
            vx, vy, vz = cx - ax, cy - ay, cz - az
            nx = (uy * vz - uz * vy) * sign
            ny = (uz * vx - ux * vz) * sign
            nz = (ux * vy - uy * vx) * sign
            for v in (a, b, c):
                acc[v][0] += nx
                acc[v][1] += ny
                acc[v][2] += nz
        for nx, ny, nz in acc:
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            d = (nx * lx + ny * ly + nz * lz) / length if length > 0.0 else 0.0
            light = ambient + diffuse * max(0.0, d)
            delta = int(round(31.0 * (linear_to_srgb(ref * light) - mid)))
            out.append(max(0, min(31, 16 + delta)))
    return out


def face_base_shades(tri_colors: list[int], levels: int) -> list[int]:
    """Each face's shade-palette index at the middle light level."""
    mid = int(round((levels - 1) / 2.0))
    return [1 + color * levels + mid for color in tri_colors]


def shade_palette(
    base_colors: list[tuple[int, int, int]],
    levels: int,
    ambient: float,
    diffuse: float,
) -> list[int]:
    """RGB555 entries: [0] reserved, then ``levels`` per base color, level k
    lit at ambient + diffuse * k / (levels - 1), in linear space."""
    palette = [0x8000]
    for (r, g, b) in base_colors:
        lin = [srgb_to_linear(ch / 255.0) for ch in (r, g, b)]
        for k in range(levels):
            light = ambient + diffuse * (k / (levels - 1) if levels > 1 else 1.0)
            r5, g5, b5 = (min(31, int(round(linear_to_srgb(ch * light) * 31.0))) for ch in lin)
            palette.append(0x8000 | (b5 << 10) | (g5 << 5) | r5)
    if len(palette) > PALETTE_SIZE:
        raise GltfError(f"shade palette needs {len(palette)} entries (max {PALETTE_SIZE})")
    return palette
