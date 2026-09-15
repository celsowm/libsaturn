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
        "ancestor_cos": 0.707,
    },
    "balanced": {
        "seam_lock": True,
        "material_lock": True,
        "crease_angle_deg": 60.0,
        "crease_mult": 10.0,
        "boundary_mult": 10.0,
        "aggressive_merge_mult": 50.0,
        "ancestor_cos": 0.5,
    },
    "aggressive": {
        "seam_lock": False,
        "material_lock": False,
        "crease_angle_deg": 80.0,
        "crease_mult": 4.0,
        "boundary_mult": 2.0,
        "aggressive_merge_mult": 8.0,
        "ancestor_cos": 0.0,
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


def _weld_exact_duplicates(model: SourceModel):
    """Merge vertices identical in every attribute (position/UV/normal/
    joints/weights). Exact duplicates carry no information; welding them can
    only restore manifold sharing (closed solids chamfer instead of losing
    whole faces) and shrink the pose stream. UV seams and material data are
    untouched: any attribute difference vetoes the weld, and faces keep
    their own material indices. Returns (model, welded_count, dropped_degenerate).
    """
    n = len(model.vertices)
    has_j = bool(model.joints) and len(model.joints) == n
    has_w = bool(model.weights) and len(model.weights) == n
    has_n = model.normals is not None and len(model.normals) == n
    has_uv = bool(model.uvs) and len(model.uvs) == n
    key_of = []
    for i in range(n):
        p = model.vertices[i]
        key = (
            round(p[0], 9), round(p[1], 9), round(p[2], 9),
            tuple(model.uvs[i]) if has_uv else None,
            tuple(model.normals[i]) if has_n else None,
            tuple(model.joints[i]) if has_j else None,
            tuple(model.weights[i]) if has_w else None,
        )
        key_of.append(key)
    first: dict = {}
    remap = [0] * n
    for i, key in enumerate(key_of):
        if key in first:
            remap[i] = first[key]
        else:
            first[key] = i
            remap[i] = i
    survivors = sorted(set(remap))
    if len(survivors) == n:
        return model, list(range(n)), 0, 0
    new_index = {old: new for new, old in enumerate(survivors)}
    new_remap = [new_index[r] for r in remap]
    welded = SourceModel()
    welded.vertices = [model.vertices[o] for o in survivors]
    welded.normals = [model.normals[o] for o in survivors] if has_n else None
    welded.uvs = [model.uvs[o] for o in survivors] if has_uv else []
    welded.joints = [model.joints[o] for o in survivors] if has_j else []
    welded.weights = [model.weights[o] for o in survivors] if has_w else []
    welded.materials = list(model.materials)
    welded.textures = list(model.textures)
    welded.nodes = model.nodes
    welded.skins = model.skins
    welded.clips = model.clips
    welded.mesh_node = model.mesh_node
    welded.skin_index = model.skin_index
    dropped = 0
    for tri, mt in zip(model.triangles, model.tri_materials):
        mapped = (new_remap[tri[0]], new_remap[tri[1]], new_remap[tri[2]])
        if len(set(mapped)) < 3:
            dropped += 1
            continue
        welded.triangles.append(mapped)
        welded.tri_materials.append(mt)
    return welded, new_remap, n - len(survivors), dropped


def simplify(
    model: SourceModel,
    anim_importance: list[float] | None = None,
    sil_importance: list[float] | None = None,
    options: SimplificationOptions | None = None,
    pose_positions: list[list[tuple[float, float, float]]] | None = None,
) -> SimplifiedMesh:
    """Reduce ``model`` toward ``options.target_triangles`` deterministically.

    ``pose_positions`` carries baked animation poses (bind pose is always
    included automatically). The cost of a collapse is the MAXIMUM error over
    every pose -- cheap in all poses collapses first, expensive in any pose
    is protected -- so rigidly-swinging limbs cost exactly what bind-pose
    QEM says (rotation preserves error magnitude) while genuinely deforming
    regions (elbows, knees, shoulders) pay their worst-pose price. Per-vertex
    skinning makes the per-pose survivor positions exact for subset
    collapse. Foldover is rejected in every pose for the same reason.
    """
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
    pose_list = list(pose_positions or [])
    for pose in pose_list:
        if len(pose) != n_src:
            raise GltfError("simplify: pose vertex count must match the model")

    # Exact-duplicate weld FIRST (even when already under target): it
    # restores manifold sharing where the exporter split verts redundantly
    # (closed solids chamfer instead of losing whole faces), drops degenerate
    # source faces, and shrinks the pose stream. Importance follows the max
    # of each welded group; poses remap by the same index map. Reported,
    # deterministic, seam-safe: any attribute difference vetoes the weld,
    # and the final source_vertex map composes back to ORIGINAL indices.
    orig_n_src = n_src
    orig_n_tri0 = n_tri0
    model, weld_remap, welded_verts, dropped_degenerate = _weld_exact_duplicates(model)
    first_old = list(range(len(model.vertices)))
    if welded_verts or dropped_degenerate:
        n_src = len(model.vertices)
        nv = n_src
        grouped_a: dict[int, float] = {}
        grouped_s: dict[int, float] = {}
        first_old = [0] * nv
        seen_new: set[int] = set()
        for old, new in enumerate(weld_remap):
            if new not in seen_new:  # first occurrence wins deterministically
                seen_new.add(new)
                first_old[new] = old
            if anim_imp[old] > grouped_a.get(new, 0.0):
                grouped_a[new] = anim_imp[old]
            if sil_imp[old] > grouped_s.get(new, 0.0):
                grouped_s[new] = sil_imp[old]
        anim_imp = [grouped_a.get(i, 0.0) for i in range(nv)]
        sil_imp = [grouped_s.get(i, 0.0) for i in range(nv)]
        pose_list = [[pose[old] for old in first_old] for pose in pose_list]
        n_tri0 = len(model.triangles)
    if n_tri0 <= opts.target_triangles:
        done = _identity(model, opts, "target already satisfied")
        done.source_vertex = [first_old[w] for w in done.source_vertex]
        done.report["welded_vertices"] = welded_verts
        done.report["dropped_degenerate"] = dropped_degenerate
        done.report["source_vertices"] = orig_n_src
        done.report["source_triangles"] = orig_n_tri0
        return done

    npos, diag = _normalize_positions(model.vertices)
    if diag <= 0.0:
        diag = 1.0
    # Every pose shares the bind-pose normalization so costs stay comparable.
    pose_sets = [npos]
    for pose in pose_list:
        pose_sets.append([(p[0] / diag, p[1] / diag, p[2] / diag) for p in pose])
    uv_eps = 1e-6
    use_uv = opts.preserve_uv and bool(model.uvs) and len(model.uvs) == nv
    has_skin = (
        bool(model.joints) and bool(model.weights)
        and len(model.joints) == nv and len(model.weights) == nv
    )

    # Position groups. Exporters split one surface point into several vertex
    # copies (flat normals, UV seams, palette-swatch UVs give nearly every
    # corner its own copy). Collapsing one copy alone drags it away from its
    # twins and tears a crack through the surface, so collapses run on groups
    # of coincident, identically skinned copies and move every copy at once.
    group_of = [0] * nv
    members: list[list[int]] = []
    group_index: dict[tuple, int] = {}
    for vi in range(nv):
        p = model.vertices[vi]
        key = (
            round(p[0], 9), round(p[1], 9), round(p[2], 9),
            tuple(model.joints[vi]) if has_skin else None,
            tuple(model.weights[vi]) if has_skin else None,
        )
        gi = group_index.get(key)
        if gi is None:
            gi = len(members)
            group_index[key] = gi
            members.append([])
        members[gi].append(vi)
        group_of[vi] = gi
    ng = len(members)
    rep = [m[0] for m in members]
    g_anim = [max(anim_imp[v] for v in m) for m in members]
    g_sil = [max(sil_imp[v] for v in m) for m in members]

    tris: list[list[int]] = [list(t) for t in model.triangles]
    tri_mats = list(model.tri_materials)
    tri_alive = [True] * len(tris)
    alive_tris = len(tris)
    for ti, t in enumerate(tris):
        # Two corners on one position: zero-area, invisible, and it would
        # confuse group adjacency. Drop it like any other degenerate face.
        if len({group_of[x] for x in t}) < 3:
            tri_alive[ti] = False
            alive_tris -= 1
            dropped_degenerate += 1

    vertex_tris: list[set[int]] = [set() for _ in range(nv)]
    group_tris: list[set[int]] = [set() for _ in range(ng)]
    for ti, t in enumerate(tris):
        if not tri_alive[ti]:
            continue
        for x in t:
            vertex_tris[x].add(ti)
            group_tris[group_of[x]].add(ti)

    def gkey(a: int, b: int) -> tuple[int, int]:
        return (a, b) if a < b else (b, a)

    def uv_same(a: int, b: int) -> bool:
        if not use_uv:
            return True
        ua, ub = model.uvs[a], model.uvs[b]
        return abs(ua[0] - ub[0]) <= uv_eps and abs(ua[1] - ub[1]) <= uv_eps

    def corner(ti: int, g: int) -> int:
        for x in tris[ti]:
            if group_of[x] == g:
                return x
        return -1

    edge_faces0: dict[tuple[int, int], list[int]] = {}
    for ti, t in enumerate(tris):
        if not tri_alive[ti]:
            continue
        for i in range(3):
            key = gkey(group_of[t[i]], group_of[t[(i + 1) % 3]])
            edge_faces0.setdefault(key, []).append(ti)

    # Feature edges get perpendicular constraint planes (Garland-Heckbert
    # boundary quadrics): open borders, UV seams and material boundaries.
    # Without them a flat region's outline -- a sleeve cuff, a color patch
    # edge -- could wander at zero QEM cost.
    feature_edges: list[tuple[int, int, int]] = []
    for key in sorted(edge_faces0):
        faces = edge_faces0[key]
        g0, g1 = key
        if len(faces) == 1:
            if opts.preserve_boundaries:
                feature_edges.append((g0, g1, faces[0]))
        elif len(faces) == 2:
            f0, f1 = faces
            seam = use_uv and not (
                uv_same(corner(f0, g0), corner(f1, g0))
                and uv_same(corner(f0, g1), corner(f1, g1))
            )
            if seam or tri_mats[f0] != tri_mats[f1]:
                feature_edges.append((g0, g1, f0))
                feature_edges.append((g0, g1, f1))

    crease_cos = math.cos(math.radians(qp["crease_angle_deg"]))
    constraint_weight = qp["boundary_mult"]

    # Per-pose quadrics per group: pose_quadrics[p][g]. Summing across poses
    # would let rigidly-moving regions outvote static ones by pose count; the
    # edge cost takes the MAXIMUM over poses instead (see edge_cost).
    pose_quadrics: list[list[list[float]]] = []
    for positions in pose_sets:
        quadrics: list[list[float]] = [[0.0] * 10 for _ in range(ng)]
        for ti, (a, b, c) in enumerate(tris):
            if not tri_alive[ti]:
                continue
            n = _face_normal(positions[a], positions[b], positions[c])
            length = math.sqrt(_dot(n, n))
            if length <= 1e-15:
                continue  # degenerate face contributes no plane
            n = (n[0] / length, n[1] / length, n[2] / length)
            q = _plane_quadric(n, -_dot(n, positions[a]))
            for x in (a, b, c):
                g = group_of[x]
                quadrics[g] = _add_quadric(quadrics[g], q)
        for g0, g1, ti in feature_edges:
            a, b, c = tris[ti]
            n = _face_normal(positions[a], positions[b], positions[c])
            length = math.sqrt(_dot(n, n))
            if length <= 1e-15:
                continue
            n = (n[0] / length, n[1] / length, n[2] / length)
            p0 = positions[rep[g0]]
            e = _sub(positions[rep[g1]], p0)
            m = _cross(e, n)
            ml = math.sqrt(_dot(m, m))
            if ml <= 1e-15:
                continue
            m = (m[0] / ml, m[1] / ml, m[2] / ml)
            q = [w * constraint_weight for w in _plane_quadric(m, -_dot(m, p0))]
            quadrics[g0] = _add_quadric(quadrics[g0], q)
            quadrics[g1] = _add_quadric(quadrics[g1], q)
        pose_quadrics.append(quadrics)

    alive_g = [True] * ng
    INF = float("inf")

    def live_edge_faces(g0: int, g1: int) -> list[int]:
        a, b = group_tris[g0], group_tris[g1]
        if len(a) > len(b):
            a, b = b, a
        return sorted(t for t in a if t in b and tri_alive[t])

    has_normals = model.normals is not None and len(model.normals) == nv

    def closest_copy(candidates: list[int], d: int) -> int:
        """Of several usable keep copies, the one facing like ``d``.

        Flat-shaded exports give every side of a corner its own copy; taking
        the lowest index could hand a face the copy from the far side of an
        edge, whose normal and source adjacency describe another surface.
        """
        if not has_normals or len(candidates) == 1:
            return candidates[0]
        nd = model.normals[d]
        return max(candidates, key=lambda k: (_dot(model.normals[k], nd), -k))

    def plan_remap(keep: int, drop: int, edge_faces: list[int]) -> tuple[dict[int, int], bool]:
        """Pair each live copy of ``drop`` with a copy of ``keep``.

        A copy sharing a collapsing face with a keep copy takes that copy
        (same chart, the ordinary subset collapse). Any other copy takes the
        keep copy with the identical UV. With no such copy the collapse would
        push a UV seam across the surface: ``crossed`` reports it, and the
        closest-chart fallback keeps the output a strict source subset.
        """
        ef = set(edge_faces)
        remap: dict[int, int] = {}
        crossed = False
        for d in members[drop]:
            if not any(tri_alive[t] for t in vertex_tris[d]):
                continue
            linked = sorted({
                x
                for t in vertex_tris[d] if t in ef
                for x in tris[t] if group_of[x] == keep
            })
            if linked:
                same = [x for x in linked if uv_same(x, d)]
                if not same and any(not uv_same(x, linked[0]) for x in linked[1:]):
                    crossed = True
                remap[d] = closest_copy(same if same else linked, d)
                continue
            same = [k for k in members[keep] if uv_same(k, d)]
            if same:
                remap[d] = closest_copy(same, d)
            else:
                crossed = True
                remap[d] = closest_copy(members[keep], d)
        return remap, crossed

    def edge_cost(g0: int, g1: int):
        """(cost, survivor, remap): survivor copies keep exact source attributes."""
        live = live_edge_faces(g0, g1)
        if not live:
            return INF, g0, None
        edge_mats = {tri_mats[t] for t in live}
        if len(edge_mats) > 1 and qp["material_lock"]:
            return INF, g0, None
        # Survivor: higher animation importance wins, ties -> lower index.
        # Deterministic and keeps articulation vertices' exact attributes.
        # When that direction is locked (it would drag a seam or material
        # boundary inward) the reverse is often still legal: a patch's
        # interior vertex may fold onto its outline, never the outline in.
        keep, drop = g0, g1
        if (g_anim[drop], g_sil[drop], -drop) > (g_anim[keep], g_sil[keep], -keep):
            keep, drop = drop, keep
        cost, remap = directed_cost(keep, drop, live)
        if cost == INF:
            cost, remap = directed_cost(drop, keep, live)
            if cost != INF:
                return cost, drop, remap
        return cost, keep, remap

    def directed_cost(keep: int, drop: int, live: list[int]):
        drop_mats = {tri_mats[t] for t in group_tris[drop] if tri_alive[t]}
        keep_mats = {tri_mats[t] for t in group_tris[keep] if tri_alive[t]}
        if qp["material_lock"] and not drop_mats <= keep_mats:
            return INF, None  # would drag a material boundary inward
        remap, crossed = plan_remap(keep, drop, live)
        if crossed and use_uv and qp["seam_lock"]:
            return INF, None
        # Worst-pose QEM cost: collapse error evaluated in every pose at
        # the survivor's posed position, maximum wins. Rigid motion is
        # error-magnitude preserving, so rigid regions cost exactly their
        # bind-pose price while deforming regions pay their worst pose.
        cost = 0.0
        for pi, positions in enumerate(pose_sets):
            q = _add_quadric(pose_quadrics[pi][keep], pose_quadrics[pi][drop])
            err = _quadric_at(q, positions[rep[keep]])
            if err > cost:
                cost = err
        # Shortest-first ordering among geometrically free edges.
        d = _sub(npos[rep[keep]], npos[rep[drop]])
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
        if crossed:
            mult *= qp["aggressive_merge_mult"]
        if not qp["material_lock"] and len(drop_mats | keep_mats) > 1:
            mult *= qp["aggressive_merge_mult"]
        imp = (g_anim[keep] + g_anim[drop]) * 0.5 * opts.animation_weight + (
            g_sil[keep] + g_sil[drop]
        ) * 0.5 * opts.silhouette_weight
        mult *= 1.0 + imp
        return cost * mult, remap

    seq = itertools.count()
    heap: list[tuple[float, int, int, int, int]] = []  # (cost, seq, keep, drop, gen)
    edge_gen: dict[tuple[int, int], int] = {}
    for key in sorted(edge_faces0.keys()):
        g0, g1 = key
        c, surv, _ = edge_cost(g0, g1)
        if c == INF:
            continue
        edge_gen[key] = 0
        heapq.heappush(heap, (c, next(seq), surv, g1 if surv == g0 else g0, 0))

    live_tri_set = {tuple(sorted(t)) for ti, t in enumerate(tris) if tri_alive[ti]}
    collapses = 0
    rejected = 0

    # Rejecting only a flip relative to the previous step lets a face creep
    # past 90 degrees over several collapses of up to 89 degrees each, and a
    # face turned that far is dropped by backface culling, leaving a
    # see-through hole. Every face is also held within acos(ancestor_cos) of
    # its ORIGINAL source orientation, in every pose.
    ancestor_cos = qp["ancestor_cos"]
    ancestor_normals = [
        [_face_normal(positions[t[0]], positions[t[1]], positions[t[2]]) for t in tris]
        for positions in pose_sets
    ]

    def would_fold(faces: list[int], remap: dict[int, int]) -> bool:
        for pi, positions in enumerate(pose_sets):
            for ti in faces:
                t = tris[ti]
                old = _face_normal(positions[t[0]], positions[t[1]], positions[t[2]])
                old_len = math.sqrt(_dot(old, old))
                if old_len <= 1e-15:
                    continue
                moved = [positions[remap.get(x, x)] for x in t]
                new = _face_normal(moved[0], moved[1], moved[2])
                new_len = math.sqrt(_dot(new, new))
                if _dot(new, old) <= 0.0:
                    return True
                anc = ancestor_normals[pi][ti]
                anc_len = math.sqrt(_dot(anc, anc))
                if anc_len > 1e-15 and _dot(new, anc) <= ancestor_cos * anc_len * new_len:
                    return True
        return False

    while heap and alive_tris > opts.target_triangles:
        cost, _, keep, drop, gen = heapq.heappop(heap)
        if not alive_g[keep] or not alive_g[drop] or keep == drop:
            continue
        key = gkey(keep, drop)
        if edge_gen.get(key, -1) != gen:
            continue  # stale entry
        fresh, surv, remap = edge_cost(keep, drop)
        if surv != keep:
            keep, drop = drop, keep
        if fresh == INF or abs(fresh - cost) > 1e-12 * max(1.0, abs(cost)):
            if fresh != INF:
                edge_gen[key] = gen + 1
                heapq.heappush(heap, (fresh, next(seq), keep, drop, gen + 1))
            continue
        edge_set = set(live_edge_faces(keep, drop))
        moved_faces = [
            ti for ti in sorted(group_tris[drop]) if tri_alive[ti] and ti not in edge_set
        ]
        if would_fold(moved_faces, remap):
            rejected += 1
            continue
        # Simulate first: a collapse that removes no triangle is pure
        # attribute damage with zero progress toward the face budget.
        planned: list[tuple[int, list[int] | None]] = []
        kills = 0
        sim_set = set(live_tri_set)
        for ti in sorted(group_tris[drop]):
            if not tri_alive[ti]:
                continue
            t = tris[ti]
            new_t = [remap.get(x, x) for x in t]
            if len({group_of[x] for x in new_t}) < 3 or tuple(sorted(new_t)) in sim_set:
                planned.append((ti, None))
                kills += 1
            else:
                sim_set.discard(tuple(sorted(t)))
                sim_set.add(tuple(sorted(new_t)))
                planned.append((ti, new_t))
        if kills == 0:
            rejected += 1
            continue
        if alive_tris - kills < 1:
            # Never commit an empty mesh: a zero-triangle collapse removes
            # the surface instead of reducing it. Stop here and deliver the
            # smallest nonzero candidate.
            rejected += 1
            break
        # Commit: merge per-pose quadrics, move adjacency, retire `drop`.
        for pi in range(len(pose_sets)):
            pose_quadrics[pi][keep] = _add_quadric(
                pose_quadrics[pi][keep], pose_quadrics[pi][drop]
            )
        alive_g[drop] = False
        for ti, new_t in planned:
            live_tri_set.discard(tuple(sorted(tris[ti])))
            if new_t is None:
                tri_alive[ti] = False
                alive_tris -= 1
            else:
                live_tri_set.add(tuple(sorted(new_t)))
                tris[ti] = new_t
                for x in new_t:
                    vertex_tris[x].add(ti)
                group_tris[keep].add(ti)
        for d in members[drop]:
            vertex_tris[d] = set()
        members[drop] = []
        group_tris[drop] = set()
        # Refresh group edges touching `keep`.
        neighbors = set()
        for ti in group_tris[keep]:
            if not tri_alive[ti]:
                continue
            for x in tris[ti]:
                g = group_of[x]
                if g != keep and alive_g[g]:
                    neighbors.add(g)
        for g in sorted(neighbors):
            k2 = gkey(keep, g)
            c2, surv2, _ = edge_cost(k2[0], k2[1])
            if c2 == INF:
                continue
            other = k2[1] if surv2 == k2[0] else k2[0]
            edge_gen[k2] = edge_gen.get(k2, 0) + 1
            heapq.heappush(heap, (c2, next(seq), surv2, other, edge_gen[k2]))
        collapses += 1

    # Only vertices referenced by a surviving face reach the output: retired
    # copies and the corners stranded by removed faces are swept here.
    alive_v = [False] * nv
    for ti, t in enumerate(tris):
        if tri_alive[ti]:
            for x in t:
                alive_v[x] = True

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
        "source_triangles": orig_n_tri0,
        "source_vertices": orig_n_src,
        "welded_vertices": welded_verts,
        "dropped_degenerate": dropped_degenerate,
        "reduction_percent": round(100.0 * (1.0 - delivered / orig_n_tri0), 2) if orig_n_tri0 else 0.0,
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
        source_vertex=[first_old[w] for w in src_idx],
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
