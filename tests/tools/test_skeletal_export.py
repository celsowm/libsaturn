#!/usr/bin/env python3
"""tools/model_pipeline/skeletal_export.py: skeleton, clip, skin and C emission."""

from __future__ import annotations

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from model_pipeline import skeletal_export as sx  # noqa: E402
from model_pipeline.errors import ImportError as ModelImportError  # noqa: E402
from model_pipeline.model import (  # noqa: E402
    AnimationChannel,
    AnimationClip,
    SourceModel,
    SourceNode,
    SourceSkin,
)


def ibm(y: float) -> list[float]:
    m = [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]
    m[13] = -y
    return m


def chain_model(scale=(1.0, 1.0, 1.0)) -> SourceModel:
    """Three joints stacked along +Y; joint 1 swings 90 degrees about Z."""
    s = math.sqrt(0.5)
    model = SourceModel()
    model.nodes = [
        SourceNode("root", children=[1]),
        SourceNode("mid", children=[2], translation=(0.0, 1.0, 0.0), scale=scale),
        SourceNode("tip", translation=(0.0, 1.0, 0.0)),
    ]
    model.skins = [SourceSkin(joints=[0, 1, 2], inverse_bind=[ibm(0), ibm(1), ibm(2)])]
    model.skin_index = 0
    model.vertices = [(0.2, 0.0, 0.0), (0.2, 1.0, 0.0), (0.2, 2.0, 0.0), (0.1, 1.5, 0.0)]
    model.joints = [(0, 0, 0, 0), (0, 1, 0, 0), (2, 0, 0, 0), (1, 2, 0, 0)]
    model.weights = [(1.0, 0, 0, 0), (0.5, 0.5, 0, 0), (1.0, 0, 0, 0), (0.6, 0.4, 0, 0)]
    model.triangles = [(0, 1, 2), (1, 2, 3)]
    model.tri_materials = [0, 0]
    model.clips = [AnimationClip("swing", 1.0, [
        AnimationChannel(1, "rotation", [0.0, 1.0],
                         [(0.0, 0.0, 0.0, 1.0), (0.0, 0.0, s, s)]),
    ])]
    return model


class SkeletalExportTests(unittest.TestCase):
    def test_skeleton_is_parent_first_with_relative_rest(self):
        skel = sx.build_skeleton(chain_model())
        self.assertEqual(skel.parent, [sx.ROOT_PARENT, 0, 1])
        self.assertEqual(skel.translation[1], (0, 65536, 0))
        self.assertEqual(skel.rotation[1], (0, 0, 0, 32767))

    def test_shuffled_joint_list_is_reordered(self):
        model = chain_model()
        model.skins[0].joints = [2, 0, 1]
        model.skins[0].inverse_bind = [ibm(2), ibm(0), ibm(1)]
        model.joints = [tuple({0: 1, 1: 2, 2: 0}[j] for j in js) for js in model.joints]
        skel = sx.build_skeleton(model)
        self.assertEqual(skel.node_of_joint, [0, 1, 2])
        exp = sx.export_model(model)
        self.assertLess(sx.verify(model, exp)["max_error"], 0.01)

    def test_non_unit_scale_is_rejected(self):
        with self.assertRaises(ModelImportError):
            sx.build_skeleton(chain_model(scale=(2.0, 1.0, 1.0)))

    def test_clip_quaternions_are_continuous_and_w_positive_at_start(self):
        exp = sx.export_model(chain_model(), rate=10)
        clip = exp.clips[0]
        self.assertEqual(clip.frame_count, 11)
        prev = None
        for row in clip.frames:
            q = row[1][1]
            if prev:
                self.assertGreaterEqual(sum(a * b for a, b in zip(q, prev)), 0)
            prev = q
        self.assertEqual(clip.frames[0][1][1][3], 32767)

    def test_skin_split_and_weights(self):
        exp = sx.export_model(chain_model())
        skin = exp.skin
        self.assertEqual(len(skin.rigid), 2)
        self.assertEqual(len(skin.blend), 2)
        self.assertEqual(skin.source_vertex, [0, 2, 1, 3])
        self.assertEqual(skin.blend[0][2][0], 16384)
        self.assertEqual(skin.max_joint, 2)
        for joints, n, w, _pos in skin.blend:
            self.assertLessEqual(sum(w[: n - 1]), 32768)
        # indices were remapped to the export order
        self.assertEqual(skin.triangles[0], (0, 2, 1))

    def test_export_matches_float_oracle_over_the_clip(self):
        model = chain_model()
        exp = sx.export_model(model, rate=30)
        result = sx.verify(model, exp)
        self.assertLess(result["rest_error"], 0.002)
        self.assertLess(result["max_error"], 0.004)

    def test_scale_parameter_scales_output(self):
        model = chain_model()
        exp = sx.export_model(model, scale=8.0)
        self.assertEqual(exp.skeleton.translation[1], (0, 8 * 65536, 0))
        self.assertLess(sx.verify(model, exp)["max_error"], 0.004)

    def test_emit_c_is_deterministic_and_complete(self):
        exp = sx.export_model(chain_model(), rate=10, loop_clips={"swing"})
        h1, c1 = sx.emit_c(exp, "Chain Guy")
        h2, c2 = sx.emit_c(exp, "Chain Guy")
        self.assertEqual((h1, c1), (h2, c2))
        for needle in ("Chain_Guy_skeleton", "Chain_Guy_skin", "Chain_Guy_clip_swing",
                       "SAT_CLIP_FLAG_LOOP", "sat_skin_blend_t"):
            self.assertIn(needle, h1 + c1)
        self.assertIn("CHAIN_GUY_FACE_COUNT 2", h1.upper())

    def test_size_report(self):
        rep = sx.size_report(sx.export_model(chain_model(), rate=10))
        self.assertEqual(rep["skeleton_bytes"], 72)
        self.assertEqual(rep["skin_bytes"], 2 * 8 + 2 * 26)
        self.assertEqual(rep["clip_bytes"], 11 * 3 * 14)


if __name__ == "__main__":
    unittest.main()
