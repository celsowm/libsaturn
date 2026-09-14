#!/usr/bin/env python3
"""Tests for animation/silhouette quality metrics, target search, LODs and
the Saturn resource profile.

All fixtures are synthetic (hinged two-bone strip, spiked plane, grids);
nothing depends on copyrighted assets.
"""

import math
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

from model_pipeline import lod as lod_mod
from model_pipeline import metrics as metrics_mod
from model_pipeline import model as model_mod
from model_pipeline import saturn_profile as profile_mod
from model_pipeline import silhouette as sil_mod
from model_pipeline import simplification as simp_mod
from model_pipeline.animation import bake_clip_poses
from model_pipeline.gltf import GltfError
from model_pipeline.model import AnimationChannel, AnimationClip


# ----------------------------------------------------------------------
# Fixtures
# ----------------------------------------------------------------------


def build_hinge(hinge_x=1, bend_deg=60):
    """Two-bone strip hinged at ``hinge_x``; bone 1 flexes ``bend_deg``."""
    m = model_mod.SourceModel()
    m.normals = []
    idx = {}
    for j in (0, 1):
        for i in range(9):
            idx[(i, j)] = len(m.vertices)
            m.vertices.append((float(i), float(j) - 0.5, 0.0))
            m.normals.append((0.0, 0.0, 1.0))
            m.uvs.append((i / 8.0, float(j)))
    for j in (0,):
        for i in range(8):
            a, b, c, d = idx[(i, j)], idx[(i + 1, j)], idx[(i + 1, j + 1)], idx[(i, j + 1)]
            m.triangles += [(a, b, c), (a, c, d)]
            m.tri_materials += [0, 0]
    for vi, (x, y, z) in enumerate(m.vertices):
        if x < hinge_x:
            m.joints.append((0, 0, 0, 0))
            m.weights.append((1, 0, 0, 0))
        elif x > hinge_x:
            m.joints.append((1, 1, 1, 1))
            m.weights.append((1, 0, 0, 0))
        else:
            m.joints.append((0, 1, 0, 0))
            m.weights.append((0.5, 0.5, 0, 0))
    m.nodes = [
        model_mod.SourceNode(name="b0", children=[1]),
        model_mod.SourceNode(name="b1", translation=(float(hinge_x), 0.0, 0.0)),
    ]
    m.skins = [
        model_mod.SourceSkin(
            joints=[0, 1],
            inverse_bind=[
                [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1],
                [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -float(hinge_x), 0, 0, 1],
            ],
        )
    ]
    m.skin_index = 0
    s = math.sin(math.radians(bend_deg / 2))
    c = math.cos(math.radians(bend_deg / 2))
    m.clips = [
        AnimationClip(
            name="flex",
            duration=1.0,
            channels=[
                AnimationChannel(
                    node=1,
                    path="rotation",
                    times=[0.0, 0.5, 1.0],
                    values=[(0, 0, 0, 1), (0, 0, s, c), (0, 0, 0, 1)],
                    interpolation="LINEAR",
                )
            ],
        )
    ]
    return m


def build_spike():
    """Subdivided plane with a thin tall pyramid spike at its center."""
    m = model_mod.SourceModel()
    m.normals = []
    n = 6
    idx = {}
    for j in range(n + 1):
        for i in range(n + 1):
            idx[(i, j)] = len(m.vertices)
            m.vertices.append((float(i), float(j), 0.0))
            m.normals.append((0.0, 0.0, 1.0))
            m.uvs.append((i / n, j / n))
    for j in range(n):
        for i in range(n):
            a, b, c, d = idx[(i, j)], idx[(i + 1, j)], idx[(i + 1, j + 1)], idx[(i, j + 1)]
            m.triangles += [(a, b, c), (a, c, d)]
            m.tri_materials += [0, 0]
    cx, cy, hs, h = 3.0, 3.0, 0.1, 1.0
    b0 = len(m.vertices)
    for x, y in ((cx - hs, cy - hs), (cx + hs, cy - hs), (cx + hs, cy + hs), (cx - hs, cy + hs)):
        m.vertices.append((x, y, 0.0))
        m.normals.append((0.0, 0.0, 1.0))
        m.uvs.append((0.5, 0.5))
    apex = len(m.vertices)
    m.vertices.append((cx, cy, h))
    m.normals.append((0.0, 0.0, 1.0))
    m.uvs.append((0.5, 0.5))
    for k in range(4):
        m.triangles.append((b0 + k, b0 + (k + 1) % 4, apex))
        m.tri_materials.append(0)
    return m, apex


HINGE_TIMES = [0.0, 0.25, 0.5, 0.75, 1.0]


# ----------------------------------------------------------------------
# Animation-aware regression: the knee/elbow-like bend must survive
# ----------------------------------------------------------------------


class AnimationAwareTests(unittest.TestCase):
    def test_importance_marks_hinge(self):
        m = build_hinge()
        imp = metrics_mod.compute_animation_importance(m, m.clips[0], HINGE_TIMES)
        self.assertEqual(len(imp), len(m.vertices))
        for v in imp:
            self.assertGreaterEqual(v, 0.0)
            self.assertLessEqual(v, 1.0)
        hinge = [imp[v] for v in range(len(m.vertices)) if m.vertices[v][0] == 1.0]
        tips = [imp[0], imp[1]]
        # The bend region outranks the rigid root in deviation-driven terms.
        self.assertGreater(min(hinge), 0.0)
        # Split-weight hinge verts must carry nonzero importance.
        self.assertGreater(sum(hinge) / len(hinge), sum(tips) / len(tips) * 0.5)

    def test_unskinned_importance_is_zero(self):
        m, _apex = build_spike()
        self.assertEqual(
            metrics_mod.compute_animation_importance(m, None), [0.0] * len(m.vertices)
        )

    def test_animation_aware_beats_bind_only(self):
        m = build_hinge()
        clip = m.clips[0]
        imp = metrics_mod.compute_animation_importance(m, clip, HINGE_TIMES)
        poses = bake_clip_poses(m, clip, HINGE_TIMES)
        bind = simp_mod.simplify(
            m, options=simp_mod.SimplificationOptions(target_triangles=10)
        )
        aware = simp_mod.simplify(
            m,
            anim_importance=imp,
            options=simp_mod.SimplificationOptions(target_triangles=10),
            pose_positions=poses,
        )
        bind_rep = metrics_mod.evaluate_candidate(m, bind, clip, "balanced", HINGE_TIMES, 8)
        aware_rep = metrics_mod.evaluate_candidate(m, aware, clip, "balanced", HINGE_TIMES, 8)
        # The animation-aware path demonstrably scores better under the
        # tool's own objective metrics on this bend fixture.
        self.assertLess(
            aware_rep["surface"]["max"],
            bind_rep["surface"]["max"],
            f"aware {aware_rep['surface']['max']} vs bind {bind_rep['surface']['max']}",
        )
        self.assertLessEqual(aware_rep["surface"]["mean"], bind_rep["surface"]["mean"] * 1.05)
        hinge_surv_bind = sum(1 for v in bind.source_vertex if m.vertices[v][0] == 1.0)
        hinge_surv_aware = sum(1 for v in aware.source_vertex if m.vertices[v][0] == 1.0)
        self.assertGreaterEqual(hinge_surv_aware, hinge_surv_bind)

    def test_target_search_grows_on_quality_failure(self):
        m = build_hinge()
        clip = m.clips[0]
        imp = metrics_mod.compute_animation_importance(m, clip, HINGE_TIMES)
        poses = bake_clip_poses(m, clip, HINGE_TIMES)
        simp, rep = metrics_mod.search_upward(
            m,
            clip,
            4,
            "balanced",
            simp_mod.SimplificationOptions(target_triangles=4),
            anim_importance=imp,
            times=HINGE_TIMES,
        )
        # Must deliver a larger mesh that passes, not force 4 triangles.
        self.assertGreater(len(simp.triangles), 4)
        self.assertTrue(rep["passed"], rep["failing_gates"])
        self.assertEqual(rep["preset"], "balanced")

    def test_search_deterministic(self):
        m = build_hinge()
        kw = dict(
            clip=m.clips[0],
            requested_triangles=4,
            preset="balanced",
            simplify_options=simp_mod.SimplificationOptions(target_triangles=4),
            times=HINGE_TIMES,
        )
        s1, r1 = metrics_mod.search_upward(m, **kw)
        s2, r2 = metrics_mod.search_upward(m, **kw)
        self.assertEqual(s1.triangles, s2.triangles)
        self.assertEqual(len(s1.triangles), len(s2.triangles))

    def test_metrics_deterministic(self):
        m = build_hinge()
        out = simp_mod.simplify(m, options=simp_mod.SimplificationOptions(target_triangles=10))
        r1 = metrics_mod.evaluate_candidate(m, out, m.clips[0], "balanced", HINGE_TIMES, 8)
        r2 = metrics_mod.evaluate_candidate(m, out, m.clips[0], "balanced", HINGE_TIMES, 8)
        self.assertEqual(r1["surface"], r2["surface"])
        self.assertEqual(r1["silhouette"], r2["silhouette"])
        self.assertEqual(r1["passed"], r2["passed"])

    def test_integrity_clean_on_backend_output(self):
        m = build_hinge()
        out = simp_mod.simplify(m, options=simp_mod.SimplificationOptions(target_triangles=10))
        rep = metrics_mod.integrity_report(m, out)
        self.assertEqual(rep["uv_violations"], 0)
        self.assertEqual(rep["material_violations"], 0)


# ----------------------------------------------------------------------
# Silhouette preservation
# ----------------------------------------------------------------------


class SilhouetteTests(unittest.TestCase):
    def test_spike_tip_has_top_importance(self):
        m, apex = build_spike()
        imp = sil_mod.compute_silhouette_importance(m, n_views=16)
        self.assertEqual(len(imp), len(m.vertices))
        self.assertAlmostEqual(imp[apex], 1.0)
        # Plane interior (never an outline) scores strictly lower.
        interior = (len(m.vertices) - 5 - 49) // 2  # a mid-plane vertex
        self.assertLess(imp[interior], imp[apex])

    def test_silhouette_weighted_preserves_outline_better(self):
        m, _apex = build_spike()
        imp = sil_mod.compute_silhouette_importance(m, n_views=16)
        geo = simp_mod.simplify(
            m, options=simp_mod.SimplificationOptions(target_triangles=30)
        )
        sil = simp_mod.simplify(
            m,
            sil_importance=imp,
            options=simp_mod.SimplificationOptions(
                target_triangles=30, silhouette_weight=8.0
            ),
        )
        geo_rep = metrics_mod.evaluate_candidate(m, geo, None, "balanced", [0.0], 8)
        sil_rep = metrics_mod.evaluate_candidate(m, sil, None, "balanced", [0.0], 8)
        self.assertGreater(
            sil_rep["silhouette"]["iou_mean"], geo_rep["silhouette"]["iou_mean"]
        )
        self.assertGreater(
            sil_rep["silhouette"]["iou_min"], geo_rep["silhouette"]["iou_min"]
        )

    def test_fibonacci_views_deterministic(self):
        self.assertEqual(sil_mod.fibonacci_views(16), sil_mod.fibonacci_views(16))
        views = sil_mod.fibonacci_views(8)
        self.assertEqual(len(views), 8)
        for x, y, z in views:
            self.assertAlmostEqual(x * x + y * y + z * z, 1.0, places=6)


# ----------------------------------------------------------------------
# Saturn profile gates
# ----------------------------------------------------------------------


class SaturnProfileTests(unittest.TestCase):
    def test_face_budget_math(self):
        p = profile_mod.SaturnProfile()
        # 512 - 2 setup - 1 end - 8 HUD - 16 headroom = 485.
        self.assertEqual(profile_mod.face_command_budget(p), 485)

    def test_passing_candidate(self):
        p = profile_mod.SaturnProfile()
        rep = profile_mod.check_resources(
            p, faces=300, texture_payload_bytes=100 * 1024, pose_stream_bytes=120 * 1024
        )
        self.assertTrue(rep["passed"], rep["failing_gates"])
        self.assertEqual(rep["worst_case_model_commands"], 300)
        self.assertEqual(rep["reserved_commands"], 2 + 1 + 8)
        self.assertGreaterEqual(rep["command_headroom"], p.min_command_headroom)

    def test_face_over_budget_fails(self):
        p = profile_mod.SaturnProfile()
        rep = profile_mod.check_resources(p, faces=600, texture_payload_bytes=1024)
        self.assertFalse(rep["passed"])
        self.assertTrue(any("face budget" in g for g in rep["failing_gates"]))

    def test_vram_over_budget_fails(self):
        p = profile_mod.SaturnProfile()
        rep = profile_mod.check_resources(
            p, faces=100, texture_payload_bytes=profile_mod.VDP1_TEXTURE_BUDGET_BYTES
        )
        self.assertFalse(rep["passed"])
        self.assertTrue(any("VRAM" in g for g in rep["failing_gates"]))

    def test_vram_alignment_per_texture(self):
        self.assertEqual(profile_mod.vram_for_textures([9, 16]), 16 + 16)

    def test_pose_stream_over_budget_fails(self):
        p = profile_mod.SaturnProfile()
        rep = profile_mod.check_resources(
            p, faces=100, texture_payload_bytes=1024,
            pose_stream_bytes=p.max_pose_stream_bytes + 1,
        )
        self.assertFalse(rep["passed"])

    def test_hard_failure_message_has_numbers(self):
        msg = profile_mod.format_hard_failure(412, 340)
        self.assertIn("FAIL", msg)
        self.assertIn("412", msg)
        self.assertIn("340", msg)


# ----------------------------------------------------------------------
# LOD generation
# ----------------------------------------------------------------------


class LodTests(unittest.TestCase):
    def test_default_specs_derive_from_budget(self):
        specs = lod_mod.default_lod_specs(485, 1552)
        self.assertGreaterEqual(len(specs), 2)
        targets = [s.target_triangles for s in specs]
        self.assertEqual(targets, sorted(targets, reverse=True))
        self.assertLessEqual(targets[0], 485)
        self.assertLessEqual(targets[0], 1552)

    def test_lods_derive_from_source_with_reports(self):
        m = build_hinge()
        specs = [
            lod_mod.LodSpec("near", 12),
            lod_mod.LodSpec("far", 8),
        ]
        lods = lod_mod.generate_lods(m, m.clips[0], specs, times=HINGE_TIMES)
        self.assertEqual(len(lods), 2)
        for entry, spec in zip(lods, specs):
            simp = entry["simplified"]
            self.assertLessEqual(len(simp.triangles), spec.target_triangles + 2)
            # Derived from the source: every vertex maps back, indices valid.
            for sv in simp.source_vertex:
                self.assertLess(sv, len(m.vertices))
            for (a, b, c) in simp.triangles:
                self.assertLess(a, len(simp.positions))
            self.assertIn("passed", entry)
            self.assertIn("surface", entry)

    def test_quality_presets_explicit(self):
        for name in ("conservative", "balanced", "aggressive"):
            self.assertIn(name, metrics_mod.QUALITY_PRESETS)
            preset = metrics_mod.QUALITY_PRESETS[name]
            self.assertIn("max_surface_error", preset)
            self.assertIn("silhouette_iou_min", preset)
        with self.assertRaises(GltfError):
            metrics_mod.evaluate_candidate(
                build_hinge(),
                simp_mod.simplify(build_hinge(), options=simp_mod.SimplificationOptions(target_triangles=100)),
                build_hinge().clips[0],
                "nonexistent",
                HINGE_TIMES,
                4,
            )


if __name__ == "__main__":
    unittest.main()
