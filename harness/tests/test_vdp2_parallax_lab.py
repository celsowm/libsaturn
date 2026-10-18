r"""Acceptance for the VDP2 Parallax Lab.

    .\harness\run-vdp2-parallax-lab.ps1 -Bios .\bios\saturn_bios_us.bin
    .\harness\run-vdp2-parallax-lab.ps1 -Bios .\bios\saturn_bios_us.bin -Long
    .\harness\run-vdp2-parallax-lab.ps1 -Bios .\bios\saturn_bios_eu.bin -Pal

Four hand-painted 512x256 layers pan at 8/24/52/100 percent: NBG3 sky (8bpp
cell map), NBG2 mountains (4bpp cell map), NBG1 sea (8bpp bitmap) and NBG0
foreground (8bpp bitmap).  The test checks the public API verdicts, the VDP2
register geometry, the uploaded maps/chars and the live picture.  Emulator
evidence only; physical hardware remains PENDING.
"""

import json
import os
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x504C4142  # "PLAB"
FIELDS = (
    "magic", "init_ok", "layers_ok", "foreground_ok", "sea_ok", "sky_ok",
    "mountain_ok", "frames", "pan", "sky_x", "mountain_x", "sea_x",
    "foreground_x", "paused", "update_us_last", "update_us_max",
    "vdp1_commands",
)
LONG = os.environ.get("LIBSATURN_PLAB_LONG") == "1"


class ParallaxLabTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise unittest.SkipTest("run harness/run-vdp2-parallax-lab.ps1 first")
        cls.shots = os.environ.get("LIBSATURN_PLAB_SHOTS", "")
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

    def test_setup_and_scroll_model(self):
        r = self.r
        for key in ("init_ok", "layers_ok", "foreground_ok", "sea_ok",
                    "sky_ok", "mountain_ok"):
            self.assertEqual(r[key], 1, key)
        self.assertEqual(r["paused"], 0)
        self.assertGreater(r["frames"], 2000 if LONG else 300)
        pan_fx = r["pan"]
        self.assertGreaterEqual(pan_fx, 0)
        self.assertLessEqual(pan_fx, 176 << 16)
        self.assertEqual(r["sky_x"], (pan_fx * 8) // 100)
        self.assertEqual(r["mountain_x"], (pan_fx * 24) // 100)
        self.assertEqual(r["sea_x"], (pan_fx * 65) // 100)
        self.assertEqual(r["foreground_x"], pan_fx)
        self.assertGreater(r["update_us_max"], 0)
        self.assertLess(r["update_us_max"], 60000)
        self.assertGreater(r["vdp1_commands"], 0)
        self.assertLess(r["vdp1_commands"], 128, "VDP1 should only draw the HUD")

    def test_registers_and_vram(self):
        path = os.environ.get("LIBSATURN_PLAB_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            doc = json.load(stream)
        regs = doc["vdp2"]
        self.assertEqual(regs["bgon"] & 0x000F, 0x000F,
                         "all four NBGs must be enabled")
        self.assertEqual(regs["bgon"] & 0x0800, 0x0800,
                         "the opaque sky must draw dot code 0")
        self.assertEqual(regs["prina"], 0x0406, "NBG0/NBG1 priorities")
        self.assertEqual(regs["prinb"], 0x0102, "NBG2/NBG3 priorities")
        self.assertEqual(regs["plsz"], 0, "all planes are 1x1 pages")
        self.assertEqual(regs["mpofn"] & 0x00F0, 0x0010,
                         "NBG1 bitmap base must be 0x20000")
        self.assertEqual(regs["mpabn2"], 0x1010)
        self.assertEqual(regs["mpcdn2"], 0x1010)
        self.assertEqual(regs["mpabn3"], 0x1111)
        self.assertEqual(regs["mpcdn3"], 0x1111)
        self.assertTrue(regs["chctlb"] & 0x0020, "NBG3 must be 256 colours")
        self.assertFalse(regs["chctlb"] & 0x0002, "NBG2 must be 16 colours")

        ranges = {entry["base_word"]: entry["words"]
                  for entry in regs.get("vram_ranges", [])}
        foreground = ranges.get(0)
        self.assertIsNotNone(foreground)
        self.assertTrue(any(foreground), "foreground bitmap rows are empty")
        mountain_map = ranges.get(131072)  # 0x20000
        self.assertIsNotNone(mountain_map)
        self.assertEqual(mountain_map[:4], [0x0004, 0x2401, 0x0004, 0x2402])
        sky_map = ranges.get(139264)  # 0x22000
        self.assertIsNotNone(sky_map)
        self.assertEqual(sky_map[:4], [0x0020, 0x3000, 0x0020, 0x3002])
        sky_chars = ranges.get(196608)  # 0x30000
        self.assertIsNotNone(sky_chars)
        self.assertTrue(any(sky_chars), "sky character data is empty")

    def test_visual_layers(self):
        for name in ("early", "mid", "late"):
            self.assertIn(name, self.images, "missing %s capture" % name)
        early, mid, late = self.images["early"], self.images["mid"], self.images["late"]
        self.assertNotEqual(early.tobytes(), mid.tobytes(), "the pan did not advance")
        self.assertNotEqual(mid.tobytes(), late.tobytes(), "the pan did not advance")
        for name, image in (("early", early), ("late", late)):
            colors = image.getcolors(1 << 16) or []
            sky = sum(n for n, c in colors
                      if c[2] > 150 and c[2] > c[1] * 1.4 and c[0] < 80)
            sea = sum(n for n, c in colors
                      if c[2] > 120 and c[2] > c[1] * 1.1 and c[0] < 110)
            green = sum(n for n, c in colors
                        if c[1] > 60 and c[1] > c[0] * 1.3 and c[1] > c[2] * 0.7)
            self.assertGreater(sky, 5000, "%s: no blue sky" % name)
            self.assertGreater(sea, 7000, "%s: no sea" % name)
            self.assertGreater(green, 1200, "%s: no foreground foliage" % name)
        sky_pixel = late.getpixel((300, 20))
        self.assertTrue(sky_pixel[2] > 150 and sky_pixel[2] > sky_pixel[1] * 1.3,
                        "late sky sample: %r" % (sky_pixel,))

    def test_no_misaligned_accesses(self):
        path = os.path.join(self.shots, "misaligned.txt")
        if not os.path.isfile(path):
            self.skipTest("no misaligned report")
        with open(path, "r", encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertTrue(lines and lines[0].endswith(
            "(first access per PC; totals: master 0, slave 0)"), "\n".join(lines[:20]))

    def test_long_run_reached_2400_fields(self):
        if not LONG:
            self.skipTest("run with -Long")
        self.assertGreater(self.r["frames"], 2000)


def _load():
    dump = os.environ.get("LIBSATURN_PLAB_DUMP")
    map_path = os.environ.get("LIBSATURN_PLAB_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-parallax-lab.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_parallax_lab",
                        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
