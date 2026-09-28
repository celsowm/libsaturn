#!/usr/bin/env python3
"""Reads CITY.BIN back and renders street-level views of it (host only).

Two jobs. It is the archive's independent reader -- written from the layout in
examples/city_walk/city_format.h, not from emit_bin.py -- so
tests/test_city_chunker.py can compare writer and reader. And it is how the
result gets looked at before any Saturn code runs: the ground bitmap is drawn
as the VDP2 plane will draw it (per-scanline ray/plane intersection), and the
blobs as the VDP1 painter will (LOD by ring, far chunk first, runtime winding
with backface culling), so a wrong winding, a wrong colour or a floating
building shows up here.

    python tools/city_preview.py build/generated/city_walk/iso/CITY.BIN \\
        --eye 3,7 --yaw 30 --out street.png
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

import numpy as np

GRID_X = GRID_Z = 16
CHUNK = 32.0
ORIGIN_X, ORIGIN_Z = -128.0, -320.0
LODS = 3


def read_archive(data: bytes) -> dict:
    """Parse CITY.BIN into plain Python/numpy structures."""
    if struct.unpack_from(">I", data, 0)[0] != 0x43545931:
        raise ValueError("bad magic")
    (version, flags, gx, gz) = struct.unpack_from(">HHHH", data, 4)
    (ox, oz) = struct.unpack_from(">ii", data, 0x0C)
    (chunk_units, quant, lod_count) = struct.unpack_from(">HHB", data, 0x14)
    (mat_count,) = struct.unpack_from(">H", data, 0x1A)
    (mat_off, toc_off, blob_base, total) = struct.unpack_from(">IIII", data, 0x1C)
    (min_y, max_y, crc) = struct.unpack_from(">iiI", data, 0x2C)
    max_v = struct.unpack_from(">3H", data, 0x38)
    max_f = struct.unpack_from(">3H", data, 0x3E)
    max_b = struct.unpack_from(">3H", data, 0x44)
    (max_boxes,) = struct.unpack_from(">H", data, 0x4A)
    (g_off, g_bytes, g_pal_off, g_y) = struct.unpack_from(">IIIi", data, 0x4C)
    (g_w, g_h, g_pal_n, g_upd_x, g_upd_z) = struct.unpack_from(">HHHHH", data, 0x5C)
    (t_pal_off, t_pal_n) = struct.unpack_from(">IH", data, 0x68)
    max_t = struct.unpack_from(">3H", data, 0x6E)
    header = dict(version=version, flags=flags, grid=(gx, gz), origin=(ox, oz),
                  chunk_units=chunk_units, quant=quant, lod_count=lod_count,
                  material_count=mat_count, material_offset=mat_off, toc_offset=toc_off,
                  blob_base=blob_base, total_bytes=total, world_min_y=min_y, world_max_y=max_y,
                  toc_crc32=crc, max_vertices=max_v, max_faces=max_f, max_blob_bytes=max_b,
                  max_collision_boxes=max_boxes, ground_offset=g_off, ground_bytes=g_bytes,
                  ground_palette_offset=g_pal_off, ground_y=g_y, ground_width=g_w,
                  ground_height=g_h, ground_palette_count=g_pal_n, ground_upd=(g_upd_x, g_upd_z),
                  texture_palette_offset=t_pal_off, texture_palette_count=t_pal_n,
                  max_texture_bytes=max_t)
    toc_entry = 16 if version >= 2 else 12
    materials = [struct.unpack_from(">H", data, mat_off + 4 * i)[0] for i in range(mat_count)]
    toc = {}
    for chunk in range(gx * gz):
        for lod in range(lod_count):
            at = toc_off + toc_entry * (chunk * lod_count + lod)
            off, nbytes, vc, fc, bank, fl = struct.unpack_from(">IHHHBB", data, at)
            tex_bytes = struct.unpack_from(">H", data, at + 12)[0] if toc_entry == 16 else 0
            toc[(chunk, lod)] = dict(offset=off, bytes=nbytes, vertex_count=vc,
                                     face_count=fc, bank=bank, flags=fl, tex_bytes=tex_bytes)
    blobs = {}
    for key, e in toc.items():
        if e["flags"] & 1:
            continue
        base = blob_base + e["offset"]
        (magic, chunk_index, lod, bflags, vc, fc) = struct.unpack_from(">IHBBHH", data, base)
        bbox = struct.unpack_from(">6h", data, base + 12)
        (v_off, f_off, c_off, c_n) = struct.unpack_from(">HHHH", data, base + 24)
        verts = np.array([struct.unpack_from(">3h", data, base + v_off + 6 * i) for i in range(vc)],
                         dtype=np.int64).reshape(-1, 3)
        faces = [struct.unpack_from(">4HBB", data, base + f_off + 10 * i) for i in range(fc)]
        boxes = [struct.unpack_from(">6h", data, base + c_off + 12 * i) for i in range(c_n)]
        textures = []
        texture_flags = []
        if e["tex_bytes"]:
            # The texture block follows the blob, 32-byte aligned.
            tb = base + (e["bytes"] + 31) // 32 * 32
            (count,) = struct.unpack_from(">H", data, tb)
            for i in range(count):
                w, h, off8, flags = struct.unpack_from(">HHHH", data, tb + 4 + 8 * i)
                texels = np.frombuffer(data, dtype=np.uint8, count=w * h,
                                       offset=tb + off8 * 8).reshape(h, w)
                textures.append(texels)
                texture_flags.append(flags)
        blobs[key] = dict(magic=magic, chunk_index=chunk_index, lod=lod, flags=bflags,
                          vertices=verts, faces=faces, boxes=boxes, bbox=bbox,
                          offsets=(v_off, f_off, c_off), entry=e, textures=textures,
                          texture_flags=texture_flags)
    ground = None
    if g_bytes:
        bitmap = np.frombuffer(data, dtype=np.uint8, count=g_bytes, offset=g_off).reshape(g_h, g_w)
        pal = [struct.unpack_from(">H", data, g_pal_off + 2 * i)[0] for i in range(g_pal_n)]
        ground = dict(bitmap=bitmap, palette=pal)
    texture_palette = [struct.unpack_from(">H", data, t_pal_off + 2 * i)[0] for i in range(t_pal_n)]
    return dict(header=header, materials=materials, toc=toc, blobs=blobs, ground=ground,
                texture_palette=texture_palette)


def _rgb(rgb555: int):
    return ((rgb555 & 31) * 255 // 31, ((rgb555 >> 5) & 31) * 255 // 31,
            ((rgb555 >> 10) & 31) * 255 // 31)


def render_street(archive: dict, eye_chunk, eye_local, yaw_deg: float, eye_height: float = 1.7,
                  width: int = 320, height: int = 224, focal: float = 215.0, scale: int = 3):
    """Perspective view from a chunk-local eye position, pitch 0."""
    from PIL import Image, ImageDraw

    hdr = archive["header"]
    ex = ORIGIN_X + eye_chunk[0] * CHUNK + eye_local[0]
    ez = ORIGIN_Z + eye_chunk[1] * CHUNK + eye_local[1]
    ey = hdr["ground_y"] / 65536.0 + eye_height
    yaw = math.radians(yaw_deg)
    fwd = np.array([math.sin(yaw), 0.0, math.cos(yaw)])
    # Right-handed like glTF and the library camera: right = forward x up.
    right = np.array([-math.cos(yaw), 0.0, math.sin(yaw)])
    horizon = height // 2

    img = np.zeros((height, width, 3), dtype=np.uint8)
    for y in range(height):
        t = y / height
        img[y, :] = (int(120 + 60 * t), int(170 + 50 * t), int(230 - 20 * t))

    ground = archive["ground"]
    if ground is not None:
        gy = hdr["ground_y"] / 65536.0
        pal = np.array([_rgb(c) for c in ground["palette"]] + [(0, 0, 0)] * 256, dtype=np.uint8)
        ys = np.arange(horizon + 1, height)
        xs = np.arange(width)
        dy = (ys - horizon + 0.5) / focal  # ray slope below the horizon
        depth = (ey - gy) / dy  # distance along the forward axis
        side = (xs[None, :] - width / 2.0 + 0.5) / focal * depth[:, None]
        wx = ex + fwd[0] * depth[:, None] + right[0] * side
        wz = ez + fwd[2] * depth[:, None] + right[2] * side
        upd_x, upd_z = (max(v, 1) for v in hdr["ground_upd"])
        bx = np.floor((wx - ORIGIN_X) / upd_x).astype(np.int64)
        bz = np.floor((wz - ORIGIN_Z) / upd_z).astype(np.int64)
        inside = (bx >= 0) & (bx < hdr["ground_width"]) & (bz >= 0) & (bz < hdr["ground_height"])
        idx = np.zeros(bx.shape, dtype=np.int64)
        idx[inside] = ground["bitmap"][bz[inside], bx[inside]]
        painted = idx > 0
        region = img[horizon + 1:, :]
        region[painted] = pal[idx[painted]]

    out = Image.fromarray(img).resize((width * scale, height * scale), Image.NEAREST)
    draw = ImageDraw.Draw(out)
    mats = archive["materials"]
    items = []
    for cz in range(GRID_Z):
        for cx in range(GRID_X):
            d = max(abs(cx - eye_chunk[0]), abs(cz - eye_chunk[1]))
            lod = 0 if d <= 1 else (1 if d <= 2 else (2 if d <= 3 else -1))
            if lod < 0:
                continue
            blob = None
            for l in range(lod, LODS):
                blob = archive["blobs"].get((cz * GRID_X + cx, l))
                if blob is not None:
                    break
            if blob is None:
                continue
            dist = math.hypot(cx - eye_chunk[0] + 0.5 - eye_local[0] / CHUNK,
                              cz - eye_chunk[1] + 0.5 - eye_local[1] / CHUNK)
            items.append((dist, cx, cz, blob))
    items.sort(key=lambda t: -t[0])  # far chunks first: the painter pass order
    base_y = hdr["world_min_y"] / 65536.0
    drawn = 0
    for dist, cx, cz, blob in items:
        v = blob["vertices"].astype(np.float64) / 64.0
        world = np.stack([ORIGIN_X + cx * CHUNK + v[:, 0], base_y + v[:, 1],
                          ORIGIN_Z + cz * CHUNK + v[:, 2]], axis=1)
        rel = world - np.array([ex, ey, ez])
        cam_z = rel @ fwd
        cam_x = rel @ right
        cam_y = rel[:, 1]
        faces = []
        for a, b, c, d_, m, _r in blob["faces"]:
            idx = (a, b, c, d_)
            zs = [cam_z[i] for i in idx]
            if min(zs) <= 0.3:
                continue
            A, B, D = world[a], world[b], world[d_]
            normal = np.cross(D - A, B - A)  # runtime winding: outward normal
            centre = (world[a] + world[b] + world[c] + world[d_]) / 4.0
            if float(normal @ (np.array([ex, ey, ez]) - centre)) <= 0.0:
                continue  # back face
            faces.append((float(np.mean(zs)), idx, m))
        faces.sort(key=lambda t: -t[0])
        for _z, idx, m in faces:
            pts = [((width / 2.0 + focal * cam_x[i] / cam_z[i]) * scale,
                    (horizon - focal * cam_y[i] / cam_z[i]) * scale) for i in idx]
            draw.polygon(pts, fill=_rgb(mats[m]))
            drawn += 1
    return out, drawn


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("archive", type=Path)
    ap.add_argument("--eye", default="3,7", help="chunk x,z of the eye")
    ap.add_argument("--local", default="16,16", help="position inside the chunk, units")
    ap.add_argument("--yaw", type=float, default=0.0)
    ap.add_argument("--out", type=Path, required=True)
    opts = ap.parse_args(argv)
    archive = read_archive(opts.archive.read_bytes())
    ex, ez = (int(v) for v in opts.eye.split(","))
    lx, lz = (float(v) for v in opts.local.split(","))
    img, drawn = render_street(archive, (ex, ez), (lx, lz), opts.yaw)
    img.save(opts.out)
    print(f"{opts.out}: {drawn} faces drawn")
    return 0


if __name__ == "__main__":
    sys.exit(main())
