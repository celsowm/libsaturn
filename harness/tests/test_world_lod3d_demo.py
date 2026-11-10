"""Ymir acceptance checks for the procedural World LOD outdoor scene.

Run harness/run-world-lod3d-demo.sh to produce the BIOS-backed capture first.
"""

import os
import re
import unittest
from pathlib import Path

from harness.scripts import telemetry as tel

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "examples/world_lod3d_demo/world_lod3d_demo.h"
MAGIC = 0x574C4F44


def _field_names():
    text = HEADER.read_text(encoding="utf-8")
    body = re.search(
        r"typedef struct world_lod3d_demo_telemetry \{(.*?)\} "
        r"world_lod3d_demo_telemetry_t;", text, re.S)
    if body is None:
        raise AssertionError("world_lod3d_demo_telemetry_t not found")
    fields = []
    for line in body.group(1).splitlines():
        line = re.sub(r"/\*.*?\*/", "", line).split("//")[0].strip()
        match = re.match(r"uint32_t\s+(.+);", line)
        if match:
            fields.extend(name.strip() for name in match.group(1).split(","))
    return fields


class WorldLod3dDemoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        dump = os.environ.get("LIBSATURN_WORLDLOD_DUMP")
        map_path = os.environ.get("LIBSATURN_WORLDLOD_MAP")
        if not dump or not map_path:
            raise unittest.SkipTest("run harness/run-world-lod3d-demo.sh first")
        cls.fields = _field_names()
        values = tel.load_dump(dump, map_path, "_g_world_lod3d_demo",
                               ">%dI" % len(cls.fields), 4 * len(cls.fields))
        cls.data = dict(zip(cls.fields, values))

    def test_planner_produced_a_bounded_multilevel_scene(self):
        d = self.data
        self.assertEqual(d["magic"], MAGIC)
        self.assertGreater(d["frames"], 100)
        self.assertEqual(d["planner_status"], 0)
        self.assertGreater(sum(d[f"wanted_lod{i}"] for i in range(3)), 0)
        self.assertGreater(sum(d[f"selected_lod{i}"] for i in range(3)), 0)
        self.assertLessEqual(d["faces"], d["face_budget"])
        self.assertLessEqual(d["commands"], d["command_budget"])
        self.assertEqual(d["no_resident"], 0)
        self.assertGreater(d["frustum_rejected"], 0)
        self.assertGreater(d["hlod_draws"], 0)
        self.assertGreater(d["hlod_faces"], 0)
        self.assertGreaterEqual(d["hlod_max_draws"], 4)
        self.assertGreater(d["hlod_missing_lod_test_frames"], 0)
        self.assertGreater(d["hlod_fallbacks"], 0)
        self.assertEqual(d["hlod_lod0_missing"], 0)

    def test_route_exercised_reset_fade_governor_and_profile(self):
        d = self.data
        self.assertGreaterEqual(d["view_generation"], 3)
        self.assertEqual(d["yaw_degrees"], 0)
        self.assertEqual(d["pitch_degrees"], 0)
        self.assertEqual(d["camera_x_fx"], 0)
        self.assertEqual(d["camera_z_fx"], 0)
        self.assertEqual(d["fade_enabled"], 1)
        self.assertGreater(d["faded_objects"], 0)
        self.assertEqual(d["governor_enabled"], 1)
        self.assertGreater(d["governor_samples"], 0)
        self.assertGreater(d["cpu_frt_ticks"], 0)
        self.assertGreaterEqual(d["elapsed_fields"], 1)
        self.assertEqual(
            d["field_hist_1"] + d["field_hist_2"] + d["field_hist_3"] +
            d["field_hist_4"] + d["field_hist_gt4"], d["frames"])
        self.assertGreater(d["cpu_frt_sum"], d["cpu_frt_ticks"])

    def test_camera_screenshots_show_motion(self):
        shots = os.environ.get("LIBSATURN_WORLDLOD_SHOTS")
        if not shots:
            self.skipTest("screenshots were not captured")
        try:
            from PIL import Image, ImageChops
        except ImportError:
            self.skipTest("Pillow is unavailable")
        first = Image.open(Path(shots) / "f30.png").convert("RGB")
        moved = Image.open(Path(shots) / "f150.png").convert("RGB")
        self.assertEqual(first.size, (320, 224))
        self.assertEqual(moved.size, (320, 224))
        diff = ImageChops.difference(first, moved).tobytes()
        changed = sum(1 for i in range(0, len(diff), 3)
                      if diff[i] or diff[i + 1] or diff[i + 2])
        self.assertGreater(changed, 500)
        colors = first.crop((0, 24, 320, 196)).getcolors(maxcolors=1_000_000)
        self.assertIsNotNone(colors)
        self.assertGreater(len(colors), 12)

    def test_fade_toggle_changes_objects_at_a_stationary_camera(self):
        shots = os.environ.get("LIBSATURN_WORLDLOD_SHOTS")
        if not shots:
            self.skipTest("screenshots were not captured")
        try:
            from PIL import Image, ImageChops
        except ImportError:
            self.skipTest("Pillow is unavailable")
        opaque = Image.open(Path(shots) / "f220.png").convert("RGB")
        faded = Image.open(Path(shots) / "f240.png").convert("RGB")
        self.assertEqual(opaque.size, (320, 224))
        diff = ImageChops.difference(opaque, faded).crop((0, 20, 320, 196)).tobytes()
        changed = sum(1 for i in range(0, len(diff), 3)
                      if diff[i] or diff[i + 1] or diff[i + 2])
        self.assertGreater(changed, 100)


if __name__ == "__main__":
    unittest.main()
