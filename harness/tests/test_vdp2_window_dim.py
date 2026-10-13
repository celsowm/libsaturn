r"""Acceptance for darkening limited to a window (examples/vdp2_window_dim).

Run through the wrapper, which builds the demo, runs Ymir, and dumps Work RAM and two screenshots:

    .\harness\run-vdp2-window-dim.ps1 -Bios .\bios\saturn_bios_us.bin

NBG1 is seven flat colour bands; NBG3 is a black layer with colour calculation, shown only outside
two line windows (the beams). Inside a beam the picture is untouched; outside it shows `ratio + 1`
32nds of the band's colour. a.png is taken with half showing through, b.png (after frame 90) a quarter.
The struct read here is g_dim_demo, which also holds the final span tables.
"""

import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x44494D31  # "DIM1"
LINES = 224
HEADER_WORDS = 11
STRUCT_BYTES = HEADER_WORDS * 4 + 2 * LINES * 2 * 2
BANDS = [(31, 6, 6), (31, 20, 4), (30, 30, 6), (8, 28, 8), (6, 26, 28), (10, 12, 31), (26, 8, 28)]


def _load():
    dump = os.environ.get("LIBSATURN_DIM_DUMP")
    map_path = os.environ.get("LIBSATURN_DIM_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-window-dim.ps1 first")
    # Header: 11 unsigned 32-bit words; the rest is 2 * 2 short spans per line.
    head = tel.load_dump(dump, map_path, "_g_dim_demo", ">%dI" % HEADER_WORDS, HEADER_WORDS * 4)
    spans = tel.load_dump(
        dump, map_path, "_g_dim_demo",
        ">%dH" % (2 * LINES * 2), 2 * LINES * 2 * 2,
        offset=HEADER_WORDS * 4)
    return {
        "magic": head[0], "layers_ok": head[1], "window_ok": head[2], "screens_ok": head[3],
        "color_calc_ok": head[4], "ratio_a": head[5], "ratio_b": head[6], "frames": head[7],
        "centre100": [struct.unpack(">i", struct.pack(">I", v))[0] for v in head[8:11]],
        "spans": [[(spans[(w * LINES + y) * 2], spans[(w * LINES + y) * 2 + 1]) for y in range(LINES)]
                  for w in range(2)],
    }


def _band_rgb(y):
    r, g, b = BANDS[(y // 32) % 7]
    return (r << 3, g << 3, b << 3)


def _close(a, b, tolerance=12):
    return all(abs(x - y) <= tolerance for x, y in zip(a, b))


class Vdp2WindowDimTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_dim_demo not initialised: %r" % cls.r["magic"])
        from PIL import Image
        shots = os.environ["LIBSATURN_DIM_SHOTS"]
        cls.images = {}
        for name in ("a", "b"):
            image = Image.open(os.path.join(shots, name + ".png")).convert("RGB")
            scale = image.size[0] // 320
            if scale not in (1, 2):
                raise AssertionError("unexpected screenshot size %r" % (image.size,))
            cls.images[name] = (image, scale)

    def pixel(self, name, x, y):
        image, scale = self.images[name]
        return image.getpixel((x * scale, y * scale))

    def _samples(self):
        """(x, y, inside) points well away from every beam edge, the band joins and the text."""
        spans = self.r["spans"]
        for y in range(8, 190, 5):
            if y % 32 < 3 or y % 32 > 28:
                continue
            edges = [v for w in range(2) if spans[w][y][0] <= spans[w][y][1] for v in spans[w][y]]
            for x in range(4, 316, 7):
                if any(abs(x - e) < 6 for e in edges):
                    continue
                inside = any(spans[w][y][0] <= x <= spans[w][y][1] for w in range(2))
                yield x, y, inside

    def test_every_call_was_accepted(self):
        for key in ("layers_ok", "window_ok", "screens_ok", "color_calc_ok"):
            self.assertEqual(self.r[key], 1, key)
        self.assertGreater(self.r["frames"], 100)

    def test_beams_rotate_then_hold(self):
        c = self.r["centre100"]
        self.assertLess(c[0], c[1])
        self.assertLess(c[1], c[2])

    def test_inside_the_beams_is_not_dimmed(self):
        for name in ("a", "b"):
            wrong = [(x, y, self.pixel(name, x, y), _band_rgb(y))
                     for x, y, inside in self._samples()
                     if inside and not _close(self.pixel(name, x, y), _band_rgb(y))]
            self.assertEqual(wrong, [], "%s: %r" % (name, wrong[:6]))

    def _check_outside(self, name, ratio_register):
        below = (ratio_register + 1) / 32.0
        wrong = []
        for x, y, inside in self._samples():
            if inside:
                continue
            expected = tuple(round(c * below) for c in _band_rgb(y))
            if not _close(self.pixel(name, x, y), expected, 14):
                wrong.append((x, y, self.pixel(name, x, y), expected))
        self.assertEqual(wrong, [], "%s: %r" % (name, wrong[:6]))

    def test_outside_is_dimmed_by_the_ratio(self):
        self._check_outside("a", self.r["ratio_a"])

    def test_the_ratio_change_darkens_further(self):
        self._check_outside("b", self.r["ratio_b"])
        self.assertLess(self.r["ratio_b"], self.r["ratio_a"])

    def test_both_areas_exist(self):
        kinds = {sample[2] for sample in self._samples()}
        self.assertEqual(kinds, {True, False})


if __name__ == "__main__":
    unittest.main()
