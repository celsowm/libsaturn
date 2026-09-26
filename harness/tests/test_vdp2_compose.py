r"""Acceptance for VDP2 windows, mosaic and colour calculation
(examples/vdp2_compose_demo).

    .\harness\run-vdp2-compose.ps1 -Bios .\bios\saturn_bios_us.bin

NBG0 yellow bars shown inside an ellipse (line window W0) and outside a
rectangle (W1) with AND logic; NBG2 green checker shown only inside W1; NBG3
blue cells with an 8x8 mosaic; NBG1 red stripes with 15:17 colour calculation.
Judged on the screenshot. The struct read is g_compose_demo.
"""

import os
import re
import struct
import unittest

MAGIC = 0x434D5031  # "CMP1"
WRAM_HIGH = 0x06000000
YELLOW = (248, 224, 24)
GREEN = (48, 224, 64)
RED = (248, 48, 48)
ELLIPSE = (100, 128, 90, 70)        # cx, cy, rx, ry
RECT = (176, 48, 311, 207)          # x0, y0, x1, y1
FIELDS = ("magic", "layers_ok", "window_ok", "screens_ok", "mosaic_ok", "color_calc_ok",
          "refused_bad_window", "refused_undefined", "refused_mosaic_size",
          "refused_mosaic_on_vcs", "refused_vcs_on_mosaic", "cc_window_ok", "frames")


def _load():
    dump = os.environ.get("LIBSATURN_COMPOSE_DUMP")
    map_path = os.environ.get("LIBSATURN_COMPOSE_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-compose.ps1 first")
    if not os.path.exists(dump) or not os.path.exists(map_path):
        raise AssertionError("missing input: %s / %s" % (dump, map_path))
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_compose_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_compose_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        raw = struct.unpack(">%dI" % len(FIELDS), stream.read()[offset:offset + 4 * len(FIELDS)])
    return dict(zip(FIELDS, raw))


def _near(a, b, tol=24):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class Vdp2ComposeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_compose_demo not initialised: %r" % cls.r)
        from PIL import Image
        shots = os.environ.get("LIBSATURN_COMPOSE_SHOTS", "")
        cls.img = Image.open(os.path.join(shots, "c.png")).convert("RGB")
        cls.pix = cls.img.load()
        cls.img2 = Image.open(os.path.join(shots, "c2.png")).convert("RGB")
        cls.pix2 = cls.img2.load()
        assert cls.img.size == (320, 224), cls.img.size

    def in_ellipse(self, x, y, grow=1.0):
        cx, cy, rx, ry = ELLIPSE
        return ((x - cx) / (rx * grow)) ** 2 + ((y - cy) / (ry * grow)) ** 2 <= 1.0

    def test_calls_succeeded_and_bad_arguments_were_refused(self):
        r = self.r
        for key in FIELDS[1:-1]:
            if key != "cc_window_ok":
                self.assertEqual(r[key], 1, key)
        self.assertEqual(r["cc_window_ok"], 1)
        self.assertGreater(r["frames"], 250)

    def test_line_window_shows_bars_only_inside_the_ellipse_and_left_of_the_rectangle(self):
        yellow = [(x, y) for y in range(40, 224) for x in range(320) if _near(self.pix[x, y], YELLOW)]
        self.assertGreater(len(yellow), 800)
        for x, y in yellow:
            self.assertTrue(self.in_ellipse(x, y, 1.05), "yellow outside the ellipse at %d,%d" % (x, y))
            self.assertLess(x, RECT[0], "yellow inside the rectangle at %d,%d" % (x, y))
        # bar at x=80: tall (the ellipse is tall there), bar at x=161: short (near its edge)
        self.assertTrue(_near(self.pix[81, 128], YELLOW) and _near(self.pix[81, 72], YELLOW))
        self.assertFalse(_near(self.pix[81, 44], YELLOW) or _near(self.pix[81, 212], YELLOW))
        self.assertTrue(_near(self.pix[161, 128], YELLOW))
        self.assertFalse(_near(self.pix[161, 60], YELLOW) or _near(self.pix[161, 200], YELLOW))
        # the bar at x=200 lies in the rectangle, where NBG0 is hidden
        self.assertFalse(any(_near(self.pix[x, 128], YELLOW) for x in range(RECT[0], 320)))

    def test_rectangle_window_shows_the_checker_only_inside(self):
        inside = outside = 0
        for y in range(40, 224):
            for x in range(320):
                if _near(self.pix[x, y], GREEN, 20):
                    if RECT[0] <= x <= RECT[2] and RECT[1] <= y <= RECT[3]:
                        inside += 1
                    else:
                        outside += 1
        self.assertGreater(inside, 1500)
        self.assertEqual(outside, 0)

    def test_mosaic_makes_every_8x8_block_one_colour(self):
        colours = set()
        blocks = []
        for by in (40, 48, 208, 216):
            for bx in range(0, 320, 8):
                if by == 48 and bx >= RECT[0] - 8:
                    continue
                if by == 40 and bx >= 312:
                    continue
                blocks.append((bx, by))
        for bx, by in blocks:
            first = self.pix[bx, by]
            for dy in range(8):
                for dx in range(8):
                    self.assertTrue(_near(self.pix[bx + dx, by + dy], first, 3),
                                    "block %d,%d not flat at +%d,+%d" % (bx, by, dx, dy))
            colours.add(first)
        self.assertGreaterEqual(len(colours), 3)

    def test_colour_calculation_mixes_the_stripes_with_what_is_below(self):
        checked = 0
        for k in range(2, 6):
            for x in range(0, 7):
                top = self.pix[x, 32 * k + 1]
                below = self.pix[x, 32 * k + 5]    # same mosaic block, no stripe
                want = tuple((15 * r + 17 * b) // 32 for r, b in zip(RED, below))
                self.assertTrue(_near(top, want, 16), "x=%d k=%d got %r want %r" % (x, k, top, want))
                self.assertFalse(_near(top, RED, 24))
                checked += 1
        self.assertGreater(checked, 20)

    def test_colour_calculation_window_confines_the_blend_to_the_rectangle(self):
        # From frame 150 the blend applies only inside W1; screenshot two is later.
        for k in range(2, 6):
            for x in range(0, 7):
                self.assertTrue(_near(self.pix2[x, 32 * k + 1], RED, 6),
                                "stripe outside the window not opaque at x=%d k=%d: %r" % (x, k, self.pix2[x, 32 * k + 1]))
        mixed = 0
        for k in range(2, 5):
            for x in range(RECT[0] + 20, RECT[2] - 20):
                if x % 8 >= 4:     # the checker's green cell is directly below the stripe
                    want = tuple((15 * r + 17 * g) // 32 for r, g in zip(RED, GREEN))
                    self.assertTrue(_near(self.pix2[x, 32 * k + 1], want, 20),
                                    "x=%d k=%d got %r want %r" % (x, k, self.pix2[x, 32 * k + 1], want))
                    mixed += 1
        self.assertGreater(mixed, 100)


if __name__ == "__main__":
    unittest.main()
