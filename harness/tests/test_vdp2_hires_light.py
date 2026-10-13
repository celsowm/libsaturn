r"""Acceptance for a red light on hi-res VDP1 polygons (examples/vdp2_hires_light).

Run through the wrapper, which builds the demo, runs Ymir, and dumps Work RAM and two screenshots:

    .\harness\run-vdp2-hires-light.ps1 -Bios .\bios\saturn_bios_us.bin

640x224, 8-bit VDP1 codes. The body is 30 columns of a 15-step ramp (code k, or 128 + k to take the
light) with a stripe at y 84..103 in the plain codes. The VDP2 adds a per-line red (line colour screen
inserted under the sprite layer, add-mode colour calculation) inside an ellipse window centred on
(200, 80), radii 90 x 60, which holds until frame 240 and then sweeps. a.png is taken during the hold,
b.png during the sweep. The struct read here is g_light_demo.

The map/dump loader lives in harness/scripts/telemetry.py so every
test in this directory uses the same WRAM_HIGH constant and symbol
regex. The struct read here still uses the '>IIIiiIII' big-endian
format that the SH-2 emits directly; the C struct definition lives in
examples/vdp2_hires_light/main.c.
"""

import os
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x4C495431  # "LIT1"
BACK = (1 << 3, 2 << 3, 6 << 3)


def _load():
    dump = os.environ.get("LIBSATURN_LIGHT_DUMP")
    map_path = os.environ.get("LIBSATURN_LIGHT_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-hires-light.ps1 first")
    # '>IIIiiIII' = magic (u32), setup_ok (u32), frames (u32),
    # spot_x (i32), spot_y (i32), light_on (u32), mask_on (u32),
    # window_on (u32). The two signed i32s sit in the middle so the
    # struct layout cannot drift without breaking this loader.
    v = tel.load_dump(dump, map_path, "_g_light_demo", ">IIIiiIII")
    return {"magic": v[0], "setup_ok": v[1], "frames": v[2], "spot_x": v[3], "spot_y": v[4],
            "light_on": v[5], "mask_on": v[6], "window_on": v[7]}


def _ramp(k):
    """The demo's ramp_color(k) as 8-bit RGB."""
    return ((5 + k) << 3, (3 + (k * 2) // 3) << 3, (2 + k // 3) << 3)


def _close(a, b, tolerance=10):
    return all(abs(x - y) <= tolerance for x, y in zip(a, b))


class Vdp2HiresLightTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_light_demo not initialised: %r" % cls.r["magic"])
        from PIL import Image
        shots = os.environ["LIBSATURN_LIGHT_SHOTS"]
        cls.images = {}
        for name in ("a", "b"):
            image = Image.open(os.path.join(shots, name + ".png")).convert("RGB")
            sx, sy = image.size[0] // 640, image.size[1] // 224
            if sx not in (1, 2) or sy not in (1, 2):
                raise AssertionError("not a 640x224 picture: %r" % (image.size,))
            cls.images[name] = (image, sx, sy)

    def pixel(self, name, x, y):
        image, sx, sy = self.images[name]
        return image.getpixel((x * sx, y * sy))

    def test_setup_was_accepted(self):
        self.assertEqual(self.r["setup_ok"], 1)
        self.assertEqual((self.r["light_on"], self.r["mask_on"], self.r["window_on"]), (1, 1, 1))
        self.assertGreater(self.r["frames"], 300)

    def test_outside_the_spot_is_the_plain_ramp(self):
        # columns 7 and 22 both hold code 8 (+128); x 440 is outside the spot during the hold
        for y in (40, 60, 130, 180):
            self.assertTrue(_close(self.pixel("a", 440, y), _ramp(8)), (y, self.pixel("a", 440, y)))

    def test_lit_codes_inside_the_spot_turn_red(self):
        lit, ref = self.pixel("a", 200, 60), self.pixel("a", 440, 60)
        self.assertGreaterEqual(lit[0] - ref[0], 80, (lit, ref))
        self.assertTrue(_close(lit[2:], ref[2:]), (lit, ref))  # no blue added

    def test_light_fades_away_from_the_centre_line(self):
        near = self.pixel("a", 200, 60)[0] - self.pixel("a", 440, 60)[0]
        far = self.pixel("a", 200, 130)[0] - self.pixel("a", 440, 130)[0]
        self.assertGreater(far, 16)
        self.assertLess(far, near)

    def test_plain_codes_are_not_lit(self):
        for x in (160, 200, 240):
            k = 1 + ((x - 80) // 16) % 15
            self.assertTrue(_close(self.pixel("a", x, 94), _ramp(k)), (x, self.pixel("a", x, 94)))

    def test_background_inside_the_spot_is_not_lit(self):
        for x in (150, 200, 250):
            self.assertTrue(_close(self.pixel("a", x, 24), BACK), (x, self.pixel("a", x, 24)))

    def test_the_spot_moves(self):
        self.assertGreater(self.r["spot_x"], 285)
        self.assertTrue(_close(self.pixel("b", 200, 60), _ramp(8)), self.pixel("b", 200, 60))


if __name__ == "__main__":
    unittest.main()
