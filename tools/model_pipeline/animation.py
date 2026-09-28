#!/usr/bin/env python3
"""Deterministic host-side skeletal-animation evaluator.

Implements the glTF skinning contract offline so the Saturn runtime never
pays skeletal-skinning cost: node local transforms, global hierarchy,
animation-channel sampling (STEP/LINEAR, normalized quaternion
interpolation), skin matrices from inverse bind matrices, and weighted
vertex/normal evaluation with clean looping-clip handling.

All arithmetic here is host float; Saturn fixed-point conversion happens in
the pose-baking stage, where quantization error is measured and reported.
"""

from __future__ import annotations

import bisect
import multiprocessing
import math
import os
import sys
from concurrent.futures import ProcessPoolExecutor

from .gltf import GltfError, trs_to_matrix
from .model import AnimationClip, SourceModel

IDENTITY = [
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
]

# Process startup is significant for small assets. The cut-off is expressed
# in vertex-frame evaluations, the dominant Python work in linear-blend
# skinning. Keep a conservative worker cap to bound memory on large hosts.
_PARALLEL_MIN_VERTEX_FRAMES = 1_500_000
_PARALLEL_MAX_WORKERS = 2
_POSE_WORKER_MODEL = None
_POSE_WORKER_CLIPS = None


def _init_pose_worker(vertices, joints, weights, nodes, skins, skin_index, clips):
    """Install only data needed for skinning, excluding GLB and texture data."""
    global _POSE_WORKER_MODEL, _POSE_WORKER_CLIPS
    from .model import SourceModel

    _POSE_WORKER_MODEL = SourceModel(
        vertices=vertices,
        joints=joints,
        weights=weights,
        nodes=nodes,
        skins=skins,
        skin_index=skin_index,
    )
    _POSE_WORKER_CLIPS = clips


def _evaluate_pose_worker(task):
    clip_index, _frame_index, time = task
    return evaluate_positions(
        _POSE_WORKER_MODEL, _POSE_WORKER_CLIPS[clip_index], time
    )


def _automatic_worker_count(model, frame_count: int) -> int:
    work = len(model.vertices) * frame_count
    if (not model.is_skinned or work < _PARALLEL_MIN_VERTEX_FRAMES
            or frame_count < 4):
        return 1
    main_file = getattr(sys.modules.get("__main__"), "__file__", None)
    if not main_file or str(main_file).startswith("<"):
        # Spawn cannot import stdin/interactive entry points safely.
        return 1
    cpus = os.cpu_count() or 1
    return max(1, min(cpus, _PARALLEL_MAX_WORKERS, frame_count))


def mat_mult(a: list[float] | tuple, b: list[float] | tuple) -> list[float]:
    """Column-major 4x4 multiply: out = a @ b."""
    out = [0.0] * 16
    for col in range(4):
        for row in range(4):
            s = 0.0
            for k in range(4):
                s += a[k * 4 + row] * b[col * 4 + k]
            out[col * 4 + row] = s
    return out


def mat_vec(m: list[float] | tuple, v: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = v
    return (
        m[0] * x + m[4] * y + m[8] * z + m[12],
        m[1] * x + m[5] * y + m[9] * z + m[13],
        m[2] * x + m[6] * y + m[10] * z + m[14],
    )


def mat_vec_dir(m: list[float] | tuple, v: tuple[float, float, float]) -> tuple[float, float, float]:
    """Transform a direction (no translation), for normals."""
    x, y, z = v
    return (
        m[0] * x + m[4] * y + m[8] * z,
        m[1] * x + m[5] * y + m[9] * z,
        m[2] * x + m[6] * y + m[10] * z,
    )


def quat_normalize(q: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    n = math.sqrt(q[0] ** 2 + q[1] ** 2 + q[2] ** 2 + q[3] ** 2)
    if n <= 0.0:
        return (0.0, 0.0, 0.0, 1.0)
    return (q[0] / n, q[1] / n, q[2] / n, q[3] / n)


def quat_nlerp(
    a: tuple[float, float, float, float], b: tuple[float, float, float, float], t: float
) -> tuple[float, float, float, float]:
    """Normalized lerp with hemisphere fix: deterministic and loop-safe."""
    dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]
    s = 1.0 if dot >= 0.0 else -1.0
    return quat_normalize(
        (
            a[0] * (1.0 - t) + s * b[0] * t,
            a[1] * (1.0 - t) + s * b[1] * t,
            a[2] * (1.0 - t) + s * b[2] * t,
            a[3] * (1.0 - t) + s * b[3] * t,
        )
    )


def sample_channel(times: list[float], values: list, interp: str, t: float):
    """Sample one channel at time ``t`` (clamped to the authored range)."""
    if not times:
        raise GltfError("animation channel has no keyframes")
    if t <= times[0]:
        v = values[0]
        return tuple(v) if isinstance(v, tuple) else v
    if t >= times[-1]:
        v = values[-1]
        return tuple(v) if isinstance(v, tuple) else v
    i = bisect.bisect_right(times, t) - 1
    t0, t1 = times[i], times[i + 1]
    v0, v1 = values[i], values[i + 1]
    if interp == "STEP" or t1 <= t0:
        return tuple(v0) if isinstance(v0, tuple) else v0
    f = (t - t0) / (t1 - t0)
    if len(v0) == 4:  # rotation quaternion
        return quat_nlerp(v0, v1, f)
    return tuple(a + (b - a) * f for a, b in zip(v0, v1))


def _node_base(node) -> tuple:
    return (node.translation, node.rotation, node.scale)


def compute_local_transforms(
    model: SourceModel, clip: AnimationClip | None, t: float
) -> list[list[float]]:
    """Node local matrices at time ``t`` (bind pose when clip is None)."""
    per_node: dict[int, dict[str, tuple]] = {}
    if clip is not None:
        for ch in clip.channels:
            slot = per_node.setdefault(ch.node, {})
            slot[ch.path] = sample_channel(ch.times, ch.values, ch.interpolation, t)
    out: list[list[float]] = []
    for ni, node in enumerate(model.nodes):
        tr, ro, sc = _node_base(node)
        slot = per_node.get(ni, {})
        tr = slot.get("translation", tr)
        ro = slot.get("rotation", ro)
        sc = slot.get("scale", sc)
        if node.matrix is not None and ni not in per_node:
            out.append([float(v) for v in node.matrix])
        else:
            out.append(trs_to_matrix(tr, quat_normalize(ro), sc))
    return out


def compute_global_transforms(
    model: SourceModel, locals_: list[list[float]]
) -> list[list[float]]:
    """Global matrices through the hierarchy from scene roots down."""
    child_of: dict[int, int] = {}
    for ni, node in enumerate(model.nodes):
        for c in node.children:
            if c < 0 or c >= len(model.nodes):
                raise GltfError(f"node {ni}: child {c} out of range")
            if c in child_of:
                raise GltfError(f"node {c} has two parents")
            child_of[c] = ni
    roots = [i for i in range(len(model.nodes)) if i not in child_of]
    globals_: list[list[float] | None] = [None] * len(model.nodes)
    # Iterative DFS in child-index order: deterministic for any hierarchy.
    stack = [(r, list(IDENTITY)) for r in reversed(roots)]
    while stack:
        ni, parent = stack.pop()
        g = mat_mult(parent, locals_[ni])
        globals_[ni] = g
        for c in reversed(model.nodes[ni].children):
            stack.append((c, g))
    return [g if g is not None else list(IDENTITY) for g in globals_]


def skin_matrices(
    model: SourceModel, clip: AnimationClip | None, t: float
) -> list[list[float]]:
    """Skin matrices (global joint x inverse bind) at time ``t``."""
    if not model.skins:
        raise GltfError("model has no skin")
    skin = model.skins[model.skin_index if model.skin_index >= 0 else 0]
    locals_ = compute_local_transforms(model, clip, t)
    globals_ = compute_global_transforms(model, locals_)
    out = []
    for joint, ibm in zip(skin.joints, skin.inverse_bind):
        out.append(mat_mult(globals_[joint], ibm))
    return out


def evaluate_positions(
    model: SourceModel, clip: AnimationClip | None, t: float
) -> list[tuple[float, float, float]]:
    """Weighted skinned vertex positions at time ``t``."""
    if not model.is_skinned:
        if clip is not None:
            # Unskinned models ignore clips; still validate the request.
            pass
        return list(model.vertices)
    mats = skin_matrices(model, clip, t)
    out = []
    for pos, js, ws in zip(model.vertices, model.joints, model.weights):
        x = y = z = 0.0
        for j, w in zip(js, ws):
            if w == 0.0:
                continue
            px, py, pz = mat_vec(mats[j], pos)
            x += w * px
            y += w * py
            z += w * pz
        out.append((x, y, z))
    return out


def evaluate_normals(
    model: SourceModel, clip: AnimationClip | None, t: float
) -> list[tuple[float, float, float]] | None:
    """Skinned normals (renormalized) where the source provides them."""
    if model.normals is None or not model.is_skinned:
        return None
    mats = skin_matrices(model, clip, t)
    out = []
    for nrm, js, ws in zip(model.normals, model.joints, model.weights):
        x = y = z = 0.0
        for j, w in zip(js, ws):
            if w == 0.0:
                continue
            px, py, pz = mat_vec_dir(mats[j], nrm)
            x += w * px
            y += w * py
            z += w * pz
        n = math.sqrt(x * x + y * y + z * z)
        out.append((x / n, y / n, z / n) if n > 0.0 else (0.0, 0.0, 0.0))
    return out


def clip_sample_times(clip: AnimationClip) -> list[float]:
    """Deterministic union of all authored sampler input times, sorted."""
    times: dict[float, None] = {}
    for ch in clip.channels:
        for t in ch.times:
            times[float(t)] = None
    return sorted(times.keys())


def bake_clip_poses(
    model: SourceModel,
    clip: AnimationClip,
    times: list[float] | None = None,
    *,
    workers: int | None = None,
) -> list[list[tuple[float, float, float]]]:
    """Bake skinned positions for each sample time (host float).

    ``workers=None`` selects parallelism automatically; 1 forces serial
    evaluation and values above 1 are useful for controlled comparisons.
    Results always retain input-time order.
    """
    if times is None:
        times = clip_sample_times(clip)
    if workers is None:
        workers = _automatic_worker_count(model, len(times))
    if workers <= 1 or len(times) < 2:
        return [evaluate_positions(model, clip, t) for t in times]
    return bake_clips_poses(model, [(clip, times)], workers=workers)[0]


def bake_clips_poses(
    model: SourceModel,
    clip_times: list[tuple[AnimationClip, list[float]]],
    *,
    workers: int | None = None,
) -> list[list[list[tuple[float, float, float]]]]:
    """Bake several clips in one ordered process pool when work is large.

    A worker receives only the geometry and skinning data it needs. The
    original parsed GLB, source JSON and decoded textures stay in the parent.
    """
    frame_count = sum(len(times) for _clip, times in clip_times)
    if workers is None:
        workers = _automatic_worker_count(model, frame_count)
    if workers <= 1 or frame_count < 2:
        return [
            [evaluate_positions(model, clip, t) for t in times]
            for clip, times in clip_times
        ]

    results = [[None] * len(times) for _clip, times in clip_times]
    tasks = [
        (clip_index, frame_index, time)
        for clip_index, (_clip, times) in enumerate(clip_times)
        for frame_index, time in enumerate(times)
    ]
    clips = tuple(clip for clip, _times in clip_times)
    with ProcessPoolExecutor(
        max_workers=workers,
        mp_context=multiprocessing.get_context("spawn"),
        initializer=_init_pose_worker,
        initargs=(
            model.vertices,
            model.joints,
            model.weights,
            model.nodes,
            model.skins,
            model.skin_index,
            clips,
        ),
    ) as pool:
        for task, pose in zip(tasks, pool.map(_evaluate_pose_worker, tasks)):
            clip_index, frame_index, _time = task
            results[clip_index][frame_index] = pose
    return results


def loop_pose_distance(
    a: list[tuple[float, float, float]], b: list[tuple[float, float, float]]
) -> float:
    """Max per-vertex distance between two baked poses."""
    return max(
        math.sqrt((x0 - x1) ** 2 + (y0 - y1) ** 2 + (z0 - z1) ** 2)
        for (x0, y0, z0), (x1, y1, z1) in zip(a, b)
    )


def runtime_frame_times(
    model: SourceModel,
    clip: AnimationClip,
    loop_close_eps: float = 1e-5,
) -> tuple[list[float], bool]:
    """Authored sample times minus a redundant terminal loop duplicate.

    When the source clip ends at exactly ``duration`` with a pose equal to
    the first pose (the standard looping export), storing that terminal
    sample as a runtime frame wastes pose-stream memory and inserts a
    doubled frame into the loop. Returns ``(times, duplicate_removed)``.
    """
    times = clip_sample_times(clip)
    if len(times) < 2:
        return times, False
    poses = bake_clip_poses(model, clip, [times[0], times[-1]])
    if loop_pose_distance(poses[0], poses[1]) <= loop_close_eps:
        return times[:-1], True
    return times, False
