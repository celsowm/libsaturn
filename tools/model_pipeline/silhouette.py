#!/usr/bin/env python3
"""Silhouette-aware importance and validation (host-only).

For low-resolution console rendering the visible outline matters more than
internal tessellation. This module implements both sides deterministically:

- importance: vertices/edges that repeatedly form the silhouette across a
  fixed set of camera directions and representative animation poses cost
  more to collapse (fed into simplification as per-vertex weights);
- validation: deterministic software-rasterized silhouette masks compare
  source vs simplified outlines (IoU) across the same views and poses.

No asset-specific camera angles: directions come from a Fibonacci sphere
and poses from even authored-sample coverage.
"""

from __future__ import annotations

import math

from .gltf import GltfError


def fibonacci_views(n_views: int) -> list[tuple[float, float, float]]:
    """Approximately uniform deterministic camera directions."""
    if n_views < 1:
        raise GltfError("silhouette views must be >= 1")
    golden = math.pi * (3.0 - math.sqrt(5.0))
    out = []
    for i in range(n_views):
        y = 1.0 - (2.0 * i + 1.0) / n_views
        r = math.sqrt(max(0.0, 1.0 - y * y))
        theta = golden * i
        out.append((math.cos(theta) * r, y, math.sin(theta) * r))
    return out


def _face_normals(np, positions, triangles):
    p = np.asarray(positions, dtype=np.float64)
    t = np.asarray(triangles, dtype=np.int64)
    a, b, c = p[t[:, 0]], p[t[:, 1]], p[t[:, 2]]
    n = np.cross(b - a, c - a)
    length = np.sqrt((n * n).sum(axis=1, keepdims=True))
    length[length == 0.0] = 1.0
    return n / length


def _require_numpy():
    try:
        import numpy as np
    except ModuleNotFoundError as exc:
        raise GltfError("Silhouette analysis needs numpy: pip install numpy") from exc
    return np


def compute_silhouette_importance(
    model,
    poses: list | None = None,
    n_views: int = 16,
    clip=None,
    times: list[float] | None = None,
) -> list[float]:
    """Per-vertex silhouette importance in [0, 1].

    Counts, over deterministic views and poses, how often each vertex
    touches a silhouette edge (adjacent faces flip front/back facing) or an
    open boundary edge. Either ``poses`` (explicit baked positions) or
    ``(clip, times)`` may drive the pose loop; bind pose is used when both
    are absent.
    """
    from .animation import bake_clip_poses

    np = _require_numpy()
    n = len(model.vertices)
    if isinstance(model, object) and hasattr(model, "triangles"):
        triangles = [tuple(t) for t in model.triangles]
    else:
        raise GltfError("silhouette importance needs a model with triangles")
    # Edge -> adjacent triangle indices.
    edge_tris: dict[tuple[int, int], list[int]] = {}
    for ti, (a, b, c) in enumerate(triangles):
        for u, v in ((a, b), (b, c), (c, a)):
            edge_tris.setdefault((u, v) if u < v else (v, u), []).append(ti)
    if poses is None:
        if clip is not None and times is not None and hasattr(model, "is_skinned") and model.is_skinned:
            poses = bake_clip_poses(model, clip, times)
        else:
            poses = [list(model.vertices)]
    views = fibonacci_views(n_views)
    score = [0.0] * n
    total = len(views) * len(poses)
    for pose in poses:
        normals = _face_normals(np, pose, triangles)
        dots = normals @ np.asarray(views, dtype=np.float64).T  # (F, V)
        front = dots > 0.0
        for (u, v), adjacent in edge_tris.items():
            if len(adjacent) == 1:
                hits = n_views  # open boundary: outline from every angle
                score[u] += 0.5 * hits
                score[v] += 0.5 * hits
                continue
            if len(adjacent) != 2:
                continue  # non-manifold fan: no defined silhouette
            f0 = front[adjacent[0]]
            f1 = front[adjacent[1]]
            hits = int((f0 != f1).sum())
            if hits:
                score[u] += hits
                score[v] += hits
    peak = max(score) if score else 0.0
    if peak <= 0.0:
        return [0.0] * n
    return [min(1.0, s / peak) for s in score]


def render_masks(positions, triangles, views, size: int = 48, frame=None):
    """Orthographic coverage masks, one bool array per view (numpy).

    ``frame`` is an optional ``(lo, hi)`` normalization pair. Masks that
    are compared (source vs simplified) MUST share one frame -- normalizing
    each mesh by its own extents registers the two pixel grids differently
    and manufactures IoU damage out of sub-pixel extent differences. When
    omitted, the frame derives from ``positions`` and is returned for reuse
    as ``(masks, (lo, hi))`` via :func:`render_masks_with_frame`.
    """
    np = _require_numpy()
    p = np.asarray(positions, dtype=np.float64)
    t = np.asarray(triangles, dtype=np.int64)
    if frame is None:
        lo, hi = p.min(axis=0), p.max(axis=0)
    else:
        lo, hi = frame
    masks = []
    for view in views:
        v = np.asarray(view, dtype=np.float64)
        v = v / (np.sqrt((v * v).sum()) or 1.0)
        # Orthonormal basis around the view axis.
        helper = np.array([0.0, 1.0, 0.0] if abs(v[1]) < 0.9 else [1.0, 0.0, 0.0])
        u = np.cross(v, helper)
        u = u / (np.sqrt((u * u).sum()) or 1.0)
        w = np.cross(v, u)
        # Project the shared frame corners so both meshes land in one grid.
        corners = np.array(
            [[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
        )
        c2 = np.stack([corners @ u, corners @ w], axis=1)
        flo, fhi = c2.min(axis=0), c2.max(axis=0)
        span = np.maximum(fhi - flo, 1e-9)
        q = np.stack([p @ u, p @ w], axis=1)
        g = (q - flo) / span * (size - 1)
        mask = np.zeros((size, size), dtype=bool)
        for (a, b, c) in t:
            tri = g[[a, b, c]]
            x0 = max(0, int(tri[:, 0].min()))
            x1 = min(size - 1, int(tri[:, 0].max()))
            y0 = max(0, int(tri[:, 1].min()))
            y1 = min(size - 1, int(tri[:, 1].max()))
            if x1 < x0 or y1 < y0:
                continue
            xs = np.arange(x0, x1 + 1)
            ys = np.arange(y0, y1 + 1)
            xx, yy = np.meshgrid(xs, ys)
            px, py = xx.ravel() + 0.5, yy.ravel() + 0.5
            (ax, ay), (bx, by), (cx, cy) = tri
            d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(d) < 1e-12:
                continue
            l0 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / d
            l1 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / d
            l2 = 1.0 - l0 - l1
            inside = (l0 >= -1e-9) & (l1 >= -1e-9) & (l2 >= -1e-9)
            pts = np.stack([xx.ravel()[inside], yy.ravel()[inside]], axis=1)
            mask[pts[:, 1], pts[:, 0]] = True
        masks.append(mask)
    return masks


def _contour_points(mask):
    """Boundary pixel coordinates of a coverage mask (4-neighborhood)."""
    import numpy as np

    inner = mask[1:-1, 1:-1] & mask[:-2, 1:-1] & mask[2:, 1:-1] & mask[1:-1, :-2] & mask[1:-1, 2:]
    edge = np.zeros_like(mask, dtype=bool)
    edge[1:-1, 1:-1] = mask[1:-1, 1:-1] & ~inner
    edge[0, :] |= mask[0, :]
    edge[-1, :] |= mask[-1, :]
    edge[:, 0] |= mask[:, 0]
    edge[:, -1] |= mask[:, -1]
    ys, xs = np.nonzero(edge)
    return np.stack([xs, ys], axis=1).astype(np.float64)


def contour_chamfer(a, b) -> float:
    """Symmetric mean contour distance in pixels (thin-structure friendly).

    IoU collapses on thin limbs (a 1-pixel erosion of a 3-pixel limb reads
    as catastrophic); Chamfer reports the geometric outline shift instead.
    Returns 0.0 for two empty masks, infinity when exactly one is empty.
    """
    import numpy as np

    pa, pb = _contour_points(a), _contour_points(b)
    if len(pa) == 0 and len(pb) == 0:
        return 0.0
    if len(pa) == 0 or len(pb) == 0:
        return float("inf")
    # Contour sets are small (hundreds of points); broadcast is exact.
    d2 = ((pa[:, None, :] - pb[None, :, :]) ** 2).sum(axis=2)
    return float((np.sqrt(d2.min(axis=1)).mean() + np.sqrt(d2.min(axis=0)).mean()) / 2.0)


def silhouette_iou(
    source,
    simplified,
    clip=None,
    times: list[float] | None = None,
    n_views: int = 16,
    size: int = 48,
) -> dict:
    """Mask-IoU comparison of source vs simplified silhouettes.

    Returns mean/min IoU over views x representative poses plus the worst
    (view, time) diagnostic so quality inspection can point at the pose and
    angle that produced the largest outline damage.
    """
    from .animation import bake_clip_poses
    from .metrics import simplified_as_source

    np = _require_numpy()
    views = fibonacci_views(n_views)
    if times is None:
        if clip is not None and hasattr(source, "is_skinned") and source.is_skinned:
            from .metrics import sample_times_for_importance

            all_times = sample_times_for_importance(clip)
            step = max(1, len(all_times) // 8)
            times = all_times[::step][:8] or all_times[:1]
        else:
            times = [0.0]
    simp_view = simplified_as_source(simplified, source)
    ious = []
    chamfers = []
    coverages = []
    worst = {"iou": 1.0, "view": 0, "time": 0.0}
    worst_chamfer = {"chamfer_px": 0.0, "view": 0, "time": 0.0}
    for t in times:
        if clip is not None and simp_view.is_skinned:
            sp = bake_clip_poses(source, clip, [t])[0]
            pp = bake_clip_poses(simp_view, clip, [t])[0]
        else:
            sp, pp = source.vertices, simplified.positions
        # One shared frame per pose: the source extents register both grids.
        np_src = np.asarray(sp, dtype=np.float64)
        frame = (np_src.min(axis=0), np_src.max(axis=0))
        src_masks = render_masks(sp, source.triangles, views, size, frame)
        smp_masks = render_masks(pp, simplified.triangles, views, size, frame)
        for vi, (a, b) in enumerate(zip(src_masks, smp_masks)):
            inter = int((a & b).sum())
            union = int((a | b).sum())
            iou = inter / union if union else 1.0
            ious.append(iou)
            if iou < worst["iou"]:
                worst = {"iou": float(iou), "view": vi, "time": float(t)}
            ch = contour_chamfer(a, b)
            chamfers.append(ch)
            if ch > worst_chamfer["chamfer_px"]:
                worst_chamfer = {"chamfer_px": float(ch), "view": vi, "time": float(t)}
            na, nb = int(a.sum()), int(b.sum())
            coverages.append(nb / na if na else (1.0 if nb == 0 else 0.0))
    arr = np.asarray(ious, dtype=np.float64)
    cha = np.asarray([c for c in chamfers if c != float("inf")], dtype=np.float64)
    cov = np.asarray(coverages, dtype=np.float64)
    return {
        "iou_mean": float(arr.mean()) if len(arr) else 1.0,
        "iou_min": float(arr.min()) if len(arr) else 1.0,
        "chamfer_px_mean": float(cha.mean()) if len(cha) else 0.0,
        "chamfer_px_max": float(cha.max()) if len(cha) else 0.0,
        "coverage_mean": float(cov.mean()) if len(cov) else 1.0,
        "coverage_min": float(cov.min()) if len(cov) else 1.0,
        "mask_size": size,
        "views": n_views,
        "poses": len(times),
        "worst": worst,
        "worst_chamfer": worst_chamfer,
    }
