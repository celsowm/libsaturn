#!/usr/bin/env python3
"""Attribute-aware edge-collapse simplification backend (host-only).

A deterministic, dependency-free QEM/edge-collapse simplifier tuned for the
Saturn animated-model path. It reduces triangle counts into the VDP1 command
budget while protecting everything the Saturn renderer cannot repair at
runtime:

- UV seams (welding a seam rewrites texture mapping with no UV runtime),
- material boundaries (faces carry one baked texture each),
- hard normal creases and open boundaries,
- high-importance articulation regions (via caller-supplied per-vertex
  animation/silhouette importance; see ``metrics`` and ``silhouette``).

Collapse keeps one endpoint (subset simplification): the survivor reuses its
exact source attributes (UV, joints, weights), so simplified output is a
subset/remap of source vertices and can never invent invalid skin data.
Foldover (normal-flip) collapses are rejected so low budgets degrade by
stopping early rather than turning the surface inside out.

The native-accelerator question: this backend deliberately uses only the
standard library so ``make test`` and model imports stay hermetic on
Windows/MSYS2 with no host C++ toolchain. A vendored meshoptimizer
accelerator may replace the inner loop later behind the same
``simplify()`` contract; quality gates in ``metrics`` decide whether such
a swap is behavior-preserving.
"""

from __future__ import annotations

import heapq
import itertools
import math
from dataclasses import dataclass, field

from .gltf import GltfError
from .model import SourceModel


@dataclass
class SimplificationOptions:
    target_triangles: int = 300
    quality: str = "balanced"  # conservative | balanced | aggressive
    preserve_uv: bool = True
    preserve_normals: bool = True
    preserve_boundaries: bool = True
    animation_weight: float = 1.0
    silhouette_weight: float = 1.0


@dataclass
class SimplifiedMesh:
    positions: list[tuple[float, float, float]]
    normals: list[tuple[float, float, float]] | None
    uvs: list[tuple[float, float]]
    joints: list[tuple[int, int, int, int]] | None
    weights: list[tuple[float, float, float, float]] | None
    triangles: list[tuple[int, int, int]]
    tri_materials: list[int]
    source_vertex: list[int]  # new index -> source vertex index
    report: dict = field(default_factory=dict)


_QUALITY_PARAMS = {
    "conservative": {
        "seam_lock": True,
        "material_lock": True,
        "crease_angle_deg": 45.0,
        "crease_mult": 25.0,
        "boundary_mult": 25.0,
        "aggressive_merge_mult": 100.0,
    },
    "balanced": {
        "seam_lock": True,
        "material_lock": True,
        "crease_angle_deg": 60.0,
        "crease_mult": 10.0,
        "boundary_mult": 10.0,
        "aggressive_merge_mult": 50.0,
    },
    "aggressive": {
        "seam_lock": False,
        "material_lock": False,
        "crease_angle_deg": 80.0,
        "crease_mult": 4.0,
        "boundary_mult": 2.0,
        "aggressive_merge_mult": 8.0,
    },
}


def _sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _face_normal(p0, p1, p2):
    return _cross(_sub(p1, p0), _sub(p2, p0))


def _normalize_positions(positions):
    """Scale positions by bbox diagonal for scale-independent costs."""
    xs = [p[0] for p in positions]
    ys = [p[1] for p in positions]
    zs = [p[2] for p in positions]
    diag = math.sqrt(
        (max(xs) - min(xs)) ** 2 + (max(ys) - min(ys)) ** 2 + (max(zs) - min(zs)) ** 2
    )
    if diag <= 0.0:
        diag = 1.0
    return [(p[0] / diag, p[1] / diag, p[2] / diag) for p in positions], diag


def _plane_quadric(n, d):
    """Symmetric 4x4 outer product of plane (a,b,c,d), 10 floats."""
    v = (n[0], n[1], n[2], d)
    return [v[i] * v[j] for i in range(4) for j in range(i, 4)]


def _quadric_at(q, p):
    """Evaluate [p,1]^T Q [p,1] from packed symmetric Q."""
    v = (p[0], p[1], p[2], 1.0)
    s = 0.0
    k = 0
    for i in range(4):
        for j in range(i, 4):
            w = q[k]
            k += 1
            s += w * v[i] * v[j] * (1.0 if i == j else 2.0)
    return s


def _add_quadric(a, b):
    return [x + y for x, y in zip(a, b)]


def simplify(
    model: SourceModel,
    anim_importance: list[float] | None = None,
    sil_importance: list[float] | None = None,
    options: SimplificationOptions | None = None,
) -> SimplifiedMesh:
    """Reduce ``model`` toward ``options.target_triangles`` deterministically."""
    opts = options or SimplificationOptions()
    if opts.quality not in _QUALITY_PARAMS:
        raise GltfError(f"unknown simplification quality {opts.quality!r}")
    qp = _QUALITY_PARAMS[opts.quality]
    n_src = len(model.vertices)
    if n_src == 0 or not model.triangles:
        raise GltfError("simplify: source model has no geometry")
    if opts.target_triangles < 1:
        raise GltfError("simplify: target_triangles must be >= 1")

    n_tri0 = len(model.triangles)
    nv = n_src
    anim_imp = list(anim_importance) if anim_importance else [0.0] * nv
    sil_imp = list(sil_importance) if sil_importance else [0.0] * nv
    if len(anim_imp) != nv or len(sil_imp) != nv:
        raise GltfError("simplify: importance arrays must match vertex count")
    if n_tri0 <= opts.target_triangles:
        return _identity(model, opts, "target already satisfied")

    npos, _diag = _normalize_positions(model.vertices)
    pos_eps = 1e-9
    uv_eps = 1e-6

    nv = n_src
    alive_v = [True] * nv
    quadrics: list[list[float]] = [[0.0] * 10 for _ in range(nv)]
    for (a, b, c) in model.triangles:
        n = _face_normal(npos[a], npos[b], npos[c])
        length = math.sqrt(_dot(n, n))
        if length <= 1e-15:
            continue  # degenerate source face contributes no plane
        n = (n[0] / length, n[1] / length, n[2] / length)
        d = -_dot(n, npos[a])
        q = _plane_quadric(n, d)
        quadrics[a] = _add_quadric(quadrics[a], q)
        quadrics[b] = _add_quadric(quadrics[b], q)
        quadrics[c] = _add_quadric(quadrics[c], q)

    tris: list[list[int]] = [list(t) for t in model.triangles]
    tri_mats = list(model.tri_materials)
    tri_alive = [True] * len(tris)
    alive_tris = len(tris)

    # Edge -> list of triangle indices (keys always (min, max)).
    edge_tris: dict[tuple[int, int], list[int]] = {}
    for ti, (a, b, c) in enumerate(tris):
        for u, v in ((a, b), (b, c), (c, a)):
            key = (u, v) if u < v else (v, u)
            edge_tris.setdefault(key, []).append(ti)

    # Seam vertices: same position, different UV.
    pos_key: dict[tuple, list[int]] = {}
    for vi, p in enumerate(npos):
        key = (round(p[0], 9), round(p[1], 9), round(p[2], 9))
        pos_key.setdefault(key, []).append(vi)
    seam_vertex = [False] * nv
    if opts.preserve_uv and model.uvs:
        for group in pos_key.values():
            if len(group) < 2:
                continue
            base_uv = model.uvs[group[0]]
            if any(
                abs(model.uvs[v][0] - base_uv[0]) > uv_eps
                or abs(model.uvs[v][1] - base_uv[1]) > uv_eps
                for v in group[1:]
            ):
                for v in group:
                    seam_vertex[v] = True

    crease_cos = math.cos(math.radians(qp["crease_angle_deg"]))

    def same_position(u: int, v: int) -> bool:
        a, b = npos[u], npos[v]
        return (
            abs(a[0] - b[0]) <= pos_eps
            and abs(a[1] - b[1]) <= pos_eps
            and abs(a[2] - b[2]) <= pos_eps
        )

    def edge_locked(u: int, v: int, adjacent: list[int]) -> bool:
        live = [t for t in adjacent if tri_alive[t]]
        if opts.preserve_uv and qp["seam_lock"] and model.uvs:
            if same_position(u, v):
                au, av = model.uvs[u], model.uvs[v]
                if abs(au[0] - av[0]) > uv_eps or abs(au[1] - av[1]) > uv_eps:
                    return True
        mats = {tri_mats[t] for t in live}
        if len(mats) > 1 and qp["material_lock"]:
            return True
        return False

    INF = float("inf")

    def edge_cost(u: int, v: int, adjacent: list[int]) -> tuple[float, int]:
        """(cost, survivor): survivor reuses exact source attributes."""
        live = [t for t in adjacent if tri_alive[t]]
        if not live:
            return INF, u
        if edge_locked(u, v, adjacent):
            return INF, u
        # Survivor: higher animation importance wins, ties -> lower index.
        # Deterministic and keeps articulation vertices' exact attributes.
        if (anim_imp[v], sil_imp[v], -v) > (anim_imp[u], sil_imp[u], -u):
            u, v = v, u
        q = _add_quadric(quadrics[u], quadrics[v])
        cost = _quadric_at(q, npos[u])
        # Shortest-first ordering among geometrically free edges.
        d = _sub(npos[u], npos[v])
        cost += 1e-9 * _dot(d, d)
        mult = 1.0
        if len(live) == 1 and opts.preserve_boundaries:
            mult *= qp["boundary_mult"]
        if len(live) == 2 and opts.preserve_normals:
            t0, t1 = tris[live[0]], tris[live[1]]
            n0 = _face_normal(npos[t0[0]], npos[t0[1]], npos[t0[2]])
            n1 = _face_normal(npos[t1[0]], npos[t1[1]], npos[t1[2]])
            l0 = math.sqrt(_dot(n0, n0))
            l1 = math.sqrt(_dot(n1, n1))
            if l0 > 1e-15 and l1 > 1e-15:
                if _dot(n0, n1) / (l0 * l1) < crease_cos:
                    mult *= qp["crease_mult"]
        if not qp["seam_lock"] and seam_vertex[u] and seam_vertex[v]:
            mult *= qp["aggressive_merge_mult"]
        if not qp["material_lock"] and len({tri_mats[t] for t in live}) > 1:
            mult *= qp["aggressive_merge_mult"]
        imp = (anim_imp[u] + anim_imp[v]) * 0.5 * opts.animation_weight + (
            sil_imp[u] + sil_imp[v]
        ) * 0.5 * opts.silhouette_weight
        mult *= 1.0 + imp
        return cost * mult, u

    seq = itertools.count()
    heap: list[tuple[float, int, int, int, int]] = []  # (cost, seq, u, v, gen)
    edge_gen: dict[tuple[int, int], int] = {}
    for key in sorted(edge_tris.keys()):
        u, v = key
        c, surv = edge_cost(u, v, edge_tris[key])
        if c == INF:
            continue
        edge_gen[key] = 0
        heapq.heappush(heap, (c, next(seq), surv, v if surv == u else u, 0))

    vertex_tris: list[set[int]] = [set() for _ in range(nv)]
    for ti, (a, b, c) in enumerate(tris):
        vertex_tris[a].add(ti)
        vertex_tris[b].add(ti)
        vertex_tris[c].add(ti)

    live_tri_set = {tuple(sorted(t)) for t in tris}
    collapses = 0
    rejected = 0

    def would_fold(keep: int, drop: int) -> bool:
        kp = npos[keep]
        for ti in vertex_tris[drop]:
            if not tri_alive[ti]:
                continue
            t = tris[ti]
            if keep in t:
                continue  # collapses to degenerate, removed by design
            old = _face_normal(npos[t[0]], npos[t[1]], npos[t[2]])
            if math.sqrt(_dot(old, old)) <= 1e-15:
                continue
            moved = [kp if x == drop else npos[x] for x in t]
            new = _face_normal(moved[0], moved[1], moved[2])
            if _dot(new, old) <= 0.0:
                return True
        return False

    while heap and alive_tris > opts.target_triangles:
        cost, _, keep, drop, gen = heapq.heappop(heap)
        if not alive_v[keep] or not alive_v[drop] or keep == drop:
            continue
        key = (keep, drop) if keep < drop else (drop, keep)
        if edge_gen.get(key, -1) != gen:
            continue  # stale entry
        adjacent = [t for t in edge_tris.get(key, []) if tri_alive[t]]
        if not adjacent:
            continue
        fresh, surv = edge_cost(keep, drop, edge_tris.get(key, []))
        if surv != keep:
            keep, drop = drop, keep
            key = (keep, drop) if keep < drop else (drop, keep)
        if fresh == INF or abs(fresh - cost) > 1e-12 * max(1.0, abs(cost)):
            if fresh != INF:
                edge_gen[key] = gen + 1
                heapq.heappush(heap, (fresh, next(seq), keep, drop, gen + 1))
            continue
        if would_fold(keep, drop):
            rejected += 1
            continue
        # Simulate first: a collapse that removes no triangle is pure
        # attribute damage (e.g. welding coincident seam sides that share
        # no face) with zero progress toward the face budget. Skip it.
        planned: list[tuple[int, list[int] | None]] = []
        kills = 0
        sim_set = set(live_tri_set)
        for ti in sorted(vertex_tris[drop]):
            if not tri_alive[ti]:
                continue
            t = tris[ti]
            old_canon = tuple(sorted(t))
            if keep in t:
                planned.append((ti, None))
                kills += 1
                continue
            new_t = [keep if x == drop else x for x in t]
            if len(set(new_t)) < 3 or tuple(sorted(new_t)) in sim_set:
                planned.append((ti, None))
                kills += 1
            else:
                sim_set.discard(old_canon)
                sim_set.add(tuple(sorted(new_t)))
                planned.append((ti, new_t))
        if kills == 0:
            rejected += 1
            continue
        # Commit: merge quadrics, move adjacency, retire `drop`.
        quadrics[keep] = _add_quadric(quadrics[keep], quadrics[drop])
        alive_v[drop] = False
        for ti, new_t in planned:
            t = tris[ti]
            if new_t is None:
                tri_alive[ti] = False
                live_tri_set.discard(tuple(sorted(t)))
                alive_tris -= 1
            else:
                live_tri_set.discard(tuple(sorted(t)))
                live_tri_set.add(tuple(sorted(new_t)))
                tris[ti] = new_t
                vertex_tris[keep].add(ti)
        vertex_tris[drop] = set()
        # Refresh keys touching `keep`.
        neighbors = set()
        for ti in vertex_tris[keep]:
            if not tri_alive[ti]:
                continue
            for x in tris[ti]:
                if x != keep and alive_v[x]:
                    neighbors.add(x)
        for x in sorted(neighbors):
            k2 = (keep, x) if keep < x else (x, keep)
            # Rebuild adjacency for k2 from live triangles.
            adj = [
                ti
                for ti in (vertex_tris[keep] & vertex_tris[x])
                if tri_alive[ti] and keep in tris[ti] and x in tris[ti]
            ]
            edge_tris[k2] = adj
            c2, surv2 = edge_cost(keep if keep < x else x, x if keep < x else keep, adj)
            if c2 == INF:
                continue
            a2, b2 = (keep, x) if surv2 == keep else (x, keep)
            edge_gen[k2] = edge_gen.get(k2, 0) + 1
            heapq.heappush(heap, (c2, next(seq), a2, b2, edge_gen[k2]))
        collapses += 1

    # Sweep orphaned vertices: UV-split characters leave most corners with a
    # single user, so each collapse strands the opposite corners of its
    # retired faces. Only referenced vertices reach the pose stream.
    referenced: set[int] = set()
    for ti, t in enumerate(tris):
        if tri_alive[ti]:
            referenced.update(t)
    for vi in range(nv):
        if alive_v[vi] and vi not in referenced:
            alive_v[vi] = False

    # Compact surviving vertices in source order (deterministic).
    remap = {}
    new_positions, new_uvs, new_j, new_w, new_n, src_idx = [], [], [], [], [], []
    has_joints = bool(model.joints) and len(model.joints) == n_src
    has_weights = bool(model.weights) and len(model.weights) == n_src
    has_normals = model.normals is not None and len(model.normals) == n_src
    has_uvs = bool(model.uvs) and len(model.uvs) == n_src
    for vi in range(nv):
        if not alive_v[vi]:
            continue
        remap[vi] = len(new_positions)
        new_positions.append(model.vertices[vi])
        src_idx.append(vi)
        new_uvs.append(model.uvs[vi] if has_uvs else (0.0, 0.0))
        if has_joints:
            new_j.append(model.joints[vi])
        if has_weights:
            new_w.append(model.weights[vi])
        if has_normals:
            new_n.append(model.normals[vi])
    new_tris = []
    new_mats = []
    for ti, t in enumerate(tris):
        if not tri_alive[ti]:
            continue
        if len({remap[t[0]], remap[t[1]], remap[t[2]]}) < 3:
            continue  # paranoia: never emit degenerate faces
        new_tris.append((remap[t[0]], remap[t[1]], remap[t[2]]))
        new_mats.append(tri_mats[ti])

    delivered = len(new_tris)
    report = {
        "requested_target": opts.target_triangles,
        "delivered_triangles": delivered,
        "delivered_vertices": len(new_positions),
        "source_triangles": n_tri0,
        "source_vertices": n_src,
        "reduction_percent": round(100.0 * (1.0 - delivered / n_tri0), 2) if n_tri0 else 0.0,
        "collapses": collapses,
        "foldover_rejected": rejected,
        "target_met": delivered <= opts.target_triangles,
        "quality": opts.quality,
    }
    return SimplifiedMesh(
        positions=new_positions,
        normals=new_n if has_normals else None,
        uvs=new_uvs,
        joints=new_j if has_joints else None,
        weights=new_w if has_weights else None,
        triangles=new_tris,
        tri_materials=new_mats,
        source_vertex=src_idx,
        report=report,
    )


def _identity(model: SourceModel, opts: SimplificationOptions, reason: str) -> SimplifiedMesh:
    return SimplifiedMesh(
        positions=list(model.vertices),
        normals=list(model.normals) if model.normals else None,
        uvs=list(model.uvs),
        joints=list(model.joints) if model.joints else None,
        weights=list(model.weights) if model.weights else None,
        triangles=list(model.triangles),
        tri_materials=list(model.tri_materials),
        source_vertex=list(range(len(model.vertices))),
        report={
            "requested_target": opts.target_triangles,
            "delivered_triangles": len(model.triangles),
            "delivered_vertices": len(model.vertices),
            "source_triangles": len(model.triangles),
            "source_vertices": len(model.vertices),
            "reduction_percent": 0.0,
            "collapses": 0,
            "foldover_rejected": 0,
            "target_met": True,
            "quality": opts.quality,
            "note": reason,
        },
    )
