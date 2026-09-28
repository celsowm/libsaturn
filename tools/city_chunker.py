#!/usr/bin/env python3
"""GLB city -> CITY.BIN for examples/city_walk.

Pipeline (see docs/CITY_WALK_STREAMING_PLAN.md):

  decode Draco -> world-space triangles -> drop what nobody sees -> quantise
  colours -> sample everything into a top-down surface model (0.5-unit cells:
  height, top colour, wall colour) -> the low part (street, kerb, lawn) becomes
  the VDP2 ground bitmap -> the tall part becomes extruded blocks, one level of
  detail per chunk and LOD, each the finest that fits the LOD's caps -> quantise
  to int16, add collision boxes from the same blocks -> pack.

Why blocks and not mesh simplification: a city chunk has ~1,700 triangles and
LOD0 holds ~170 faces. Decimating hollow building shells by 90% tears them
(floating roof slabs, missing walls, shards). Blocks derived from height steps
are always closed, and a walker at street level sees exactly that: walls, plus
the tops of whatever is lower than the eye.

Every cap is a hard gate that fails loudly with the numbers: if even the
coarsest block level cannot fit a chunk, the build stops instead of truncating.
"""

from __future__ import annotations

import argparse
import json
import math
import multiprocessing
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))

import pickle  # noqa: E402

from model_pipeline import blocks  # noqa: E402
from model_pipeline import facades  # noqa: E402
from model_pipeline import foliage  # noqa: E402
from model_pipeline import object_lod  # noqa: E402
from model_pipeline import chunking as ch  # noqa: E402
from model_pipeline import draco, emit_bin, gltf  # noqa: E402
from model_pipeline.gltf import GltfError  # noqa: E402

CHUNKER_VERSION = 7
EYE_UNITS = 2.0  # examples/city_walk/player.c EYE_HEIGHT_FX: the RBG0 ground's focal
# Finest block level each LOD may start from (blocks.LEVELS index). A LOD never
# takes a finer level than the LOD before it.
LOD_FIRST_LEVEL = (0, 2, 4)
# Share of the finest level's built-up area a LOD must keep. LOD2 sits 80-112
# units away, already faded into the sky: there, scattered props may go.
LOD_MIN_AREA = (0.5, 0.25, 0.0)


# ---------------------------------------------------------------------------
# One chunk, three LODs
# ---------------------------------------------------------------------------

def _quantize(vertices, x0, z0, base_y):
    """Chunk-local int16 ticks. Clamps to the overhang the runtime accepts and
    reports how many coordinates that touched (always 0 for blocks, which never
    leave their chunk)."""
    lo = -ch.OVERHANG_UNITS * emit_bin.QUANT_SCALE
    hi = (ch.CHUNK_UNITS + ch.OVERHANG_UNITS) * emit_bin.QUANT_SCALE
    q = np.rint((vertices - np.array([x0, base_y, z0])) * emit_bin.QUANT_SCALE).astype(np.int64)
    clamped = int(((q[:, 0] < lo) | (q[:, 0] > hi) | (q[:, 2] < lo) | (q[:, 2] > hi)).sum())
    q[:, 0] = np.clip(q[:, 0], lo, hi)
    q[:, 2] = np.clip(q[:, 2], lo, hi)
    q[:, 1] = np.clip(q[:, 1], -32768, 32767)
    return q, clamped


def _runtime_faces(faces, quantized):
    """Faces -> (a, b, c, d, material) in runtime winding, dropping any that
    quantisation collapsed to nothing."""
    out = []
    for corners, material in faces:
        a, b, c, d = ch.to_runtime_face(corners)
        pts = {tuple(quantized[i]) for i in {a, b, c, d}}
        if len(pts) < 3:
            continue
        out.append((int(a), int(b), int(c), int(d), int(material)))
    return out


def build_chunk(levels, cx, cz, x0, z0, base_y, ground_y, caps, material_of):
    """All LODs of chunk (cx, cz). ``material_of(palette, direction)`` maps a
    block face to its shaded material. Raises when even the coarsest level
    overflows a LOD's caps."""
    result = {"lods": {}, "boxes": []}
    foliage_items = list(foliage_items or [])
    first = 0
    c = blocks.CHUNK_CELLS
    window = (slice(cz * c, (cz + 1) * c), slice(cx * c, (cx + 1) * c))
    # Built-up area: what is at least 1.5 units wide (buildings, not poles),
    # i.e. what survives the thinning of LOD1's first level.
    built = int((levels[LOD_FIRST_LEVEL[1]].region[window] > 0).sum())
    for lod in range(emit_bin.LOD_COUNT):
        face_cap, vert_cap = caps[lod]
        start = max(first, LOD_FIRST_LEVEL[lod])
        chosen = None
        for li in range(start, len(levels)):
            # A level may simplify the chunk but not make it vanish up close.
            # (A chunk of a few props, under 16 square units, may lose them.)
            kept = int((levels[li].region[window] > 0).sum())
            if built * blocks.CELL * blocks.CELL >= 16.0 and kept < LOD_MIN_AREA[lod] * built:
                continue
            verts, faces = blocks.chunk_faces(levels[li], cx, cz, EYE_UNITS)
            if len(faces) <= face_cap and len(verts) <= vert_cap:
                chosen = (li, verts, faces)
                break
        if chosen is None:
            raise GltfError(f"chunk ({cx},{cz}) LOD{lod}: no block level keeps "
                            f"{LOD_MIN_AREA[lod]:.0%} of its "
                            f"{built * blocks.CELL * blocks.CELL:.0f} built-up square units "
                            f"within the caps {face_cap} faces / {vert_cap} vertices")
        li, verts, faces = chosen
        first = li
        if not faces:
            continue
        world = verts + np.array([0.0, ground_y, 0.0])
        q, clamped = _quantize(world, x0, z0, base_y)
        rt = _runtime_faces([(c, material_of(p, d)) for (c, p, d) in faces], q)
        if not rt:
            continue
        result["lods"][lod] = {"vertices": q.tolist(), "faces": rt, "level": li,
                               "clamped": clamped}
    lod0_level = result["lods"].get(0, {}).get("level", LOD_FIRST_LEVEL[0])
    boxes = blocks.chunk_boxes(levels[lod0_level], cx, cz, ground_y, emit_bin.LOD2_BOX_CAP)
    scale = emit_bin.QUANT_SCALE
    result["boxes"] = [
        (int(round((bx - x0) * scale)), int(round((by - base_y) * scale)),
         int(round((bz - z0) * scale)), int(round(hx * scale)), int(round(hy * scale)),
         int(round(hz * scale)))
        for (bx, by, bz, hx, hy, hz) in boxes]
    return result


# ---------------------------------------------------------------------------
# Facades: blocks whose walls carry textures baked from the source
# ---------------------------------------------------------------------------

# Preferred texels per unit, densest first. If even the last preferred rate
# cannot cover EVERY facade in the slot, _fit_textures keeps halving the rate
# until the dimensions stop shrinking. Geometry detail may outrank texel
# density, but a nearer LOD must never lose texture coverage altogether.
FACADE_DENSITIES = ((4.0, 3.0, 2.0, 1.5, 1.0, 0.75, 0.5), (2.0, 1.5, 1.0, 0.75, 0.5), (1.0, 0.75, 0.5, 0.375, 0.25))
# Facade subdivision candidates, finest first. A close VDP1 distorted sprite
# is unsafe not only when it crosses the near plane: a corner projected beyond
# the renderer's bounded off-screen window also forces the whole textured quad
# to its solid fallback. The old city only cut walls horizontally, so a tall
# facade could suddenly become one flat-colour slab as the player approached.
#
# Try small independently baked rectangles first, then progressively recover
# the old horizontal-only policy when a dense chunk would exceed its hard face
# or vertex cap. LOD2 is distant enough to stay unsplit.
FACADE_SPLITS = (
    ((4.0, 4.0), (8.0, 4.0), (4.0, 8.0), (8.0, 8.0),
     (8.0, 16.0), (8.0, None), (16.0, 8.0), (16.0, 16.0),
     (16.0, None), (None, None)),
    ((8.0, 4.0), (16.0, 4.0), (8.0, 8.0), (16.0, 8.0),
     (16.0, 16.0), (16.0, None), (None, None)),
    ((None, None),),
)


def _tiled_faces(verts, faces, horizontal_tile, vertical_tile):
    """Block faces -> (runtime corners A B C D as world points, colour,
    direction), adaptively cut in both screen-relevant axes."""
    out = []
    for corners, colour, direction in faces:
        a, b, c, d = ch.to_runtime_face(corners)
        quad = verts[[a, b, c, d]]
        pieces = ([quad] if horizontal_tile is None and vertical_tile is None
                  else facades.split_face(quad, horizontal_tile, vertical_tile))
        out.extend((piece, colour, direction) for piece in pieces)
    return out


def _index_faces(tiled):
    """Shared vertices for runtime-ordered quads: (vertices, [(a, b, c, d)])."""
    lookup: dict = {}
    verts: list = []
    quads = []
    for piece, _colour, _direction in tiled:
        idx = []
        for p in piece:
            key = tuple(int(round(v * 64)) for v in p)
            if key not in lookup:
                lookup[key] = len(verts)
                verts.append(p)
            idx.append(lookup[key])
        quads.append(tuple(idx))
    return np.array(verts, dtype=np.float64).reshape(-1, 3), quads


def _fit_textures(tiled, lod, baker, ground_y, fallback_rgb, extra_textures=None):
    """Bake every facade plus already-baked foliage inside one fixed VRAM slot."""
    budget = emit_bin.LOD_TEXTURE_BYTES[lod]
    extra_textures = list(extra_textures or [])
    worlds = [piece + np.array([0.0, ground_y, 0.0]) for piece, _c, _d in tiled]

    def image_of(tex):
        return tex[0] if isinstance(tex, tuple) else tex

    extra_sizes = [(image_of(tex).shape[1], image_of(tex).shape[0]) for tex in extra_textures]

    def cost(sizes):
        all_sizes = list(sizes) + extra_sizes
        head = emit_bin.align(emit_bin.TEXTURE_TABLE_HEADER +
                              emit_bin.TEXTURE_ENTRY_BYTES * len(all_sizes), 8)
        return head + sum(emit_bin.align(w * h, 8) for w, h in all_sizes)

    total_count = len(tiled) + len(extra_textures)
    if total_count > 255:
        raise GltfError(f"LOD{lod}: {total_count} textures exceed the 255-entry table")
    if not budget or total_count == 0:
        return [], [0] * len(tiled), 0.0

    chosen_sizes = []
    chosen_rate = 0.0
    if tiled:
        chosen_sizes = None
        for rate in FACADE_DENSITIES[lod]:
            sizes = [facades.texture_size(w, rate) for w in worlds]
            if cost(sizes) <= budget:
                chosen_sizes, chosen_rate = sizes, rate
                break
        if chosen_sizes is None:
            rate = FACADE_DENSITIES[lod][-1] * 0.5
            previous = None
            while True:
                sizes = [facades.texture_size(w, rate) for w in worlds]
                if cost(sizes) <= budget:
                    chosen_sizes, chosen_rate = sizes, rate
                    break
                if sizes == previous:
                    break
                previous = sizes
                rate *= 0.5
        if chosen_sizes is None:
            minimum = cost(previous or [facades.texture_size(w, 0.0) for w in worlds])
            raise GltfError(
                f"LOD{lod}: facade + foliage textures need at least {minimum} B, "
                f"slot budget is {budget} B")
    elif cost([]) > budget:
        raise GltfError(f"LOD{lod}: foliage textures do not fit {budget} B slot")

    textures = []
    face_texture = []
    for i, (w, h) in enumerate(chosen_sizes):
        _p, colour, direction = tiled[i]
        textures.append(baker.bake(worlds[i], w, h, fallback_rgb(colour, direction)))
        face_texture.append(i + 1)
    textures.extend(extra_textures)
    return textures, face_texture, chosen_rate


def build_facade_chunk(levels, cx, cz, x0, z0, base_y, ground_y, caps, material_of,
                       baker, fallback_rgb, foliage_items=None):
    """Like build_chunk, with facade textures on the LODs that have a VDP1
    texture slot. Texels stay RGB here; the caller quantises them once the
    whole city's palette is known."""
    result = {"lods": {}, "boxes": []}
    first = 0
    c = blocks.CHUNK_CELLS
    window = (slice(cz * c, (cz + 1) * c), slice(cx * c, (cx + 1) * c))
    built = int((levels[LOD_FIRST_LEVEL[1]].region[window] > 0).sum())
    for lod in range(emit_bin.LOD_COUNT):
        face_cap, vert_cap = caps[lod]
        start = max(first, LOD_FIRST_LEVEL[lod])
        candidates = foliage.select_for_lod(foliage_items, lod)
        chosen = None
        selected_foliage = []
        # Preserve as much foliage as possible without violating the existing
        # hard building caps. Each billboard costs exactly 1 face + 4 vertices.
        for foliage_count in range(len(candidates), -1, -1):
            selected_foliage = candidates[:foliage_count]
            building_face_cap = face_cap - foliage_count
            building_vert_cap = vert_cap - 4 * foliage_count
            if building_face_cap < 0 or building_vert_cap < 0:
                continue
            for li in range(start, len(levels)):
                kept = int((levels[li].region[window] > 0).sum())
                if built * blocks.CELL * blocks.CELL >= 16.0 and kept < LOD_MIN_AREA[lod] * built:
                    continue
                verts, faces = blocks.chunk_faces(levels[li], cx, cz, EYE_UNITS)
                for horizontal_tile, vertical_tile in FACADE_SPLITS[lod]:
                    tiled = _tiled_faces(verts, faces, horizontal_tile, vertical_tile)
                    tv, quads = _index_faces(tiled)
                    if len(quads) <= building_face_cap and len(tv) <= building_vert_cap:
                        chosen = (li, tiled, tv, quads)
                        break
                if chosen is not None:
                    break
            if chosen is not None:
                break
        if chosen is None:
            raise GltfError(f"chunk ({cx},{cz}) LOD{lod}: no block level fits the caps "
                            f"{face_cap} faces / {vert_cap} vertices")
        li, tiled, tv, quads = chosen
        first = li
        if not quads and not selected_foliage:
            continue
        foliage_textures = [foliage.texture_for_lod(item, lod) for item in selected_foliage]
        textures, face_texture, texture_rate = _fit_textures(
            tiled, lod, baker, ground_y, fallback_rgb, foliage_textures)
        world = tv + np.array([0.0, ground_y, 0.0])
        q, clamped = _quantize(world, x0, z0, base_y)
        rt = []
        kept_textures, renumber = [], {}
        for (a, b, cc, d), (_piece, colour, direction), tex in zip(quads, tiled, face_texture):
            pts = {tuple(q[i]) for i in (a, b, cc, d)}
            if len(pts) < 3:
                continue  # collapsed by quantisation: its texture goes too
            if tex:
                renumber[tex] = len(kept_textures) + 1
                kept_textures.append(textures[tex - 1])
                tex = renumber[tex]
            rt.append((a, b, cc, d, material_of(colour, direction), tex))
        facade_texture_count = len(tiled)
        foliage_encoded = textures[facade_texture_count:]
        foliage_texture_base = len(kept_textures)
        kept_textures.extend(foliage_encoded)
        out_vertices = q.tolist()
        for fi, item in enumerate(selected_foliage):
            fq, fc = _quantize(foliage.world_quad(item), x0, z0, base_y)
            clamped += fc
            vi = len(out_vertices)
            out_vertices.extend(fq.tolist())
            rt.append((vi, vi + 1, vi + 2, vi + 3, 0,
                       foliage_texture_base + fi + 1))
        textures = kept_textures
        if rt:
            result["lods"][lod] = {"vertices": out_vertices, "faces": rt, "level": li,
                                   "clamped": clamped, "textures": textures,
                                   "texture_rate": texture_rate,
                                   "foliage": len(selected_foliage)}
    lod0_level = result["lods"].get(0, {}).get("level", LOD_FIRST_LEVEL[0])
    boxes = blocks.chunk_boxes(levels[lod0_level], cx, cz, ground_y, emit_bin.LOD2_BOX_CAP)
    scale = emit_bin.QUANT_SCALE
    result["boxes"] = [
        (int(round((bx - x0) * scale)), int(round((by - base_y) * scale)),
         int(round((bz - z0) * scale)), int(round(hx * scale)), int(round(hy * scale)),
         int(round(hz * scale)))
        for (bx, by, bz, hx, hy, hz) in boxes]
    return result


def _facade_job(job):
    (levels_path, cx, cz, x0, z0, base_y, ground_y, caps, shading,
     baker_path, foliage_path) = job
    levels = _worker_cache("levels", levels_path)
    baker = _worker_cache("baker", baker_path)
    foliage_by_chunk = _worker_cache("foliage", foliage_path)
    base, levels_n, light, amb, dif, dir_level = shading

    def material_of(colour, direction):
        return int(colour) * levels_n + int(dir_level[direction])

    def fallback_rgb(colour, direction):
        lin = ch._srgb_decode(base[int(colour)].astype(np.float64) / 255.0)
        n = blocks.DIRECTION_NORMALS[direction]
        l_vec = np.asarray(light, dtype=np.float64) / np.linalg.norm(light)
        k = amb + dif * max(0.0, float(n @ l_vec))
        return np.clip(np.rint(ch._srgb_encode(np.clip(lin * k, 0, 1)) * 255), 0, 255)

    chunk = cz * emit_bin.GRID_X + cx
    return (chunk,
            build_facade_chunk(levels, cx, cz, x0, z0, base_y, ground_y, caps, material_of,
                               baker, fallback_rgb, foliage_by_chunk.get(chunk, [])))


_WORKER: dict = {}


def _worker_cache(name, path):
    """Big read-only inputs travel to workers as one pickle file each, loaded
    once per worker process instead of once per chunk."""
    if _WORKER.get(name + "_path") != path:
        _WORKER[name] = pickle.loads(Path(path).read_bytes())
        _WORKER[name + "_path"] = path
    return _WORKER[name]


def quantise_textures(results, log=print):
    """One 255-colour palette; index 0 is reserved for cutout transparency."""
    samples = []

    def unpack(tex):
        if isinstance(tex, tuple):
            return tex[0], int(tex[1])
        return tex, 0

    for res in results.values():
        for data in res["lods"].values():
            for tex in data.get("textures", []):
                image, _flags = unpack(tex)
                rgb = image[..., :3]
                if image.shape[-1] == 4:
                    rgb = rgb[image[..., 3] != 0]
                if rgb.size:
                    samples.append(rgb.reshape(-1, 3)[::3])
    if not samples:
        return None
    palette = facades.build_palette(np.concatenate(samples), 255)
    keys = np.arange(32768)
    key_rgb = np.stack([(keys & 31), (keys >> 5) & 31, (keys >> 10) & 31], axis=1) * 255 // 31
    lut = ch.nearest_palette(key_rgb.astype(np.uint8), palette).astype(np.uint8) + 1
    for res in results.values():
        for data in res["lods"].values():
            out = []
            for tex in data.get("textures", []):
                image, flags = unpack(tex)
                rgb = image[..., :3]
                t = rgb.astype(np.int64) * 31 // 255
                key = t[..., 0] | (t[..., 1] << 5) | (t[..., 2] << 10)
                idx = lut[key]
                if image.shape[-1] == 4:
                    idx = idx.copy()
                    idx[image[..., 3] == 0] = 0
                out.append((image.shape[1], image.shape[0], idx.tobytes(), flags))
            data["textures"] = out
    rgb555 = [int((int(r) * 31 // 255) | ((int(g) * 31 // 255) << 5) | ((int(b) * 31 // 255) << 10))
              for r, g, b in palette.tolist()]
    log(f"  texture palette: {len(rgb555)} colours")
    return [0] + rgb555


# ---------------------------------------------------------------------------
# Objects: per-object LOD chains from the source, chosen per chunk
# ---------------------------------------------------------------------------

def _levels_job(job):
    key, pos, colour, colour_count = job
    return key, object_lod.build_levels(pos, colour, colour_count)


def split_objects(pos, colour, node, log=print):
    """(chunk, pos, colour) per object. An object is one glTF node; one that
    reaches past a chunk's 16-unit overhang is cut at chunk borders and each
    piece becomes its own object, so every coordinate stays in range."""
    out = []
    lo, hi = -ch.OVERHANG_UNITS, ch.CHUNK_UNITS + ch.OVERHANG_UNITS
    cut = 0
    for n in np.unique(node).tolist():
        sel = node == n
        p, c = pos[sel], colour[sel]
        flat = p.reshape(-1, 3)
        centre = (flat.min(axis=0) + flat.max(axis=0)) / 2.0
        cx = int(math.floor((centre[0] - ch.ORIGIN_X) / ch.CHUNK_UNITS))
        cz = int(math.floor((centre[2] - ch.ORIGIN_Z) / ch.CHUNK_UNITS))
        x0 = ch.ORIGIN_X + cx * ch.CHUNK_UNITS
        z0 = ch.ORIGIN_Z + cz * ch.CHUNK_UNITS
        fits = (0 <= cx < ch.GRID_X and 0 <= cz < ch.GRID_Z and
                flat[:, 0].min() - x0 >= lo and flat[:, 0].max() - x0 <= hi and
                flat[:, 2].min() - z0 >= lo and flat[:, 2].max() - z0 <= hi)
        if fits:
            out.append((cz * ch.GRID_X + cx, p, c))
            continue
        cut += 1
        cp, attrs, _rep = ch.clip_to_grid(p, {"c": c})
        chunks = ch.chunk_of(cp)
        for k in np.unique(chunks).tolist():
            m = chunks == k
            out.append((int(k), cp[m], attrs["c"][m]))
    log(f"  {len(out)} objects ({cut} nodes cut at chunk borders)")
    return out


def build_object_chunks(objects_src, caps, colour_count, material_of, world_min_y,
                        ground_y, cache_dir, jobs, log=print):
    """Level chains for every object (cached by content), then per chunk and
    LOD the greedy level choice. Returns ``(results, stats)``."""
    todo, objs = [], []
    hits = 0
    for chunk, p, c in objects_src:
        key = object_lod.object_key(p, c)
        obj = object_lod.CityObject(key=key, chunk=chunk,
                                    area=float(ch.triangle_normals(p)[1].sum()),
                                    source_triangles=len(p))
        objs.append((obj, p, c))
        path = cache_dir / f"{key}.pkl" if cache_dir is not None else None
        if path is not None and path.exists():
            obj.levels = pickle.loads(path.read_bytes())
            hits += 1
        else:
            todo.append((key, p, c, colour_count))
    t0 = time.time()
    if todo:
        if jobs > 1 and len(todo) > 1:
            # Biggest first so the long ones do not end up last on one worker.
            todo.sort(key=lambda j: -len(j[1]))
            with multiprocessing.Pool(jobs) as pool:
                produced = dict(pool.imap_unordered(_levels_job, todo, chunksize=1))
        else:
            produced = dict(_levels_job(j) for j in todo)
        for obj, _p, _c in objs:
            if obj.key in produced:
                obj.levels = produced[obj.key]
                if cache_dir is not None:
                    (cache_dir / f"{obj.key}.pkl").write_bytes(pickle.dumps(obj.levels))
    log(f"  object levels: {hits} cached, {len(todo)} built in {time.time() - t0:.1f} s")

    by_chunk: dict = {}
    for obj, p, _c in objs:
        by_chunk.setdefault(obj.chunk, []).append((obj, p))
    results = {}
    kept_area = [0.0] * emit_bin.LOD_COUNT
    total_area = 0.0
    for chunk, members in sorted(by_chunk.items()):
        cx, cz = chunk % emit_bin.GRID_X, chunk // emit_bin.GRID_X
        x0, z0 = ch.ORIGIN_X + cx * ch.CHUNK_UNITS, ch.ORIGIN_Z + cz * ch.CHUNK_UNITS
        objects = [o for o, _p in members if o.levels]
        total_area += sum(o.area for o in objects)
        res = {"lods": {}, "boxes": []}
        finer = None
        for lod in range(emit_bin.LOD_COUNT):
            face_cap, vert_cap = caps[lod]
            if finer is None:
                choice, _f, _v = object_lod.choose_levels(objects, face_cap, vert_cap)
            else:
                # A coarser LOD never draws an object finer than, or absent
                # from, the finer LOD.
                allowed = [i for i, lv in enumerate(finer) if lv is not None]
                sub = [objects[i] for i in allowed]
                sub_choice, _f, _v = object_lod.choose_levels(
                    sub, face_cap, vert_cap, min_level=[finer[i] for i in allowed])
                choice = [None] * len(objects)
                for i, lv in zip(allowed, sub_choice):
                    choice[i] = lv
            finer = choice
            kept_area[lod] += sum(o.area for o, lv in zip(objects, choice) if lv is not None)
            verts, faces = object_lod.assemble(objects, choice)
            if not faces:
                continue
            q, clamped = _quantize(verts, x0, z0, world_min_y)
            shaded = []
            for corners, colour in faces:
                pts = verts[list(corners)]
                n = np.cross(pts[1] - pts[0], pts[2] - pts[0])
                shaded.append((corners, material_of(colour, n)))
            rt = _runtime_faces(shaded, q)
            if rt:
                res["lods"][lod] = {"vertices": q.tolist(), "faces": rt, "level": 0,
                                    "clamped": clamped}
        chunk_pos = np.concatenate([p for _o, p in members])
        boxes = ch.collision_boxes(chunk_pos, x0, z0, ground_y, emit_bin.LOD2_BOX_CAP)
        scale = emit_bin.QUANT_SCALE
        res["boxes"] = [
            (int(round((bx - x0) * scale)), int(round((by - world_min_y) * scale)),
             int(round((bz - z0) * scale)), int(round(hx * scale)), int(round(hy * scale)),
             int(round(hz * scale)))
            for (bx, by, bz, hx, hy, hz) in boxes]
        if res["lods"]:
            results[chunk] = res
    stats = {"objects": len(objs), "surface_kept_by_lod": [
        round(a / max(total_area, 1e-9), 4) for a in kept_area]}
    return results, stats


# ---------------------------------------------------------------------------
# Whole city
# ---------------------------------------------------------------------------

def build_city(glb_path: Path, opts: argparse.Namespace, log=print):
    """Returns ``(archive_bytes, info, report, extras)``."""
    started = time.time()
    glb = gltf.parse_glb(glb_path)
    tris = ch.load_world_triangles(glb)
    materials = ch.read_materials(glb)
    rgb, keep = ch.triangle_colors(tris, materials)
    normals, areas = ch.triangle_normals(tris.pos)

    report: dict = {"tool_version": CHUNKER_VERSION, "source_triangles": int(len(tris.pos))}
    mask_materials = {i for i, m in enumerate(materials) if m.alpha_mode == ch.ALPHA_MASK}
    masked = np.isin(tris.material, list(mask_materials)) if mask_materials else np.zeros(len(keep), bool)
    foliage_materials = foliage.foliage_material_ids(materials)
    foliage_mask = (np.isin(tris.material, list(foliage_materials))
                    if foliage_materials else np.zeros(len(keep), bool))
    degenerate = areas <= 1e-9
    ground_y = ch.estimate_ground_y(tris, normals, areas)
    ground = ch.classify_ground(tris, normals, ground_y) & ~degenerate & ~masked
    under = ch.underground_mask(tris, ground_y) & ~ground
    visible = keep & ~foliage_mask & ~degenerate & ~under
    report["dropped"] = {
        "alpha_mask_materials": int(masked.sum()),
        "foliage_source_triangles": int(foliage_mask.sum()),
        "transparent_texels": int((~keep & ~masked).sum()),
        "degenerate": int(degenerate.sum()),
        "underground": int((under & ~masked & ~degenerate).sum()),
    }
    report["ground"] = {"ground_y": ground_y, "triangles": int(ground.sum())}
    world_min_y = int(math.floor(tris.pos[:, :, 1].min()))
    world_max_y = int(math.ceil(tris.pos[:, :, 1].max()))
    foliage_by_chunk = foliage.build_billboards(tris, materials, rgb, foliage_materials)
    foliage_items = sum(len(items) for items in foliage_by_chunk.values())
    source_mask_items = sum(
        item.source_kind == "source_mask" for items in foliage_by_chunk.values() for item in items)
    mesh_impostors = foliage_items - source_mask_items
    report["foliage"] = {"items": foliage_items, "source_mask": source_mask_items,
                         "mesh_impostors": mesh_impostors,
                         "chunks": len(foliage_by_chunk)}
    log(f"  {len(tris.pos)} triangles; ground_y={ground_y:.3f}; {int(visible.sum())} architectural, "
        f"{int(under.sum())} underground; foliage {foliage_items} objects "
        f"({source_mask_items} source cards, {mesh_impostors} mesh impostors)")

    # --- palette: one base palette, shaded per block face direction --------
    levels = opts.levels
    light = ch_parse_light(opts.light_dir)
    base = ch.median_cut(rgb[visible], areas[visible], opts.colors)
    colour = ch.nearest_palette(rgb[visible], base)
    if len(base) * levels > emit_bin.MATERIAL_MAX:
        raise GltfError(f"{len(base)} colours x {levels} shades = {len(base) * levels} materials; "
                        f"the limit is {emit_bin.MATERIAL_MAX}. Lower --colors or --levels.")
    dir_level = ch.shade_levels(blocks.DIRECTION_NORMALS, light, levels)
    material_rgb555 = [
        ch.shade_rgb555(base[b], k, levels, opts.ambient, opts.diffuse)
        for b in range(len(base)) for k in range(levels)]

    def material_of(palette_index: int, direction) -> int:
        """A block direction index, or a face normal vector."""
        if np.ndim(direction) == 0:
            return int(palette_index) * levels + int(dir_level[direction])
        n = np.asarray(direction, dtype=np.float64)
        n = n / max(float(np.linalg.norm(n)), 1e-12)
        return int(palette_index) * levels + int(ch.shade_levels(n[None, :], light, levels)[0])

    report["palette"] = {"base_colors": int(len(base)), "levels": levels,
                         "materials": len(material_rgb555)}

    if opts.geometry == "objects":
        return _build_objects_city(tris, rgb, normals, areas, visible, under, colour, base,
                                   ground_y, world_min_y, world_max_y, material_rgb555,
                                   material_of, light, levels, report, started, opts, log)

    # --- surface model and block levels -----------------------------------
    t0 = time.time()
    sm = blocks.surface_model(tris.pos[visible], colour, normals[visible], ground_y, base)
    block_levels = [blocks.block_level(sm, spec) for spec in blocks.LEVELS]
    has = np.isfinite(sm.height)
    report["surface"] = {
        "cell_units": blocks.CELL, "sampled_fraction": float(has.mean()),
        "tall_fraction": float((has & (sm.height > blocks.LOW_UNITS)).mean()),
        "regions_by_level": [int(lv.region.max()) for lv in block_levels],
        "seconds": round(time.time() - t0, 1)}
    log(f"  surface model: {report['surface']['sampled_fraction']:.1%} of cells sampled, "
        f"{report['surface']['tall_fraction']:.1%} tall; regions per level "
        f"{report['surface']['regions_by_level']} ({report['surface']['seconds']} s)")

    # --- ground bitmap ------------------------------------------------------
    # Flat triangles no higher than the low band, painted low to high: lane
    # marks, crosswalks and kerbs survive at one dot per unit.
    vpos, vnorm = tris.pos[visible], normals[visible]
    low_flat = ((vpos[:, :, 1].max(axis=1) <= ground_y + blocks.LOW_UNITS) &
                (np.abs(vnorm[:, 1]) > blocks.UP_NY))
    ground_bitmap, ground_palette_rgb, palette, (gw, gh, upd_x, upd_z) = _ground_section(
        vpos[low_flat], colour[low_flat], base, under, rgb, areas, tris.pos.reshape(-1, 3),
        light, levels, report, opts)
    ground_section = emit_bin.GroundSection(
        bitmap=ground_bitmap.tobytes(), width=gw, height=gh, palette=palette,
        ground_y_units=ground_y, units_per_dot_x=upd_x, units_per_dot_z=upd_z)

    # --- chunks -------------------------------------------------------------
    caps = [tuple(int(v) for v in pair_) for pair_ in opts.caps]
    results = {}
    texture_palette = None
    todo = []
    for chunk in range(emit_bin.CHUNK_COUNT):
        cx, cz = chunk % emit_bin.GRID_X, chunk // emit_bin.GRID_X
        cell_count = blocks.CHUNK_CELLS
        has_blocks = (block_levels[0].region[
            cz * cell_count:(cz + 1) * cell_count,
            cx * cell_count:(cx + 1) * cell_count] > 0).any()
        if not has_blocks and chunk not in foliage_by_chunk:
            continue
        todo.append((cx, cz))
    if opts.geometry == "facades":
        t0 = time.time()
        lit_n = np.where(normals[visible][:, 1:2] < -0.5, -normals[visible], normals[visible])
        shaded = facades.shade_triangles(rgb[visible], lit_n, light, opts.ambient, opts.diffuse)
        work = Path(opts.cache_dir) / "facade_work"
        work.mkdir(parents=True, exist_ok=True)
        levels_path = work / "levels.pkl"
        baker_path = work / "baker.pkl"
        foliage_path = work / "foliage.pkl"
        levels_path.write_bytes(pickle.dumps(block_levels))
        baker_path.write_bytes(pickle.dumps(facades.FacadeBaker(tris.pos[visible], shaded)))
        foliage_path.write_bytes(pickle.dumps(foliage_by_chunk))
        shading = (base, levels, light, opts.ambient, opts.diffuse, dir_level)
        jobs = [(str(levels_path), cx, cz, ch.ORIGIN_X + cx * ch.CHUNK_UNITS,
                 ch.ORIGIN_Z + cz * ch.CHUNK_UNITS, world_min_y, ground_y, caps, shading,
                 str(baker_path), str(foliage_path)) for cx, cz in todo]
        if opts.jobs > 1 and len(jobs) > 1:
            with multiprocessing.Pool(opts.jobs) as pool:
                produced = pool.map(_facade_job, jobs, chunksize=1)
        else:
            produced = [_facade_job(j) for j in jobs]
        for chunk, res in produced:
            if res["lods"]:
                results[chunk] = res
        texture_palette = quantise_textures(results, log)
        log(f"  facades baked in {time.time() - t0:.1f} s")
    else:
        for cx, cz in todo:
            res = build_chunk(block_levels, cx, cz,
                              ch.ORIGIN_X + cx * ch.CHUNK_UNITS, ch.ORIGIN_Z + cz * ch.CHUNK_UNITS,
                              world_min_y, ground_y, caps, material_of)
            if res["lods"]:
                results[chunk] = res
    log(f"  {len(results)} chunks with blocks of {emit_bin.CHUNK_COUNT}")

    # --- pack ---------------------------------------------------------------
    blobs = {}
    per_chunk = {}
    clamped = 0
    for chunk, res in sorted(results.items()):
        row = {"boxes": len(res["boxes"])}
        for lod, data in res["lods"].items():
            spec = emit_bin.BlobSpec(
                chunk_index=chunk, lod=lod, vertices=[tuple(v) for v in data["vertices"]],
                faces=[tuple(f) for f in data["faces"]],
                boxes=res["boxes"] if lod == emit_bin.LOD_COUNT - 1 else [],
                textures=data.get("textures", []))
            blobs[(chunk, lod)] = spec
            row[f"lod{lod}"] = {"faces": len(data["faces"]), "vertices": len(data["vertices"]),
                                "level": data["level"], "textures": len(spec.textures),
                                "texture_rate": data.get("texture_rate", 0.0),
                                "foliage": data.get("foliage", 0)}
            clamped += data["clamped"]
        per_chunk[chunk] = row
    archive, info = emit_bin.pack_archive(
        blobs, material_rgb555, ground_section, world_min_y, world_max_y,
        max_archive_bytes=opts.max_archive_bytes, texture_palette=texture_palette)

    level_hist = [[0] * len(blocks.LEVELS) for _ in range(emit_bin.LOD_COUNT)]
    for row in per_chunk.values():
        for lod in range(emit_bin.LOD_COUNT):
            if f"lod{lod}" in row:
                level_hist[lod][row[f"lod{lod}"]["level"]] += 1
    report["chunks"] = {"non_empty": len(results), "truncated_faces": 0,
                        "clamped_coordinates": clamped,
                        "faces_by_lod_max": info["max_faces"],
                        "vertices_by_lod_max": info["max_vertices"],
                        "block_level_histogram_by_lod": level_hist}
    report["archive"] = info
    report["seconds"] = round(time.time() - started, 1)
    extras = {"per_chunk": per_chunk, "base_palette": base, "levels": levels,
              "ground_bitmap": ground_bitmap, "ground_palette": ground_palette_rgb,
              "material_rgb555": material_rgb555, "blobs": blobs}
    return archive, info, report, extras


def _ground_section(gpos, gcol, base, under, rgb, areas, allp, light, levels, report, opts):
    """Ground bitmap from flat low triangles, painted low to high so kerbs,
    lane marks and crosswalks land on the road they sit on."""
    upd_x, upd_z = opts.ground_upd
    extent = emit_bin.CHUNK_UNITS * emit_bin.GRID_X
    gw, gh = extent // upd_x, extent // upd_z
    palette_rgb = base.copy()
    fill_index, fill_bounds = 0, None
    if opts.ground_fill == "auto" and under.any():
        colours, inverse = np.unique(rgb[under], axis=0, return_inverse=True)
        weight = np.bincount(inverse.reshape(-1), weights=areas[under])
        fill_colour = colours[int(np.argmax(weight))]
        palette_rgb = np.vstack([base, fill_colour[None, :]]).astype(np.uint8)
        fill_index = len(palette_rgb)
        fill_bounds = (float(allp[:, 0].min()), float(allp[:, 2].min()),
                       float(allp[:, 0].max()), float(allp[:, 2].max()))
        report["ground"]["fill_colour"] = [int(v) for v in fill_colour]
    bitmap = ch.rasterize_ground_marked(gpos, gcol + 1, gpos[:, :, 1].mean(axis=1), gw, gh,
                                        upd_x, upd_z, fill_index, fill_bounds)
    up_light = light[1] / math.sqrt(sum(c * c for c in light))
    palette = [0] + [ch.shade_rgb555(c, up_light * (levels - 1), levels, opts.ambient, opts.diffuse)
                     for c in palette_rgb]
    report["ground"]["bitmap_coverage"] = float((bitmap > 0).mean())
    report["ground"]["palette_entries"] = len(palette)
    return bitmap, palette_rgb, palette, (gw, gh, upd_x, upd_z)


def _build_objects_city(tris, rgb, normals, areas, visible, under, colour, base, ground_y,
                        world_min_y, world_max_y, material_rgb555, material_of, light, levels,
                        report, started, opts, log):
    vis_idx = np.nonzero(visible)[0]
    vpos = tris.pos[vis_idx]
    vnorm = normals[vis_idx]
    low = vpos[:, :, 1].max(axis=1) <= ground_y + blocks.LOW_UNITS
    flat = np.abs(vnorm[:, 1]) > blocks.UP_NY
    report["ground"]["low_triangles"] = int(low.sum())
    # Low and flat: the VDP2 ground. Low and upright (kerb sides): too small to draw.
    bitmap, palette_rgb, palette, (gw, gh, upd_x, upd_z) = _ground_section(
        vpos[low & flat], colour[low & flat], base, under, rgb, areas, tris.pos.reshape(-1, 3),
        light, levels, report, opts)
    ground_section = emit_bin.GroundSection(
        bitmap=bitmap.tobytes(), width=gw, height=gh, palette=palette,
        ground_y_units=ground_y, units_per_dot_x=upd_x, units_per_dot_z=upd_z)

    tall = ~low
    objects_src = split_objects(vpos[tall], colour[tall], tris.node[vis_idx][tall], log)
    cache_dir = Path(opts.cache_dir) / "objects" if opts.incremental else None
    if cache_dir is not None:
        cache_dir.mkdir(parents=True, exist_ok=True)
    caps = [tuple(int(v) for v in pair_) for pair_ in opts.caps]
    results, stats = build_object_chunks(objects_src, caps, len(base), material_of,
                                         world_min_y, ground_y, cache_dir, opts.jobs, log)
    report["objects"] = stats
    log(f"  {len(results)} chunks; surface kept by LOD {stats['surface_kept_by_lod']}")

    blobs, per_chunk, clamped = {}, {}, 0
    for chunk, res in sorted(results.items()):
        row = {"boxes": len(res["boxes"])}
        for lod, data in res["lods"].items():
            blobs[(chunk, lod)] = emit_bin.BlobSpec(
                chunk_index=chunk, lod=lod, vertices=[tuple(v) for v in data["vertices"]],
                faces=[tuple(f) for f in data["faces"]],
                boxes=res["boxes"] if lod == emit_bin.LOD_COUNT - 1 else [])
            row[f"lod{lod}"] = {"faces": len(data["faces"]), "vertices": len(data["vertices"])}
            clamped += data["clamped"]
        per_chunk[chunk] = row
    archive, info = emit_bin.pack_archive(
        blobs, material_rgb555, ground_section, world_min_y, world_max_y,
        max_archive_bytes=opts.max_archive_bytes)
    report["chunks"] = {"non_empty": len(results), "truncated_faces": 0,
                        "clamped_coordinates": clamped,
                        "faces_by_lod_max": info["max_faces"],
                        "vertices_by_lod_max": info["max_vertices"]}
    report["archive"] = info
    report["seconds"] = round(time.time() - started, 1)
    extras = {"per_chunk": per_chunk, "base_palette": base, "levels": levels,
              "ground_bitmap": bitmap, "ground_palette": palette_rgb,
              "material_rgb555": material_rgb555, "blobs": blobs}
    return archive, info, report, extras


def ch_parse_light(text):
    from model_pipeline.face_colors import parse_light_dir
    return parse_light_dir(text)


def check_gates(info: dict, report: dict, opts) -> list[str]:
    """Hard failures, each with the numbers that caused it."""
    failures = []
    for lod in range(emit_bin.LOD_COUNT):
        if info["max_faces"][lod] > opts.caps[lod][0]:
            failures.append(f"LOD{lod}: {info['max_faces'][lod]} faces > cap {opts.caps[lod][0]}")
        if info["max_vertices"][lod] > opts.caps[lod][1]:
            failures.append(f"LOD{lod}: {info['max_vertices'][lod]} vertices > cap {opts.caps[lod][1]}")
        if info["max_blob_bytes"][lod] > emit_bin.LOD_SLOT_BYTES[lod]:
            failures.append(f"LOD{lod}: blob {info['max_blob_bytes'][lod]} B > slot "
                            f"{emit_bin.LOD_SLOT_BYTES[lod]} B")
    if info["max_collision_boxes"] > emit_bin.LOD2_BOX_CAP:
        failures.append(f"{info['max_collision_boxes']} collision boxes > {emit_bin.LOD2_BOX_CAP}")
    if info["material_count"] > emit_bin.MATERIAL_MAX:
        failures.append(f"{info['material_count']} materials > {emit_bin.MATERIAL_MAX}")
    if info["total_bytes"] > opts.max_archive_bytes:
        failures.append(f"archive {info['total_bytes']} B > --max-archive-bytes {opts.max_archive_bytes}")
    if report["chunks"]["clamped_coordinates"]:
        failures.append(f"{report['chunks']['clamped_coordinates']} coordinates outside the "
                        f"{ch.OVERHANG_UNITS}-unit overhang (clipping failed)")
    if report["chunks"]["truncated_faces"] > opts.max_truncated_faces:
        failures.append(f"{report['chunks']['truncated_faces']} faces dropped by area truncation "
                        f"(> --max-truncated-faces {opts.max_truncated_faces})")
    return failures


def parse_caps(text: str):
    caps = []
    for part in text.split(","):
        verts, faces = part.split(":")
        caps.append((int(faces), int(verts)))  # stored as (face_cap, vert_cap)
    if len(caps) != emit_bin.LOD_COUNT:
        raise argparse.ArgumentTypeError("--lod-caps needs three verts:faces pairs")
    return caps


def write_previews(out_dir: Path, extras: dict, log=print):
    """Ground bitmap, colour swatches and a top-down LOD0 map, for eyeballing
    the result (plan risk R10: colours can come out as grey mush)."""
    from PIL import Image, ImageDraw

    out_dir.mkdir(parents=True, exist_ok=True)
    levels = extras["levels"]
    base = extras["base_palette"]
    sw = Image.new("RGB", (levels * 24, len(base) * 12), (0, 0, 0))
    draw = ImageDraw.Draw(sw)
    for row, colour in enumerate(base.tolist()):
        for k in range(levels):
            rgb555 = ch.shade_rgb555(np.array(colour), k, levels, 0.42, 0.60)
            r, g, b = (rgb555 & 31) * 255 // 31, ((rgb555 >> 5) & 31) * 255 // 31, ((rgb555 >> 10) & 31) * 255 // 31
            draw.rectangle([k * 24, row * 12, k * 24 + 23, row * 12 + 11], fill=(r, g, b))
    sw.save(out_dir / "building_swatches.png")
    if extras["ground_bitmap"] is not None:
        pal = [(0, 0, 0)] + [tuple(int(v) for v in c) for c in extras["ground_palette"]]
        lut = np.zeros((256, 3), dtype=np.uint8)
        lut[:len(pal)] = pal
        Image.fromarray(lut[extras["ground_bitmap"]]).save(out_dir / "ground.png")
    # Top-down LOD0, painter-sorted by height.
    size = emit_bin.GRID_X * emit_bin.CHUNK_UNITS
    img = Image.new("RGB", (size * 2, size * 2), (24, 24, 32))
    draw = ImageDraw.Draw(img)
    mats = extras["material_rgb555"]
    polys = []
    for (chunk, lod), spec in extras["blobs"].items():
        if lod != 0:
            continue
        cx, cz = chunk % emit_bin.GRID_X, chunk // emit_bin.GRID_X
        for a, b, c, d, m in spec.faces:
            pts = [spec.vertices[i] for i in (a, b, c, d)]
            y = sum(p[1] for p in pts) / 4.0
            polys.append((y, cx, cz, pts, m))
    polys.sort(key=lambda t: t[0])
    for y, cx, cz, pts, m in polys:
        rgb555 = mats[m]
        colour = ((rgb555 & 31) * 255 // 31, ((rgb555 >> 5) & 31) * 255 // 31,
                  ((rgb555 >> 10) & 31) * 255 // 31)
        xy = [((cx * emit_bin.CHUNK_UNITS + p[0] / 64.0) * 2, (cz * emit_bin.CHUNK_UNITS + p[2] / 64.0) * 2)
              for p in pts]
        draw.polygon(xy, fill=colour)
    img.save(out_dir / "topdown_lod0.png")
    log(f"  previews in {out_dir}")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--input", required=True, type=Path)
    ap.add_argument("--out-bin", required=True, type=Path)
    ap.add_argument("--out-h", type=Path)
    ap.add_argument("--cache-dir", type=Path, default=Path("build/generated/city_walk"))
    ap.add_argument("--incremental", action="store_true",
                    help="cache per-object LOD chains in --cache-dir/objects")
    ap.add_argument("--geometry", choices=("facades", "blocks", "objects"), default="facades",
                    help="facades: blocks with wall textures baked from the source (default); "
                         "blocks: untextured blocks; objects: per-object simplified LODs")
    ap.add_argument("--grid", default="16x16")
    ap.add_argument("--chunk-units", type=int, default=32)
    ap.add_argument("--grid-origin", default="-128,-320")
    ap.add_argument("--lod-caps", type=parse_caps, default="384:176,144:64,48:24", dest="caps")
    ap.add_argument("--face-budget", type=int, default=600)
    ap.add_argument("--light-dir", default="-0.5,0.6,0.8")
    ap.add_argument("--ambient", type=float, default=0.42)
    ap.add_argument("--diffuse", type=float, default=0.60)
    ap.add_argument("--levels", type=int, default=5)
    ap.add_argument("--colors", type=int, default=44)
    ap.add_argument("--ground-colors", type=int, default=48)
    ap.add_argument("--ground-upd", type=lambda t: tuple(int(v) for v in t.split(",")), default="1,2",
                    help="world units per ground-bitmap dot in x,z (default 1,2: a 512x256 bitmap)")
    ap.add_argument("--ground-fill", choices=("auto", "none"), default="auto")
    ap.add_argument("--max-archive-bytes", type=int, default=2097152)
    ap.add_argument("--max-truncated-faces", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=max(1, min(8, (multiprocessing.cpu_count() or 2) - 1)))
    ap.add_argument("--report", type=Path)
    ap.add_argument("--preview-dir", type=Path)
    opts = ap.parse_args(argv)
    if isinstance(opts.caps, str):
        opts.caps = parse_caps(opts.caps)
    if isinstance(opts.ground_upd, str):
        opts.ground_upd = tuple(int(v) for v in opts.ground_upd.split(","))

    if opts.grid != "16x16" or opts.chunk_units != 32 or opts.grid_origin != "-128,-320":
        print("error: this build of the chunker fixes the grid at 16x16, 32 units, origin "
              "-128,-320 (examples/city_walk/city_grid.h)", file=sys.stderr)
        return 2
    if opts.face_budget != 600:
        print(f"note: --face-budget {opts.face_budget} is checked at runtime; the LOD caps "
              f"are what the archive enforces")
    print(f"city_chunker: {opts.input}")
    try:
        plain = draco.decode_to_plain_glb(opts.input, opts.cache_dir)
        archive, info, report, extras = build_city(plain, opts)
        failures = check_gates(info, report, opts)
    except GltfError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    report["source_sha256"] = draco.sha256_file(opts.input)
    if failures:
        print("GATE FAILURES:", file=sys.stderr)
        for line in failures:
            print(f"  - {line}", file=sys.stderr)
        return 1
    opts.out_bin.parent.mkdir(parents=True, exist_ok=True)
    opts.out_bin.write_bytes(archive)
    if opts.out_h:
        opts.out_h.parent.mkdir(parents=True, exist_ok=True)
        opts.out_h.write_text(emit_bin.emit_header_h(info, report["source_sha256"]), encoding="utf-8")
    if opts.report:
        report["per_chunk"] = {str(k): v for k, v in extras["per_chunk"].items()}
        opts.report.parent.mkdir(parents=True, exist_ok=True)
        opts.report.write_text(json.dumps(report, indent=1, default=lambda o: o.tolist()
                                          if hasattr(o, "tolist") else str(o)), encoding="utf-8")
    if opts.preview_dir:
        write_previews(opts.preview_dir, extras)
    print(f"  wrote {opts.out_bin} ({info['total_bytes']} bytes, {info['blobs']} blobs, "
          f"{info['material_count']} materials, ground {info['ground_bytes']} B) "
          f"in {report['seconds']} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
