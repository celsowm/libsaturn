import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
TOOL = REPO / "tools" / "import_model.py"
DATA = REPO / "tests" / "data" / "model3d"


def run_import(args, cwd=REPO):
    return subprocess.run(
        ["python", str(TOOL), *args],
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
    )


def import_api(obj_name, **kwargs):
    import sys

    sys.path.insert(0, str(REPO / "tools"))
    import import_model

    return import_model.import_model(DATA / obj_name, **kwargs)


class ImporterObjTests(unittest.TestCase):
    def test_quad_parses_one_quad(self):
        import sys

        sys.path.insert(0, str(REPO / "tools"))
        from import_model import parse_obj

        model = parse_obj(DATA / "quad.obj")
        self.assertEqual(len(model.vertices), 4)
        self.assertEqual(len(model.uvs), 4)
        self.assertEqual(len(model.faces), 1)
        self.assertEqual(len(model.faces[0]["verts"]), 4)

    def test_tri_parses_one_triangle(self):
        import sys

        sys.path.insert(0, str(REPO / "tools"))
        from import_model import parse_obj

        model = parse_obj(DATA / "tri.obj")
        self.assertEqual(len(model.faces), 1)
        self.assertEqual(len(model.faces[0]["verts"]), 3)

    def test_negative_indices_resolve(self):
        import sys

        sys.path.insert(0, str(REPO / "tools"))
        from import_model import parse_obj

        model = parse_obj(DATA / "mixed.obj")
        self.assertEqual(len(model.faces), 7)
        # Last face uses negative indices; all must resolve in range.
        for vi, vti in model.faces[-1]["verts"]:
            self.assertGreaterEqual(vi, 0)
            self.assertLess(vi, len(model.vertices))
            self.assertIsNotNone(vti)

    def test_fan_triangulates_ngons(self):
        import sys

        sys.path.insert(0, str(REPO / "tools"))
        from import_model import parse_obj

        with tempfile.TemporaryDirectory() as tmp:
            obj = Path(tmp) / "ngon.obj"
            obj.write_text(
                "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv -1 0.5 0\n"
                "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt 0 0.5\n"
                "usemtl M\nf 1/1 2/2 3/3 4/4 5/5\n",
                encoding="utf-8",
            )
            model = parse_obj(obj)
            # Pentagon fans into 3 triangles.
            self.assertEqual(len(model.faces), 3)
            for f in model.faces:
                self.assertEqual(len(f["verts"]), 3)

    def test_malformed_obj_diagnostic(self):
        with tempfile.TemporaryDirectory() as tmp:
            obj = Path(tmp) / "bad.obj"
            obj.write_text("v 0 0\nf 1 2\n", encoding="utf-8")
            result = run_import(
                ["--input", str(obj), "--out-prefix", str(Path(tmp) / "out")]
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("malformed", (result.stderr + result.stdout).lower())

    def test_missing_mtl_diagnostic(self):
        with tempfile.TemporaryDirectory() as tmp:
            obj = Path(tmp) / "m.obj"
            obj.write_text(
                "mtllib nope.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\n"
                "vt 0 0\nvt 1 0\nvt 0 1\nusemtl M\nf 1/1 2/2 3/3\n",
                encoding="utf-8",
            )
            result = run_import(
                ["--input", str(obj), "--out-prefix", str(Path(tmp) / "out")]
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("MTL", result.stderr + result.stdout)

    def test_missing_texture_diagnostic(self):
        import sys

        sys.path.insert(0, str(REPO / "tools"))
        from import_model import ImportError as IE, resolve_material_texture

        with self.assertRaises(IE) as ctx:
            resolve_material_texture("Nope", {}, Path("."), Path("x.obj"))
        self.assertIn("Nope", str(ctx.exception))

    def test_material_switching_counts_materials(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            import shutil

            shutil.copy(DATA / "atlas.png", tmp_path / "atlas.png")
            (tmp_path / "two.mtl").write_text(
                "newmtl A\nmap_Kd atlas.png\nnewmtl B\nmap_Kd atlas.png\n",
                encoding="utf-8",
            )
            (tmp_path / "two.obj").write_text(
                "mtllib two.mtl\n"
                "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                "usemtl A\nf 1/1 2/2 3/3\nusemtl B\nf 1/1 3/3 4/4\n",
                encoding="utf-8",
            )
            result = import_api_two(tmp_path / "two.obj")
            self.assertEqual(result.stats["materials"], 2)
            self.assertEqual(result.stats["source_faces"], 2)


def import_api_two(path):
    import sys

    sys.path.insert(0, str(REPO / "tools"))
    import import_model

    return import_model.import_model(path)


class ImporterWindingTests(unittest.TestCase):
    def test_ccw_triangle_produces_outward_normal(self):
        # tri.obj is CCW from +Z; default import must wind outward so that
        # cross(D-A, B-A) points +Z under LibSaturn's convention.
        r = import_api("tri.obj")
        self.assertEqual(len(r.indices_abcd), 1)
        a, b, c, d = r.indices_abcd[0]
        # Triangle duplicated as degenerate quad.
        self.assertEqual(c, d)
        verts = [(x / 65536.0, y / 65536.0, z / 65536.0) for (x, y, z) in r.vertices_fx]
        ax, ay, az = verts[a]
        bx, by, bz = verts[b]
        dx, dy, dz = verts[d]
        # normal = cross(D-A, B-A)
        dax, day, daz = dx - ax, dy - ay, dz - az
        bax, bay, baz = bx - ax, by - ay, bz - az
        nx = day * baz - daz * bay
        ny = daz * bax - dax * baz
        nz = dax * bay - day * bax
        self.assertGreater(nz, 0.0)
        self.assertAlmostEqual(nx, 0.0)
        self.assertAlmostEqual(ny, 0.0)

    def test_reverse_winding_flips_normal(self):
        r0 = import_api("tri.obj")
        r1 = import_api("tri.obj", reverse_winding=True)
        self.assertNotEqual(r0.indices_abcd, r1.indices_abcd)

    def test_ccw_quad_produces_outward_normal(self):
        r = import_api("quad.obj")
        a, b, c, d = r.indices_abcd[0]
        verts = [(x / 65536.0, y / 65536.0, z / 65536.0) for (x, y, z) in r.vertices_fx]
        ax, ay, az = verts[a]
        bx, by, bz = verts[b]
        dx, dy, dz = verts[d]
        nz = (dx - ax) * (by - ay) - (dy - ay) * (bx - ax)
        self.assertGreater(nz, 0.0)

    def test_axis_flips_mirror_geometry(self):
        r0 = import_api("quad.obj")
        r1 = import_api("quad.obj", flip_x=True)
        xs0 = [v[0] for v in r0.vertices_fx]
        xs1 = [v[0] for v in r1.vertices_fx]
        self.assertEqual(xs0, [-x for x in xs1])

    def test_uv_v_axis_samples_correct_rows(self):
        import sys

        sys.path.insert(0, str(REPO / "tools"))
        from import_model import bake_face_rgba

        # 2x2 image: top row red, bottom row blue.
        img = [(255, 0, 0, 255), (255, 0, 0, 255),
               (0, 0, 255, 255), (0, 0, 255, 255)]
        # Full-quad UVs; output 2x2. v=1 is image top (red).
        uvs = ((0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0))
        out = bake_face_rgba(uvs, 2, 2, img, 2, 2, "nearest")
        # Top output row (t~0.25, near v=1 side after mapping?) must be red-ish,
        # bottom row blue-ish. Exact rows depend on parameterization; assert
        # both colors appear and top != bottom.
        tops = out[0:2]
        bottoms = out[2:4]
        self.assertTrue(all(p[:3] == (255, 0, 0) for p in tops))
        self.assertTrue(all(p[:3] == (0, 0, 255) for p in bottoms))


class ImporterBakeTests(unittest.TestCase):
    def test_rotated_mapping_differs_from_baseline(self):
        r = import_api("mixed.obj")
        # Faces 0 (baseline) and 2 (rotated) must not dedup together.
        self.assertEqual(r.face_texture_indices[0], r.face_texture_indices[1])
        self.assertNotEqual(r.face_texture_indices[0], r.face_texture_indices[2])

    def test_flipped_mapping_differs(self):
        r = import_api("mixed.obj")
        self.assertNotEqual(r.face_texture_indices[0], r.face_texture_indices[3])

    def test_non_axis_aligned_mapping_bakes(self):
        r = import_api("mixed.obj")
        self.assertNotEqual(r.face_texture_indices[0], r.face_texture_indices[4])
        w, h = r.textures[r.face_texture_indices[4]]["width"], r.textures[r.face_texture_indices[4]]["height"]
        self.assertGreaterEqual(w, 8)
        self.assertGreaterEqual(h, 1)

    def test_deduplication_shares_identical_faces(self):
        r = import_api("mixed.obj")
        # Baseline, duplicate, and negative-index faces share one texture.
        self.assertEqual(r.face_texture_indices[0], r.face_texture_indices[1])
        self.assertEqual(r.face_texture_indices[0], r.face_texture_indices[6])
        self.assertLess(r.stats["unique_textures_after_dedup"], r.stats["baked_faces_before_dedup"])

    def test_width_alignment_by_resampling(self):
        r = import_api("mixed.obj")
        for t in r.textures:
            self.assertEqual(t["width"] % 8, 0)
            self.assertGreaterEqual(t["width"], 8)
            self.assertLessEqual(t["width"], 504)
            self.assertLessEqual(t["height"], 255)
        # Resampling, not padding: stored pixel count equals w*h.
        for t in r.textures:
            self.assertEqual(len(t["pixels"]), t["width"] * t["height"])

    def test_max_dimension_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = run_import(
                ["--input", str(DATA / "quad.obj"),
                 "--out-prefix", str(Path(tmp) / "out"),
                 "--max-texture-width", "8",
                 "--max-texture-height", "1"]
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("limit", (result.stderr + result.stdout).lower())

    def test_shared_palette_single(self):
        r = import_api("mixed.obj")
        self.assertEqual(r.stats["palette_count"], 1)
        self.assertEqual(len(r.palette_rgb555), 256)

    def test_transparency_reserves_index_zero(self):
        r = import_api("mixed.obj")
        self.assertTrue(r.stats["has_transparency"])
        self.assertEqual(r.palette_rgb555[0], 0x0000)
        # No opaque texel may use index 0 (reserved for transparent).
        from collections import Counter

        for face_idx, tex_idx in enumerate(r.face_texture_indices):
            # Face 5 samples the transparent corner; other faces are opaque.
            pixels = r.textures[tex_idx]["pixels"]
            if face_idx == 5:
                self.assertIn(0, pixels)
            else:
                # Opaque faces must not contain index 0.
                self.assertNotIn(0, pixels)

    def test_fully_opaque_sets_opaque_flag(self):
        r = import_api("quad.obj")
        self.assertFalse(r.stats["has_transparency"])
        for t in r.textures:
            self.assertEqual(t["flags"] & 0x0001, 0x0001)

    def test_fixed_point_geometry_emission(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "m"
            result = run_import(
                ["--input", str(DATA / "quad.obj"), "--out-prefix", str(out),
                 "--symbol", "quad"]
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            c_text = out.with_suffix(".c").read_text(encoding="utf-8")
            # Fixed-point constants, no float literals in vertex table.
            self.assertIn("const sat_vec3_t quad_vertices[4]", c_text)
            self.assertNotIn("1.0", c_text.split("quad_vertices")[1].split("};")[0])

    def test_deterministic_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "m"
            for _ in range(2):
                result = run_import(
                    ["--input", str(DATA / "mixed.obj"), "--out-prefix", str(out),
                     "--symbol", "m"]
                )
                self.assertEqual(result.returncode, 0, msg=result.stderr)
            first_c = out.with_suffix(".c").read_bytes()
            result = run_import(
                ["--input", str(DATA / "mixed.obj"), "--out-prefix", str(out),
                 "--symbol", "m"]
            )
            self.assertEqual(result.returncode, 0)
            second_c = out.with_suffix(".c").read_bytes()
            self.assertEqual(first_c, second_c)


if __name__ == "__main__":
    unittest.main()
