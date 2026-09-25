#!/usr/bin/env python3
"""Bake-time splitting of intersecting faces for the VDP1 painter.

The VDP1 has no depth buffer: the scene painter orders whole faces, so two
faces that pass THROUGH each other are drawn one over the other everywhere,
and no pass or depth key can fix that. Splitting each of the pair along the
other's plane leaves pieces that each lie on one side of the other face,
which a per-face order can then draw correctly.

Faces are convex planar polygons of (position, uv) corners. Splitting
interpolates positions and UVs linearly along the cut edges, which is exact
for a planar face (the texture mapping of a flat polygon is affine). The pass
repeats until no pair intersects or the face budget is reached; the result
keeps each piece's source face so per-face data (material, texture) follows.

Non-intersecting cycles (three faces overlapping each other in a ring) are
NOT resolved here: they need no split to be drawable, only an order the
per-face depth key may not find. They are rare in authored level geometry and
are reported as unsupported by the documentation, not hidden.
"""

from __future__ import annotations

from dataclasses import dataclass

# Distances within this fraction of the face's size count as ON the plane:
# a face merely touching another (a wall standing on a floor) is not split.
_EPS = 1e-6


@dataclass
class Face:
    """A convex planar polygon. corners: [(pos xyz, uv)], source: input face."""

    corners: list[tuple[tuple[float, float, float], tuple[float, float]]]
    source: int


class SplitBudgetError(ValueError):
    pass


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _lerp(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def plane(face: Face):
    """(unit normal, offset, scale) of the face's plane via Newell's method."""
    nx = ny = nz = 0.0
    pts = [c[0] for c in face.corners]
    for i, p in enumerate(pts):
        q = pts[(i + 1) % len(pts)]
        nx += (p[1] - q[1]) * (p[2] + q[2])
        ny += (p[2] - q[2]) * (p[0] + q[0])
        nz += (p[0] - q[0]) * (p[1] + q[1])
    length = (nx * nx + ny * ny + nz * nz) ** 0.5
    if length == 0.0:
        return None
    n = (nx / length, ny / length, nz / length)
    scale = max(max(abs(_sub(p, pts[0])[k]) for p in pts) for k in range(3)) or 1.0
    return n, _dot(n, pts[0]), scale


def _signed(face: Face, pl):
    n, d, scale = pl
    eps = _EPS * scale
    out = []
    for pos, _ in face.corners:
        s = _dot(n, pos) - d
        out.append(0.0 if abs(s) <= eps else s)
    return out


def split_by_plane(face: Face, pl):
    """(front, back) pieces of face cut by plane pl; either may be None."""
    dist = _signed(face, pl)
    if all(s >= 0.0 for s in dist):
        return face, None
    if all(s <= 0.0 for s in dist):
        return None, face
    front, back = [], []
    n = len(face.corners)
    for i in range(n):
        a, b = face.corners[i], face.corners[(i + 1) % n]
        sa, sb = dist[i], dist[(i + 1) % n]
        if sa >= 0.0:
            front.append(a)
        if sa <= 0.0:
            back.append(a)
        if (sa > 0.0 and sb < 0.0) or (sa < 0.0 and sb > 0.0):
            t = sa / (sa - sb)
            cut = (_lerp(a[0], b[0], t), _lerp(a[1], b[1], t))
            front.append(cut)
            back.append(cut)
    return Face(front, face.source), Face(back, face.source)


def _interval_on_line(face: Face, pl, origin, direction):
    """Parameter range along the intersection line where face meets plane pl."""
    dist = _signed(face, pl)
    ts = []
    n = len(face.corners)
    for i in range(n):
        a, b = face.corners[i][0], face.corners[(i + 1) % n][0]
        sa, sb = dist[i], dist[(i + 1) % n]
        if sa == 0.0:
            ts.append(_dot(_sub(a, origin), direction))
        if (sa > 0.0 and sb < 0.0) or (sa < 0.0 and sb > 0.0):
            p = _lerp(a, b, sa / (sa - sb))
            ts.append(_dot(_sub(p, origin), direction))
    return (min(ts), max(ts)) if ts else None


def intersects(a: Face, b: Face) -> bool:
    """True when the interiors of convex faces a and b pass through each other."""
    pa, pb = plane(a), plane(b)
    if pa is None or pb is None:
        return False
    da, db = _signed(a, pb), _signed(b, pa)
    # Each face must have corners strictly on both sides of the other's plane
    # (touching or coplanar faces are not intersections a split can fix).
    if not (min(da) < 0.0 < max(da)) or not (min(db) < 0.0 < max(db)):
        return False
    direction = _cross(pa[0], pb[0])
    length = _dot(direction, direction) ** 0.5
    if length == 0.0:
        return False
    direction = tuple(x / length for x in direction)
    origin = a.corners[0][0]
    ia = _interval_on_line(a, pb, origin, direction)
    ib = _interval_on_line(b, pa, origin, direction)
    if ia is None or ib is None:
        return False
    eps = _EPS * max(pa[2], pb[2])
    return min(ia[1], ib[1]) - max(ia[0], ib[0]) > eps


def _bounds(face: Face):
    pts = [c[0] for c in face.corners]
    return tuple(min(p[k] for p in pts) for k in range(3)), tuple(max(p[k] for p in pts) for k in range(3))


def _bounds_overlap(a, b):
    return all(a[0][k] <= b[1][k] and b[0][k] <= a[1][k] for k in range(3))


def split_intersections(faces: list[Face], max_faces: int):
    """Split every intersecting pair until none remain.

    Returns (faces, report). Raises SplitBudgetError when more than max_faces
    faces would be needed, so a level never silently ships unsplit.
    """
    work = list(faces)
    splits = 0
    changed = True
    while changed:
        changed = False
        bounds = [_bounds(f) for f in work]
        for i in range(len(work)):
            for j in range(i + 1, len(work)):
                if not _bounds_overlap(bounds[i], bounds[j]):
                    continue
                a, b = work[i], work[j]
                if not intersects(a, b):
                    continue
                a_front, a_back = split_by_plane(a, plane(b))
                b_front, b_back = split_by_plane(b, plane(a))
                pieces = [p for p in (a_front, a_back, b_front, b_back) if p is not None]
                if len(work) - 2 + len(pieces) > max_faces:
                    raise SplitBudgetError(
                        f"splitting intersecting faces needs more than {max_faces} faces"
                    )
                work = [f for k, f in enumerate(work) if k not in (i, j)] + pieces
                splits += 1
                changed = True
                break
            if changed:
                break
    return work, {"input_faces": len(faces), "output_faces": len(work), "splits": splits}


def to_corner_lists(face: Face):
    """Split a convex polygon into fan pieces of 3 or 4 corners (VDP1 quads).

    Quads (0, i, i+1, i+2) of a convex planar polygon are themselves convex
    and planar; a leftover gives one triangle.
    """
    c = face.corners
    if len(c) <= 4:
        return [list(c)]
    out = []
    i = 1
    while i + 1 < len(c):
        if i + 2 < len(c):
            out.append([c[0], c[i], c[i + 1], c[i + 2]])
            i += 2
        else:
            out.append([c[0], c[i], c[i + 1]])
            i += 1
    return out
