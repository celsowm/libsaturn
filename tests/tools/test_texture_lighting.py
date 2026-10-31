"""Physical reference cases for the optional indexed texture-light bake."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from model_pipeline.texture_lighting import smooth_normals, shade_texture


class TextureLightingTests(unittest.TestCase):
    def test_uv_copies_share_smooth_normal(self):
        # Two faces of equal area meet at a right angle with duplicate UV vertices.
        positions = [(0,0,0), (1,0,0), (0,1,0), (0,0,0), (0,1,0), (0,0,1)]
        normals = smooth_normals(positions, [(0,1,2), (3,4,5)], [0,1,2,0,2,3])
        self.assertEqual(normals[0], normals[3])
        self.assertEqual(normals[2], normals[4])
        self.assertAlmostEqual(normals[0][0], 2**-.5)
        self.assertAlmostEqual(normals[0][2], 2**-.5)

    def test_front_and_back_light_preserve_alpha(self):
        pixels = [(100,80,60,255), (100,80,60,0)]
        lit = shade_texture(pixels, 2, 1, [(0,0,1)]*4, (0,0,1), .25, 1.25)
        dark = shade_texture(pixels, 2, 1, [(0,0,-1)]*4, (0,0,1), .25, 1.25)
        self.assertEqual(lit, [(150,120,90,255), (150,120,90,0)])
        self.assertEqual(dark, [(25,20,15,255), (25,20,15,0)])

    def test_equal_corner_light_has_no_artificial_gradient(self):
        pixels = [(90,120,150,255)]*15
        result = shade_texture(pixels, 5, 3, [(0,1,0)]*4, (0,1,0), .3, .7)
        self.assertEqual(result, pixels)


if __name__ == '__main__':
    unittest.main()
