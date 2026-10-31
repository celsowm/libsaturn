"""Normal seams may merge; UV and animation seams must remain independent."""
import copy
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tools'), str(ROOT/'tests')]
import import_model
from bake_ps1_dino_walk import write_glb
from model_pipeline.gltf import parse_model
from model_pipeline.model import from_gltf
from test_import_animated import make_quadrant_glb


class MotionEdgeTests(unittest.TestCase):
    def import_split(self, directory, seam=None):
        glb = parse_model(make_quadrant_glb(directory))
        source = from_gltf(glb, 'quads')
        model = copy.copy(source)
        corners = np.asarray(source.triangles).reshape(-1)
        positions = np.asarray(source.vertices)[corners]
        model.vertices = positions.tolist()
        model.uvs = np.asarray(source.uvs)[corners].tolist()
        model.normals = [(0, 0, 1) if i//3 % 2 == 0 else (0, .6, .8)
                         for i in range(len(corners))]
        model.triangles = np.arange(len(corners)).reshape(-1, 3).tolist()
        poses = np.asarray([positions, positions+[0, 0, .1], positions])
        if seam == 'uv': model.uvs[3][0] += .03
        if seam == 'motion': poses[1, 3, 0] += .1
        path = directory/'split.glb'
        write_glb(path, glb, model, positions, np.arange(len(corners)), poses, poses)
        return import_model.import_animated_model(path, simplify='off', merge_quads=True,
                                                  weld_vertices=True)

    def test_normal_split_edges_still_merge(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.import_split(Path(directory))
            self.assertEqual(result.report['quad_merge']['merged_quads'], 4)
            self.assertEqual(len(result.static.indices_abcd), 4)

    def test_uv_discontinuity_keeps_the_pair_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.import_split(Path(directory), 'uv')
            self.assertEqual(result.report['quad_merge']['merged_quads'], 3)
            self.assertEqual(len(result.static.indices_abcd), 5)

    def test_different_motion_keeps_the_pair_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.import_split(Path(directory), 'motion')
            self.assertEqual(result.report['quad_merge']['merged_quads'], 3)
            self.assertEqual(len(result.static.indices_abcd), 5)


if __name__ == '__main__':
    unittest.main()
