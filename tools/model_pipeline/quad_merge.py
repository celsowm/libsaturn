"""Pair adjacent triangles into VDP1 quads.

A VDP1 distorted sprite draws four corners at the cost of one command, but
the importer used to submit every triangle as a quad with a repeated corner.
That spends one command per triangle and bakes each triangle into a whole
rectangle whose last row collapses to a point, so about half its texels are
spent oversampling the apex.

Two triangles that share an edge draw as one quad when the result looks the
same as the pair did:

* same material, and the shared edge is used by exactly these two triangles
  in opposite directions (consistent winding);
* the hinge-unfolded quad and its UV quad are both convex, so the drawn
  outline is the union of the two triangles;
* the texture lands where it did: the VDP1 maps a quad's texture bilinearly
  over its corners, while the source mapped each triangle affinely. The
  largest difference over a sample grid must stay under ``max_texel_error``
  baked texels;
* the fold stays shallow in every baked animation frame, so the projected
  quad does not bow-tie or visibly flatten a crease.

Candidates are accepted greedily, best score first. Polygons come back in
source (glTF, counter-clockwise) winding: 3-tuples for triangles left alone,
4-tuples ``(a, d, b, c)`` for a pair ``(a, b, c)`` + ``(b, a, d)``.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass
class QuadMergeOptions:
    max_texel_error: float = 1.0
    max_fold_deg: float = 30.0
    max_corner_deg: float = 170.0
    samples: int = 5


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _len(a):
    return math.sqrt(_dot(a, a))


def _normal(p, a, b, c):
    return _cross(_sub(p[b], p[a]), _sub(p[c], p[a]))


def _fold_cos(p, a, b, c, d):
    """Cosine between the normals of (a, b, c) and (b, a, d)."""
    n1 = _normal(p, a, b, c)
    n2 = _normal(p, b, a, d)
    l1, l2 = _len(n1), _len(n2)
    if l1 <= 0.0 or l2 <= 0.0:
        return -1.0
    return _dot(n1, n2) / (l1 * l2)


def _unfold(p, a, b, c, d):
    """Isometric hinge unfold of (a, b, c) + (b, a, d) into the plane.

    a at the origin, b on +x, c above the axis and d below it, so the quad
    (a, d, b, c) comes out counter-clockwise when it is convex.
    """
    ab = _len(_sub(p[b], p[a]))
    if ab <= 0.0:
        return None

    def place(v, sign):
        ra = _len(_sub(p[v], p[a]))
        rb = _len(_sub(p[v], p[b]))
        x = (ra * ra - rb * rb + ab * ab) / (2.0 * ab)
        y2 = ra * ra - x * x
        return (x, sign * math.sqrt(y2) if y2 > 0.0 else 0.0)

    return [(0.0, 0.0), place(d, -1.0), (ab, 0.0), place(c, 1.0)]


def _convex(poly, max_corner_deg):
    """Strictly convex, counter-clockwise, no corner flatter than the limit."""
    n = len(poly)
    limit = math.cos(math.radians(max_corner_deg))
    for i in range(n):
        p0, p1, p2 = poly[i - 1], poly[i], poly[(i + 1) % n]
        e0 = (p1[0] - p0[0], p1[1] - p0[1])
        e1 = (p2[0] - p1[0], p2[1] - p1[1])
        if e0[0] * e1[1] - e0[1] * e1[0] <= 0.0:
            return False
        # Interior angle = 180 - turn; reject near-straight corners.
        u0 = (-e0[0], -e0[1])
        l0 = math.hypot(*u0)
        l1 = math.hypot(*e1)
        if l0 <= 0.0 or l1 <= 0.0:
            return False
        if (u0[0] * e1[0] + u0[1] * e1[1]) / (l0 * l1) < limit:
            return False
    return True


def _bilinear(q, s, t):
    top = (q[0][0] + (q[1][0] - q[0][0]) * s, q[0][1] + (q[1][1] - q[0][1]) * s)
    bot = (q[3][0] + (q[2][0] - q[3][0]) * s, q[3][1] + (q[2][1] - q[3][1]) * s)
    return (top[0] + (bot[0] - top[0]) * t, top[1] + (bot[1] - top[1]) * t)


def _bary(p, a, b, c):
    det = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])
    if det == 0.0:
        return None
    l1 = ((p[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (p[1] - a[1])) / det
    l2 = ((b[0] - a[0]) * (p[1] - a[1]) - (p[0] - a[0]) * (b[1] - a[1])) / det
    return (1.0 - l1 - l2, l1, l2)


def _texel_error(flat, uv, texel_scale, samples):
    """Largest texel distance between the quad's bilinear texture mapping
    and the pair's piecewise-affine one.

    ``flat`` and ``uv`` list the quad corners (a, d, b, c); the source
    triangles are (a, b, c) = corners 0, 2, 3 and (b, a, d) = 2, 0, 1.
    """
    tris = ((0, 2, 3), (2, 0, 1))
    worst = 0.0
    for j in range(samples):
        t = j / (samples - 1)
        for i in range(samples):
            s = i / (samples - 1)
            pos = _bilinear(flat, s, t)
            want = _bilinear(uv, s, t)
            got = None
            for tri in tris:
                w = _bary(pos, flat[tri[0]], flat[tri[1]], flat[tri[2]])
                if w is None:
                    continue
                if min(w) >= -1e-6 or got is None:
                    got = (
                        w[0] * uv[tri[0]][0] + w[1] * uv[tri[1]][0] + w[2] * uv[tri[2]][0],
                        w[0] * uv[tri[0]][1] + w[1] * uv[tri[1]][1] + w[2] * uv[tri[2]][1],
                    )
                    if min(w) >= -1e-6:
                        break
            if got is None:
                return math.inf
            du = (want[0] - got[0]) * texel_scale[0]
            dv = (want[1] - got[1]) * texel_scale[1]
            worst = max(worst, math.hypot(du, dv))
    return worst


def merge_quads(
    triangles: list[tuple[int, int, int]],
    tri_materials: list[int],
    uvs: list[tuple[float, float]],
    rest_positions: list[tuple[float, float, float]],
    frames: list[list[tuple[float, float, float]]],
    texel_scales: dict[int, tuple[float, float]],
    options: QuadMergeOptions | None = None,
) -> tuple[list[tuple[int, ...]], list[int], dict]:
    """Returns (polygons, polygon_materials, report).

    ``texel_scales`` maps a material to its baked texels per UV unit
    (texture size times --texture-scale) on each axis.
    """
    opts = options or QuadMergeOptions()
    fold_cos = math.cos(math.radians(opts.max_fold_deg))

    edge_tris: dict[tuple[int, int], list[int]] = {}
    for ti, (a, b, c) in enumerate(triangles):
        for u, v in ((a, b), (b, c), (c, a)):
            edge_tris.setdefault((min(u, v), max(u, v)), []).append(ti)

    candidates: list[tuple[float, int, int, tuple[int, int, int, int]]] = []
    rejected = {"material": 0, "winding": 0, "convex": 0, "texel_error": 0, "fold": 0}
    for (u, v), owners in edge_tris.items():
        if len(owners) != 2:
            continue
        t1, t2 = owners
        if tri_materials[t1] != tri_materials[t2]:
            rejected["material"] += 1
            continue
        # Rotate t1 so the shared edge is its (a, b); t2 must then run b -> a.
        tri1 = triangles[t1]
        k = next((i for i in range(3) if {tri1[i], tri1[(i + 1) % 3]} == {u, v}), None)
        a, b, c = tri1[k], tri1[(k + 1) % 3], tri1[(k + 2) % 3]
        tri2 = triangles[t2]
        m = next((i for i in range(3) if tri2[i] == b and tri2[(i + 1) % 3] == a), None)
        if m is None:
            rejected["winding"] += 1
            continue
        d = tri2[(m + 2) % 3]
        if d in (a, b, c):
            rejected["winding"] += 1
            continue

        corners = (a, d, b, c)
        uv = [uvs[i] for i in corners]
        # UV v grows upwards in glTF-as-imported space; convexity only needs a
        # consistent orientation, so test both and require the flat one's.
        flat = _unfold(rest_positions, a, b, c, d)
        if flat is None or not _convex(flat, opts.max_corner_deg):
            rejected["convex"] += 1
            continue
        uv_ccw = _convex(uv, opts.max_corner_deg)
        uv_cw = _convex(list(reversed(uv)), opts.max_corner_deg)
        if not (uv_ccw or uv_cw):
            rejected["convex"] += 1
            continue
        err = _texel_error(flat, uv, texel_scales.get(tri_materials[t1], (1.0, 1.0)), opts.samples)
        if err > opts.max_texel_error:
            rejected["texel_error"] += 1
            continue
        worst_cos = 1.0
        ok = True
        for frame in frames or [rest_positions]:
            fc = _fold_cos(frame, a, b, c, d)
            worst_cos = min(worst_cos, fc)
            if fc < fold_cos:
                ok = False
                break
            ff = _unfold(frame, a, b, c, d)
            if ff is None or not _convex(ff, opts.max_corner_deg):
                ok = False
                break
        if not ok:
            rejected["fold"] += 1
            continue
        fold_deg = math.degrees(math.acos(max(-1.0, min(1.0, worst_cos))))
        # A quad's natural diagonal is the longest edge of both halves;
        # pairing across it first leaves fewer triangles stranded.
        hinge = _len(_sub(rest_positions[b], rest_positions[a]))
        longest = max(hinge,
                      _len(_sub(rest_positions[c], rest_positions[a])),
                      _len(_sub(rest_positions[c], rest_positions[b])),
                      _len(_sub(rest_positions[d], rest_positions[a])),
                      _len(_sub(rest_positions[d], rest_positions[b])))
        diagonal = 1.0 - hinge / longest if longest > 0.0 else 1.0
        score = (err / max(opts.max_texel_error, 1e-9)
                 + fold_deg / max(opts.max_fold_deg, 1e-9) + diagonal)
        candidates.append((score, t1, t2, corners))

    candidates.sort(key=lambda item: (item[0], item[1], item[2]))
    used = [False] * len(triangles)
    merged: dict[int, tuple[int, tuple[int, int, int, int]]] = {}
    for score, t1, t2, corners in candidates:
        if used[t1] or used[t2]:
            continue
        used[t1] = used[t2] = True
        merged[min(t1, t2)] = (max(t1, t2), corners)

    polygons: list[tuple[int, ...]] = []
    materials: list[int] = []
    skip: set[int] = set()
    for ti, tri in enumerate(triangles):
        if ti in skip:
            continue
        if ti in merged:
            other, corners = merged[ti]
            skip.add(other)
            polygons.append(corners)
        else:
            polygons.append(tuple(tri))
        materials.append(tri_materials[ti])

    report = {
        "enabled": True,
        "source_triangles": len(triangles),
        "candidates": len(candidates),
        "merged_quads": len(merged),
        "single_triangles": len(triangles) - 2 * len(merged),
        "polygons": len(polygons),
        "rejected": rejected,
        "max_texel_error": opts.max_texel_error,
        "max_fold_deg": opts.max_fold_deg,
    }
    return polygons, materials, report
