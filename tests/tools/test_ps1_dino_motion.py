"""Surface transfer and standard glTF pose preservation regressions."""
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
from bake_ps1_dino_walk import closest_surface, source_correspondence, write_glb, original_surface
from model_pipeline.animation import evaluate_positions
from model_pipeline.gltf import parse_model
from model_pipeline.model import SourceModel, from_gltf


class DinoMotionTests(unittest.TestCase):
    def test_original_surface_keeps_corner_uvs_and_materials(self):
        template = SourceModel(materials=[{'name': 'PS1 | skin'}, {'name': 'PS1 | eye'}])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'original.obj'
            path.write_text('v 0 0 0\nv 10 0 0\nv 0 10 0\n'
                            'vt .1 .2\nvt .8 .3\nvt .2 .9\n'
                            'usemtl eye\nf 1/1 2/2 3/3\n'
                            'usemtl skin\nf 3/1 2/3 1/2\n')
            model, points = original_surface(path, template)
        self.assertEqual(model.tri_materials, [0, 1])
        np.testing.assert_array_equal(points, [2, 1, 0, 0, 1, 2])
        np.testing.assert_allclose(model.vertices[:3], [[0, 1, 0], [1, 0, 0], [0, 0, 0]])
        np.testing.assert_allclose(model.uvs[:3], [[.1, .2], [.2, .9], [.8, .3]])

    def test_closest_surface_handles_interior_edge_and_corner(self):
        triangle = np.array([[[0., 0., 0.], [1., 0., 0.], [0., 1., 0.]]])
        points = np.array([[.2, .3, 2.], [.8, .8, 0.], [-1., -1., 0.]])
        faces, weights, _ = closest_surface(points, triangle)
        reconstructed = np.einsum('ijk,ij->ik', triangle[faces], weights)
        np.testing.assert_allclose(reconstructed, [[.2, .3, 0.], [.5, .5, 0.], [0., 0., 0.]])
        np.testing.assert_allclose(weights.sum(axis=1), 1)
        self.assertTrue(np.all(weights >= 0))

    def test_surface_transfer_preserves_affine_displacements(self):
        triangle = np.array([[[0., 0., 0.], [2., 0., 0.], [0., 2., 0.]]])
        points = np.array([[.2, .7, 0.], [.5, .1, 0.]])
        faces, weights, _ = closest_surface(points, triangle)
        transform = np.array([[2., .1, 0.], [0., 3., .2], [0., 0., 1.]])
        transformed = triangle @ transform.T + [1., 2., 3.]
        actual = np.einsum('ijk,ij->ik', transformed[faces], weights)
        np.testing.assert_allclose(actual, points @ transform.T + [1., 2., 3.])

    def test_source_mapping_accounts_for_axis_flip_and_origin(self):
        rest = np.array([[0, 0, 0], [400, 0, 0], [0, 400, 400]])
        obj = rest*[1, -1, 1]/400 + [0, 10, 20]
        mapping, offset, error = source_correspondence(rest, obj[[2, 0, 1, 0]])
        np.testing.assert_array_equal(mapping, [2, 0, 1, 0])
        np.testing.assert_allclose(offset, [0, 10, 20])
        self.assertEqual(error, 0)
        with self.assertRaises(AssertionError):
            source_correspondence(rest, obj + [.01, 0, 0])

    def test_emitted_glb_replays_both_clips_without_double_transform(self):
        positions = np.array([[2., 3., 4.], [3., 3., 4.], [2., 4., 4.]])
        model = SourceModel(vertices=positions.tolist(), normals=[(0, 0, 1)]*3,
                            uvs=[(0, 0), (1, 0), (0, 1)], triangles=[(0, 1, 2)],
                            tri_materials=[0])
        original = SimpleNamespace(bin=b'', json={
            'bufferViews': [], 'accessors': [], 'materials': [{}]})
        walk = np.array([positions, positions+[0., .5, -.25], positions])
        roar = np.array([positions, positions+[.1, -.2, .3], positions])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'motion.glb'
            write_glb(path, original, model, positions, np.arange(3), walk, roar)
            loaded = from_gltf(parse_model(path), 'roundtrip')
            np.testing.assert_allclose(loaded.uvs, model.uvs, atol=1e-7)
            for clip, expected in zip(loaded.clips, (walk, roar)):
                for time, pose in zip((0., clip.duration/2, clip.duration), expected):
                    np.testing.assert_allclose(evaluate_positions(loaded, clip, time),
                                               pose, atol=1e-6)


if __name__ == '__main__':
    unittest.main()
