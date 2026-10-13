"""Regression coverage for texture transfer across disconnected UV islands."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import numpy as np
from model_pipeline.model import SourceModel, SourceTexture
from model_pipeline.surface_bake import SurfaceBaker, closest_surface


class SurfaceBakeTests(unittest.TestCase):
    def model(self):
        m = SourceModel()
        m.vertices = [(0,0,0), (1,0,0), (0,1,0), (1,0,0), (1,1,0), (0,1,0)]
        m.triangles = [(0,1,2), (3,4,5)]
        m.tri_materials = [0,0]
        # Left island is red; right island is green. Blue atlas padding must
        # never be sampled between these islands during a geometric collapse.
        m.uvs = [(0,0)] * 3 + [(1,0)] * 3
        m.materials = [{'texture':0}]
        m.textures = [SourceTexture(3,1,[(255,0,0,255),(0,0,255,255),(0,255,0,255)])]
        return m

    def test_closest_point_regions_and_degenerate_triangle(self):
        tri = np.array([[(0,0,0),(1,0,0),(0,1,0)], [(2,0,0)]*3], dtype=float)
        points = np.array([(0.2,0.3,1),(-1,-1,0),(1,1,0),(2,0,0)])
        dist,bary = closest_surface(points,tri)
        np.testing.assert_allclose(dist[:,0], [1,2,.5,1])
        np.testing.assert_allclose(bary[0,0], [.5,.2,.3])
        self.assertEqual(dist[3,1],0)
        np.testing.assert_allclose(bary.sum(2),1)

    def test_surface_transfer_does_not_interpolate_between_uv_islands(self):
        baker = SurfaceBaker(self.model())
        quad = [(0,0,0),(0,1,0),(1,1,0),(1,0,0)]
        colors = baker.bake(quad,0,8,8,'nearest')
        self.assertIn((255,0,0,255),colors)
        self.assertIn((0,255,0,255),colors)
        self.assertTrue(all(b == 0 for r,g,b,a in colors))

    def test_search_expands_for_displaced_reduced_surface(self):
        baker = SurfaceBaker(self.model())
        quad = [(0,0,2),(0,1,2),(1,1,2),(1,0,2)]
        colors = baker.bake(quad,0,8,8,'nearest')
        self.assertTrue(all(b == 0 for r,g,b,a in colors))

    def test_emission_and_material_factor_are_baked_in_linear_space(self):
        m = self.model()
        m.materials[0].update(rgb=(0,0,0), emission_texture=1,
                              emission_factor=(0,1,0), emission_strength=1)
        m.textures.append(SourceTexture(1,1,[(255,255,255,255)]))
        colors = SurfaceBaker(m).bake([(0,0,0),(0,1,0),(1,0,0),(1,0,0)],0,8,8,'area')
        self.assertTrue(all(c == (0,255,0,255) for c in colors))

    def test_factor_only_material_is_not_multiplied_twice(self):
        m = self.model()
        m.textures[0] = SourceTexture(1,1,[(128,0,0,255)])
        m.materials[0].update(rgb=(128,0,0),factor_only=True)
        colors = SurfaceBaker(m).bake([(0,0,0),(0,1,0),(1,0,0),(1,0,0)],0,8,8,'nearest')
        self.assertTrue(all(c == (128,0,0,255) for c in colors))

    def test_area_filter_preserves_opaque_alpha(self):
        colors = SurfaceBaker(self.model(),exposure=3,studio=True).bake(
            [(0,0,0),(0,1,0),(1,1,0),(1,0,0)],0,8,8,'area')
        self.assertTrue(all(c[3] == 255 and c[2] == 0 for c in colors))

    def test_reversed_winding_keeps_the_same_source_surface(self):
        m = self.model()
        # A nearby, oppositely oriented blue layer must not replace the
        # intended red/green surface when the caller reverses render winding.
        m.vertices += [(0,0,.01),(0,1,.01),(1,0,.01)]
        m.triangles.append((6,7,8)); m.tri_materials.append(0)
        m.uvs += [(.5,0)] * 3
        colors = SurfaceBaker(m,reverse_winding=True).bake(
            [(0,0,.01),(1,0,.01),(1,1,.01),(0,1,.01)],0,8,8,'nearest')
        self.assertTrue(all(c[2] == 0 for c in colors))


if __name__ == '__main__':
    unittest.main()
