#!/usr/bin/env python3
"""Animation-aware quality metrics and automatic target search (host-only).

Every metric here is normalized by model scale (posed bounding-box
diagonal) so thresholds are scale-independent, and every evaluation runs
across sampled animation poses rather than the bind pose alone. The
quality presets are explicit data structures; the target search grows the
triangle count deterministically until the active preset passes instead of
forcing a damaging fixed count.
"""

from __future__ import annotations

import math

from .gltf import GltfError
from .model import SourceModel


def _require_numpy():
    try:
        import numpy as np
    except ModuleNotFoundError as exc:
        raise GltfError("Quality metrics need numpy: pip install numpy") from exc
    return np


# ----------------------------------------------------------------------
# Quality presets (explicit data, not scattered magic values)
# ----------------------------------------------------------------------
#
# Starting points to calibrate against tests and the acceptance asset:
# - conservative: seam/boundary protection strongest, max animated surface
#   error around <= 0.5% of bbox diagonal, strong silhouette preservation.
# - balanced: max error around <= 0.75-1.0%, strong seam protection,
#   moderate/strong silhouette preservation.
# - aggressive: higher geometric tolerance; seams/material boundaries still
#   protected by default (the backend locks them unless asked otherwise).

# Calibrated console-scale thresholds (documented, not magic): worst-vertex
# error is dominated by intentionally shed sub-pixel appendages (fingers are
# ~2 screen pixels on the 320-wide target), so p95/mean carry the binding
# surface gates while max stays a lenient catastrophic cap. Chamfer (outline
# shift in 48-grid pixels) is the perceptually binding silhouette gate; IoU
# is a lenient disaster floor. Measured acceptance behavior: 300 tris gives
# p95 ~2.2% / mean ~0.8% / max ~3.1% / chamfer ~1.7px; 200 tris fails p95.
QUALITY_PRESETS = {
    "conservative": {
        "max_surface_error": 0.040,
        "p95_surface_error": 0.010,
        "mean_surface_error": 0.004,
        "silhouette_iou_min": 0.50,
        "chamfer_px_max": 1.5,
        "max_flipped_facets": 0,
    },
    "balanced": {
        "max_surface_error": 0.050,
        "p95_surface_error": 0.025,
        "mean_surface_error": 0.010,
        "silhouette_iou_min": 0.35,
        "chamfer_px_max": 2.5,
        "max_flipped_facets": 0,
    },
    "aggressive": {
        "max_surface_error": 0.080,
        "p95_surface_error": 0.040,
        "mean_surface_error": 0.015,
        "silhouette_iou_min": 0.20,
        "chamfer_px_max": 4.0,
        "max_flipped_facets": 0,
    },
}


# ----------------------------------------------------------------------
# Animation importance
# ----------------------------------------------------------------------


def sample_times_for_importance(clip, max_samples: int = 64) -> list[float]:
    """Deterministic pose sampling: every authored frame when the clip is
    small, otherwise an even subsample plus the key extrema."""
    from .animation import clip_sample_times

    times = clip_sample_times(clip)
    if len(times) <= max_samples:
        return times
    step = (len(times) - 1) / (max_samples - 1)
    picked = sorted({times[round(i * step)] for i in range(max_samples)})
    for extreme in (times[0], times[-1]):
        if extreme not in picked:
            picked.append(extreme)
    return sorted(picked)


def compute_animation_importance(
    model: SourceModel, clip, times: list[float] | None = None
) -> list[float]:
    """Per-vertex animation importance in [0, 1] from generic motion signals.

    High-motion articulation regions (elbows, knees, shoulders, hips, ...)
    become expensive to damage through these metrics, never through
    bone-name heuristics:

    - displacement range across sampled poses (limbs travel far),
    - deviation from whole-mesh rigid behavior (joints bend, torsos ride),
    - normal variation where normals exist (folding cloth/skin),
    - split skin weights (vertices blended across joints deform most).
    """
    from .animation import bake_clip_poses, evaluate_normals

    if not model.is_skinned or clip is None:
        return [0.0] * len(model.vertices)
    if times is None:
        times = sample_times_for_importance(clip)
    if not times:
        return [0.0] * len(model.vertices)
    np = _require_numpy()
    rest = np.asarray(model.vertices, dtype=np.float64)
    poses = [np.asarray(p, dtype=np.float64) for p in bake_clip_poses(model, clip, times)]

    n = len(model.vertices)
    disp_range = np.zeros(n)
    rigid_dev = np.zeros(n)
    for p in poses:
        d = p - rest
        disp_range = np.maximum(disp_range, np.sqrt((d * d).sum(axis=1)))
        mean_d = d.mean(axis=0, keepdims=True)
        rigid_dev = np.maximum(rigid_dev, np.sqrt(((d - mean_d) ** 2).sum(axis=1)))

    def _norm01(v):
        peak = v.max() if v.size else 0.0
        return v / peak if peak > 0 else np.zeros_like(v)

    d1 = _norm01(disp_range)
    d2 = _norm01(rigid_dev)

    if model.normals is not None:
        rest_n = np.asarray(model.normals, dtype=np.float64)
        worst = np.ones(n)
        for t in times:
            nn = evaluate_normals(model, clip, t)
            if nn is None:
                break
            cur = np.asarray(nn, dtype=np.float64)
            dots = (cur * rest_n).sum(axis=1).clip(-1.0, 1.0)
            worst = np.minimum(worst, dots)
        d3 = _norm01(1.0 - worst)
    else:
        d3 = np.zeros(n)

    split = np.zeros(n)
    for i, w in enumerate(model.weights):
        split[i] = max(0.0, 1.0 - max(w)) / 0.75
    split = np.clip(split, 0.0, 1.0)

    importance = 0.45 * d1 + 0.30 * d2 + 0.15 * d3 + 0.10 * split
    return [float(min(max(v, 0.0), 1.0)) for v in importance]


# ----------------------------------------------------------------------
# Simplified mesh as an evaluatable source view
# ----------------------------------------------------------------------


def simplified_as_source(simplified, template: SourceModel) -> SourceModel:
    """View a SimplifiedMesh as a SourceModel sharing the template skeleton.

    Bind positions/attributes come from the simplified subset; hierarchy,
    skins and clips are shared so host skinning evaluates simplified poses
    with the exact contract used for the source.
    """
    view = SourceModel()
    view.vertices = list(simplified.positions)
    view.normals = list(simplified.normals) if simplified.normals else None
    view.uvs = list(simplified.uvs)
    view.triangles = list(simplified.triangles)
    view.tri_materials = list(simplified.tri_materials)
    view.materials = list(template.materials)
    view.textures = list(template.textures)
    view.joints = list(simplified.joints) if simplified.joints else []
    view.weights = list(simplified.weights) if simplified.weights else []
    view.nodes = template.nodes
    view.skins = template.skins
    view.clips = template.clips
    view.mesh_node = template.mesh_node
    view.skin_index = template.skin_index
    return view


def posed_bbox_diagonal(model: SourceModel, clip, times: list[float]) -> float:
    """Bounding-box diagonal over all sampled poses (scale normalization)."""
    from .animation import bake_clip_poses

    np = _require_numpy()
    all_pts = [np.asarray(model.vertices, dtype=np.float64)]
    if clip is not None and model.is_skinned:
        all_pts.extend(
            np.asarray(p, dtype=np.float64) for p in bake_clip_poses(model, clip, times)
        )
    cloud = np.concatenate(all_pts, axis=0)
    lo, hi = cloud.min(axis=0), cloud.max(axis=0)
    return float(np.sqrt(((hi - lo) ** 2).sum())) or 1.0


# ----------------------------------------------------------------------
# Animated surface error (nearest-point, across poses)
# ----------------------------------------------------------------------


def _point_triangle_dist2_chunk(np, P, A, B, C):
    """Squared distances, P (N,3), A/B/C (M,3) -> (N,M). Ericson 5.1.5."""
    ab = B - A
    ac = C - A
    ap = P[:, None, :] - A[None, :, :]
    d1 = (ab[None, :, :] * ap).sum(axis=2)
    d2 = (ac[None, :, :] * ap).sum(axis=2)
    bp = P[:, None, :] - B[None, :, :]
    d3 = (ab[None, :, :] * bp).sum(axis=2)
    d4 = (ac[None, :, :] * bp).sum(axis=2)
    cp = P[:, None, :] - C[None, :, :]
    d5 = (ab[None, :, :] * cp).sum(axis=2)
    d6 = (ac[None, :, :] * cp).sum(axis=2)
    out = np.empty((P.shape[0], A.shape[0]), dtype=np.float64)
    out.fill(np.inf)
    # Vertex regions.
    va = (d1 <= 0) & (d2 <= 0)
    out[va] = (ap[va] * ap[va]).sum(axis=1)
    vb = (d3 >= 0) & (d4 <= d3)
    out[vb] = (bp[vb] * bp[vb]).sum(axis=1)
    vc = (d6 >= 0) & (d5 <= d6)
    out[vc] = (cp[vc] * cp[vc]).sum(axis=1)
    # Edge regions (mutually exclusive with the vertex regions).
    vc_ab = d1 * d4 - d3 * d2
    eab = (~va) & (~vb) & (~vc) & (vc_ab <= 0) & (d1 >= 0) & (d3 <= 0)
    qi_ab = np.nonzero(eab)
    den_ab = d1[eab] - d3[eab]
    den_ab[den_ab == 0.0] = 1.0  # measure-zero: point already at A
    v = d1[eab] / den_ab
    q = A[qi_ab[1]] + ab[qi_ab[1]] * v[:, None]
    diff = P[qi_ab[0]] - q
    out[eab] = (diff * diff).sum(axis=1)
    vc_ac = d2 * d5 - d1 * d6
    eac = (~va) & (~vb) & (~vc) & (~eab) & (vc_ac <= 0) & (d2 >= 0) & (d6 <= 0)
    qi_ac = np.nonzero(eac)
    den_ac = d2[eac] - d6[eac]
    den_ac[den_ac == 0.0] = 1.0
    w = d2[eac] / den_ac
    q = A[qi_ac[1]] + ac[qi_ac[1]] * w[:, None]
    diff = P[qi_ac[0]] - q
    out[eac] = (diff * diff).sum(axis=1)
    vc_bc = d3 * d6 - d5 * d4
    ebc = (~vb) & (~vc) & (~va) & (~eab) & (~eac) & (vc_bc <= 0) & ((d4 - d3) >= 0) & ((d5 - d6) >= 0)
    den_bc = (d4[ebc] - d3[ebc]) + (d5[ebc] - d6[ebc])
    den_bc[den_bc == 0.0] = 1.0
    w = (d4[ebc] - d3[ebc]) / den_bc
    qi = np.nonzero(ebc)
    q = B[qi[1]] + (C - B)[qi[1]] * w[:, None]
    diff = P[qi[0]] - q
    out[ebc] = (diff * diff).sum(axis=1)
    # Face interior.
    handled = va | vb | vc | eab | eac | ebc
    rem = ~handled
    # Ericson: va = vc_bc, vb = vc_ac, vc = vc_ab; v = vb/sum, w = vc/sum.
    denom = 1.0 / (vc_ab[rem] + vc_ac[rem] + vc_bc[rem] + 1e-30)
    v = (vc_ac[rem]) * denom
    w = (vc_ab[rem]) * denom
    qi = np.nonzero(rem)
    q = (
        A[qi[1]]
        + ab[qi[1]] * v[:, None]
        + ac[qi[1]] * w[:, None]
    )
    diff = P[qi[0]] - q
    out[rem] = (diff * diff).sum(axis=1)
    return out


def surface_error_stats(
    source: SourceModel,
    simplified,
    clip,
    times: list[float] | None = None,
    tri_chunk: int = 64,
) -> dict:
    """Nearest-point animated surface error over sampled poses.

    Samples original vertices (plus deterministic barycentric face centers
    for coverage) and measures distance to the simplified posed triangles.
    Returns mean/RMS/p95/max normalized by the posed bbox diagonal.
    """
    from .animation import bake_clip_poses

    np = _require_numpy()
    if times is None:
        times = sample_times_for_importance(clip) if clip is not None else [0.0]
    simp_view = simplified_as_source(simplified, source)
    if not simplified.triangles or not simplified.positions:
        raise GltfError("simplification produced an empty mesh")
    diag = posed_bbox_diagonal(source, clip, times)

    # Deterministic extra surface samples: barycenters of an even subset of
    # faces. They are interpolated from POSED corners per time step (the
    # posed face is the linear blend of its posed corners), never skinned
    # independently, which would leave the surface on bent faces.
    step = max(1, len(source.triangles) // 512)
    sample_tris = source.triangles[::step]

    dists_all = []
    worst = {"time": 0.0, "vertex": None, "error": 0.0}
    n_src_verts = len(source.vertices)
    for t in times:
        if clip is not None and simp_view.is_skinned:
            simp_pose = np.asarray(
                bake_clip_poses(simp_view, clip, [t])[0], dtype=np.float64
            )
            src_pose = np.asarray(
                bake_clip_poses(source, clip, [t])[0], dtype=np.float64
            )
            if sample_tris:
                tri_idx = np.asarray(sample_tris, dtype=np.int64)
                extra = (
                    src_pose[tri_idx[:, 0]]
                    + src_pose[tri_idx[:, 1]]
                    + src_pose[tri_idx[:, 2]]
                ) / 3.0
                src_pose = np.concatenate([src_pose, extra], axis=0)
        else:
            simp_pose = np.asarray(simplified.positions, dtype=np.float64)
            src_pose = np.asarray(source.vertices, dtype=np.float64)
            if sample_tris:
                sv = np.asarray(source.vertices, dtype=np.float64)
                tri_idx = np.asarray(sample_tris, dtype=np.int64)
                extra = (sv[tri_idx[:, 0]] + sv[tri_idx[:, 1]] + sv[tri_idx[:, 2]]) / 3.0
                src_pose = np.concatenate([src_pose, extra], axis=0)
        tri_idx = np.asarray(simplified.triangles, dtype=np.int64)
        A = simp_pose[tri_idx[:, 0]]
        B = simp_pose[tri_idx[:, 1]]
        C = simp_pose[tri_idx[:, 2]]
        best = np.full(src_pose.shape[0], np.inf)
        for start in range(0, len(tri_idx), tri_chunk):
            d2 = _point_triangle_dist2_chunk(
                np, src_pose, A[start : start + tri_chunk], B[start : start + tri_chunk], C[start : start + tri_chunk]
            )
            best = np.minimum(best, d2.min(axis=1))
        dist = np.sqrt(np.maximum(best, 0.0))
        dists_all.append(dist)
        local_worst = int(dist.argmax())
        if float(dist[local_worst]) > worst["error"]:
            worst = {
                "time": float(t),
                "vertex": local_worst if local_worst < n_src_verts else None,
                "error": float(dist[local_worst]),
            }
    dists = np.concatenate(dists_all, axis=0) / diag
    flat = np.sort(dists)
    worst["error"] = worst["error"] / diag
    return {
        "mean": float(dists.mean()),
        "rms": float(np.sqrt((dists * dists).mean())),
        "p95": float(flat[min(len(flat) - 1, int(0.95 * len(flat)))]),
        "max": float(flat[-1]) if len(flat) else 0.0,
        "samples": int(dists.size),
        "bbox_diagonal": float(diag),
        "worst": worst,
    }


def normal_error_stats(source, simplified, clip, times=None) -> dict | None:
    """Angular deviation (degrees) of simplified facets vs covered surface.

    For each simplified triangle, the source triangles around its corners
    are the surface it replaced. Corners are matched by POSITION, not by
    vertex index: split-vertex exports give every side of a corner its own
    copy, and one copy's adjacency is often a single sliver of the surface.
    The error is the smallest angle between the posed simplified facet and
    those posed source facets. No nearest-point matching is involved, so
    thin closed limbs cannot manufacture orientation flips: at full
    resolution every facet trivially matches its own source face for 0.0
    degrees.

    Tilt itself is expected -- one facet replacing a curved patch cannot
    match every facet it covers -- so the gate is ``flipped_facets``: facets
    turned more than 90 degrees from all of the surface they replaced in
    some sampled pose. Those are the ones backface culling drops, leaving a
    see-through hole. (Textured VDP1 faces bypass runtime lighting, so this
    gate guards culling consistency.)
    """
    from .animation import bake_clip_poses

    np = _require_numpy()
    simp_view = simplified_as_source(simplified, source)
    if source.normals is None or simp_view.normals is None:
        return None
    if times is None:
        times = sample_times_for_importance(clip) if clip is not None else [0.0]
    src_tris = np.asarray(source.triangles, dtype=np.int64)
    simp_tris = np.asarray(simplified.triangles, dtype=np.int64)
    def pos_key(p):
        return (round(p[0], 9), round(p[1], 9), round(p[2], 9))

    adjacency: dict[tuple, set[int]] = {}
    for k, tri in enumerate(source.triangles):
        for v in tri:
            adjacency.setdefault(pos_key(source.vertices[v]), set()).add(k)
    # Candidate source faces per simplified triangle (bind-time, sorted).
    cover: list[list[int]] = []
    for tri in simplified.triangles:
        cand: set[int] = set()
        for x in tri:
            cand.update(adjacency.get(pos_key(simplified.positions[x]), ()))
        cover.append(sorted(cand))
    angs = []
    for t in times:
        if clip is not None and source.is_skinned:
            sp = np.asarray(bake_clip_poses(source, clip, [t])[0], dtype=np.float64)
            pp = np.asarray(
                bake_clip_poses(simp_view, clip, [t])[0], dtype=np.float64
            )
        else:
            sp = np.asarray(source.vertices, dtype=np.float64)
            pp = np.asarray(simplified.positions, dtype=np.float64)
        sA, sB, sC = sp[src_tris[:, 0]], sp[src_tris[:, 1]], sp[src_tris[:, 2]]
        src_n = np.cross(sB - sA, sC - sA)
        s_len = np.sqrt((src_n * src_n).sum(axis=1, keepdims=True))
        s_len[s_len == 0.0] = 1.0
        src_n = src_n / s_len
        pA, pB, pC = pp[simp_tris[:, 0]], pp[simp_tris[:, 1]], pp[simp_tris[:, 2]]
        smp_n = np.cross(pB - pA, pC - pA)
        p_len = np.sqrt((smp_n * smp_n).sum(axis=1, keepdims=True))
        p_len[p_len == 0.0] = 1.0
        smp_n = smp_n / p_len
        best = np.ones(len(simp_tris))
        for i, cand in enumerate(cover):
            if not cand:
                continue
            dots = (src_n[cand] * smp_n[i]).sum(axis=1)
            best[i] = dots.max()
        angs.append(np.degrees(np.arccos(best.clip(-1.0, 1.0))))
    all_a = np.concatenate(angs, axis=0)
    per_face = np.stack(angs, axis=0).max(axis=0) if angs else np.zeros(0)
    return {
        "flipped_facets": int((per_face > 90.0).sum()),
        "mean_deg": float(all_a.mean()),
        "p95_deg": float(np.sort(all_a)[min(len(all_a) - 1, int(0.95 * len(all_a)))]),
        "max_deg": float(all_a.max()) if len(all_a) else 0.0,
        "samples": int(all_a.size),
    }


def _resolve_preset(preset) -> tuple[str, dict]:
    if isinstance(preset, dict):
        return str(preset.get("__name__", "custom")), preset
    if preset not in QUALITY_PRESETS:
        raise GltfError(f"unknown quality preset {preset!r}")
    return str(preset), QUALITY_PRESETS[preset]


def integrity_report(source: SourceModel, simplified) -> dict:
    """UV/material integrity: subset simplification must never invent or mix."""
    src_uvs = {tuple(u) for u in source.uvs} if source.uvs else set()
    uv_violations = 0
    if source.uvs:
        for u in simplified.uvs:
            if tuple(u) not in src_uvs:
                uv_violations += 1
    # Material exclusivity: no output triangle may mix vertices that are
    # exclusive to different source materials.
    mat_of: dict[int, set[int]] = {}
    for (a, b, c), mt in zip(source.triangles, source.tri_materials):
        for v in (a, b, c):
            mat_of.setdefault(v, set()).add(mt)
    exclusive: dict[int, int] = {
        v: next(iter(s)) for v, s in mat_of.items() if len(s) == 1
    }
    mat_violations = 0
    for (a, b, c) in simplified.triangles:
        owners = {
            exclusive.get(simplified.source_vertex[v])
            for v in (a, b, c)
        } - {None}
        if len(owners) > 1:
            mat_violations += 1
    return {
        "uv_violations": uv_violations,
        "material_violations": mat_violations,
        "crack_edges": max(
            0,
            open_edge_count(simplified.positions, simplified.triangles)
            - open_edge_count(source.vertices, source.triangles),
        ),
    }


def open_edge_count(positions, triangles) -> int:
    """Edges used by exactly one face once coincident vertex copies weld.

    Split-vertex exports (flat normals, UV seams) have an open edge on every
    raw index, so openness is judged by position: a simplification that adds
    open edges has torn the surface, which shows as see-through cracks.
    """
    key_of = [(round(p[0], 9), round(p[1], 9), round(p[2], 9)) for p in positions]
    counts: dict[tuple, int] = {}
    for tri in triangles:
        for i in range(3):
            a, b = key_of[tri[i]], key_of[tri[(i + 1) % 3]]
            if a == b:
                continue
            edge = (a, b) if a < b else (b, a)
            counts[edge] = counts.get(edge, 0) + 1
    return sum(1 for n in counts.values() if n == 1)


# ----------------------------------------------------------------------
# Candidate evaluation and automatic target search
# ----------------------------------------------------------------------


def evaluate_candidate(
    source: SourceModel,
    simplified,
    clip,
    preset="balanced",
    times: list[float] | None = None,
    silhouette_views: int = 16,
) -> dict:
    """Full quality report for one simplified candidate plus PASS/FAIL."""
    from . import silhouette as sil_mod

    preset_name, preset_dict = _resolve_preset(preset)
    preset = preset_dict
    if times is None:
        times = sample_times_for_importance(clip) if clip is not None else [0.0]
    surf = surface_error_stats(source, simplified, clip, times)
    norm = normal_error_stats(source, simplified, clip, times)
    integrity = integrity_report(source, simplified)
    sil_poses = times[:: max(1, len(times) // 8)][:8] if times else [0.0]
    sil = sil_mod.silhouette_iou(
        source, simplified, clip, sil_poses, n_views=silhouette_views
    )
    failing: list[str] = []
    if surf["max"] > preset["max_surface_error"]:
        failing.append(
            f"max surface error {surf['max']:.4f} > {preset['max_surface_error']:.4f}"
        )
    if surf["p95"] > preset["p95_surface_error"]:
        failing.append(
            f"p95 surface error {surf['p95']:.4f} > {preset['p95_surface_error']:.4f}"
        )
    if surf["mean"] > preset["mean_surface_error"]:
        failing.append(
            f"mean surface error {surf['mean']:.4f} > {preset['mean_surface_error']:.4f}"
        )
    if sil["iou_min"] < preset["silhouette_iou_min"]:
        failing.append(
            f"silhouette IoU {sil['iou_min']:.4f} < {preset['silhouette_iou_min']:.4f}"
        )
    if sil["chamfer_px_max"] > preset["chamfer_px_max"]:
        failing.append(
            f"silhouette chamfer {sil['chamfer_px_max']:.2f}px > {preset['chamfer_px_max']:.2f}px"
        )
    if norm is not None and norm["flipped_facets"] > preset["max_flipped_facets"]:
        failing.append(
            f"{norm['flipped_facets']} facets flipped past 90deg "
            f"(> {preset['max_flipped_facets']}; culling would open holes)"
        )
    if integrity["uv_violations"]:
        failing.append(f"{integrity['uv_violations']} UV integrity violations")
    if integrity["material_violations"]:
        failing.append(f"{integrity['material_violations']} material violations")
    if integrity["crack_edges"]:
        failing.append(
            f"{integrity['crack_edges']} crack edges (surface torn open between vertex copies)"
        )
    return {
        "preset": preset_name,
        "surface": surf,
        "normals": norm,
        "silhouette": sil,
        "integrity": integrity,
        "sampled_poses": len(times),
        "passed": not failing,
        "failing_gates": failing,
    }


def search_upward(
    source: SourceModel,
    clip,
    requested_triangles: int,
    preset="balanced",
    simplify_options=None,
    anim_importance=None,
    sil_importance=None,
    times: list[float] | None = None,
    pose_positions: list | None = None,
    max_steps: int = 8,
    enforce_floor: bool = False,
) -> tuple:
    """Smallest triangle count in [requested, source] passing quality gates.

    Bounded deterministic binary search: simplify candidates from the source
    each time (never re-simplify a simplification, which compounds errors),
    evaluate across animation poses, and grow on failure. ``pose_positions``
    (baked once by the caller) drives worst-pose collapse costs inside every
    candidate simplification. Returns ``(simplified, quality_report)``.
    Raises GltfError when even the full source mesh cannot pass (a tool bug
    or an impossible preset, never a silent force-down).

    When ``enforce_floor`` is set (explicit numeric --simplify target), the
    requested count is a floor instead of a starting hint: the search never
    slides below it, so an explicit request is honored when it passes.
    """
    from . import simplification as simp_mod

    if requested_triangles < 1:
        raise GltfError("search_upward: requested_triangles must be >= 1")
    opts = simplify_options
    preset_name, _ = _resolve_preset(preset)
    if times is None:
        times = sample_times_for_importance(clip) if clip is not None else [0.0]

    def _run(target: int):
        o = simp_mod.SimplificationOptions(
            target_triangles=target,
            quality=opts.quality if opts else "balanced",
            preserve_uv=opts.preserve_uv if opts else True,
            preserve_normals=opts.preserve_normals if opts else True,
            preserve_boundaries=opts.preserve_boundaries if opts else True,
            animation_weight=opts.animation_weight if opts else 1.0,
            silhouette_weight=opts.silhouette_weight if opts else 1.0,
            material_weights=opts.material_weights if opts else None,
        )
        simp = simp_mod.simplify(
            source,
            anim_importance=anim_importance,
            sil_importance=sil_importance,
            options=o,
            pose_positions=pose_positions,
        )
        rep = evaluate_candidate(source, simp, clip, preset, times)
        return simp, rep

    lo = min(requested_triangles, len(source.triangles))
    simp, rep = _run(lo)
    if rep["passed"]:
        if enforce_floor:
            # Explicit target: deliver it when it passes; never shrink.
            return simp, rep
        # Try lower counts while quality still passes (bounded steps).
        best, best_rep = simp, rep
        floor = max(1, lo // 2)
        for _ in range(max_steps):
            if lo <= floor:
                break
            mid = (lo + floor) // 2
            cand, cand_rep = _run(mid)
            if cand_rep["passed"] and len(cand.triangles) < len(best.triangles):
                best, best_rep = cand, cand_rep
                lo = mid
            else:
                floor = mid
            if lo - floor <= 1:
                break
        return best, best_rep
    # Grow: binary search the smallest passing count in (lo, source].
    hi = len(source.triangles)
    _, hi_rep = _run(hi)
    if not hi_rep["passed"]:
        raise GltfError(
            f"quality preset {preset_name!r} fails even at full resolution "
            f"({hi} triangles): {hi_rep['failing_gates']}"
        )
    best, best_rep = None, hi_rep
    for _ in range(max_steps + 4):
        if hi - lo <= 1:
            break
        mid = (lo + hi) // 2
        cand, cand_rep = _run(mid)
        delivered = len(cand.triangles)
        if cand_rep["passed"]:
            best, best_rep = cand, cand_rep
            hi = delivered
        else:
            lo = max(lo + 1, delivered)
    if best is None:
        best, best_rep = _run(hi)
    return best, best_rep
