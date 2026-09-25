#!/usr/bin/env python3
"""Tests for box-filtered face baking (--sampling area) and --texel-extent."""

import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))
sys.path.insert(0, str(REPO / "tests"))

import import_model
from test_import_animated import make_quadrant_glb

FULL = ((0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0))


def checker(size: int) -> list[tuple[int, int, int, int]]:
    return [((255, 255, 255, 255) if (x + y) % 2 else (0, 0, 0, 255))
            for y in range(size) for x in range(size)]


class AreaBakeTests(unittest.TestCase):
    def test_nearest_downsample_aliases_but_area_averages(self):
        src = checker(16)
        nearest = import_model.bake_face_rgba(FULL, 16, 16, src, 4, 4, "nearest")
        area = import_model.bake_face_rgba(FULL, 16, 16, src, 4, 4, "area")
        # Nearest picks single texels: pure black or white.
        self.assertTrue(all(p[0] in (0, 255) for p in nearest))
        # Area averages equal black/white coverage in linear light: the
        # sRGB value of linear 0.5 is ~186, the same for every texel.
        values = {p[0] for p in area}
        self.assertTrue(all(170 <= v <= 200 for v in values), values)
        self.assertLessEqual(max(values) - min(values), 6)
        self.assertTrue(all(p[3] == 255 for p in area))

    def test_area_keeps_flat_color_exact(self):
        src = [(200, 100, 50, 255)] * 64
        area = import_model.bake_face_rgba(FULL, 8, 8, src, 8, 8, "area")
        self.assertEqual(set(area), {(200, 100, 50, 255)})

    def test_unknown_sampling_rejected(self):
        with self.assertRaises(import_model.ImportError):
            import_model.bake_face_rgba(FULL, 1, 1, [(0, 0, 0, 255)], 8, 1, "cubic")


class TexelExtentTests(unittest.TestCase):
    def test_extent_caps_face_texture_size(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            full = import_model.import_animated_model(src, simplify="off")
            capped = import_model.import_animated_model(
                src, simplify="off", sampling="area", texel_extent=4.0)
            big = max(t["height"] for t in full.static.textures)
            small = max(t["height"] for t in capped.static.textures)
            self.assertLess(small, big)
            self.assertLessEqual(small, 4)

    def test_extent_must_be_positive(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = make_quadrant_glb(Path(tmp))
            with self.assertRaises(import_model.ImportError):
                import_model.import_animated_model(src, simplify="off", texel_extent=0.0)


if __name__ == "__main__":
    unittest.main()
