"""Bake-time splitting of intersecting faces (tools/model_pipeline/intersections.py)."""

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

import import_model  # noqa: E402
from model_pipeline import intersections as ix  # noqa: E402

DATA = REPO / "tests" / "data" / "model3d"


def quad(points, uvs=((0, 0), (1, 0), (1, 1), (0, 1)), source=0):
    return ix.Face([(tuple(map(float, p)), tuple(map(float, uv))) for p, uv in zip(points, uvs)], source)


# Upright quad in the z=0 plane, u = (x + 1) / 2.
WALL_Z = quad(((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)), source=0)
# Upright quad in the x=0 plane: passes through WALL_Z along the y axis.
WALL_X = quad(((0, -1, -1), (0, -1, 1), (0, 1, 1), (0, 1, -1)), source=1)
# Floor both walls stand on: touching, not intersecting.
FLOOR = quad(((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)), source=2)


def area(face):
    pts = [c[0] for c in face.corners]
    total = [0.0, 0.0, 0.0]
    for i in range(1, len(pts) - 1):
        a, b = ix._sub(pts[i], pts[0]), ix._sub(pts[i + 1], pts[0])
        c = ix._cross(a, b)
        total = [t + x for t, x in zip(total, c)]
    return 0.5 * sum(t * t for t in total) ** 0.5


class IntersectionTests(unittest.TestCase):
    def test_detects_only_faces_that_pass_through_each_other(self):
        self.assertTrue(ix.intersects(WALL_Z, WALL_X))
        self.assertFalse(ix.intersects(WALL_Z, FLOOR))
        self.assertFalse(ix.intersects(WALL_X, FLOOR))
        # A wall starting ON the other one (a T junction) only touches it.
        half = quad(((0, -1, 0), (0, -1, 1), (0, 1, 1), (0, 1, 0)))
        self.assertFalse(ix.intersects(WALL_Z, half))
        # Planes cross, but the faces' spans along the line do not overlap.
        apart = quad(((0, 2, -1), (0, 2, 1), (0, 3, 1), (0, 3, -1)))
        self.assertFalse(ix.intersects(WALL_Z, apart))

    def test_split_leaves_no_intersection_and_keeps_area_and_uv(self):
        faces, report = ix.split_intersections([WALL_Z, WALL_X, FLOOR], max_faces=16)
        self.assertEqual(report["splits"], 1)
        self.assertEqual(len(faces), 5)
        self.assertIn(FLOOR, faces)  # untouched, the very same object
        for i in range(len(faces)):
            for j in range(i + 1, len(faces)):
                self.assertFalse(ix.intersects(faces[i], faces[j]))
        for source, original in ((0, WALL_Z), (1, WALL_X)):
            pieces = [f for f in faces if f.source == source]
            self.assertEqual(len(pieces), 2)
            self.assertAlmostEqual(sum(area(p) for p in pieces), area(original))
            # Same facing as the source face: the painter's culling still works.
            n0 = ix.plane(original)[0]
            for p in pieces:
                self.assertAlmostEqual(ix._dot(ix.plane(p)[0], n0), 1.0)
        # Cut corners of WALL_Z lie at x=0, where u must be exactly 0.5.
        cut = [c for f in faces if f.source == 0 for c in f.corners if c[0][0] == 0.0]
        self.assertTrue(cut)
        for pos, uv in cut:
            self.assertAlmostEqual(uv[0], 0.5)
            self.assertAlmostEqual(uv[1], (pos[1] + 1) / 2)

    def test_budget_is_an_error_not_a_partial_split(self):
        with self.assertRaises(ix.SplitBudgetError):
            ix.split_intersections([WALL_Z, WALL_X], max_faces=3)

    def test_fan_pieces_are_vdp1_quads_or_triangles(self):
        def polygon(n):
            return ix.Face([((float(i), float(i * i % 7), 0.0), (0.0, 0.0)) for i in range(n)], 0)
        self.assertEqual([len(p) for p in ix.to_corner_lists(polygon(6))], [4, 4])
        pieces = ix.to_corner_lists(polygon(5))
        self.assertEqual([len(p) for p in pieces], [4, 3])
        # Every piece fans from corner 0 and the pieces share their edges.
        self.assertEqual(pieces[0][0], pieces[1][0])
        self.assertEqual(pieces[0][3], pieces[1][1])


class ImporterSplitTests(unittest.TestCase):
    def test_off_by_default_and_bit_identical_without_intersections(self):
        plain = import_model.import_model(DATA / "quad.obj")
        split = import_model.import_model(DATA / "quad.obj", split_intersections=True)
        self.assertEqual(plain.indices_abcd, split.indices_abcd)
        self.assertEqual(plain.vertices_fx, split.vertices_fx)
        self.assertEqual(split.stats["split_intersections"]["splits"], 0)
        self.assertNotIn("split_intersections", plain.stats)

    def test_crossing_walls_become_four_pieces_and_the_floor_stays(self):
        plain = import_model.import_model(DATA / "cross.obj")
        split = import_model.import_model(DATA / "cross.obj", split_intersections=True)
        self.assertEqual(len(plain.indices_abcd), 3)
        self.assertEqual(len(split.indices_abcd), 5)
        self.assertEqual(split.stats["split_intersections"]["splits"], 1)
        self.assertIn(plain.indices_abcd[2], split.indices_abcd)  # floor
        # The cut adds the two ends of the crossing line, shared by all pieces.
        self.assertEqual(len(split.vertices_fx), len(plain.vertices_fx) + 2)

    def test_budget_overflow_fails_the_import(self):
        with self.assertRaises(import_model.ImportError):
            import_model.import_model(DATA / "cross.obj", split_intersections=True,
                                      split_face_budget=4)


if __name__ == "__main__":
    unittest.main()
