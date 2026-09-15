#!/usr/bin/env python3
"""Tests for the animated GLB -> Saturn import path (tools/import_model.py).

Fixtures are synthetic GLBs generated in-test (plus the grid builder shared
with the CLI tests); nothing depends on copyrighted assets.
"""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))
sys.path.insert(0, str(REPO / "tests"))

import import_model
from test_simplify_cli import build_grid_glb


def make_quadrant_glb(tmp: Path) -> Path:
    """Four separate quads over an 8x8 4-quadrant texture (V-convention lock).

    Each quad owns its vertices with UVs strictly inside one quadrant, so
    every baked face is unanimously its quadrant color: quad k (tris 2k and
    2k+1) must bake color k. glTF v=0 is the image top.
    """
    import io
    import struct

    from PIL import Image

    img = Image.new("RGBA", (8, 8))
    quad_colors = [(255, 0, 0, 255), (0, 255, 0, 255),
                   (0, 0, 255, 255), (255, 255, 0, 255)]
    quads_px = [((0, 0), (4, 4)), ((4, 0), (8, 4)), ((0, 4), (4, 8)), ((4, 4), (8, 8))]
    for ((x0, y0), (x1, y1)), color in zip(quads_px, quad_colors):
        for y in range(y0, y1):
            for x in range(x0, x1):
                img.putpixel((x, y), color)
    png = io.BytesIO()
    img.save(png, format="PNG")

    # Quad k at grid cell k with UVs inset strictly inside quadrant k.
    quad_uvs = [
        ((0.1, 0.1), (0.4, 0.1), (0.4, 0.4), (0.1, 0.4)),
        ((0.6, 0.1), (0.9, 0.1), (0.9, 0.4), (0.6, 0.4)),
        ((0.1, 0.6), (0.4, 0.6), (0.4, 0.9), (0.1, 0.9)),
        ((0.6, 0.6), (0.9, 0.6), (0.9, 0.9), (0.6, 0.9)),
    ]
    pos, nrm, uv, jnt, wgt, idx = [], [], [], [], [], []
    for k in range(4):
        ox, oy = (k % 2) * 2.0, (k // 2) * 2.0
        base = len(pos)
        for (px, py), (u, v) in zip(
            ((0, 0), (1, 0), (1, 1), (0, 1)), quad_uvs[k]
        ):
            pos.append((ox + px, oy + py, 0.0))
            nrm.append((0.0, 0.0, 1.0))
            uv.append((u, v))
            jnt.append((0, 0, 0, 0))
            wgt.append((1, 0, 0, 0))
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]

    blob = bytearray()
    views, accessors = [], []

    def push(payload, target=None):
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(payload),
                      **({"target": target} if target is not None else {})})
        blob.extend(payload)
        return len(views) - 1

    def acc(view, comp, count, atype):
        accessors.append({"bufferView": view, "componentType": comp, "count": count, "type": atype})
        return len(accessors) - 1

    def floats(rows):
        flat = [float(x) for r in rows for x in r]
        return struct.pack("<%df" % len(flat), *flat)

    pa = acc(push(floats(pos), 34962), 5126, len(pos), "VEC3")
    na = acc(push(floats(nrm), 34962), 5126, len(nrm), "VEC3")
    ua = acc(push(floats(uv), 34962), 5126, len(uv), "VEC2")
    flat_j = [v for j in jnt for v in j]
    ja = acc(push(struct.pack("<%dH" % len(flat_j), *flat_j), 34962), 5123, len(jnt), "VEC4")
    wa = acc(push(floats(wgt)), 5126, len(wgt), "VEC4")
    ia = acc(push(struct.pack("<%dH" % len(idx), *idx), 34963), 5123, len(idx), "SCALAR")
    ibm = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    ba = acc(push(struct.pack("<16f", *[float(v) for v in ibm])), 5126, 1, "MAT4")
    ta = acc(push(struct.pack("<2f", 0.0, 1.0)), 5126, 2, "SCALAR")
    oa = acc(push(floats([(0, 0, 0), (0, 0, 0)]), 3), 5126, 2, "VEC3")
    img_view = push(png.getvalue())

    doc = {
        "asset": {"version": "2.0"},
        "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(blob)}],
        "images": [{"bufferView": img_view, "mimeType": "image/png"}],
        "textures": [{"source": 0}],
        "materials": [{"name": "q", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
        "meshes": [{"primitives": [{
            "attributes": {"POSITION": pa, "NORMAL": na, "TEXCOORD_0": ua, "JOINTS_0": ja, "WEIGHTS_0": wa},
            "indices": ia, "material": 0}]}],
        "nodes": [{"name": "meshnode", "mesh": 0, "skin": 0}, {"name": "j0"}],
        "skins": [{"joints": [1], "inverseBindMatrices": ba}],
        "animations": [{
            "name": "still",
            "samplers": [{"input": ta, "output": oa, "interpolation": "LINEAR"}],
            "channels": [{"sampler": 0, "target": {"node": 1, "path": "translation"}}],
        }],
        "scenes": [{"nodes": [0]}], "scene": 0,
    }
    raw = json.dumps(doc).encode()
    while len(raw) % 4:
        raw += b" "
    total = 12 + 8 + len(raw) + 8 + len(blob)
    out = bytearray(b"glTF" + struct.pack("<II", 2, total))
    out.extend(struct.pack("<I", len(raw)) + struct.pack("<I", 0x4E4F534A) + raw)
    out.extend(struct.pack("<I", len(blob)) + struct.pack("<I", 0x004E4942) + bytes(blob))
    path = tmp / "quadrant.glb"
    path.write_bytes(bytes(out))
    return path


def run_import_cli(*argv):
    return subprocess.run(
        [sys.executable, str(REPO / "tools" / "import_model.py"), *argv],
        cwd=REPO, capture_output=True, text=True, check=False,
    )


class AnimatedImportTests(unittest.TestCase):
    def test_end_to_end_grid(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = tmp_path / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(src, simplify="auto", quality="balanced")
            # "lift" moves whole grid columns rigidly, so auto search halves
            # the 32-triangle grid with exactly zero surface error; 8
            # triangles can no longer follow the lifted edge and fail.
            self.assertEqual(len(result.static.indices_abcd), 16)
            self.assertEqual(len(result.animations), 1)
            anim = result.animations[0]
            self.assertEqual(anim["name"], "lift")
            self.assertEqual(anim["frame_count"], 2)
            self.assertEqual((anim["sample_rate_num"], anim["sample_rate_den"]), (2, 1))
            self.assertFalse(anim["loop"])
            self.assertTrue(result.report["result"]["pass"])
            out = tmp_path / "grid_model"
            header, source = import_model.emit_anim.emit_animated_c_h(
                result.static, result.animations, out, "grid")
            self.assertTrue(header.exists() and source.exists())

    def test_deterministic_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = tmp_path / "grid.glb"
            src.write_bytes(build_grid_glb())
            outs = []
            for name in ("a", "b"):
                result = import_model.import_animated_model(src, simplify="auto")
                out = tmp_path / name / "grid_model"
                h, c = import_model.emit_anim.emit_animated_c_h(
                    result.static, result.animations, out, "grid")
                outs.append((h.read_bytes(), c.read_bytes()))
            self.assertEqual(outs[0], outs[1])

    def test_generated_c_host_compiles(self):
        cc = shutil.which("g++") or shutil.which("gcc") or shutil.which("cc") or shutil.which("clang")
        if cc is None:
            self.skipTest("no host C compiler")
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = tmp_path / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(src, simplify="auto")
            out = tmp_path / "grid_model"
            _, c = import_model.emit_anim.emit_animated_c_h(
                result.static, result.animations, out, "grid")
            r = subprocess.run(
                [cc, "-fsyntax-only", f"-I{REPO / 'include'}", str(c)],
                capture_output=True, text=True, check=False)
            self.assertEqual(r.returncode, 0, msg=r.stderr[-2000:])

    def test_ccw_triangle_winds_outward(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(src, simplify="off")
            a, b, c, d = result.static.indices_abcd[0]
            verts = [(x / 65536.0, y / 65536.0, z / 65536.0)
                     for (x, y, z) in result.static.vertices_fx]
            ax, ay, az = verts[a]
            bx, by, bz = verts[b]
            dx, dy, dz = verts[d]
            nz = (dx - ax) * (by - ay) - (dy - ay) * (bx - ax)
            self.assertGreater(nz, 0.0)

    def test_glb_v_convention_samples_intended_texels(self):
        from saturn_asset_common import rgb888_to_rgb555

        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            result = import_model.import_animated_model(src, simplify="off")
            palette = set(result.static.palette_rgb555)
            expected = [rgb888_to_rgb555(*c[:3]) for c in
                        ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0))]
            # Quad k (faces 2k, 2k+1) bakes unanimously its quadrant color.
            # A V-flip would rotate every face to the wrong quadrant.
            for k, want in enumerate(expected):
                for face in (2 * k, 2 * k + 1):
                    tex_idx = result.static.face_texture_indices[face]
                    pixels = result.static.textures[tex_idx]["pixels"]
                    self.assertTrue(len(pixels) > 0)
                    self.assertTrue(all(p == pixels[0] for p in pixels),
                                    f"face {face} should sample one solid quadrant")
                    self.assertEqual(result.static.palette_rgb555[pixels[0]], want,
                                     f"face {face} has the wrong quadrant color")
            # Fully opaque asset: index 0 is an ordinary color entry (it
            # carries the RGB code bit), not the transparent reservation.
            self.assertNotEqual(result.static.palette_rgb555[0], 0x0000)

    def test_face_colors_bake_solid_lit_faces(self):
        from saturn_asset_common import rgb888_to_rgb555

        colors = ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0))
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            # Light straight along the +Z face normal, no ambient: every face
            # sits at the top level, which is exactly its source color.
            lit = import_model.import_animated_model(
                src, simplify="off", face_colors="on",
                light_dir=(0.0, 0.0, 1.0), ambient=0.0, diffuse=1.0)
            st = lit.static
            self.assertEqual(st.textures, [])
            self.assertTrue(all(i == 0xFFFF for i in st.face_texture_indices))
            palette = st.shade_palette_rgb555
            self.assertEqual(palette[0], 0x8000)  # reserved entry
            self.assertLessEqual(len(palette), 256)
            anim = lit.animations[0]
            nf = len(st.indices_abcd)
            self.assertEqual(len(anim["shades"]), anim["frame_count"] * nf)
            for k, rgb in enumerate(colors):
                for face in (2 * k, 2 * k + 1):
                    self.assertEqual(palette[anim["shades"][face]], rgb888_to_rgb555(*rgb),
                                     f"face {face} should be its lit quadrant color")
            report = lit.report["face_colors"]
            self.assertTrue(report["enabled"])
            self.assertEqual(report["distinct_colors"], 4)
            self.assertEqual(report["non_uniform_faces"], 0)

            # Light from behind every face: all faces at the unlit level,
            # which with no ambient is black.
            dark = import_model.import_animated_model(
                src, simplify="off", face_colors="on",
                light_dir=(0.0, 0.0, -1.0), ambient=0.0, diffuse=1.0)
            dark_pal = dark.static.shade_palette_rgb555
            self.assertTrue(all(dark_pal[s] == 0x8000 for s in dark.animations[0]["shades"]))

            cc = shutil.which("gcc") or shutil.which("cc") or shutil.which("clang")
            if cc is not None:
                _, c = import_model.emit_anim.emit_animated_c_h(
                    lit.static, lit.animations, Path(tmp) / "solid", "solid")
                text = c.read_text(encoding="utf-8")
                self.assertIn("solid_shade_palette", text)
                self.assertIn("solid_anim0_shades", text)
                self.assertNotIn("solid_textures", text)
                r = subprocess.run(
                    [cc, "-fsyntax-only", "-Wall", "-Wextra", "-Werror",
                     f"-I{REPO / 'include'}", str(c)],
                    capture_output=True, text=True, check=False)
                self.assertEqual(r.returncode, 0, msg=r.stderr[-2000:])

    def test_face_colors_weld_uv_split_copies(self):
        from model_pipeline import face_colors, model as srcmodel

        m = srcmodel.SourceModel()
        # Two triangles sharing an edge, split into separate copies by UV.
        m.vertices = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]
        m.uvs = [(0, 0), (0.1, 0), (0, 0.1), (0.9, 0.9), (1, 1), (0.8, 0.9)]
        m.triangles = [(0, 1, 2), (3, 4, 5)]
        m.tri_materials = [0, 0]
        m.materials = [{}]
        analysis = face_colors.FaceColorAnalysis(colors=[(255, 0, 0), (0, 0, 255)], uniform=2)
        welded, base, count = face_colors.flatten(m, analysis)
        self.assertEqual(len(welded.vertices), 4)
        self.assertEqual(count, 2)
        self.assertEqual(base, [(255, 0, 0), (0, 0, 255)])
        self.assertEqual(welded.tri_materials, [0, 1])
        self.assertEqual(welded.triangles[1], (1, 3, 2))

    def test_face_colors_auto_refuses_textured_detail(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(src, simplify="off", face_colors="sometimes")
            off = import_model.import_animated_model(src, simplify="off", face_colors="off")
            self.assertFalse(off.report["face_colors"]["enabled"])
            self.assertTrue(off.static.textures)

    def test_quantization_round_trip_bounded(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(src, simplify="off")
            anim = result.animations[0]
            stream = anim["stream"]
            bias, scale = anim["encoding"]["bias"], anim["encoding"]["scale"]
            self.assertTrue(all(-32767 <= q <= 32767 for q in stream))
            self.assertEqual(len(stream), anim["frame_count"] * anim["vertex_count"] * 3)
            # Reported max error bounds the true decode error.
            self.assertGreaterEqual(anim["max_error"], 0.0)
            self.assertLess(anim["max_error"], 0.01)

    def test_loop_duplicate_removed(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            # 'lift' ends displaced -> not a loop.
            result = import_model.import_animated_model(src, animation="lift")
            self.assertFalse(result.animations[0]["loop"])

    def test_animation_selection(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(src, animation="0")
            self.assertEqual(result.animations[0]["name"], "lift")
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(src, animation="nope")

    def test_generated_asset_has_no_source_state(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = tmp_path / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(src, simplify="off")
            out = tmp_path / "m"
            h, c = import_model.emit_anim.emit_animated_c_h(
                result.static, result.animations, out, "m")
            text = c.read_text(encoding="utf-8")
            header = h.read_text(encoding="utf-8")
            self.assertNotIn("JOINTS", text)
            self.assertNotIn("WEIGHTS", text)
            self.assertNotIn("malloc", text)
            self.assertIn("sat_animated_model_asset_t m_anim_asset", text)
            self.assertIn("M_ANIM0_FRAMES (2u)", header)

    def test_simplify_off_and_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(src, simplify="off")
            self.assertEqual(len(result.static.indices_abcd), 32)
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(Path(tmp) / "missing.glb")
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(src, simplify="sometimes")

    def test_saturn_cap_hard_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "grid.glb"
            src.write_bytes(build_grid_glb())
            # Full 32-triangle mesh against a 1-triangle cap: the tool must
            # FAIL with numbers, never force the cap.
            with self.assertRaises(import_model.ImportError) as ctx:
                import_model.import_animated_model(src, simplify="off", max_triangles=1)
            self.assertIn("FAIL", str(ctx.exception))

    def test_report_keys_stable(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = tmp_path / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(
                src, simplify="auto", generate_lods=True)
            for key in ("source", "simplification", "animation_quality",
                        "saturn_animation", "textures", "vdp1", "lods", "result"):
                self.assertIn(key, result.report)
            self.assertTrue(result.report["lods"])
            anim = result.report["saturn_animation"][0]
            for key in ("clip", "baked_frames", "sample_rate_num", "sample_rate_den",
                        "loop", "position_encoding", "pose_stream_bytes",
                        "quantization_max_error"):
                self.assertIn(key, anim)

    def test_cli_animated_flow(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = tmp_path / "grid.glb"
            src.write_bytes(build_grid_glb())
            r = run_import_cli("--input", str(src), "--target", "saturn",
                               "--simplify", "auto", "--quality", "balanced",
                               "--animation", "all",
                               "--out-prefix", str(tmp_path / "grid_model"),
                               "--report", str(tmp_path / "grid.json"))
            self.assertEqual(r.returncode, 0, msg=r.stderr[-2000:])
            report = json.loads((tmp_path / "grid.json").read_text())
            self.assertTrue(report["result"]["pass"])
            self.assertIn("sha256_c", report["output"])

    def test_cli_rejects_bad_suffix(self):
        r = run_import_cli("--input", "foo.txt", "--out-prefix", "out/x")
        self.assertNotEqual(r.returncode, 0)


if __name__ == "__main__":
    unittest.main()
