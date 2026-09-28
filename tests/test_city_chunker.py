"""tools/city_chunker.py and its model_pipeline modules, on a synthetic city.

The fixture is built here (a ground plane, boxes, a wall that crosses a chunk
border) so no large binary lives in git and no test depends on Draco or Node.
The archive is read back with tools/city_preview.py, which parses the layout
independently of emit_bin.py: writer and reader check each other.
"""

import io
import json
import math
import struct
import sys
import tempfile
import unittest
import zlib
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

import city_chunker  # noqa: E402
import city_preview  # noqa: E402
from model_pipeline import chunking as ch  # noqa: E402
from model_pipeline import emit_bin  # noqa: E402
from model_pipeline import foliage  # noqa: E402
from model_pipeline.gltf import GltfError  # noqa: E402

# Chunk (4,4) spans x 0..32, z -192..-160; chunk (5,4) x 32..64.
CX0, CZ0 = 4, 4
X0 = ch.ORIGIN_X + CX0 * ch.CHUNK_UNITS
Z0 = ch.ORIGIN_Z + CZ0 * ch.CHUNK_UNITS


def cuboid(x0, y0, z0, x1, y1, z1):
    """12 CCW-outward triangles of an axis-aligned box."""
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
         (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    quads = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 4, 7, 3), (1, 2, 6, 5), (3, 7, 6, 2), (0, 1, 5, 4)]
    tris = []
    for a, b, c, d in quads:
        tris += [(v[a], v[b], v[c]), (v[a], v[c], v[d])]
    return tris


def plane_up(x0, z0, x1, z1, y):
    a, b, c, d = (x0, y, z0), (x0, y, z1), (x1, y, z1), (x1, y, z0)
    return [(a, b, d), (b, c, d)]


def make_glb(meshes):
    """meshes: list of (triangles, material_index). Materials: 0 grey, 1 red, 2 blue."""
    bin_data = bytearray()
    accessors, views, gl_meshes, nodes = [], [], [], []
    for tris, material in meshes:
        flat = [c for tri in tris for p in tri for c in p]
        lo = [min(flat[i::3]) for i in range(3)]
        hi = [max(flat[i::3]) for i in range(3)]
        while len(bin_data) % 4:
            bin_data.append(0)
        views.append({"buffer": 0, "byteOffset": len(bin_data), "byteLength": len(flat) * 4})
        bin_data += struct.pack("<%df" % len(flat), *flat)
        accessors.append({"bufferView": len(views) - 1, "componentType": 5126,
                          "count": len(flat) // 3, "type": "VEC3", "min": lo, "max": hi})
        while len(bin_data) % 4:
            bin_data.append(0)
        idx = list(range(len(flat) // 3))
        views.append({"buffer": 0, "byteOffset": len(bin_data), "byteLength": len(idx) * 2})
        bin_data += struct.pack("<%dH" % len(idx), *idx)
        accessors.append({"bufferView": len(views) - 1, "componentType": 5123,
                          "count": len(idx), "type": "SCALAR"})
        gl_meshes.append({"primitives": [{"attributes": {"POSITION": len(accessors) - 2},
                                          "indices": len(accessors) - 1, "material": material}]})
        nodes.append({"mesh": len(gl_meshes) - 1})
    doc = {
        "asset": {"version": "2.0"},
        "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes, "meshes": gl_meshes, "accessors": accessors,
        "bufferViews": views, "buffers": [{"byteLength": len(bin_data)}],
        "materials": [
            {"pbrMetallicRoughness": {"baseColorFactor": [0.6, 0.6, 0.6, 1]}},
            {"pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.05, 0.05, 1]}},
            {"pbrMetallicRoughness": {"baseColorFactor": [0.05, 0.1, 0.8, 1]}},
        ],
    }
    js = json.dumps(doc, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    bin_data += b"\x00" * (-len(bin_data) % 4)
    total = 12 + 8 + len(js) + 8 + len(bin_data)
    return (struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(js), b"JSON") + js
            + struct.pack("<I4s", len(bin_data), b"BIN\x00") + bytes(bin_data))


def synthetic_city():
    ground = plane_up(X0, Z0, X0 + 64, Z0 + 32, 0.0)
    return make_glb([
        (ground, 0),
        (cuboid(X0 + 4, 0.0, Z0 + 4, X0 + 12, 9.0, Z0 + 14), 1),   # inside chunk (4,4)
        (cuboid(X0 + 20, 0.0, Z0 + 20, X0 + 44, 6.0, Z0 + 26), 2),  # crosses x = 32
        (cuboid(X0 + 40, 0.0, Z0 + 4, X0 + 50, 12.0, Z0 + 12), 1),  # inside chunk (5,4)
    ])


class Run:
    """One chunker invocation in a temp dir."""

    def __init__(self, tmp, glb_bytes, *extra):
        self.tmp = Path(tmp)
        self.glb = self.tmp / "city.glb"
        self.glb.write_bytes(glb_bytes)
        self.bin = self.tmp / "out" / "CITY.BIN"
        self.h = self.tmp / "out" / "city_data.h"
        self.report = self.tmp / "out" / "report.json"
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            self.code = city_chunker.main([
                "--input", str(self.glb), "--out-bin", str(self.bin), "--out-h", str(self.h),
                "--report", str(self.report), "--cache-dir", str(self.tmp / "cache"),
                "--jobs", "1", *map(str, extra)])
        self.stdout, self.stderr = out.getvalue(), err.getvalue()

    def archive(self):
        return city_preview.read_archive(self.bin.read_bytes())


def face_normal(verts, face):
    a, b, c, d = face[:4]
    v = verts.astype(np.float64)
    return np.cross(v[d] - v[a], v[b] - v[a])


class ArchiveLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.res = Run(cls.tmp.name, synthetic_city())
        assert cls.res.code == 0, cls.res.stderr
        cls.raw = cls.res.bin.read_bytes()
        cls.archive = city_preview.read_archive(cls.raw)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_header_is_big_endian_at_documented_offsets(self):
        raw = self.raw
        self.assertEqual(raw[0:4], b"CTY1")
        self.assertEqual(struct.unpack_from(">H", raw, 0x04)[0], emit_bin.VERSION)
        self.assertEqual(struct.unpack_from(">HH", raw, 0x08), (16, 16))
        self.assertEqual(struct.unpack_from(">i", raw, 0x0C)[0], -128 << 16)
        self.assertEqual(struct.unpack_from(">i", raw, 0x10)[0], -320 << 16)
        self.assertEqual(struct.unpack_from(">HH", raw, 0x14), (32, 64))
        self.assertEqual(raw[0x18], 3)
        h = self.archive["header"]
        self.assertEqual(h["total_bytes"], len(raw))
        self.assertEqual(len(raw) % 32, 0)
        self.assertEqual(h["blob_base"] % 32, 0)
        self.assertEqual(h["toc_offset"] % 32, 0)
        self.assertEqual(h["material_offset"], 128)

    def test_toc_matches_blob_headers_and_alignment(self):
        toc, blobs = self.archive["toc"], self.archive["blobs"]
        self.assertEqual(len(toc), 256 * 3)
        seen = 0
        for (chunk, lod), e in toc.items():
            if e["flags"] & 1:
                self.assertEqual((e["offset"], e["bytes"], e["vertex_count"], e["face_count"]),
                                 (0, 0, 0, 0))
                continue
            seen += 1
            blob = blobs[(chunk, lod)]
            self.assertEqual(e["offset"] % 32, 0, "blobs are 32-byte aligned")
            self.assertEqual(blob["magic"], emit_bin.BLOB_MAGIC)
            self.assertEqual(blob["chunk_index"], chunk)
            self.assertEqual(blob["lod"], lod)
            self.assertEqual(len(blob["vertices"]), e["vertex_count"])
            self.assertEqual(len(blob["faces"]), e["face_count"])
            v_off, f_off, c_off = blob["offsets"]
            self.assertEqual(v_off, 32)
            self.assertEqual(f_off, (32 + 6 * e["vertex_count"] + 3) // 4 * 4)
            end = f_off + 10 * e["face_count"] + 12 * len(blob["boxes"])
            self.assertLessEqual(end, e["bytes"])
            self.assertLessEqual(e["bytes"], emit_bin.LOD_SLOT_BYTES[lod])
            self.assertEqual(e["bank"], 0)
        self.assertEqual(seen, 2 * 3, "two occupied chunks x three LODs")
        # Chunks with nothing in them are flagged empty, not zero-length blobs.
        empty = toc[(0, 0)]
        self.assertTrue(empty["flags"] & 1)

    def test_toc_crc_covers_materials_and_toc(self):
        h = self.archive["header"]
        end = h["toc_offset"] + 16 * 256 * 3
        self.assertEqual(zlib.crc32(self.raw[h["material_offset"]:end]) & 0xFFFFFFFF, h["toc_crc32"])

    def test_faces_wind_outward_in_runtime_order(self):
        """cross(D - A, B - A) must point away from the solid (mesh3d.h)."""
        blob = self.archive["blobs"][(CZ0 * 16 + CX0, 0)]
        verts = blob["vertices"]
        # The lone box in this chunk: 8x9x10 units at (4, 0, 4). Its centre:
        centre = np.array([(4 + 8) / 2.0, 4.5, (4 + 14) / 2.0]) * 64.0
        box_faces = [f for f in blob["faces"]
                     if verts[list(f[:4])].max(axis=0)[1] > 1 * 64 and
                     verts[list(f[:4])].max(axis=0)[0] < 13 * 64]
        # LOD0 first tries 4x4 facade patches. The 8-unit sides become 2x3
        # patches and the 10-unit sides 3x3 for this 9-unit-tall block:
        # 2*(2*3) + 2*(3*3) = 30. A near camera can therefore reject one
        # projected patch without flattening the whole building facade.
        self.assertEqual(len(box_faces), 30,
                         "LOD0 should select the 4x4 near-safe facade split")
        self.assertTrue(all(f[5] > 0 for f in box_faces), "an untextured wall")
        self.assertEqual(len(blob["textures"]), len(blob["faces"]))
        for f in box_faces:
            a, b, c, d = f[:4]
            self.assertEqual(len({a, b, c, d}), 4, "box faces are true quads")
            pts = verts[[a, b, c, d]]
            self.assertLessEqual(int(np.ptp(pts[:, 1])), 4 * 64,
                                 "LOD0 facade patch is too tall")
            self.assertLessEqual(max(int(np.ptp(pts[:, 0])), int(np.ptp(pts[:, 2]))),
                                 4 * 64, "LOD0 facade patch is too wide")
            middle = pts.astype(np.float64).mean(axis=0)
            self.assertGreater(float(face_normal(verts, f) @ (middle - centre)), 0.0,
                               f"face {f} winds inward")

    def test_every_facade_face_keeps_a_texture_at_every_lod(self):
        for (chunk, lod), blob in self.archive["blobs"].items():
            self.assertEqual(len(blob["textures"]), len(blob["faces"]),
                             f"chunk {chunk} LOD{lod} lost facade texture coverage")
            self.assertTrue(all(face[5] > 0 for face in blob["faces"]),
                            f"chunk {chunk} LOD{lod} contains a solid fallback face")

    def test_triangles_repeat_their_last_corner(self):
        for blob in self.archive["blobs"].values():
            for a, b, c, d, _m, _r in blob["faces"]:
                if len({a, b, c, d}) == 3:
                    self.assertEqual(d, c, "a runtime triangle repeats its last corner: D == C")

    def test_quantisation_round_trips_within_a_sixty_fourth(self):
        archive = self.archive
        blob = archive["blobs"][(CZ0 * 16 + CX0, 0)]
        v = blob["vertices"].astype(np.float64) / 64.0
        # The box's x extent 4..12 and z extent 4..14 in chunk-local units.
        top = v[np.isclose(v[:, 1], (9.0 - (archive["header"]["world_min_y"] >> 16)))]
        self.assertTrue(len(top))
        self.assertLessEqual(abs(top[:, 0].min() - 4.0), 1 / 64)
        self.assertLessEqual(abs(top[:, 0].max() - 12.0), 1 / 64)
        self.assertLessEqual(abs(top[:, 2].min() - 4.0), 1 / 64)
        self.assertLessEqual(abs(top[:, 2].max() - 14.0), 1 / 64)

    def test_every_coordinate_stays_inside_the_promised_overhang(self):
        lo, hi = -16 * 64, (32 + 16) * 64
        for blob in self.archive["blobs"].values():
            v = blob["vertices"]
            self.assertTrue((v[:, 0] >= lo).all() and (v[:, 0] <= hi).all())
            self.assertTrue((v[:, 2] >= lo).all() and (v[:, 2] <= hi).all())
            self.assertTrue((v[:, 1] >= -32768).all() and (v[:, 1] <= 32767).all())

    def test_border_crossing_wall_is_cut_into_both_chunks(self):
        blobs = self.archive["blobs"]
        left = blobs[(CZ0 * 16 + CX0, 0)]["vertices"]
        right = blobs[(CZ0 * 16 + CX0 + 1, 0)]["vertices"]
        # Wall spans x 20..44: 12 units in the left chunk, 12 in the right.
        self.assertTrue((left[:, 0] == 32 * 64).any(), "left half ends exactly on the border")
        self.assertTrue((right[:, 0] == 0).any(), "right half starts exactly on the border")
        self.assertTrue((left[:, 0] >= 0).all() and (left[:, 0] <= 32 * 64).all())

    def test_ground_leaves_the_blobs_and_lands_in_the_bitmap(self):
        g = self.archive["ground"]
        self.assertIsNotNone(g)
        h = self.archive["header"]
        self.assertEqual((h["ground_width"], h["ground_height"]), (512, 256))
        self.assertEqual(h["ground_upd"], (1, 2))
        self.assertEqual(h["ground_bytes"], 512 * 256)
        bitmap = g["bitmap"]
        # The plane covers x 0..64, z -192..-160: dots x 128..192, z (128..160)/2 = 64..80.
        self.assertTrue((bitmap[65:79, 130:190] > 0).all())
        self.assertEqual(int(bitmap[0:50, 0:100].max()), 0, "no ground far from the city")
        base_y = h["world_min_y"] / 65536.0
        for blob in self.archive["blobs"].values():
            for a, b, c, d, _m, _r in blob["faces"]:
                ys = blob["vertices"][[a, b, c, d]][:, 1] / 64.0 + base_y
                pts = blob["vertices"][[a, b, c, d]]
                flat = np.ptp(ys) < 1e-6 and abs(ys[0] - h["ground_y"] / 65536.0) < 0.5
                normal = face_normal(blob["vertices"], (a, b, c, d))
                self.assertFalse(flat and normal[1] > 0, "a ground face survived in a blob")

    def test_collision_boxes_live_only_in_lod2_and_cover_the_buildings(self):
        blobs = self.archive["blobs"]
        for lod in (0, 1):
            self.assertEqual(blobs[(CZ0 * 16 + CX0, lod)]["boxes"], [])
        boxes = blobs[(CZ0 * 16 + CX0, 2)]["boxes"]
        self.assertGreaterEqual(len(boxes), 1)
        self.assertLessEqual(len(boxes), 24)
        # A walker at (8, 9): inside the red box's footprint x 4..12, z 4..14.
        px, pz = 8 * 64, 9 * 64
        self.assertTrue(any(abs(px - cx) <= hx and abs(pz - cz) <= hz
                            for cx, _cy, cz, hx, _hy, hz in boxes))
        # An open spot on the street (x 20, z 2) is free.
        self.assertFalse(any(abs(20 * 64 - cx) <= hx and abs(2 * 64 - cz) <= hz
                             for cx, _cy, cz, hx, _hy, hz in boxes))

    def test_generated_header_matches_the_archive(self):
        text = self.res.h.read_text()
        self.assertIn(f"#define CITY_ARCHIVE_BYTES {len(self.raw)}u", text)
        self.assertIn(f"#define CITY_MATERIAL_COUNT {self.archive['header']['material_count']}u", text)
        self.assertIn("#define CITY_SOURCE_SHA256", text)

    def test_report_records_what_was_dropped_and_clipped(self):
        rep = json.loads(self.res.report.read_text())
        self.assertEqual(rep["chunks"]["truncated_faces"], 0)
        self.assertEqual(rep["chunks"]["clamped_coordinates"], 0)
        self.assertGreater(rep["surface"]["tall_fraction"], 0.0)
        self.assertGreater(rep["ground"]["triangles"], 0)


class FoliageQualityTests(unittest.TestCase):
    def test_near_foliage_has_a_real_pixel_floor_and_deduplicates(self):
        rgba = np.zeros((64, 32, 4), dtype=np.uint8)
        rgba[4:-4, 4:-4] = (40, 180, 60, 255)
        item = foliage.Billboard(
            node=1, chunk=0, center_x=0.0, center_z=0.0,
            bottom_y=0.0, top_y=4.0, width=2.0,
            rgba=rgba, source_kind="source_mask")
        tex, flags = foliage.texture_for_lod(item, 0)
        self.assertGreaterEqual(tex.shape[1], 32)
        self.assertGreaterEqual(tex.shape[0], 32)
        self.assertEqual(flags, emit_bin.TEXTURE_FLAG_CUTOUT |
                                emit_bin.TEXTURE_FLAG_BILLBOARD)
        unique, refs = foliage.texture_set_for_lod([item, item], 0)
        self.assertEqual(len(unique), 1)
        self.assertEqual(refs, [1, 1])


class ChunkerBehaviourTests(unittest.TestCase):
    def test_output_is_byte_identical_across_runs(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            first = Run(a, synthetic_city())
            second = Run(b, synthetic_city())
            self.assertEqual(first.code, 0)
            self.assertEqual(second.code, 0)
            self.assertEqual(first.bin.read_bytes(), second.bin.read_bytes())

    def test_incremental_flag_is_accepted_and_keeps_the_bytes(self):
        # The block model builds in seconds, so there is no chunk cache any
        # more; the Makefile still passes --incremental and must keep working.
        with tempfile.TemporaryDirectory() as tmp:
            first = Run(tmp, synthetic_city(), "--incremental")
            self.assertEqual(first.code, 0, first.stderr)
            before = first.bin.read_bytes()
            second = Run(tmp, synthetic_city(), "--incremental")
            self.assertEqual(second.bin.read_bytes(), before)

    def test_over_cap_chunks_are_decimated_not_truncated(self):
        # A 40x40 grid of small boxes crammed into one chunk: far over the caps.
        boxes = []
        for i in range(10):
            for j in range(10):
                boxes += cuboid(X0 + 1 + i * 3.0, 0, Z0 + 1 + j * 3.0,
                                X0 + 3 + i * 3.0, 4 + (i + j) % 5, Z0 + 3 + j * 3.0)
        glb = make_glb([(plane_up(X0, Z0, X0 + 32, Z0 + 32, 0.0), 0), (boxes, 1)])
        with tempfile.TemporaryDirectory() as tmp:
            run = Run(tmp, glb)
            self.assertEqual(run.code, 0, run.stderr)
            rep = json.loads(run.report.read_text())
            self.assertEqual(rep["chunks"]["truncated_faces"], 0)
            self.assertLessEqual(rep["chunks"]["faces_by_lod_max"][0], 176)
            self.assertLessEqual(rep["chunks"]["faces_by_lod_max"][1], 64)
            self.assertLessEqual(rep["chunks"]["faces_by_lod_max"][2], 24)
            self.assertLessEqual(rep["chunks"]["vertices_by_lod_max"][2], 48)
            self.assertGreaterEqual(rep["source_triangles"], 1000)
            hist = rep["chunks"]["block_level_histogram_by_lod"]
            self.assertEqual(sum(hist[0]), 1, "one chunk with blocks")

    def test_archive_over_the_limit_fails_loudly_with_numbers(self):
        with tempfile.TemporaryDirectory() as tmp:
            run = Run(tmp, synthetic_city(), "--max-archive-bytes", 1000)
            self.assertNotEqual(run.code, 0)
            self.assertIn("1000", run.stderr)
            self.assertFalse(run.bin.exists(), "no archive is written when a gate fails")

    def test_grid_flags_that_would_desync_from_the_runtime_are_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            run = Run(tmp, synthetic_city(), "--chunk-units", 64)
            self.assertNotEqual(run.code, 0)
            self.assertIn("city_grid.h", run.stderr)


class FacadeTextureSamplingTests(unittest.TestCase):
    def test_baker_samples_source_uv_per_pixel_not_one_centroid_colour(self):
        from model_pipeline import facades
        from model_pipeline.chunking import MaterialInfo

        pos = np.array([[[0., 0., 0.], [4., 0., 0.], [0., 4., 0.]]])
        uv = np.array([[[0., 0.], [1., 0.], [0., 1.]]])
        image = np.array([
            [[255, 0, 0, 255], [0, 255, 0, 255]],
            [[0, 0, 255, 255], [255, 255, 0, 255]],
        ], dtype=np.uint8)
        mat = MaterialInfo("wall", (1.0, 1.0, 1.0), image=image)
        baker = facades.FacadeBaker(
            pos, np.array([[128, 128, 128]], dtype=np.uint8),
            uv, np.array([0]), [mat], np.array([[0., 0., 1.]]),
            (0., 0., 1.), 1.0, 0.0)
        corners = np.array([[0., 0., 0.], [4., 0., 0.],
                            [4., 4., 0.], [0., 4., 0.]])
        out = baker.bake(corners, 32, 32, np.array([0, 0, 0]))
        colours = np.unique(out.reshape(-1, 3), axis=0)
        self.assertGreater(len(colours), 4)
        self.assertGreater(int(out[..., 0].max()) - int(out[..., 0].min()), 100)
        self.assertGreater(int(out[..., 1].max()) - int(out[..., 1].min()), 100)


class FacadeOrientationTests(unittest.TestCase):
    """The VDP1 draws texel (0, 0) of a distorted sprite at the face's corner
    A, (w, 0) at B and (0, h) at D. A red plaque on one corner of a grey wall
    must come back, through exactly that mapping, where it is in the world;
    a transposed or mirrored bake would put it on another corner."""

    @classmethod
    def setUpClass(cls):
        # Grey box x 4..12, z 4..14, 9 tall; a red plaque proud of its +Z face
        # (z = 14..14.3) near x = 5, high up (y 7..8).
        glb = make_glb([
            (plane_up(X0, Z0, X0 + 32, Z0 + 32, 0.0), 0),
            (cuboid(X0 + 4, 0.0, Z0 + 4, X0 + 12, 9.0, Z0 + 14), 0),
            (cuboid(X0 + 4.5, 7.0, Z0 + 14, X0 + 5.5, 8.0, Z0 + 14.3), 1),
        ])
        cls.tmp = tempfile.TemporaryDirectory()
        cls.res = Run(cls.tmp.name, glb)
        assert cls.res.code == 0, cls.res.stderr
        cls.archive = cls.res.archive()

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_texels_land_where_the_geometry_is(self):
        blob = self.archive["blobs"][(CZ0 * 16 + CX0, 0)]
        pal = self.archive["texture_palette"]
        v = blob["vertices"].astype(np.float64) / 64.0
        found = []
        for a, b, c, d, _m, tex in blob["faces"]:
            if not tex:
                continue
            A, B, D = v[a], v[b], v[d]
            if not (np.allclose([A[2], B[2], D[2]], 14.0, atol=0.02)):
                continue  # only the box's +Z wall pieces
            image = blob["textures"][tex - 1]
            h, w = image.shape
            rgb = np.array([[((pal[i] & 31) << 3, ((pal[i] >> 5) & 31) << 3, ((pal[i] >> 10) & 31) << 3)
                             for i in row] for row in image], dtype=np.int64)
            red = (rgb[..., 0] > 150) & (rgb[..., 1] < 90) & (rgb[..., 2] < 90)
            for row, col in zip(*np.nonzero(red)):
                u, t = (col + 0.5) / w, (row + 0.5) / h
                found.append(A + (B - A) * u + (D - A) * t)
        self.assertTrue(found, "the red plaque is in no texture of the +Z wall")
        p = np.mean(found, axis=0)
        self.assertAlmostEqual(p[0], 5.0, delta=0.6)  # x: left end of the wall
        self.assertAlmostEqual(p[1] + self.archive["header"]["world_min_y"] / 65536.0, 7.5, delta=0.6)


class PackerTests(unittest.TestCase):
    @staticmethod
    def blob(chunk, lod, n_faces):
        verts = [(0, 0, 0), (64, 0, 0), (64, 0, 64), (0, 0, 64)]
        return emit_bin.BlobSpec(chunk, lod, verts, [(0, 1, 2, 3, 0)] * n_faces)

    @staticmethod
    def sized(chunk, faces=6_500):
        return emit_bin.BlobSpec(chunk, 0, [(0, 0, 0)] * 4, [(0, 1, 2, 3, 0)] * faces)

    def test_blob_never_straddles_a_two_mebibyte_boundary(self):
        # ~65 KB blobs: 32 fit below 2 MiB and the 33rd would straddle, so it
        # must start exactly at the next bank.
        blobs = {(c, 0): self.sized(c) for c in range(34)}
        data, info = emit_bin.pack_archive(blobs, [0x7FFF], None, -9, 44)
        toc = city_preview.read_archive(data)["toc"]
        bank1 = [c for c in range(34) if toc[(c, 0)]["bank"] == 1]
        self.assertTrue(bank1, "some blob must land in the second bank")
        self.assertEqual(toc[(bank1[0], 0)]["offset"], emit_bin.BANK_BYTES)
        for c in range(34):
            e = toc[(c, 0)]
            self.assertEqual(e["offset"] // emit_bin.BANK_BYTES, e["bank"])
            self.assertEqual((e["offset"] + e["bytes"] - 1) // emit_bin.BANK_BYTES, e["bank"],
                             "no blob crosses a bank boundary")

    def test_a_third_bank_is_refused(self):
        blobs = {(c, 0): self.sized(c) for c in range(70)}
        with self.assertRaisesRegex(GltfError, "third 2 MiB bank"):
            emit_bin.pack_archive(blobs, [0x7FFF], None, -9, 44)

    def test_blob_larger_than_the_u16_toc_size_is_refused(self):
        with self.assertRaisesRegex(GltfError, "u16"):
            emit_bin.pack_blob(self.sized(0, faces=7_000))

    def test_materials_over_the_limit_are_refused(self):
        with self.assertRaisesRegex(GltfError, "materials"):
            emit_bin.pack_archive({}, [0] * 241, None, -9, 44)

    def test_int16_overflow_is_refused(self):
        spec = emit_bin.BlobSpec(0, 0, [(40000, 0, 0)] * 4, [(0, 1, 2, 3, 0)])
        with self.assertRaisesRegex(GltfError, "int16"):
            emit_bin.pack_blob(spec)

    def test_ground_bitmap_size_must_match_its_dimensions(self):
        bad = emit_bin.GroundSection(b"\x00" * 10, 4, 4, [0, 1], -0.125)
        with self.assertRaisesRegex(GltfError, "ground bitmap"):
            emit_bin.pack_archive({}, [0x7FFF], bad, -9, 44)


class GeometryHelperTests(unittest.TestCase):
    def test_square_of_two_triangles_becomes_one_outward_quad(self):
        verts = np.array([[0, 0, 0], [0, 0, 1], [1, 0, 1], [1, 0, 0]], dtype=float)
        # CCW seen from +Y: (0,1,3) and (1,2,3).
        faces = ch.merge_to_quads(verts, [(0, 1, 3), (1, 2, 3)], [5, 5])
        self.assertEqual(len(faces), 1)
        corners, material = faces[0]
        self.assertEqual((len(corners), material), (4, 5))
        a, b, c, d = ch.to_runtime_face(corners)
        normal = np.cross(verts[d] - verts[a], verts[b] - verts[a])
        self.assertGreater(normal[1], 0.0, "runtime winding faces the same way as the CCW input")

    def test_different_materials_and_sharp_folds_do_not_merge(self):
        verts = np.array([[0, 0, 0], [0, 0, 1], [1, 0, 1], [1, 0, 0]], dtype=float)
        self.assertEqual(len(ch.merge_to_quads(verts, [(0, 1, 3), (1, 2, 3)], [1, 2])), 2)
        folded = np.array([[0, 0, 0], [0, 0, 1], [1, 1, 1], [1, 0, 0]], dtype=float)
        folded[2] = [0.5, 3.0, 1.0]  # a steep roof ridge
        self.assertEqual(len(ch.merge_to_quads(folded, [(0, 1, 3), (1, 2, 3)], [1, 1],
                                               max_fold_deg=35.0)), 2)

    def test_triangle_runtime_face_repeats_the_last_corner(self):
        a, b, c, d = ch.to_runtime_face((7, 8, 9))
        self.assertEqual((a, b, c, d), (7, 9, 8, 8))

    def test_clip_splits_at_borders_and_conserves_area(self):
        tri = np.array([[[X0 - 20, 0, Z0 + 5], [X0 + 50, 0, Z0 + 5], [X0 + 10, 0, Z0 + 80]]])
        pos, attrs, rep = ch.clip_to_grid(tri, {"m": np.array([3])})
        area = lambda p: 0.5 * np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1).sum()
        self.assertAlmostEqual(area(pos), area(tri), places=6)
        self.assertGreater(len(pos), 1)
        self.assertTrue((attrs["m"] == 3).all())
        self.assertEqual(len(np.unique(ch.chunk_of(pos))) > 1, True)
        lo = np.floor((pos.min(axis=1)[:, 0] - ch.ORIGIN_X) / 32 + 1e-9)
        hi = np.floor((pos.max(axis=1)[:, 0] - ch.ORIGIN_X) / 32 - 1e-9)
        self.assertTrue((lo == hi).all(), "every piece lies inside one chunk column")

    def test_median_cut_keeps_a_rare_vivid_colour(self):
        colours = np.array([[128, 128, 128]] * 1000 + [[250, 20, 20]] * 3, dtype=np.uint8)
        weights = np.array([1.0] * 1000 + [1.0] * 3)
        palette = ch.median_cut(colours, weights, 2)
        self.assertEqual(len(palette), 2)
        self.assertTrue(any(p[0] > 200 and p[1] < 60 for p in palette.tolist()))

    def test_shading_is_lit_in_linear_light_and_monotonic(self):
        base = np.array([200, 100, 50])
        values = [ch.shade_rgb555(base, k, 5, 0.42, 0.60) for k in range(5)]
        reds = [v & 31 for v in values]
        self.assertEqual(reds, sorted(reds))
        self.assertLess(reds[0], reds[-1])
        # Fully lit at ambient + diffuse = 1.02 must not overflow the 5-bit channel.
        self.assertLessEqual(max(reds), 31)
        # Not a naive gamma-space scale: mid-tones stay brighter than the linear ratio.
        naive = round(200 / 255 * 31 * (0.42 + 0.60 * 0.5))
        self.assertGreater(reds[2], naive)

    def test_collision_cover_grows_boxes_rather_than_dropping_them(self):
        # 40 separate pillars cannot fit 24 boxes at 1-unit cells; the cover must
        # coarsen, and every pillar must still be inside some box.
        pillars = []
        for i in range(8):
            for j in range(5):
                pillars += cuboid(X0 + 1 + i * 3.5, 0, Z0 + 1 + j * 6, X0 + 2.5 + i * 3.5, 6, Z0 + 2.5 + j * 6)
        pos = np.array(pillars)
        boxes = ch.collision_boxes(pos, X0, Z0, 0.0, 24)
        self.assertLessEqual(len(boxes), 24)
        for tri in pos:
            cx, cz = tri[:, 0].mean(), tri[:, 2].mean()
            self.assertTrue(any(abs(cx - bx) <= hx + 1e-6 and abs(cz - bz) <= hz + 1e-6
                                for bx, _by, bz, hx, _hy, hz in boxes))

    def test_low_curbs_do_not_block_a_walker(self):
        curb = np.array(cuboid(X0 + 4, 0.0, Z0 + 4, X0 + 30, 0.4, Z0 + 6))
        self.assertEqual(ch.collision_boxes(curb, X0, Z0, 0.0, 24), [])

    def test_ground_height_is_the_area_weighted_mode_not_the_mean(self):
        street = plane_up(X0, Z0, X0 + 30, Z0 + 30, -0.1)
        roofs = cuboid(X0 + 1, -0.1, Z0 + 1, X0 + 5, 30.0, Z0 + 5)
        glb = make_glb([(street, 0), (roofs, 1)])
        import tempfile as tf
        from model_pipeline import gltf
        with tf.TemporaryDirectory() as tmp:
            path = Path(tmp) / "g.glb"
            path.write_bytes(glb)
            tris = ch.load_world_triangles(gltf.parse_glb(path))
        normals, areas = ch.triangle_normals(tris.pos)
        self.assertAlmostEqual(ch.estimate_ground_y(tris, normals, areas), -0.125, places=6)


def find_host_gxx():
    import shutil
    for candidate in (shutil.which("g++"), "C:/msys64/ucrt64/bin/g++.exe"):
        if candidate and Path(candidate).exists():
            return candidate
    return None


@unittest.skipUnless(find_host_gxx(), "no host g++ to compile the C reader")
class CrossLanguageTests(unittest.TestCase):
    """The Saturn-side reader (city_format.h) parses what the Python packer wrote."""

    def test_c_reader_accepts_the_packed_archive(self):
        import os
        import subprocess
        gxx = find_host_gxx()
        env = {**os.environ, "PATH": str(Path(gxx).parent) + os.pathsep + os.environ.get("PATH", "")}
        with tempfile.TemporaryDirectory() as tmp:
            run = Run(tmp, synthetic_city())
            self.assertEqual(run.code, 0, run.stderr)
            exe = Path(tmp) / "test_city_walk.exe"
            build = subprocess.run(
                [gxx, "-std=c++20", "-O1", "-I" + str(REPO / "include"), "-I" + str(REPO),
                 str(REPO / "tests" / "host" / "test_city_walk.cpp"), "-o", str(exe)],
                capture_output=True, text=True, env=env)
            self.assertEqual(build.returncode, 0, build.stderr)
            result = subprocess.run([str(exe), str(run.bin)], capture_output=True, text=True, env=env)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS: archive", result.stdout)


if __name__ == "__main__":
    unittest.main()
