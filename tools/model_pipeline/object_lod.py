"""Per-object LOD chains for city_walk, each level derived from the source.

The first city build simplified every 32-unit chunk as one triangle soup and
tore buildings apart; the second rebuilt them as extruded blocks, which is
solid but no longer looks like the model. This module follows the repository's
LOD rule instead (model_pipeline/lod.py): every level of an object is simplified
directly from that object's source triangles, never from the previous level, so
errors do not compound. Simplifying one closed building is well behaved in a way
a whole-chunk soup is not.

A chunk then *chooses* a level per object to fit its face cap: a greedy
exchange that spends faces where they buy the most visible surface.
"""

from __future__ import annotations

import hashlib
import math
from dataclasses import dataclass, field

import numpy as np

from . import chunking as ch
from .gltf import GltfError
from .model import SourceModel
from .simplification import SimplificationOptions, simplify

# Share of the source triangles each level keeps (level 0 = the source itself).
LEVEL_FRACTIONS = (1.0, 0.5, 0.25, 0.12, 0.06, 0.03, 0.015)
MIN_TRIANGLES = 4


@dataclass
class ObjectLevel:
    vertices: np.ndarray  # (V, 3) world
    faces: list  # [(corners_ccw_outward, palette_index)]


@dataclass
class CityObject:
    key: str
    chunk: int
    area: float  # source surface area, units^2
    source_triangles: int
    levels: list = field(default_factory=list)  # ObjectLevel, finest first, distinct face counts


def _weld(pos: np.ndarray, colour: np.ndarray):
    flat = np.round(pos.reshape(-1, 3), 4)
    verts, inverse = np.unique(flat, axis=0, return_inverse=True)
    tris = inverse.reshape(-1, 3)
    good = (tris[:, 0] != tris[:, 1]) & (tris[:, 1] != tris[:, 2]) & (tris[:, 0] != tris[:, 2])
    return verts, tris[good], colour[good]


def _compact(vertices: np.ndarray, faces):
    remap: dict = {}
    out = []
    for corners, colour in faces:
        new = []
        for c in corners:
            if c not in remap:
                remap[c] = len(remap)
            new.append(remap[c])
        out.append((tuple(new), colour))
    verts = np.zeros((len(remap), 3))
    for old, new in remap.items():
        verts[new] = vertices[old]
    return verts, out


def _model(vertices, triangles, colours, colour_count):
    m = SourceModel()
    m.vertices = [tuple(float(c) for c in v) for v in vertices]
    m.uvs = [(0.0, 0.0)] * len(m.vertices)
    m.triangles = [tuple(int(i) for i in t) for t in triangles]
    m.tri_materials = [int(x) for x in colours]
    m.materials = [{"name": f"c{i}"} for i in range(colour_count)]
    return m


def build_levels(pos: np.ndarray, colour: np.ndarray, colour_count: int) -> list:
    """Level chain for one object: source triangles ``pos`` (N, 3, 3) with
    palette ``colour`` (N,). Each level comes from the source; levels whose
    face count does not drop are skipped."""
    verts, tris, cols = _weld(pos, colour)
    if len(tris) == 0:
        return []
    levels = []
    last_faces = None
    for frac in LEVEL_FRACTIONS:
        target = max(MIN_TRIANGLES, int(round(len(tris) * frac)))
        if frac == 1.0:
            spos, stris, smat = verts, [tuple(t) for t in tris.tolist()], cols.tolist()
        else:
            if target >= len(tris):
                continue
            try:
                sm = simplify(_model(verts, tris, cols, colour_count), options=SimplificationOptions(
                    target_triangles=target, quality="balanced",
                    preserve_uv=False, preserve_normals=False))
            except GltfError:
                break
            spos = np.asarray(sm.positions, dtype=np.float64)
            stris, smat = [tuple(t) for t in sm.triangles], list(sm.tri_materials)
        if not stris:
            break
        faces = ch.merge_to_quads(np.asarray(spos, dtype=np.float64), stris, smat)
        v, faces = _compact(np.asarray(spos, dtype=np.float64), faces)
        if last_faces is not None and len(faces) >= last_faces:
            continue
        levels.append(ObjectLevel(v, faces))
        last_faces = len(faces)
        if len(faces) <= MIN_TRIANGLES:
            break
    return levels


def object_key(pos: np.ndarray, colour: np.ndarray) -> str:
    h = hashlib.sha256()
    h.update(np.ascontiguousarray(np.round(pos, 5)).tobytes())
    h.update(np.ascontiguousarray(colour.astype(np.int32)).tobytes())
    return h.hexdigest()[:24]


# ---------------------------------------------------------------------------
# Choosing levels inside a chunk
# ---------------------------------------------------------------------------

def _error(obj: CityObject, level: int | None) -> float:
    """Visible-surface error of drawing ``obj`` at ``level`` (None = dropped).

    Surface area stands in for screen coverage; a simplified level loses detail
    in proportion to the triangles it removed (square root: the first halvings
    remove the least visible detail). Dropping loses all of the surface."""
    if level is None:
        return obj.area * 2.0
    kept = len(obj.levels[level].faces) / max(len(obj.levels[0].faces), 1)
    return obj.area * (1.0 - math.sqrt(kept))


def choose_levels(objects: list, face_cap: int, vert_cap: int, min_level=None):
    """Level per object (index or None) maximising kept surface within caps.

    Greedy marginal exchange: start with everything dropped and repeatedly take
    the single upgrade (dropped -> coarsest, or one level finer) with the best
    error reduction per extra face, while it fits. ``min_level`` optionally
    bounds how fine each object may go (a coarser LOD never outdoes a finer)."""
    choice: list = [None] * len(objects)
    faces = verts = 0
    import heapq

    def next_step(i):
        obj = objects[i]
        cur = choice[i]
        finest = 0 if min_level is None or min_level[i] is None else min_level[i]
        nxt = len(obj.levels) - 1 if cur is None else cur - 1
        if nxt < finest or nxt < 0:
            return None
        cur_f = 0 if cur is None else len(obj.levels[cur].faces)
        cur_v = 0 if cur is None else len(obj.levels[cur].vertices)
        df = len(obj.levels[nxt].faces) - cur_f
        dv = len(obj.levels[nxt].vertices) - cur_v
        gain = _error(obj, cur) - _error(obj, nxt)
        return (-(gain / max(df, 1)), i, nxt, df, dv)

    heap = []
    for i in range(len(objects)):
        if objects[i].levels:
            step = next_step(i)
            if step:
                heapq.heappush(heap, step)
    while heap:
        _score, i, nxt, df, dv = heapq.heappop(heap)
        expected = len(objects[i].levels) - 1 if choice[i] is None else choice[i] - 1
        if nxt != expected:
            continue  # stale entry
        if faces + df > face_cap or verts + dv > vert_cap:
            continue  # this object cannot grow further; others may
        choice[i] = nxt
        faces += df
        verts += dv
        step = next_step(i)
        if step:
            heapq.heappush(heap, step)
    return choice, faces, verts


def assemble(objects: list, choice: list):
    """Shared vertex list and faces ``(corners_ccw, palette_index)``."""
    verts = []
    faces = []
    base = 0
    for obj, lv in zip(objects, choice):
        if lv is None:
            continue
        level = obj.levels[lv]
        verts.append(level.vertices)
        faces.extend((tuple(c + base for c in corners), colour) for corners, colour in level.faces)
        base += len(level.vertices)
    if not verts:
        return np.zeros((0, 3)), []
    return np.concatenate(verts), faces
