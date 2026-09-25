#!/usr/bin/env python3
"""Tests for LUT4 (4-bit, per-face lookup table) baked model textures."""

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
from saturn_asset_common import rgb888_to_rgb555
from test_import_animated import make_quadrant_glb
from test_simplify_cli import build_grid_glb

LUT4_FLAG = 0x8000


def unpack(pixels: bytes) -> list[int]:
    codes = []
    for b in pixels:
        codes += [b >> 4, b & 0x0F]
    return codes


class QuantizeFaceTests(unittest.TestCase):
    def test_few_colors_are_kept_exactly_and_code_zero_unused(self):
        red, blue = (248, 0, 0, 255), (0, 0, 248, 255)
        rgba = [red, blue] * 8  # 8x2
        lut, packed = import_model.quantize_face_lut4(rgba, 8, 2)
        self.assertEqual(len(lut), 16)
        self.assertEqual(len(packed), 8)
        codes = unpack(packed)
        self.assertNotIn(0, codes)
        self.assertEqual([lut[c] for c in codes],
                         [rgb888_to_rgb555(*p[:3]) for p in rgba])

    def test_many_colors_reduce_to_fifteen(self):
        rgba = [(x * 8, y * 8, (x + y) * 4, 255) for y in range(16) for x in range(32)]
        lut, packed = import_model.quantize_face_lut4(rgba, 32, 16)
        codes = unpack(packed)
        self.assertEqual(len(codes), 32 * 16)
        self.assertTrue(1 <= min(codes) and max(codes) <= 15)
        self.assertTrue(all(c & 0x8000 for c in lut))


class Lut4ImportTests(unittest.TestCase):
    def test_quadrants_bake_solid_lut4_faces(self):
        expected = [rgb888_to_rgb555(*c) for c in
                    ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0))]
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            result = import_model.import_animated_model(
                src, simplify="off", texture_format="lut4")
            static = result.static
            self.assertEqual(static.palette_rgb555, [])
            self.assertEqual(len(static.luts_rgb555) % 16, 0)
            for k, want in enumerate(expected):
                for face in (2 * k, 2 * k + 1):
                    tex = static.textures[static.face_texture_indices[face]]
                    self.assertTrue(tex["flags"] & LUT4_FLAG)
                    self.assertEqual(len(tex["pixels"]) * 2, tex["pixel_count"])
                    lut = static.luts_rgb555[tex["palette_slot"] * 16:][:16]
                    self.assertEqual({lut[c] for c in unpack(tex["pixels"])}, {want})
            vdp1 = result.report["vdp1"]
            self.assertEqual(vdp1["texture_payload_bytes"],
                             sum(len(t["pixels"]) for t in static.textures)
                             + 32 * (len(static.luts_rgb555) // 16))

    def test_lut_codes_index_one_shared_palette(self):
        expected = [rgb888_to_rgb555(*c) for c in
                    ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0))]
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            result = import_model.import_animated_model(
                src, simplify="off", texture_format="lut4", lut_codes=(2, 253),
                palette_index=1)
            static = result.static
            pal = static.palette_rgb555
            self.assertEqual(len(pal), 256)
            self.assertEqual(static.palette_base, 1)
            self.assertEqual([pal[0], pal[1], pal[254], pal[255]], [0, 0, 0, 0])
            for k, want in enumerate(expected):
                for face in (2 * k, 2 * k + 1):
                    tex = static.textures[static.face_texture_indices[face]]
                    lut = static.luts_rgb555[tex["palette_slot"] * 16:][:16]
                    self.assertTrue(all(2 <= code <= 253 for code in lut))
                    self.assertEqual({pal[lut[c]] for c in unpack(tex["pixels"])}, {want})

    def test_lut_codes_need_lut4_and_a_legal_range(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(src, simplify="off", lut_codes=(2, 253))
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(
                    src, simplify="off", texture_format="lut4", lut_codes=(0, 253))

    def test_lut4_c_compiles(self):
        cc = shutil.which("g++") or shutil.which("gcc") or shutil.which("cc") or shutil.which("clang")
        if cc is None:
            self.skipTest("no host C compiler")
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            src = tmp_path / "grid.glb"
            src.write_bytes(build_grid_glb())
            result = import_model.import_animated_model(
                src, simplify="auto", texture_format="lut4")
            _, c = import_model.emit_anim.emit_animated_c_h(
                result.static, result.animations, tmp_path / "grid_model", "grid")
            text = c.read_text()
            self.assertIn("grid_luts[", text)
            r = subprocess.run(
                [cc, "-fsyntax-only", "-Wall", "-Wextra", "-Werror", f"-I{REPO / 'include'}", str(c)],
                capture_output=True, text=True, check=False)
            self.assertEqual(r.returncode, 0, msg=r.stderr[-2000:])

    def test_unknown_format_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(src, simplify="off", texture_format="rgb")


if __name__ == "__main__":
    unittest.main()
