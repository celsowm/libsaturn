r"""Acceptance for the VDP2 neon city (E12).

    .\harness\run-vdp2-neon-city.ps1 -Bios .\bios\saturn_bios_us.bin
    .\harness\run-vdp2-neon-city.ps1 -Bios .\bios\saturn_bios_us.bin -Long

The scene integrates the plan's final slice: a rotating RBG0 neon sign, a
64x64-cell street streamed through two pattern-name rings, a colour-
calculation window around a moving light pool with the line-colour screen,
per-layer colour offsets on the signs and rain, and the four NBGs around them.
The test checks the public API verdicts, the stream model/VRAM CRC, the ring
and layer registers, and the live picture.

The streaming CRC is the example's own model-vs-VRAM comparison (recomputed
every 60 fields); the probe dump is kept for inspection. Emulator evidence
only; physical hardware remains PENDING.
"""

import json
import os
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x4E454F31  # "NEO1"
FIELDS = (
    "magic", "init_ok", "layers_ok", "rotation_ok", "stream_ok",
    "window_ok", "color_offset_ok", "refused_base", "refused_window",
    "frames", "switches", "active_ring", "camera_x", "win_x0", "win_y0",
    "win_cols", "win_rows", "sign_angle", "bloom_x", "staged_words",
    "committed_words", "max_pending_words", "crc_expected", "crc_vram",
    "crc_mismatches", "sentinel_cells", "dma_scu", "dma_cpu",
    "update_us_max", "vdp1_commands",
)
LONG = os.environ.get("LIBSATURN_NEONCITY_LONG") == "1"
SKY = (8, 16, 72)
CYAN = (24, 224, 248)
GOLD = (144, 176, 48)
ROAD = (64, 72, 112)


def _near(a, b, tol=10):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class NeonCityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise unittest.SkipTest("run harness/run-vdp2-neon-city.ps1 first")
        cls.shots = os.environ.get("LIBSATURN_NEONCITY_SHOTS", "")
        cls.images = {}
        try:
            from PIL import Image
        except ImportError:
            return
        for name in ("early", "mid", "late"):
            path = os.path.join(cls.shots, name + ".png")
            if os.path.isfile(path):
                image = Image.open(path).convert("RGB")
                assert image.size == (320, 224), (name, image.size)
                cls.images[name] = image

    def test_setup_and_refusals(self):
        r = self.r
        for key in ("init_ok", "layers_ok", "rotation_ok", "stream_ok",
                    "window_ok", "color_offset_ok", "refused_base",
                    "refused_window"):
            self.assertEqual(r[key], 1, key)

    def test_streaming_session(self):
        r = self.r
        self.assertGreater(r["frames"], 1200 if LONG else 300)
        self.assertIn(r["active_ring"], (0, 1))
        self.assertLess(r["camera_x"], 16384)
        self.assertGreaterEqual(r["switches"], r["frames"] - 3)
        self.assertEqual(r["staged_words"], r["committed_words"])
        self.assertGreater(r["staged_words"], 0)
        self.assertLessEqual(r["max_pending_words"], 2970)
        self.assertEqual(r["crc_expected"], r["crc_vram"], "model and VRAM CRC differ")
        self.assertEqual(r["crc_mismatches"], 0)
        self.assertEqual(r["sentinel_cells"], 0, "a 0xFFFF canary cell is visible")
        self.assertGreater(r["dma_scu"], 0)
        self.assertGreater(r["dma_cpu"], 0)
        self.assertGreater(r["update_us_max"], 0)
        self.assertLess(r["update_us_max"], 60000)
        self.assertLess(r["vdp1_commands"], 16, "VDP1 should have no scene draws")
        self.assertLess(r["win_cols"], 64)
        self.assertLess(r["win_rows"], 64)

    def test_registers_reached_the_hardware(self):
        path = os.environ.get("LIBSATURN_NEONCITY_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            doc = json.load(stream)
        regs = doc["vdp2"]
        self.assertEqual(regs["bgon"] & 0x001F, 0x001F,
                         "NBG0-3 and RBG0 must all be enabled")
        self.assertEqual(regs["prina"] & 0x0007, 6, "NBG0 priority")
        self.assertEqual((regs["prina"] >> 8) & 0x0007, 4, "NBG1 priority")
        self.assertEqual(regs["prinb"] & 0x0007, 2, "NBG2 priority")
        self.assertEqual((regs["prinb"] >> 8) & 0x0007, 1, "NBG3 priority")
        self.assertIn(regs["mpabn1"], (0x2020, 0x2121))
        self.assertEqual(regs["mpcdn1"], regs["mpabn1"])
        self.assertEqual(regs["mpofn"] & 0x000F, 0)
        self.assertEqual(regs["rpmd"], 0, "the sign uses parameter set A")
        self.assertTrue(regs["ccctl"] & 0x0002, "add-mode colour calculation")
        self.assertTrue(regs["lnclen"] & 0x0002, "line colour under NBG1")

    def test_visual_integration(self):
        for name in ("early", "mid", "late"):
            self.assertIn(name, self.images, "missing %s capture" % name)
        early, mid, late = self.images["early"], self.images["mid"], self.images["late"]
        self.assertNotEqual(early.tobytes(), mid.tobytes(), "the city did not animate")
        self.assertNotEqual(mid.tobytes(), late.tobytes(), "the city did not animate")
        self.assertTrue(_near(late.getpixel((300, 40)), SKY, 12),
                        "sky: %r" % (late.getpixel((300, 40)),))
        colors = late.getcolors(1 << 16) or []
        cyan = sum(n for n, c in colors if _near(c, CYAN, 12))
        gold = sum(n for n, c in colors if _near(c, GOLD, 12))
        road = sum(n for n, c in colors if _near(c, ROAD, 8))
        self.assertGreater(cyan, 300, "no cyan neon")
        self.assertGreater(gold, 200, "no sign/window gold")
        self.assertGreater(road, 2000, "no street surface")
        # The moving colour-calculation window makes the pool bright on both
        # captures, while the sign/ground move around it.
        pool = ((128, 255, 255), (64, 184, 232), (88, 216, 255),
                (255, 152, 255), (255, 120, 255), (24, 144, 192))
        samples = [(160, 160), (160, 170), (160, 180), (160, 190), (120, 170), (200, 170)]
        hit = any(_near(image.getpixel(point), color, 18)
                  for image in (mid, late) for point in samples for color in pool)
        self.assertTrue(hit, "the colour-calculation light pool is missing")

    def test_no_misaligned_accesses(self):
        path = os.path.join(self.shots, "misaligned.txt")
        if not os.path.isfile(path):
            self.skipTest("no misaligned report")
        with open(path, "r", encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertTrue(lines and lines[0].endswith("(first access per PC; totals: master 0, slave 0)"),
                        "\n".join(lines[:20]))

    def test_probe_never_shot(self):
        self.assertFalse(os.path.exists(os.path.join(self.shots, "never.png")))


def _load():
    dump = os.environ.get("LIBSATURN_NEONCITY_DUMP")
    map_path = os.environ.get("LIBSATURN_NEONCITY_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-neon-city.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_neon_city",
                        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
