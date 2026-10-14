"""Reusable source inspection reports decoded clip times and rig metadata."""
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_glb import inspect
from model_pipeline.gltf import GltfError


class InspectGlbTests(unittest.TestCase):
    def test_supplied_clown_metadata_and_decoded_duration(self):
        report = inspect(ROOT / 'examples/killer_clown_demo/assets/killer_clown.glb')
        self.assertEqual(report['skins'][0]['joints'], 66)
        self.assertEqual(report['primitives'][0]['vertices'], 746)
        self.assertEqual(report['primitives'][0]['triangles'], 896)
        self.assertEqual(len(report['clips']), 22)
        idle = next(clip for clip in report['clips'] if clip['name'] == 'Fighting_Idle')
        self.assertAlmostEqual(idle['duration_seconds'], 41 / 24, places=5)
        self.assertEqual(report['images'][0]['mimeType'], 'image/png')
        self.assertEqual(report['sha256'], '677cbbfdb2096b0070b083f69f850c81b34e480a59ccd600a6ae18736dd17baf')

    def test_invalid_container_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'invalid.glb'
            path.write_bytes(b'not a glb')
            with self.assertRaises(GltfError):
                inspect(path)


if __name__ == '__main__':
    unittest.main()
