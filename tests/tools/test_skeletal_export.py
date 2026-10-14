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
    SourceTexture,
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
    def test_animated_render_bounds_include_displaced_mid_clip_pose(self):
        from model_pipeline.skeletal_textures import build_render_asset, emit_render_tables
        model = self.textured_model()
        model.clips = [AnimationClip("displaced", 1.0, [
            AnimationChannel(0, "translation", [0.0, 0.5, 1.0],
                             [(0, 3, 0), (0, 5, 0), (0, 3, 0)])])]
        exp = sx.export_model(model, rate=20)
        render = build_render_asset(model, exp)
        lower, upper = render.stats['animated_bounds_fx']
        self.assertGreater(lower[1], render.stats['source_bounds_fx'][1][1])
        self.assertGreater(upper[1], 6 * 65536)
        for frame in exp.clips[0].frames:
            for point in sx.evaluate_export(exp, frame):
                for axis in range(3):
                    self.assertGreaterEqual(point[axis] * 65536, lower[axis] - 1)
                    self.assertLessEqual(point[axis] * 65536, upper[axis] + 1)
        header, source = emit_render_tables(render, 'fixture')
        self.assertIn('fixture_animated_min', '\n'.join(header))
        self.assertIn('fixture_animated_max', '\n'.join(source))

    def test_generic_multi_anchor_source(self):
        from model_pipeline.animation import mat_vec
        model=chain_model()
        model.skin_anchors=[[(j,w,tuple(mat_vec(model.skins[0].inverse_bind[j],p)))
            for j,w in zip(js,ws) if w] for p,js,ws in zip(model.vertices,model.joints,model.weights)]
        # Distinct positions per influence, whose rest-pose average is unchanged.
        model.skin_anchors[1]=[(0,.5,(.1,1,0)),(1,.5,(.3,0,0))]
        exp=sx.export_model(model)
        self.assertLess(sx.verify(model,exp)["max_error"],.001)
        self.assertNotEqual(exp.skin.blend[0][3][0],exp.skin.blend[0][3][1])
        model.skin_anchors[1]=[(999,1,(0,0,0))]
        with self.assertRaises(ModelImportError):sx.export_model(model)
    def test_shared_skin_records_preserve_seam_uvs_and_source_evaluation(self):
        from model_pipeline.skeletal_textures import build_render_asset
        from unittest.mock import patch
        model = self.textured_model()
        # Duplicate both a rigid and a blended source vertex at a UV seam.
        for index, uv in ((0, (0.25, 0.75)), (1, (0.75, 0.25))):
            for field in ("vertices", "joints", "weights"):
                getattr(model, field).append(getattr(model, field)[index])
            model.uvs.append(uv)
        # This slightly larger bind position quantizes to the same skin anchor.
        # Preserve its original bound despite sharing the first representative.
        model.vertices[4] = (0.20001, 0.0, 0.0)
        model.triangles.append((4, 5, 2))
        model.tri_materials.append(0)
        exp = sx.export_model(model)
        self.assertEqual(len(exp.skin.source_vertex), 4)
        self.assertEqual(exp.skin.triangles[0], exp.skin.triangles[2])
        for frame in exp.clips[0].frames:
            evaluated = sx.evaluate_export(exp, frame)
            self.assertEqual(len(evaluated), 6)
            self.assertEqual(evaluated[0], evaluated[4])
            self.assertEqual(evaluated[1], evaluated[5])
        with patch("model_pipeline.skeletal_textures.bake_face_rgba",
                   return_value=[(255, 0, 0, 255)] * 8) as bake:
            render = build_render_asset(model, exp)
        self.assertEqual(render.indices_abcd[0], render.indices_abcd[2])
        self.assertEqual(render.stats["source_bounds_fx"][1][0], 13108)
        self.assertEqual(max(p[0] for p in render.vertices_fx), 13107)
        self.assertEqual(bake.call_args_list[2].args[0],
                         ((0.25, 0.75), (0, 1), (0.75, 0.25), (0.75, 0.25)))
        self.assertLess(sx.verify(model, exp)["max_error"], 0.001)

    def test_equal_positions_with_different_deformations_stay_distinct(self):
        model = chain_model()
        for field in ("vertices", "joints", "weights"):
            getattr(model, field).append(getattr(model, field)[1])
        model.weights[-1] = (0.75, 0.25, 0, 0)
        exp = sx.export_model(model)
        self.assertEqual(len(exp.skin.source_vertex), 5)
        self.assertNotEqual(exp.skin.source_to_vertex[1], exp.skin.source_to_vertex[4])

    def textured_model(self):
        model = chain_model()
        model.uvs = [(0, 0), (1, 0), (0, 1), (1, 1)]
        model.materials = [{"texture": 0}, {"texture": 1}]
        model.textures = [SourceTexture(1, 1, [(255, 0, 0, 255)]),
                          SourceTexture(1, 1, [(0, 255, 0, 255)])]
        model.tri_materials = [0, 1]
        return model

    def test_textures_follow_skin_vertex_remap_and_materials(self):
        from model_pipeline.skeletal_textures import build_render_asset
        model = self.textured_model()
        exp = sx.export_model(model)
        render = build_render_asset(model, exp)
        # Source (0,1,2) becomes skin (0,2,1); clockwise reverses to (0,1,2).
        self.assertEqual(render.indices_abcd[0], (0, 1, 2, 2))
        self.assertEqual(render.vertices_fx[1], (13107, 131072, 0))
        self.assertEqual(len(render.textures), 2)
        colors = [render.palette_rgb555[t["pixels"][0]] for t in render.textures]
        self.assertEqual(colors, [0x801f, 0x83e0])
        header, source = sx.emit_c(exp, "robot", render)
        self.assertIn("ROBOT_TEXTURE_COUNT 2", header)
        self.assertIn("const sat_model_asset_t robot_asset", source)
        self.assertIn("robot_bind_vertices, 4, robot_indices, 2", source)

    def test_texture_uv_order_matches_clockwise_skin_topology(self):
        from model_pipeline.skeletal_textures import build_render_asset
        from unittest.mock import patch
        model = self.textured_model()
        with patch("model_pipeline.skeletal_textures.bake_face_rgba",
                   return_value=[(255, 0, 0, 255)] * 8) as bake:
            build_render_asset(model, sx.export_model(model))
        self.assertEqual(bake.call_args_list[0].args[0],
                         ((0, 0), (0, 1), (1, 0), (1, 0)))

    def test_textures_deduplicate_and_respect_palette_bank(self):
        from model_pipeline.skeletal_textures import build_render_asset
        model = self.textured_model()
        model.tri_materials = [0, 0]
        render = build_render_asset(model, sx.export_model(model), palette_index=2)
        self.assertEqual(len(render.textures), 1)
        self.assertEqual(render.face_texture_indices, [0, 0])
        self.assertEqual(render.palette_base, 2)

    def test_textures_reject_missing_uvs_and_invalid_materials(self):
        from model_pipeline.skeletal_textures import build_render_asset
        model = self.textured_model()
        model.uvs.pop()
        with self.assertRaisesRegex(ModelImportError, "one UV"):
            build_render_asset(model, sx.export_model(model))
        model = self.textured_model()
        model.tri_materials[0] = 999
        with self.assertRaisesRegex(ModelImportError, "invalid material"):
            build_render_asset(model, sx.export_model(model))

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

    def test_non_unit_scale_round_trips(self):
        # The Saturn runtime stores per-joint rest scale and composes T*R*S,
        # so rigged GLBs with non-unit rest scale (Tripo/Mixamo
        # _scaleCompensation chains, FBX exports that bake scale into the
        # rig) round-trip exactly instead of forcing a mesh bake. The
        # synthetic chain_model hardcodes IBMs that assume unit scale, so
        # we rebuild them from the actual rest-pose world matrices here.
        from model_pipeline.animation import (
            compute_global_transforms, compute_local_transforms,
        )
        from model_pipeline.model import _mat_inverse
        model = chain_model(scale=(2.0, 2.0, 2.0))
        locals_ = compute_local_transforms(model, None, 0.0)
        globals_ = compute_global_transforms(model, locals_)
        model.skins[0].inverse_bind = [_mat_inverse(g) for g in globals_]
        skel = sx.build_skeleton(model)
        self.assertEqual(skel.scale[1], (2 * 65536, 2 * 65536, 2 * 65536))
        exp = sx.export_model(model)
        self.assertLess(sx.verify(model, exp)["max_error"], 0.004)

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
        # Per-joint scale is now part of the generated record.
        self.assertIn("65536, 65536, 65536", c1)
        self.assertIn("CHAIN_GUY_FACE_COUNT 2", h1.upper())

    def test_lut4_rgb_cluts_and_archive_guard(self):
        from model_pipeline.skeletal_textures import build_render_asset, emit_render_tables
        from model_pipeline.skeletal_binary import encode
        model=self.textured_model()
        exp=sx.export_model(model,rate=10)
        render=build_render_asset(model,exp,texture_format="lut4")
        self.assertTrue(render.luts_rgb555)
        self.assertEqual(len(render.luts_rgb555)%16,0)
        for texture in render.textures:
            self.assertTrue(texture['flags']&0x8000)
            self.assertEqual(len(texture['pixels']),texture['pixel_count']//2)
            self.assertLess(texture['palette_slot'],len(render.luts_rgb555)//16)
        self.assertTrue(all(color==0 or color&0x8000 for color in render.luts_rgb555))
        _,source=emit_render_tables(render,'fixture')
        self.assertIn('fixture_luts','\n'.join(source))
        with self.assertRaisesRegex(ValueError,'SKA1'):
            encode(exp,render)

    def test_lut4_preserves_transparent_texels(self):
        from unittest.mock import patch
        from model_pipeline.skeletal_textures import build_render_asset
        model=self.textured_model()
        exp=sx.export_model(model,rate=10)
        face=[(255,0,0,0),(255,0,0,255)]*4
        with patch('model_pipeline.skeletal_textures.estimate_face_size',return_value=(8,1)), \
             patch('model_pipeline.skeletal_textures.bake_face_rgba',return_value=face):
            render=build_render_asset(model,exp,texture_format='lut4')
        for texture in render.textures:
            self.assertEqual(texture['flags']&1,0)
            for packed in texture['pixels']:
                self.assertEqual(packed>>4,0)
                self.assertNotEqual(packed&15,0)
            self.assertEqual(render.luts_rgb555[texture['palette_slot']*16],0)

    def test_size_report(self):
        rep = sx.size_report(sx.export_model(chain_model(), rate=10))
        # 3 joints * 36 bytes (parent + reserved + 12 translation + 8 quat + 12 scale)
        self.assertEqual(rep["skeleton_bytes"], 3 * 36)
        self.assertEqual(rep["skin_bytes"], 2 * 8 + 2 * 26)
        self.assertEqual(rep["clip_bytes"], 11 * 3 * 20)


if __name__ == "__main__":
    unittest.main()
