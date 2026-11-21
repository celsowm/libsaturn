#!/usr/bin/env python3
"""tools/water_prepare.py must match sat_water_prepare_mesh bit for bit."""
import random
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import water_prepare  # noqa: E402

ONE = 1 << 16
SOURCES = [
    "tools/host/water_prepare_dump.cpp",
    "src/graphics/3d/rendering/water_prepare.cpp",
    "src/graphics/3d/rendering/water.cpp",
    "src/physics/3d/water_regions.cpp",
    "src/physics/3d/water_volume.cpp",
]


def compiler():
    return next((c for c in (shutil.which("c++"), shutil.which("clang++"), shutil.which("g++")) if c), None)


def format_input(regions, tint, full_depth, kinds, vertices, light, indices, face_materials):
    lines = [str(len(regions))]
    for r in regions:
        lines.append(" ".join(str(r[k]) for k in ("min_x", "max_x", "min_z", "max_z", "bottom", "surface")))
    lines.append("%d %d %d %d" % (*tint, full_depth))
    lines.append(str(len(kinds)) + " " + " ".join(str(k) for k in kinds))
    lines.append(str(len(vertices)))
    for v, g in zip(vertices, light):
        lines.append("%d %d %d %d" % (*v, g))
    lines.append(str(len(indices) // 4))
    for f in range(len(indices) // 4):
        lines.append(" ".join(str(i) for i in indices[4 * f:4 * f + 4]) + " %d" % face_materials[f])
    return "\n".join(lines) + "\n"


def parse(text):
    rows = text.split("\n")
    above, below, cut, uncut = (int(x) for x in rows[0].split()[1:])
    n = int(rows[1].split()[1])
    vertices, gouraud = [], []
    for row in rows[2:2 + n]:
        x, y, z, g = (int(v) for v in row.split())
        vertices.append((x, y, z))
        gouraud.append(g)
    f = int(rows[2 + n].split()[1])
    faces, materials = [], []
    for row in rows[3 + n:3 + n + f]:
        values = [int(v) for v in row.split()]
        faces.append(values[:4])
        materials.append(values[4])
    return dict(vertices=vertices, gouraud=gouraud, faces=faces, face_materials=materials,
                above_faces=above, below_faces=below, cut_faces=cut, uncut_faces=uncut)


@unittest.skipUnless(compiler(), "no C++ compiler")
class WaterPrepareParityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.dir.name) / "water_prepare_dump"
        subprocess.run([compiler(), "-std=c++20", "-O1", "-I", str(ROOT / "include"), "-iquote", str(ROOT),
                        *[str(ROOT / s) for s in SOURCES], "-o", str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_case(self, regions, tint, full_depth, kinds, vertices, light, indices, face_materials):
        text = format_input(regions, tint, full_depth, kinds, vertices, light, indices, face_materials)
        done = subprocess.run([str(self.binary)], input=text, capture_output=True, text=True, check=True)
        want = parse(done.stdout)
        got = water_prepare.prepare_mesh(vertices, indices, face_materials, [k == 0 for k in kinds],
                                         light, regions, tint, full_depth)
        for key in want:
            self.assertEqual(got[key], want[key], key)
        return want

    def test_random_levels_match_the_c_implementation(self):
        rng = random.Random(64)
        saw_cut = saw_uncut = False
        for case in range(60):
            regions = []
            for _ in range(rng.randint(0, 3)):
                x0, z0 = rng.randint(-20, 10) * ONE, rng.randint(-20, 10) * ONE
                bottom = rng.randint(-12, -3) * ONE
                regions.append(dict(min_x=x0, max_x=x0 + rng.randint(5, 30) * ONE, min_z=z0,
                                    max_z=z0 + rng.randint(5, 30) * ONE, bottom=bottom,
                                    surface=bottom + rng.randint(2, 12) * ONE + rng.randint(0, 999)))
            vertices, indices, materials = [], [], []
            for _ in range(rng.randint(4, 40)):
                x, z = rng.randint(-24, 24) * ONE + rng.randint(-999, 999), rng.randint(-24, 24) * ONE
                base_y = rng.randint(-14, 6) * ONE + rng.randint(-999, 999)
                spread = rng.choice((0, ONE, 3 * ONE, 9 * ONE))
                w, d = rng.randint(1, 6) * ONE, rng.randint(1, 6) * ONE
                first = len(vertices)
                for dx, dz in ((0, 0), (w, 0), (w, d), (0, d)):
                    vertices.append((x + dx, base_y + rng.randint(0, spread) if spread else base_y, z + dz))
                indices += [first, first + 1, first + 2, first + 3]
                materials.append(rng.choice((0, 0, 0, 1)))
            light = [rng.choice((0x4210, 0x4210, rng.randint(0, 0x7FFF))) for _ in vertices]
            tint = [rng.randint(-12, 4) for _ in range(3)]
            want = self.run_case(regions, tint, rng.randint(1, 8) * ONE + rng.randint(0, 99), [0, 2],
                                 vertices, light, indices, materials)
            saw_cut |= want["cut_faces"] > 0
            saw_uncut |= want["uncut_faces"] > 0
        self.assertTrue(saw_cut and saw_uncut, "the random cases should exercise both paths")

    def test_shared_vertices_stay_shared_without_a_cut(self):
        vertices = [(0, -6 * ONE, 0), (ONE, -6 * ONE, 0), (ONE, -6 * ONE, ONE), (0, -6 * ONE, ONE),
                    (2 * ONE, -6 * ONE, 0), (2 * ONE, -6 * ONE, ONE)]
        indices = [0, 1, 2, 3, 1, 4, 5, 2]
        region = [dict(min_x=-ONE, max_x=9 * ONE, min_z=-ONE, max_z=9 * ONE, bottom=-9 * ONE, surface=0)]
        want = self.run_case(region, [-8, -3, 2], 4 * ONE, [0], vertices, [0x4210] * 6, indices, [0, 0])
        # Untouched vertices stay shared; the below faces are listed in reverse emission order.
        self.assertEqual((want["vertices"], want["faces"]), (vertices, [[1, 4, 5, 2], [0, 1, 2, 3]]))


if __name__ == "__main__":
    unittest.main()
