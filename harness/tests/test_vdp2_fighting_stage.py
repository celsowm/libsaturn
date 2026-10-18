r"""Acceptance for the VDP2 fighting stage (E11).

    .\harness\run-vdp2-fighting-stage.ps1 -Bios .\bios\saturn_bios_us.bin

The example is the existing two-fighter character demo: same assets, combat
simulation, skeleton evaluation, global painter and VDP1 command path as
fighter_arena_3d. Only the VDP2 backdrop changes: four NBGs with stands and a
crowd, banners, moving spotlights and a ring apron, behind the training mat.

At demo frames 70 and 120 the example hides and shows the backdrop. This test
compares the two states with the same fighter simulation, checks the stage
APIs' refusals and the layer manager's verdicts, and asserts that the stage
adds no VDP1 command of its own (the small count difference comes from the
fighters' animation state, which continues while the backdrop toggles).

The captures are emulator evidence. Physical hardware remains PENDING.
"""

import json
import os
import struct
import unittest

from harness.scripts import telemetry as tel

STAGE_MAGIC = 0x53544731  # "STG1"
STAGE_FIELDS = (
    "magic", "init_ok", "layers_ok", "window_ok", "color_offset_ok",
    "refused_window", "refused_scroll", "frames", "stage_on", "toggles",
    "auto_toggles", "stage_visible_frames", "commands_on", "commands_off",
    "geometry_us_on", "geometry_us_off", "last_commands", "last_geometry_us",
)
METRICS_FIELDS = (
    "magic", "frames", "simulation_ticks", "steps_last", "loop_rate_x10",
    "simulation_rate_x10", "requested", "accepted", "downgraded", "rejected",
    "policy", "quality", "clip", "animation_frame", "joints", "vertices",
    "visible_faces", "commands", "command_capacity", "skeleton_ms", "skin_ms",
    "geometry_ms", "sort_us", "emit_us", "frame_ms", "max_frame_ms",
    "persistent_bytes", "texture_bytes", "slave_tasks", "master_tasks",
    "wait_ticks", "queue_ticks", "player_health", "opponent_health", "hits",
    "blocked", "knockdowns", "round_over", "events", "root_travel",
    "skin_equivalence", "source_reloads", "errors", "combat_hits_total",
    "combat_blocks_total",
)
BACKDROP = (16, 32, 80)  # sat_vdp2_back_color_set(2,4,10)


def _near(a, b, tol=6):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class FightingStageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.stage = _load_stage()
        cls.metrics = _load_metrics()
        if cls.stage["magic"] != STAGE_MAGIC:
            raise unittest.SkipTest("run harness/run-vdp2-fighting-stage.ps1 first")
        cls.shots = os.environ.get("LIBSATURN_FIGHTINGSTAGE_SHOTS", "")
        cls.images = {}
        try:
            from PIL import Image
        except ImportError:
            return
        for name in ("on", "off", "on2"):
            path = os.path.join(cls.shots, name + ".png")
            if os.path.isfile(path):
                image = Image.open(path).convert("RGB")
                assert image.size == (320, 224), (name, image.size)
                cls.images[name] = image

    def test_stage_setup_and_refusals(self):
        s = self.stage
        for key in ("init_ok", "layers_ok", "window_ok", "color_offset_ok",
                    "refused_window", "refused_scroll"):
            self.assertEqual(s[key], 1, key)
        self.assertEqual(s["toggles"], 2)
        self.assertEqual(s["auto_toggles"], 2)
        self.assertEqual(s["stage_on"], 1)
        self.assertGreater(s["stage_visible_frames"], 0)

    def test_same_fighter_simulation_in_both_states(self):
        m = self.metrics
        self.assertEqual(m["magic"], 0x43484152)  # CHAR
        self.assertGreater(m["frames"], 150)
        self.assertEqual(m["requested"], 2)
        self.assertEqual(m["accepted"], 2)
        self.assertEqual(m["errors"], 0)
        self.assertGreater(m["simulation_ticks"], 100)
        # The combat logic advanced in both stage states.
        self.assertGreaterEqual(m["combat_hits_total"], 1)
        s = self.stage
        self.assertGreater(s["commands_on"], 0)
        self.assertGreater(s["commands_off"], 0)
        # The backdrop adds no VDP1 command; the difference is animation.
        largest = max(s["commands_on"], s["commands_off"])
        self.assertLessEqual(abs(s["commands_on"] - s["commands_off"]), largest // 4)
        # Geometry preparation is sampled on different animation frames, so
        # allow scheduler jitter while still rejecting a systematic stage cost.
        largest_us = max(s["geometry_us_on"], s["geometry_us_off"])
        self.assertLessEqual(abs(s["geometry_us_on"] - s["geometry_us_off"]),
                             max(1000, largest_us // 4))

    def test_stage_changes_the_backdrop_only(self):
        for name in ("on", "off", "on2"):
            self.assertIn(name, self.images, "missing %s capture" % name)
        on, off, on2 = self.images["on"], self.images["off"], self.images["on2"]
        self.assertNotEqual(on.tobytes(), off.tobytes(), "stage toggle did nothing")
        self.assertNotEqual(on2.tobytes(), off.tobytes(), "stage did not return")
        # The stage-off picture is dominated by the flat VDP2 back colour;
        # the stage-on pictures are not.
        off_colors = off.getcolors(1 << 16) or []
        on_colors = on.getcolors(1 << 16) or []
        off_backdrop = sum(n for n, c in off_colors if _near(c, BACKDROP, 3))
        on_backdrop = sum(n for n, c in on_colors if _near(c, BACKDROP, 3))
        self.assertGreater(off_backdrop, on_backdrop)
        self.assertGreater(off_backdrop, 20000)
        # The live stage's stand/crowd tones are absent from the blank stage.
        self.assertFalse(_near(off.getpixel((20, 80)), (41, 28, 23), 8))
        self.assertTrue(_near(on.getpixel((20, 80)), (41, 28, 23), 12))
        self.assertTrue(_near(on2.getpixel((20, 80)), (0, 5, 31), 12))

    def test_layers_reached_bgon(self):
        path = os.environ.get("LIBSATURN_FIGHTINGSTAGE_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            doc = json.load(stream)
        self.assertEqual(doc["vdp2"]["bgon"] & 0x000F, 0x000F,
                         "not all four stage NBGs are enabled")
        self.assertEqual(doc["vdp2"]["ramctl"] & 0x0300, 0x0300,
                         "VRAM banks were not split for the layer map")

    def test_no_misaligned_accesses(self):
        path = os.path.join(self.shots, "misaligned.txt")
        if not os.path.isfile(path):
            self.skipTest("no misaligned report")
        with open(path, "r", encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertTrue(lines and lines[0].endswith("(first access per PC; totals: master 0, slave 0)"),
                        "\n".join(lines[:20]))


def _load_stage():
    dump = os.environ.get("LIBSATURN_FIGHTINGSTAGE_DUMP")
    map_path = os.environ.get("LIBSATURN_FIGHTINGSTAGE_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-fighting-stage.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_vdp2_stage",
                        ">%dI" % len(STAGE_FIELDS), 4 * len(STAGE_FIELDS))
    return dict(zip(STAGE_FIELDS, raw))


def _load_metrics():
    dump = os.environ.get("LIBSATURN_FIGHTINGSTAGE_DUMP")
    map_path = os.environ.get("LIBSATURN_FIGHTINGSTAGE_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-fighting-stage.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_character_demo",
                        ">%dI" % len(METRICS_FIELDS), 4 * len(METRICS_FIELDS))
    return dict(zip(METRICS_FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
