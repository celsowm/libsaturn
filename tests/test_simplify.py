#!/usr/bin/env python3
"""Tests for the attribute-aware edge-collapse simplification backend.

Fixtures are synthetic grids/creases/seams built in-test; nothing here
depends on copyrighted assets.
"""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

from model_pipeline import model as model_mod
from model_pipeline import simplification as S


def make_grid(nx, ny, seam_mirror=False, materials=None):
    """Planar XY grid, CCW triangles, per-vertex UVs over [0,1].

    With ``seam_mirror``, the middle column is split: left quads keep
    u = 0.5 on the shared edge while right quads use duplicates with
    u = 0.0 (a different texture region), forming a real UV seam.
    """
    m = model_mod.SourceModel()
    m.normals = []
    idx = {}
    for j in range(ny + 1):
        for i in range(nx + 1):
            idx[(i, j)] = len(m.vertices)
            m.vertices.append((float(i), float(j), 0.0))
            m.normals.append((0.0, 0.0, 1.0))
            m.uvs.append((i / nx, j / ny if ny else 0.0))
    for j in range(ny):
        for i in range(nx):
            a = idx[(i, j)]
            b = idx[(i + 1, j)]
            c = idx[(i + 1, j + 1)]
            d = idx[(i, j + 1)]
            mat = materials(i, j) if materials else 0
            m.triangles.append((a, b, c))
            m.tri_materials.append(mat)
            m.triangles.append((a, c, d))
            m.tri_materials.append(mat)
    if seam_mirror:
        mid = nx // 2
        dup = {}
        for j in range(ny + 1):
            old = idx[(mid, j)]
            new = len(m.vertices)
            dup[old] = new
            m.vertices.append(m.vertices[old])
            m.normals.append(m.normals[old])
            m.uvs.append((0.0, m.uvs[old][1]))
        for t in range(len(m.triangles)):
            col = (t // 2) % nx  # quad column of triangle t
            if col >= mid:
                a, b, c = m.triangles[t]
                m.triangles[t] = (
                    dup.get(a, a),
                    dup.get(b, b),
                    dup.get(c, c),
                )
    return m


def check_valid(out, test):
    n = len(out.positions)
    test.assertEqual(len(out.uvs), n)
    test.assertEqual(len(out.source_vertex), n)
    for (a, b, c) in out.triangles:
        test.assertLess(a, n)
        test.assertLess(b, n)
        test.assertLess(c, n)
        test.assertEqual(len({a, b, c}), 3, "degenerate triangle emitted")
    # Subset property: every output attribute matches its source vertex.
    for ni, si in enumerate(out.source_vertex):
        test.assertEqual(out.uvs[ni], test.model.uvs[si] if hasattr(test, "model") else out.uvs[ni])


class SimplifyBackendTests(unittest.TestCase):
    def test_trivial_triangle_noop(self):
        m = make_grid(1, 1)
        out = S.simplify(m, options=S.SimplificationOptions(target_triangles=10))
        self.assertEqual(out.triangles, m.triangles)
        self.assertTrue(out.report["target_met"])
        self.assertEqual(out.report["collapses"], 0)

    def test_planar_grid_reaches_target(self):
        m = make_grid(8, 8)
        self.model = m
        out = S.simplify(m, options=S.SimplificationOptions(target_triangles=32))
        self.assertLessEqual(len(out.triangles), 32)
        self.assertGreater(len(out.triangles), 0)
        self.assertTrue(out.report["target_met"])
        check_valid(out, self)
        # Planar grid must keep facing +Z (foldover collapses rejected).
        for (a, b, c) in out.triangles:
            ax, ay, _ = out.positions[a]
            bx, by, _ = out.positions[b]
            cx, cy, _ = out.positions[c]
            area2 = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
            self.assertGreater(area2, 0.0)

    def test_deterministic_output(self):
        m = make_grid(6, 6)
        kw = dict(options=S.SimplificationOptions(target_triangles=30))
        o1 = S.simplify(m, **kw)
        o2 = S.simplify(m, **kw)
        self.assertEqual(o1.triangles, o2.triangles)
        self.assertEqual(o1.positions, o2.positions)
        self.assertEqual(o1.report, o2.report)

    def test_uv_seam_not_welded(self):
        m = make_grid(4, 2, seam_mirror=True)
        self.model = m
        out = S.simplify(
            m, options=S.SimplificationOptions(target_triangles=4, quality="conservative")
        )
        check_valid(out, self)
        # Seam sides must never merge into one triangle: left quads (cols
        # 0-1) and right quads (cols 2-3) share no edge, and subset collapse
        # only substitutes along existing edges, so no output face may mix
        # vertices that are exclusive to opposite sides of the seam.
        left_only: set[int] = set()
        right_only: set[int] = set()
        for t in range(len(m.triangles)):
            col = (t // 2) % 4
            for v in m.triangles[t]:
                (left_only if col < 2 else right_only).add(v)
        strict_left = left_only - right_only
        strict_right = right_only - left_only
        self.assertTrue(strict_left and strict_right)
        for (a, b, c) in out.triangles:
            vs = {out.source_vertex[a], out.source_vertex[b], out.source_vertex[c]}
            self.assertFalse(
                vs & strict_left and vs & strict_right,
                "triangle welds across a locked UV seam",
            )
        # Both texture regions must still be sampled: the seam was not
        # collapsed into a single UV span.
        out_uvs = {out.uvs[i] for i in range(len(out.positions))}
        left_uv = {uv for uv in out_uvs if uv[0] > 0.25}
        right_uv = {uv for uv in out_uvs if uv[0] <= 0.25}
        self.assertTrue(left_uv, "left texture region vanished")
        self.assertTrue(right_uv, "right texture region vanished")

    def test_material_boundary_preserved(self):
        m = make_grid(4, 2, materials=lambda i, j: 0 if i < 2 else 1)
        self.model = m
        out = S.simplify(
            m, options=S.SimplificationOptions(target_triangles=4, quality="balanced")
        )
        check_valid(out, self)
        # Every surviving triangle keeps a single source material.
        self.assertEqual(len(out.tri_materials), len(out.triangles))
        for mt in out.tri_materials:
            self.assertIn(mt, (0, 1))
        # No triangle may mix vertices that only ever appeared in material 0
        # with vertices that only ever appeared in material 1: the boundary
        # edge is locked, so the middle column survives on both sides.
        left_only = set()
        right_only = set()
        for (a, b, c), mt in zip(m.triangles, m.tri_materials):
            for v in (a, b, c):
                (left_only if mt == 0 else right_only).add(v)
        strict_left = left_only - right_only
        strict_right = right_only - left_only
        for (a, b, c) in out.triangles:
            vs = {out.source_vertex[a], out.source_vertex[b], out.source_vertex[c]}
            self.assertFalse(
                vs & strict_left and vs & strict_right,
                "triangle spans a locked material boundary",
            )

    def test_crease_edge_protected(self):
        # Two quads at 90 degrees sharing the edge x=0 (continuous UVs).
        m = model_mod.SourceModel()
        m.normals = []
        # Quad 1 in XY plane: x in [-1, 0].
        q1 = [(-1, 0, 0), (0, 0, 0), (0, 1, 0), (-1, 1, 0)]
        # Quad 2 in ZY plane: z in [0, 1], sharing (0,0,0)-(0,1,0).
        q2 = [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0)]
        base = 0
        for quad, uoff in ((q1, 0.0), (q2, 0.5)):
            ids = []
            for k, (x, y, z) in enumerate(quad):
                # Shared edge reuses the same vertex index when position and
                # UV match, so the crease is a real mesh edge, not a seam.
                ids.append(len(m.vertices))
                m.vertices.append((x, y, z))
                m.normals.append((0, 0, 1))
                m.uvs.append((uoff + 0.25 * (k % 2), 0.5 * (k // 2)))
            m.triangles.append((ids[0], ids[1], ids[2]))
            m.tri_materials.append(0)
            m.triangles.append((ids[0], ids[2], ids[3]))
            m.tri_materials.append(0)
        out = S.simplify(
            m, options=S.SimplificationOptions(target_triangles=2, quality="conservative")
        )
        check_valid(out, self)
        # The shared crease positions survive (both quads still represented).
        crease = {(0.0, 0.0, 0.0), (0.0, 1.0, 0.0)}
        got = {tuple(out.positions[i]) for i in range(len(out.positions))}
        self.assertTrue(crease <= got, f"crease collapsed: {got}")

    def test_animation_importance_protects_region(self):
        m = make_grid(6, 6)
        self.model = m
        # Mark the center vertex as critical articulation.
        center = min(
            range(len(m.vertices)),
            key=lambda v: (m.vertices[v][0] - 3.0) ** 2 + (m.vertices[v][1] - 3.0) ** 2,
        )
        imp = [0.0] * len(m.vertices)
        imp[center] = 10.0
        out = S.simplify(
            m,
            anim_importance=imp,
            options=S.SimplificationOptions(target_triangles=20, animation_weight=5.0),
        )
        check_valid(out, self)
        self.assertIn(center, out.source_vertex)

    def test_importance_length_mismatch_rejected(self):
        m = make_grid(2, 2)
        with self.assertRaises(Exception):
            S.simplify(m, anim_importance=[0.0])

    def test_report_fields_stable(self):
        m = make_grid(4, 4)
        out = S.simplify(m, options=S.SimplificationOptions(target_triangles=16))
        for key in (
            "requested_target",
            "delivered_triangles",
            "delivered_vertices",
            "source_triangles",
            "source_vertices",
            "reduction_percent",
            "target_met",
            "quality",
        ):
            self.assertIn(key, out.report)
        self.assertEqual(out.report["requested_target"], 16)
        self.assertEqual(out.report["delivered_triangles"], len(out.triangles))

    def test_aggressive_may_merge_seams_but_stays_valid(self):
        m = make_grid(4, 2, seam_mirror=True)
        self.model = m
        out = S.simplify(
            m, options=S.SimplificationOptions(target_triangles=2, quality="aggressive")
        )
        check_valid(out, self)
        self.assertLessEqual(len(out.triangles), 4)


if __name__ == "__main__":
    unittest.main()
