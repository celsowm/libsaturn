"""Offline nearest-surface texture transfer for atlas-heavy reduced meshes.

Sampling the original surface rather than interpolating surviving UVs keeps
edge collapses from drawing arbitrary paths across disconnected atlas islands.
Only NumPy is required, as in the existing area sampler.
"""
from __future__ import annotations

import numpy as np

from .texture_bake import _linear_image


def closest_surface(points, triangles):
    """Return squared distance and barycentrics for every point/triangle pair."""
    a, b, c = triangles[:, 0], triangles[:, 1], triangles[:, 2]
    ab, ac = b - a, c - a
    ap = points[:, None] - a
    d00, d01, d11 = (ab * ab).sum(1), (ab * ac).sum(1), (ac * ac).sum(1)
    d20, d21 = (ap * ab).sum(2), (ap * ac).sum(2)
    den = d00 * d11 - d01 * d01
    safe = np.where(den > 1e-24, den, 1.0)
    v = (d11 * d20 - d01 * d21) / safe
    w = (d00 * d21 - d01 * d20) / safe
    bary = np.stack((1 - v - w, v, w), axis=2)
    projected = a + v[..., None] * ab + w[..., None] * ac
    dist = ((points[:, None] - projected) ** 2).sum(2)
    dist[(bary < 0).any(2) | (den[None] <= 1e-24)] = np.inf
    # Closest point can lie on any edge, including a degenerate triangle.
    for i, j in ((0, 1), (1, 2), (2, 0)):
        start, edge = triangles[:, i], triangles[:, j] - triangles[:, i]
        length = (edge * edge).sum(1)
        t = np.clip(((points[:, None] - start) * edge).sum(2)
                    / np.maximum(length, 1e-30), 0, 1)
        q = start + t[..., None] * edge
        edge_dist = ((points[:, None] - q) ** 2).sum(2)
        better = edge_dist < dist
        eb = np.zeros_like(bary)
        eb[..., i], eb[..., j] = 1 - t, t
        bary[better] = eb[better]
        dist[better] = edge_dist[better]
    return dist, bary


class SurfaceBaker:
    def __init__(self, source, *, exposure=1.0, studio=False,
                 light_dir=(-0.5, 0.6, 0.8), ambient=0.65, diffuse=0.35,
                 reverse_winding=False):
        self.source = source
        self.positions = np.asarray(source.vertices, dtype=float)[np.asarray(source.triangles)]
        self.uvs = np.asarray(source.uvs, dtype=float)[np.asarray(source.triangles)]
        self.materials = np.asarray(source.tri_materials)
        self.lo, self.hi = self.positions.min(1), self.positions.max(1)
        self.radius = max(float(np.linalg.norm(self.hi.max(0) - self.lo.min(0))) * 0.01, 1e-6)
        self.normals = np.cross(self.positions[:, 1] - self.positions[:, 0],
                                self.positions[:, 2] - self.positions[:, 0])
        self.normals /= np.maximum(np.linalg.norm(self.normals, axis=1)[:, None], 1e-30)
        self.exposure, self.studio = exposure, studio
        self.light = np.asarray(light_dir, dtype=float)
        self.light /= max(float(np.linalg.norm(self.light)), 1e-30)
        self.ambient, self.diffuse = ambient, diffuse
        self.reverse_winding = reverse_winding

    def _transfer(self, points, quad, material):
        # AABB rejection is conservative. Expand until every selected point is
        # nearer than the expansion: any excluded triangle must then be farther.
        radius = self.radius
        normal = np.cross(quad[3] - quad[0], quad[1] - quad[0])
        normal /= max(float(np.linalg.norm(normal)), 1e-30)
        if self.reverse_winding:
            normal = -normal
        eligible = (self.materials == material) & ((self.normals @ normal) >= 0)
        if not eligible.any():
            eligible = self.materials == material
        while True:
            mask = eligible & (self.hi >= quad.min(0) - radius).all(1) & (self.lo <= quad.max(0) + radius).all(1)
            candidates = np.flatnonzero(mask)
            if not len(candidates):
                radius *= 2
                continue
            chosen, weights, distances = [], [], []
            for first in range(0, len(points), 64):
                dist, bary = closest_surface(points[first:first + 64], self.positions[candidates])
                best = dist.argmin(1)
                rows = np.arange(len(best))
                chosen.append(candidates[best])
                weights.append(bary[rows, best])
                distances.append(dist[rows, best])
            if max(float(d.max()) for d in distances) <= radius * radius:
                return np.concatenate(chosen), np.concatenate(weights)
            radius *= 2

    def _sample(self, texture, uv):
        lin = _linear_image(texture.width, texture.height, texture.pixels_rgba)
        xy = np.clip(uv, 0, 1) * (texture.width - 1, texture.height - 1)
        xy[:, 1] = texture.height - 1 - xy[:, 1]
        low = np.floor(xy).astype(int)
        high = np.minimum(low + 1, (texture.width - 1, texture.height - 1))
        wx, wy = (xy - low).T
        wx, wy = wx[:, None], wy[:, None]
        return ((lin[low[:, 1], low[:, 0]] * (1 - wx) + lin[low[:, 1], high[:, 0]] * wx) * (1 - wy)
                + (lin[high[:, 1], low[:, 0]] * (1 - wx) + lin[high[:, 1], high[:, 0]] * wx) * wy)

    def bake(self, quad, material, width, height, sampling):
        quad = np.asarray(quad, dtype=float)
        k = 2 if sampling == 'area' else 1
        sub = (np.arange(k) + 0.5) / k
        s = ((np.arange(width)[:, None] + sub) / width).reshape(-1)
        t = ((np.arange(height)[:, None] + sub) / height).reshape(-1)
        ss, tt = np.meshgrid(s, t)
        top = quad[0] + (quad[1] - quad[0]) * ss[..., None]
        bottom = quad[3] + (quad[2] - quad[3]) * ss[..., None]
        points = (top + (bottom - top) * tt[..., None]).reshape(-1, 3)
        faces, bary = self._transfer(points, quad, material)
        uv = (self.uvs[faces] * bary[..., None]).sum(1)
        mat = self.source.materials[material]
        rgba = self._sample(self.source.textures[mat['texture']], uv)
        # The loader synthesizes factor-only materials into a 1x1 image.
        if not mat.get('factor_only'):
            factor = np.asarray(mat.get('rgb', (255, 255, 255))) / 255
            rgba[:, :3] *= factor ** 2.2
        if self.studio:
            illumination = self.ambient + self.diffuse * np.maximum(0, self.normals[faces] @ self.light)
            rgba[:, :3] *= illumination[:, None]
        emission = mat.get('emission_texture')
        if emission is not None:
            emitted = self._sample(self.source.textures[emission], uv)[:, :3]
            rgba[:, :3] += emitted * np.asarray(mat['emission_factor']) * mat['emission_strength']
        rgba[:, :3] *= self.exposure
        rgba = rgba.reshape(height, k, width, k, 4).mean((1, 3))
        # Fixed display exposure, clipped to SDR; leaves ordinary albedo
        # unchanged at exposure 1 rather than applying an unsolicited curve.
        rgb = np.round(np.clip(rgba[..., :3], 0, 1) ** (1 / 2.2) * 255)
        alpha = np.round(np.clip(rgba[..., 3:4], 0, 1) * 255)
        return [tuple(int(c) for c in p) for p in np.concatenate((rgb, alpha), axis=2).reshape(-1, 4)]
