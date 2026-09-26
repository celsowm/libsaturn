r"""Acceptance for the VDP2 layer manager (examples/vdp2_layers_demo).

Run through the wrapper, which builds the demo, runs Ymir, and dumps Work RAM
and screenshots:

    .\harness\run-vdp2-layers.ps1 -Bios .\bios\saturn_bios_us.bin

Four scroll screens (NBG0-NBG3) share the VRAM banks; the library plans the
cycle pattern. The struct read here is g_layers_demo.
"""

import os
import re
import struct
import unittest

MAGIC = 0x4C594531  # "LYE1"
WRAM_HIGH = 0x06000000
# Names in A0 (NBG0, NBG1) and A1 (NBG2, NBG3), characters in B0 and B1: each
# layer reads its names at T0 or T1 and its characters at the same timing in
# the other bank, timings 2 free (no access) and 3.. left to the CPU.
EXPECTED_CYCLE = [0x01FE, 0xEEEE, 0x23FE, 0xEEEE, 0x45FE, 0xEEEE, 0x67FE, 0xEEEE]

YELLOW = (248, 224, 24)   # NBG0 bars, in front
RED = (248, 48, 48)       # NBG1 bars
GREEN = (48, 224, 64)     # NBG2 checker
BLUE = (24, 48, 160)      # NBG3 backdrop grid


def _load():
    dump = os.environ.get("LIBSATURN_LAYERS_DUMP")
    map_path = os.environ.get("LIBSATURN_LAYERS_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-layers.ps1 first")
    if not os.path.exists(dump) or not os.path.exists(map_path):
        raise AssertionError("missing input: %s / %s" % (dump, map_path))
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_layers_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_layers_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        raw = struct.unpack(">20I", stream.read()[offset:offset + 80])
    return {
        "magic": raw[0],
        "configure": [struct.unpack(">i", struct.pack(">I", v))[0] for v in raw[1:5]],
        "cycle": list(raw[5:13]),
        "refused_too_much": raw[13],
        "refused_busy": raw[14],
        "frames": raw[15],
    }


def _colours(path):
    from PIL import Image
    counts = {}
    for pixel in Image.open(path).convert("RGB").getdata():
        counts[pixel] = counts.get(pixel, 0) + 1
    return counts


class Vdp2LayersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_layers_demo not initialised: %r" % cls.r)
        cls.shots = os.environ.get("LIBSATURN_LAYERS_SHOTS", "")

    def test_every_layer_was_accepted(self):
        self.assertEqual(self.r["configure"], [0, 0, 0, 0])
        self.assertGreater(self.r["frames"], 150)

    def test_the_allocator_planned_the_expected_cycle_pattern(self):
        self.assertEqual(self.r["cycle"], EXPECTED_CYCLE,
                         " ".join("%04X" % v for v in self.r["cycle"]))

    def test_a_layer_that_cannot_be_fetched_is_refused_and_changes_nothing(self):
        self.assertEqual(self.r["refused_too_much"], 1)

    def test_the_old_nbg0_api_is_refused_while_layers_are_managed(self):
        self.assertEqual(self.r["refused_busy"], 1)

    def test_all_four_layers_are_on_screen(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest("Pillow is not installed")
        path = os.path.join(self.shots, "f120.png")
        self.assertTrue(os.path.exists(path), path)
        counts = _colours(path)
        for name, colour in (("NBG0 yellow", YELLOW), ("NBG1 red", RED),
                             ("NBG2 green", GREEN), ("NBG3 blue", BLUE)):
            self.assertGreater(counts.get(colour, 0), 1500, "%s missing: %r" % (name, colour))

    def test_layers_scroll_between_frames(self):
        try:
            from PIL import Image, ImageChops
        except ImportError:
            self.skipTest("Pillow is not installed")
        a = Image.open(os.path.join(self.shots, "f60.png")).convert("RGB")
        b = Image.open(os.path.join(self.shots, "f120.png")).convert("RGB")
        self.assertIsNotNone(ImageChops.difference(a, b).getbbox())


if __name__ == "__main__":
    unittest.main()
