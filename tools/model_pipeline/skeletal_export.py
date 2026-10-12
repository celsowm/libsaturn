"""Skeletal export: SourceModel -> libsaturn skeleton / clip / skin data.

Host-side only. Produces the data the runtime in saturn/skeleton3d.h and
saturn/skin3d.h consumes:

- a parent-first skeleton with rigid parent-relative rest transforms,
- frame-major baked clips (fx16 translation + Q15 quaternion, w >= 0, sign
  continuous so nlerp never takes the long arc),
- a multi-anchor skin: every influence stores its OWN joint-local position
  (Pi = inverse_bind_i * P), vertices reordered rigid-first, weights Q15
  summing to exactly 32768 with the last implied.

Everything is deterministic. ``verify`` re-evaluates the quantised data in float
and compares it with animation.evaluate_positions, so a bad export fails here
rather than on the console.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

from .animation import (
    compute_global_transforms,
    compute_local_transforms,
    evaluate_positions,
    mat_mult,
    mat_vec,
)
from .errors import ImportError as ModelImportError
from .model import SourceModel, _mat_inverse

ROOT_PARENT = 0xFFFF
MAX_JOINTS = 255  # skin records index joints with a uint8
Q15 = 32768
SCALE_TOLERANCE = 1e-3


@dataclass
class SkeletonData:
    node_of_joint: list[int]            # export joint -> source node
    parent: list[int]                   # ROOT_PARENT or index < joint
    translation: list[tuple[int, int, int]]  # fx16, parent-relative
    rotation: list[tuple[int, int, int, int]]  # Q15 x, y, z, w


@dataclass
class ClipData:
    name: str
    rate: int
    frame_count: int
    loop: bool
    frames: list[list[tuple[tuple[int, int, int], tuple[int, int, int, int]]]]


@dataclass
class SkinData:
    pos_shift: int
    joint_count: int
    rigid: list[tuple[tuple[int, int, int], int]]  # (pos, joint)
    blend: list[tuple[list[int], int, list[int], list[list[int]]]]
    # blend record: (joints[3], count, weights[2] (Q15), pos[3][3])
    source_vertex: list[int]            # export vertex -> source vertex
    triangles: list[tuple[int, int, int]]  # remapped
    max_joint: int = 0


@dataclass
class SkeletalExport:
    skeleton: SkeletonData
    skin: SkinData
    clips: list[ClipData] = field(default_factory=list)
    scale: float = 1.0


# --------------------------------------------------------------------------
# Matrix / quaternion helpers (column-major 4x4, glTF convention)
# --------------------------------------------------------------------------


def _quat_from_matrix(m: list[float]) -> tuple[float, float, float, float]:
    r00, r10, r20 = m[0], m[1], m[2]
    r01, r11, r21 = m[4], m[5], m[6]
    r02, r12, r22 = m[8], m[9], m[10]
    tr = r00 + r11 + r22
    if tr > 0.0:
        s = math.sqrt(tr + 1.0) * 2.0
        q = ((r21 - r12) / s, (r02 - r20) / s, (r10 - r01) / s, 0.25 * s)
    elif r00 > r11 and r00 > r22:
        s = math.sqrt(1.0 + r00 - r11 - r22) * 2.0
        q = (0.25 * s, (r01 + r10) / s, (r02 + r20) / s, (r21 - r12) / s)
    elif r11 > r22:
        s = math.sqrt(1.0 + r11 - r00 - r22) * 2.0
        q = ((r01 + r10) / s, 0.25 * s, (r12 + r21) / s, (r02 - r20) / s)
    else:
        s = math.sqrt(1.0 + r22 - r00 - r11) * 2.0
        q = ((r02 + r20) / s, (r12 + r21) / s, 0.25 * s, (r10 - r01) / s)
    n = math.sqrt(sum(c * c for c in q))
    return tuple(c / n for c in q)  # type: ignore[return-value]


def _quat_to_matrix(q: tuple[float, float, float, float], t) -> list[float]:
    x, y, z, w = q
    return [
        1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0.0,
        2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0.0,
        2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0.0,
        t[0], t[1], t[2], 1.0,
    ]


def _check_rigid(m: list[float], what: str) -> None:
    cols = [(m[0], m[1], m[2]), (m[4], m[5], m[6]), (m[8], m[9], m[10])]
    for i, c in enumerate(cols):
        if abs(math.sqrt(sum(v * v for v in c)) - 1.0) > SCALE_TOLERANCE:
            raise ModelImportError(
                f"{what}: joint transform has non-unit scale (column {i}); "
                "bake scale into the mesh before export"
            )
    for a in range(3):
        for b in range(a + 1, 3):
            dot = sum(cols[a][k] * cols[b][k] for k in range(3))
            if abs(dot) > SCALE_TOLERANCE:
                raise ModelImportError(f"{what}: joint transform is sheared")


def _q15(q: tuple[float, float, float, float], prev=None) -> tuple[int, int, int, int]:
    """Q15 quaternion, w >= 0 (or continuous with `prev` when given)."""
    if prev is not None:
        dot = sum(a * b / Q15 for a, b in zip(q, prev))
        flip = dot < 0.0
    else:
        flip = q[3] < 0.0
    if flip:
        q = tuple(-c for c in q)  # type: ignore[assignment]
    out = [max(-32767, min(32767, round(c * Q15))) for c in q]
    return (out[0], out[1], out[2], out[3])


def _fx16(v: float) -> int:
    r = round(v * 65536.0)
    if abs(r) > 0x7FFFFFFF:
        raise ModelImportError(f"value {v} does not fit 16.16")
    return r


# --------------------------------------------------------------------------
# Skeleton
# --------------------------------------------------------------------------


def _parents_by_node(model: SourceModel) -> dict[int, int]:
    parent = {}
    for ni, node in enumerate(model.nodes):
        for c in node.children:
            parent[c] = ni
    return parent


def _joint_order(model: SourceModel) -> tuple[list[int], list[int]]:
    """(node per export joint, parent export joint) parent-first, stable."""
    skin = model.skins[model.skin_index if model.skin_index >= 0 else 0]
    if len(skin.joints) > MAX_JOINTS:
        raise ModelImportError(f"{len(skin.joints)} joints exceed the {MAX_JOINTS} limit")
    node_parent = _parents_by_node(model)
    joint_nodes = list(skin.joints)
    in_skin = set(joint_nodes)

    def joint_parent(node: int) -> int | None:
        p = node_parent.get(node)
        while p is not None:
            if p in in_skin:
                return p
            p = node_parent.get(p)
        return None

    order: list[int] = []
    placed: set[int] = set()
    remaining = list(joint_nodes)
    while remaining:
        progress = False
        for node in list(remaining):
            p = joint_parent(node)
            if p is None or p in placed:
                order.append(node)
                placed.add(node)
                remaining.remove(node)
                progress = True
        if not progress:
            raise ModelImportError("joint hierarchy has a cycle")
    index = {n: i for i, n in enumerate(order)}
    parents = []
    for node in order:
        p = joint_parent(node)
        parents.append(ROOT_PARENT if p is None else index[p])
    return order, parents


def _joint_locals(order, parents, globals_) -> list[list[float]]:
    out = []
    for node, p in zip(order, parents):
        g = globals_[node]
        out.append(g if p == ROOT_PARENT else mat_mult(_mat_inverse(globals_[order[p]]), g))
    return out


def build_skeleton(model: SourceModel, scale: float = 1.0) -> SkeletonData:
    if not model.is_skinned or not model.skins:
        raise ModelImportError("model has no skin")
    order, parents = _joint_order(model)
    globals_ = compute_global_transforms(model, compute_local_transforms(model, None, 0.0))
    translation, rotation = [], []
    for ji, m in enumerate(_joint_locals(order, parents, globals_)):
        _check_rigid(m, f"joint {ji} (node {order[ji]})")
        translation.append(tuple(_fx16(m[12 + k] * scale) for k in range(3)))
        rotation.append(_q15(_quat_from_matrix(m)))
    return SkeletonData(order, parents, translation, rotation)  # type: ignore[arg-type]


# --------------------------------------------------------------------------
# Clips
# --------------------------------------------------------------------------


def bake_clip(model: SourceModel, skel: SkeletonData, clip_index: int,
              rate: int = 30, loop: bool = False, scale: float = 1.0) -> ClipData:
    clip = model.clips[clip_index]
    if rate <= 0:
        raise ModelImportError("clip rate must be positive")
    frames_n = max(2, int(math.floor(clip.duration * rate + 1e-6)) + (0 if loop else 1))
    if loop:
        frames_n = max(2, int(round(clip.duration * rate)))
    frames = []
    prev: list = [None] * len(skel.node_of_joint)
    for f in range(frames_n):
        t = f / rate
        g = compute_global_transforms(model, compute_local_transforms(model, clip, t))
        row = []
        for ji, m in enumerate(_joint_locals(skel.node_of_joint, skel.parent, g)):
            _check_rigid(m, f"clip {clip.name} frame {f} joint {ji}")
            q = _q15(_quat_from_matrix(m), prev[ji])
            prev[ji] = q
            row.append((tuple(_fx16(m[12 + k] * scale) for k in range(3)), q))
        frames.append(row)
    return ClipData(clip.name, rate, frames_n, loop, frames)  # type: ignore[arg-type]


# --------------------------------------------------------------------------
# Skin
# --------------------------------------------------------------------------


def build_skin(model: SourceModel, skel: SkeletonData, scale: float = 1.0,
               min_weight: float = 0.002) -> SkinData:
    skin = model.skins[model.skin_index if model.skin_index >= 0 else 0]
    joint_of_node = {n: i for i, n in enumerate(skel.node_of_joint)}
    # source joint slot (index into skin.joints) -> export joint
    slot_joint = [joint_of_node[n] for n in skin.joints]
    ibm = skin.inverse_bind

    records = []  # per source vertex: list of (export_joint, weight, local_pos)
    biggest = 0.0
    for pos, js, ws in zip(model.vertices, model.joints, model.weights):
        infl = sorted(
            ((w, j) for j, w in zip(js, ws) if w > min_weight),
            key=lambda e: (-e[0], e[1]),
        )[:3]
        if not infl:
            infl = [(1.0, js[0])]
        total = sum(w for w, _ in infl)
        row = []
        for w, j in infl:
            lp = mat_vec(ibm[j], pos)
            lp = tuple(c * scale for c in lp)
            biggest = max(biggest, max(abs(c) for c in lp))
            row.append((slot_joint[j], w / total, lp))
        records.append(row)

    shift = 15
    while shift > 0 and biggest * (1 << shift) > 32767:
        shift -= 1
    if biggest * (1 << shift) > 32767:
        raise ModelImportError(f"joint-local extent {biggest} does not fit int16")
    mul = float(1 << shift)

    def qpos(p):
        return [round(c * mul) for c in p]

    rigid_src, blend_src = [], []
    for sv, row in enumerate(records):
        (rigid_src if len(row) == 1 else blend_src).append(sv)

    rigid = [(tuple(qpos(records[sv][0][2])), records[sv][0][0]) for sv in rigid_src]
    blend = []
    for sv in blend_src:
        row = records[sv]
        n = len(row)
        joints = [row[i][0] if i < n else row[0][0] for i in range(3)]
        poss = [qpos(row[i][2] if i < n else row[0][2]) for i in range(3)]
        w0 = round(row[0][1] * Q15)
        weights = [w0]
        if n == 3:
            weights.append(round(row[1][1] * Q15))
            if w0 + weights[1] > Q15:
                weights[1] = Q15 - w0
        else:
            weights.append(0)
        blend.append((joints, n, weights, poss))
    source_vertex = rigid_src + blend_src
    remap = {sv: i for i, sv in enumerate(source_vertex)}
    tris = [(remap[a], remap[b], remap[c]) for a, b, c in model.triangles]
    used = [j for _, j in rigid] + [j for b in blend for j in b[0][: b[1]]]
    return SkinData(shift, len(skel.node_of_joint), rigid, blend, source_vertex, tris,
                    max(used) if used else 0)


# --------------------------------------------------------------------------
# Top level, verification, report
# --------------------------------------------------------------------------


def export_model(model: SourceModel, scale: float = 1.0, rate: int = 30,
                 loop_clips: set[str] | None = None) -> SkeletalExport:
    skel = build_skeleton(model, scale)
    skin = build_skin(model, skel, scale)
    loop_clips = loop_clips or set()
    clips = [
        bake_clip(model, skel, i, rate, model.clips[i].name in loop_clips, scale)
        for i in range(len(model.clips))
    ]
    return SkeletalExport(skel, skin, clips, scale)


def _decode_pose(skel: SkeletonData, pose) -> list[list[float]]:
    """Model-space joint matrices from (fx16 t, Q15 q) per joint."""
    world: list[list[float]] = []
    for ji, ((tx, ty, tz), q) in enumerate(pose):
        qf = tuple(c / Q15 for c in q)
        n = math.sqrt(sum(c * c for c in qf)) or 1.0
        m = _quat_to_matrix(tuple(c / n for c in qf), (tx / 65536.0, ty / 65536.0, tz / 65536.0))
        p = skel.parent[ji]
        world.append(m if p == ROOT_PARENT else mat_mult(world[p], m))
    return world


def evaluate_export(exp: SkeletalExport, pose) -> list[tuple[float, float, float]]:
    """Float evaluation of the quantised export; output in source vertex order."""
    skel, skin = exp.skeleton, exp.skin
    world = _decode_pose(skel, pose)
    mul = float(1 << skin.pos_shift)
    out_export = []
    for pos, j in skin.rigid:
        out_export.append(mat_vec(world[j], tuple(c / mul for c in pos)))
    for joints, n, weights, poss in skin.blend:
        w = [weights[0] / Q15]
        if n == 3:
            w.append(weights[1] / Q15)
            w.append(1.0 - w[0] - w[1])
        elif n == 2:
            w.append(1.0 - w[0])
        acc = [0.0, 0.0, 0.0]
        for i in range(n):
            p = mat_vec(world[joints[i]], tuple(c / mul for c in poss[i]))
            for k in range(3):
                acc[k] += w[i] * p[k]
        out_export.append(tuple(acc))
    out = [None] * len(out_export)
    for ei, sv in enumerate(skin.source_vertex):
        out[sv] = out_export[ei]
    return out  # type: ignore[return-value]


def verify(model: SourceModel, exp: SkeletalExport, samples: int = 8) -> dict:
    """Worst vertex error (model units, after un-scaling) vs the float oracle."""
    worst = 0.0
    rest = [
        (exp.skeleton.translation[j], exp.skeleton.rotation[j])
        for j in range(len(exp.skeleton.node_of_joint))
    ]
    ref = evaluate_positions(model, None, 0.0)
    got = evaluate_export(exp, rest)
    for a, b in zip(ref, got):
        worst = max(worst, max(abs(a[k] * exp.scale - b[k]) for k in range(3)))
    rest_err = worst
    for clip, cd in zip(model.clips, exp.clips):
        step = max(1, cd.frame_count // samples)
        for f in range(0, cd.frame_count, step):
            ref = evaluate_positions(model, clip, f / cd.rate)
            got = evaluate_export(exp, cd.frames[f])
            for a, b in zip(ref, got):
                worst = max(worst, max(abs(a[k] * exp.scale - b[k]) for k in range(3)))
    return {"rest_error": rest_err, "max_error": worst / exp.scale}


def size_report(exp: SkeletalExport) -> dict:
    skel, skin = exp.skeleton, exp.skin
    clip_bytes = sum(c.frame_count * len(skel.node_of_joint) * 14 for c in exp.clips)
    out = {
        "joints": len(skel.node_of_joint),
        "vertices": len(skin.source_vertex),
        "rigid": len(skin.rigid),
        "blend": len(skin.blend),
        "pos_shift": skin.pos_shift,
        "skeleton_bytes": len(skel.node_of_joint) * 24,
        "skin_bytes": len(skin.rigid) * 8 + len(skin.blend) * 26,
        "clip_bytes": clip_bytes,
        "deformed_vertex_bytes": len(skin.source_vertex) * 12,
        "triangles": len(skin.triangles),
    }
    out["total_rom_bytes"] = out["skeleton_bytes"] + out["skin_bytes"] + clip_bytes + len(skin.triangles) * 8
    return out


# --------------------------------------------------------------------------
# C emission
# --------------------------------------------------------------------------


def _ident(name: str) -> str:
    import re

    n = re.sub(r"[^0-9A-Za-z]+", "_", name).strip("_") or "asset"
    return f"_{n}" if n[0].isdigit() else n


def emit_c(exp: SkeletalExport, name: str) -> tuple[str, str]:
    """(header, source) for `name`: <name>_skeleton, <name>_skin, <name>_clip_<c>."""
    base = _ident(name)
    skel, skin = exp.skeleton, exp.skin
    h = [f"/* Generated by tools/model_pipeline/skeletal_export.py. Do not edit. */",
         f"#ifndef {base.upper()}_SKELETAL_H", f"#define {base.upper()}_SKELETAL_H",
         '#include "saturn/saturn.h"', "#ifdef __cplusplus", 'extern "C" {', "#endif",
         f"extern const sat_skeleton_asset_t {base}_skeleton;",
         f"extern const sat_skin_asset_t {base}_skin;",
         f"extern const uint16_t {base}_indices[{len(skin.triangles) * 4}];",
         f"#define {base.upper()}_FACE_COUNT {len(skin.triangles)}",
         f"#define {base.upper()}_VERTEX_COUNT {len(skin.source_vertex)}"]
    c = [f'#include "{base}_skeletal.h"', ""]
    c.append(f"static const sat_skeleton_joint_t {base}_joints[{len(skel.parent)}] = {{")
    for p, t, q in zip(skel.parent, skel.translation, skel.rotation):
        c.append(f"    {{{p}, 0, {{{t[0]}, {t[1]}, {t[2]}}}, {{{q[0]}, {q[1]}, {q[2]}, {q[3]}}}}},")
    c += ["};",
          f"const sat_skeleton_asset_t {base}_skeleton = {{{base}_joints, {len(skel.parent)}, 0}};", ""]
    if skin.rigid:
        c.append(f"static const sat_skin_rigid_t {base}_rigid[{len(skin.rigid)}] = {{")
        for pos, j in skin.rigid:
            c.append(f"    {{{{{pos[0]}, {pos[1]}, {pos[2]}}}, {j}, 0}},")
        c += ["};", ""]
    if skin.blend:
        c.append(f"static const sat_skin_blend_t {base}_blend[{len(skin.blend)}] = {{")
        for joints, n, w, p in skin.blend:
            ps = ", ".join("{%d, %d, %d}" % tuple(x) for x in p)
            c.append(f"    {{{{{joints[0]}, {joints[1]}, {joints[2]}}}, {n}, "
                     f"{{{w[0]}, {w[1]}}}, {{{ps}}}}},")
        c += ["};", ""]
    rig = f"{base}_rigid" if skin.rigid else "0"
    ble = f"{base}_blend" if skin.blend else "0"
    c.append(f"const sat_skin_asset_t {base}_skin = {{{rig}, {ble}, 0, {len(skin.rigid)}, "
             f"{len(skin.blend)}, {skin.joint_count}, {skin.pos_shift}, {skin.max_joint}}};")
    c.append("")
    c.append(f"const uint16_t {base}_indices[{len(skin.triangles) * 4}] = {{")
    for a, b, d in skin.triangles:
        c.append(f"    {a}, {b}, {d}, {d},")
    c += ["};", ""]
    for cd in exp.clips:
        cn = f"{base}_clip_{_ident(cd.name)}"
        n = len(skel.parent)
        h.append(f"extern const sat_skeleton_clip_t {cn};")
        c.append(f"static const sat_skeleton_pose_joint_t {cn}_frames[{cd.frame_count * n}] = {{")
        for row in cd.frames:
            c.append("    " + " ".join(
                f"{{{{{t[0]}, {t[1]}, {t[2]}}}, {{{q[0]}, {q[1]}, {q[2]}, {q[3]}}}}}," for t, q in row))
        c += ["};",
              f"const sat_skeleton_clip_t {cn} = {{{cn}_frames, 0, {cd.frame_count}, {n}, 0, "
              f"{cd.rate}, 1, {'SAT_CLIP_FLAG_LOOP' if cd.loop else '0'}}};", ""]
    h += ["#ifdef __cplusplus", "}", "#endif", "#endif", ""]
    return "\n".join(h), "\n".join(c)
