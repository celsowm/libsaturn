"""Facade textures for city_walk blocks, baked from the source model.

A block face (a wall or a low roof) is a flat rectangle; the source geometry
standing on it (windows, frames, balconies, signs, doors) is projected onto it
orthographically along the face normal and rasterised into a texture. The VDP1
draws a textured face as a distorted sprite: texel (0, 0) lands on the face's
corner A, (w, 0) on B, (w, h) on C and (0, h) on D. Baking in the face's own
A->B / A->D frame therefore gives the right orientation for every wall, with no
case analysis and no mirroring.

Only geometry inside a thin slab around the face counts (``INSET`` behind it,
``OUTSET`` in front), so a building across the street never lands on this one.
Textured source triangles keep their real glTF UVs: each supersample interpolates
UV barycentrically and samples the source baseColorTexture before lighting.
That preserves windows, brick, signs and facade trim instead of reducing each
triangle to one centroid colour.

Texels are supersampled 2x2 and averaged: a texture drawn at a quarter of its
size on screen would otherwise sparkle (the VDP1 samples, it does not filter).
"""

from __future__ import annotations

import math

import numpy as np

INSET = 1.5  # units behind the face that still belong to it (recessed windows)
OUTSET = 3.0  # units in front (balconies, awnings, signs)
SUPERSAMPLE = 2


class FacadeBaker:
    def __init__(self, pos: np.ndarray, colours: np.ndarray, uv: np.ndarray | None = None,
                 material_ids: np.ndarray | None = None, materials=None,
                 normals: np.ndarray | None = None, light=None,
                 ambient: float = 1.0, diffuse: float = 0.0):
        """Source triangles plus glTF material state for per-pixel UV sampling."""
        self.pos = pos
        self.colours = colours
        self.uv = uv
        self.material_ids = material_ids
        self.materials = materials
        self.lo = pos.min(axis=1)
        self.hi = pos.max(axis=1)
        self.light_factor = np.ones(len(pos), dtype=np.float64)
        if normals is not None and light is not None:
            l_vec = np.asarray(light, dtype=np.float64)
            l_vec /= max(float(np.linalg.norm(l_vec)), 1e-12)
            self.light_factor = ambient + diffuse * np.clip(
                np.asarray(normals, dtype=np.float64) @ l_vec, 0.0, 1.0)

    def _source_rgb(self, tri_index: int, w0: np.ndarray, w1: np.ndarray,
                    w2: np.ndarray) -> np.ndarray | None:
        if self.materials is None or self.material_ids is None:
            return None
        mi = int(self.material_ids[tri_index])
        if mi < 0 or mi >= len(self.materials):
            return None
        mat = self.materials[mi]
        from . import chunking as ch

        factor = np.asarray(mat.base_linear, dtype=np.float64)
        if mat.image is None:
            linear = np.broadcast_to(factor, w0.shape + (3,)).copy()
        else:
            if self.uv is None:
                return None
            tuv = self.uv[tri_index]
            u = np.mod(w0 * tuv[0, 0] + w1 * tuv[1, 0] + w2 * tuv[2, 0], 1.0)
            v = np.mod(w0 * tuv[0, 1] + w1 * tuv[1, 1] + w2 * tuv[2, 1], 1.0)
            ih, iw = mat.image.shape[:2]
            ix = np.clip((u * iw).astype(np.int64), 0, iw - 1)
            iy = np.clip((v * ih).astype(np.int64), 0, ih - 1)
            texel = mat.image[iy, ix, :3].astype(np.float64) / 255.0
            linear = ch._srgb_decode(texel) * factor

        if not mat.unlit:
            linear *= self.light_factor[tri_index]
        if mat.emissive is not None:
            linear += np.asarray(mat.emissive, dtype=np.float64)
        return np.clip(np.rint(ch._srgb_encode(np.clip(linear, 0.0, 1.0)) * 255.0),
                       0, 255).astype(np.uint8)

    def bake(self, corners: np.ndarray, width: int, height: int,
             fallback: np.ndarray) -> np.ndarray:
        """(height, width, 3) uint8 texture for the face ``corners`` (4, 3) in
        runtime order A, B, C, D (a rectangle)."""
        a, b, d = corners[0], corners[1], corners[3]
        eu, ev = b - a, d - a
        lu, lv = float(np.linalg.norm(eu)), float(np.linalg.norm(ev))
        if lu < 1e-9 or lv < 1e-9:
            out = np.empty((height, width, 3), dtype=np.uint8)
            out[:] = fallback
            return out
        u_axis, v_axis = eu / lu, ev / lv
        normal = np.cross(u_axis, v_axis)
        # Outward: the runtime winding's normal is cross(D - A, B - A).
        if float(np.cross(d - a, b - a) @ normal) < 0.0:
            normal = -normal
        sw, sh = width * SUPERSAMPLE, height * SUPERSAMPLE
        img = np.empty((sh, sw, 3), dtype=np.float64)
        img[:] = fallback
        depth = np.full((sh, sw), -np.inf)

        # Candidate triangles: bounding box of the face grown by the slab.
        grow = np.abs(normal) * max(INSET, OUTSET) + 1e-3
        box_lo = corners.min(axis=0) - grow
        box_hi = corners.max(axis=0) + grow
        sel = np.all((self.hi >= box_lo) & (self.lo <= box_hi), axis=1)
        idx = np.nonzero(sel)[0]
        if len(idx):
            rel = self.pos[idx] - a
            us = (rel @ u_axis) / lu * sw
            vs = (rel @ v_axis) / lv * sh
            ds = rel @ normal  # > 0: in front of the face
            keep = (ds.max(axis=1) >= -INSET) & (ds.min(axis=1) <= OUTSET)
            for t in np.nonzero(keep)[0].tolist():
                x, y, dd = us[t], vs[t], ds[t]
                x0, x1 = max(int(math.floor(x.min())), 0), min(int(math.ceil(x.max())), sw)
                y0, y1 = max(int(math.floor(y.min())), 0), min(int(math.ceil(y.max())), sh)
                if x0 >= x1 or y0 >= y1:
                    continue
                den = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
                if abs(den) < 1e-12:
                    continue  # seen edge-on from this face: a wall of a box, not its front
                gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
                w0 = ((y[1] - y[2]) * (gx - x[2]) + (x[2] - x[1]) * (gy - y[2])) / den
                w1 = ((y[2] - y[0]) * (gx - x[2]) + (x[0] - x[2]) * (gy - y[2])) / den
                w2 = 1.0 - w0 - w1
                inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
                z = w0 * dd[0] + w1 * dd[1] + w2 * dd[2]
                inside &= (z >= -INSET) & (z <= OUTSET)
                zb = depth[y0:y1, x0:x1]
                win = inside & (z > zb)
                if not win.any():
                    continue
                zb[win] = z[win]
                src = self._source_rgb(int(idx[t]), w0, w1, w2)
                patch = img[y0:y1, x0:x1]
                if src is None:
                    patch[win] = self.colours[idx[t]]
                else:
                    patch[win] = src[win]
        img = img.reshape(height, SUPERSAMPLE, width, SUPERSAMPLE, 3).mean(axis=(1, 3))
        return np.clip(np.rint(img), 0, 255).astype(np.uint8)


def texture_size(corners: np.ndarray, texels_per_unit: float,
                 max_width: int = 504, max_height: int = 255) -> tuple[int, int]:
    """VDP1 size for a face: width a multiple of 8 (at least 8), height >= 1."""
    lu = float(np.linalg.norm(corners[1] - corners[0]))
    lv = float(np.linalg.norm(corners[3] - corners[0]))
    w = int(math.ceil(lu * texels_per_unit / 8.0)) * 8
    h = int(math.ceil(lv * texels_per_unit))
    return max(8, min(w, max_width)), max(1, min(h, max_height))


def split_face(corners: np.ndarray, tile_units: float | None,
               vertical_tile_units: float | None = None):
    """Cut rectangle A, B, C, D into independently textured pieces.

    Horizontal edges use tile_units and vertical edges use
    vertical_tile_units; None leaves that axis unsplit. Pieces keep A, B, C, D
    order.

    Short horizontal pieces reduce affine VDP1 texture warp. Vertical cutting
    is also required near the camera: even with a level view, a tall wall can
    project a top or bottom corner beyond the renderer's bounded off-screen
    window. In that case an ordinary distorted sprite cannot be submitted
    safely and the material falls back to a solid polygon. Smaller separately
    baked patches confine that fallback to the actually unsafe part.
    """

    a, b, d = corners[0], corners[1], corners[3]

    def pieces(edge):
        length = float(np.linalg.norm(edge))
        vertical = abs(float(edge[1])) > 0.5 * length
        tile = vertical_tile_units if vertical else tile_units
        if tile is None or length <= 0.0:
            return 1
        return max(1, int(math.ceil(length / tile - 1e-9)))

    nu, nv = pieces(b - a), pieces(d - a)
    out = []
    for j in range(nv):
        for i in range(nu):
            def at(s, t):
                return a + (b - a) * s + (d - a) * t
            out.append(np.array([at(i / nu, j / nv), at((i + 1) / nu, j / nv),
                                 at((i + 1) / nu, (j + 1) / nv), at(i / nu, (j + 1) / nv)]))
    return out


def shade_triangles(rgb: np.ndarray, normals: np.ndarray, light, ambient: float,
                    diffuse: float) -> np.ndarray:
    """Flat Lambert per triangle in linear light, the chunker's light model
    without its shade-level quantisation: baked texels keep the full ramp."""
    from . import chunking as ch

    l_vec = np.asarray(light, dtype=np.float64)
    l_vec = l_vec / np.linalg.norm(l_vec)
    k = ambient + diffuse * np.clip(normals @ l_vec, 0.0, 1.0)
    lin = ch._srgb_decode(rgb.astype(np.float64) / 255.0) * k[:, None]
    return np.clip(np.rint(ch._srgb_encode(np.clip(lin, 0, 1)) * 255.0), 0, 255).astype(np.uint8)


def build_palette(samples: np.ndarray, count: int = 255) -> np.ndarray:
    """Weighted median cut of texel colours into <= ``count`` sRGB entries."""
    from . import chunking as ch

    colours, weights = np.unique(samples.reshape(-1, 3), axis=0, return_counts=True)
    return ch.median_cut(colours, weights.astype(np.float64), count)
