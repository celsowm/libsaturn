#!/usr/bin/env python3
"""Tests for triangle-pair -> VDP1 quad merging (tools/model_pipeline/quad_merge.py)."""

import math
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))
sys.path.insert(0, str(REPO / "tests"))

import import_model
from model_pipeline import quad_merge
from test_import_animated import make_quadrant_glb
from test_simplify_cli import build_grid_glb

# Unit square a(0,0) b(1,1) c(0,1) d(1,0) split along a-b, CCW from +z:
# (a, b, c) and (b, a, d) share the diagonal in opposite directions.
SQUARE = [(0.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0), (1.0, 0.0, 0.0)]
TRIS = [(0, 1, 2), (1, 0, 3)]
UVS = [(0.0, 0.0), (1.0, 1.0), (0.0, 1.0), (1.0, 0.0)]
SCALES = {0: (16.0, 16.0)}


def merge(positions=SQUARE, uvs=UVS, frames=None, **opts):
    return quad_merge.merge_quads(
        TRIS, [0, 0], uvs, positions, frames or [positions], SCALES,
        quad_merge.QuadMergeOptions(**opts))


class QuadMergeTests(unittest.TestCase):
    def test_planar_pair_becomes_one_quad(self):
        polys, mats, report = merge()
        self.assertEqual(polys, [(0, 3, 1, 2)])
        self.assertEqual(mats, [0])
        self.assertEqual(report["merged_quads"], 1)

    def test_different_materials_stay_triangles(self):
        polys, _, report = quad_merge.merge_quads(
            TRIS, [0, 1], UVS, SQUARE, [SQUARE], SCALES)
        self.assertEqual(polys, TRIS)
        self.assertEqual(report["rejected"]["material"], 1)

    def test_texture_shear_is_rejected(self):
        # d's UV pulled towards the middle: the bilinear quad would smear
        # the texture several texels away from where the triangles put it.
        skewed = list(UVS)
        skewed[3] = (0.6, 0.4)
        polys, _, report = merge(uvs=skewed)
        self.assertEqual(len(polys), 2)
        self.assertEqual(report["rejected"]["texel_error"], 1)

    def test_fold_in_any_frame_is_rejected(self):
        # Rest pose flat, but one animation frame folds d up by 60 degrees
        # about the a-b hinge.
        h = math.sqrt(0.5)
        mid = (0.5, 0.5, 0.0)
        away = (0.5 * math.cos(math.radians(60)), -0.5 * math.cos(math.radians(60)))
        folded = list(SQUARE)
        folded[3] = (mid[0] + away[0], mid[1] + away[1], h * math.sin(math.radians(60)))
        polys, _, report = merge(frames=[SQUARE, folded], max_fold_deg=30.0)
        self.assertEqual(len(polys), 2)
        self.assertEqual(report["rejected"]["fold"], 1)

    def test_concave_pair_is_rejected(self):
        dart = list(SQUARE)
        dart[3] = (-0.3, -0.5, 0.0)  # past the c-a line: reflex corner at a
        uvs = [(p[0], p[1]) for p in dart]
        polys, _, report = merge(positions=dart, uvs=uvs)
        self.assertEqual(len(polys), 2)
        self.assertEqual(report["rejected"]["convex"], 1)


class MergeImportTests(unittest.TestCase):
    def test_grid_merges_and_winds_outward(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            plain = import_model.import_animated_model(src, simplify="off")
            merged = import_model.import_animated_model(src, simplify="off", merge_quads=True)
            self.assertEqual(len(merged.static.indices_abcd) * 2,
                             len(plain.static.indices_abcd))
            self.assertTrue(merged.report["result"]["pass"])
            verts = [(x / 65536.0, y / 65536.0, z / 65536.0)
                     for (x, y, z) in merged.static.vertices_fx]
            for a, b, c, d in merged.static.indices_abcd:
                self.assertEqual(len({a, b, c, d}), 4)
                ax, ay, _ = verts[a]
                bx, by, _ = verts[b]
                dx, dy, _ = verts[d]
                self.assertGreater((dx - ax) * (by - ay) - (dy - ay) * (bx - ax), 0.0)

    def test_quadrants_merge_to_solid_quads(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            result = import_model.import_animated_model(src, simplify="off", merge_quads=True)
            self.assertEqual(len(result.static.indices_abcd), 4)
            for tex_idx in result.static.face_texture_indices:
                pixels = result.static.textures[tex_idx]["pixels"]
                self.assertTrue(all(p == pixels[0] for p in pixels))

    def test_weld_drops_uv_split_copies_only(self):
        # Each quadrant owns its 4 corners, so the 16 vertices share only 9
        # grid points; welding must keep every face's outline intact.
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp), spacing=1.0)
            plain = import_model.import_animated_model(src, simplify="off", merge_quads=True)
            welded = import_model.import_animated_model(
                src, simplify="off", merge_quads=True, weld_vertices=True)
            self.assertEqual(len(plain.static.vertices_fx), 16)
            self.assertEqual(len(welded.static.vertices_fx), 9)
            self.assertEqual(welded.animations[0]["vertex_count"],
                             len(welded.static.vertices_fx))
            self.assertEqual(welded.static.face_texture_indices,
                             plain.static.face_texture_indices)
            for pq, wq in zip(plain.static.indices_abcd, welded.static.indices_abcd):
                self.assertEqual([plain.static.vertices_fx[i] for i in pq],
                                 [welded.static.vertices_fx[i] for i in wq])

    def test_locality_order_only_reorders(self):
        def faces(result):
            s = result.static
            anim = result.animations[0]
            nv = anim["vertex_count"]
            out = []
            for q, t in zip(s.indices_abcd, s.face_texture_indices):
                frames = tuple(
                    tuple(tuple(anim["stream"][f * nv * 3 + i * 3: f * nv * 3 + i * 3 + 3])
                          for i in q)
                    for f in range(anim["frame_count"]))
                out.append((tuple(s.vertices_fx[i] for i in q), frames,
                            s.textures[t]["pixels"]))
            return sorted(out)

        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            plain = import_model.import_animated_model(
                src, simplify="off", merge_quads=True, weld_vertices=True)
            ordered = import_model.import_animated_model(
                src, simplify="off", merge_quads=True, weld_vertices=True,
                locality_order=True)
            self.assertEqual(faces(plain), faces(ordered))
            # Vertices are numbered by first use in face order.
            seen = []
            for q in ordered.static.indices_abcd:
                for i in q:
                    if i not in seen:
                        seen.append(i)
            self.assertEqual(seen, sorted(seen))

    def test_material_weight_names_are_checked(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(
                    src, simplify="auto", material_weights=["no_such_material=0.1"])


if __name__ == "__main__":
    unittest.main()
