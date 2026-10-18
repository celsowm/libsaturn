r"""Acceptance for the VDP2 parallax world (E10).

    .\harness\run-vdp2-parallax-world.ps1 -Bios .\bios\saturn_bios_us.bin
    .\harness\run-vdp2-parallax-world.ps1 -Bios .\bios\saturn_bios_us.bin -Long

The scene is four NBGs plus RBG0: a per-line dawn sky, clouds clipped by
window W0, mountains, the repeating RBG0 sea, a line-scrolled shore and
foreground leaves. Colour calculation runs in add mode with the line-colour
screen on the shore. Around frames 600-660 the example hides the VDP2 layers
and draws a pre-rendered 320x224 indexed8 background with one VDP1 sprite, so
the two paths can be compared with identical scene intent.

This test checks the public APIs' verdicts, the measured per-frame table
update cost against the one-time software build, both VDP1 command counts,
the two visibly different paths, and the live picture after the reference is
removed. It is emulator evidence; physical hardware remains PENDING.
"""

import json
import os
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x50415231  # "PAR1"
FIELDS = (
    "magic", "init_ok", "layers_ok", "rotation_ok", "back_ok",
    "line_color_ok", "window_ok", "refused_scroll", "refused_window",
    "frames", "reference", "reference_seen", "reference_ready",
    "bg_commands_vdp2", "bg_commands_reference", "commands_vdp2",
    "commands_reference", "update_us_last", "update_us_max",
    "software_fill_us", "software_upload_us", "sea_scroll", "sun_x",
    "late_swaps",
)
LONG = os.environ.get("LIBSATURN_PARALLAX_LONG") == "1"
SKY = (24, 40, 96)
SEA = (24, 56, 128)
SAND = (255, 200, 72)
LEAF = (16, 64, 16)


def _near(a, b, tol=6):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class ParallaxWorldTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise unittest.SkipTest("run harness/run-vdp2-parallax-world.ps1 first")
        cls.shots = os.environ.get("LIBSATURN_PARALLAX_SHOTS", "")
        cls.images = {}
        try:
            from PIL import Image
        except ImportError:
            return
        for name in ("early", "mid", "reference", "late"):
            path = os.path.join(cls.shots, name + ".png")
            if os.path.isfile(path):
                image = Image.open(path).convert("RGB")
                assert image.size == (320, 224), (name, image.size)
                cls.images[name] = image

    def test_setup_and_refusals(self):
        r = self.r
        for key in ("init_ok", "layers_ok", "rotation_ok", "back_ok",
                    "line_color_ok", "window_ok", "refused_scroll",
                    "refused_window", "reference_ready"):
            self.assertEqual(r[key], 1, key)
        self.assertEqual(r["late_swaps"], 0)

    def test_replay_length(self):
        self.assertGreater(self.r["frames"], 3400 if LONG else 650)
        if LONG:
            self.assertEqual(self.r["reference_seen"], 1)

    def test_measurement_paths(self):
        r = self.r
        self.assertEqual(r["reference_seen"], 1)
        self.assertEqual(r["bg_commands_vdp2"], 0)
        self.assertEqual(r["bg_commands_reference"], 1)
        self.assertEqual(r["commands_reference"], r["commands_vdp2"] + 1)
        self.assertGreater(r["update_us_last"], 0)
        self.assertLess(r["update_us_last"], 8000)
        self.assertGreater(r["software_fill_us"], r["update_us_last"])
        self.assertGreater(r["software_upload_us"], 0)
        self.assertLess(r["sea_scroll"], 512)

    def test_visible_layers_and_parallax(self):
        for name in ("early", "mid", "late"):
            self.assertIn(name, self.images, "missing %s capture" % name)
        early, mid, late = self.images["early"], self.images["mid"], self.images["late"]
        self.assertNotEqual(early.tobytes(), mid.tobytes(), "the scene did not move")
        self.assertNotEqual(mid.tobytes(), late.tobytes(), "the scene did not move")
        for image, name in ((early, "early"), (mid, "mid"), (late, "late")):
            # The foreground leaves sway across the top-right corner; sample a
            # clear sky window instead of a fixed corner pixel.
            self.assertTrue(_near(image.getpixel((210, 20)), SKY, 12),
                            "%s sky: %r" % (name, image.getpixel((210, 20))))
            self.assertTrue(_near(image.getpixel((250, 130)), SEA, 12),
                            "%s sea: %r" % (name, image.getpixel((250, 130))))
            self.assertTrue(_near(image.getpixel((250, 175)), SAND, 14),
                            "%s shore: %r" % (name, image.getpixel((250, 175))))
        colors = early.getcolors(4096) or []
        self.assertTrue(any(_near(color, LEAF, 12) for _, color in colors),
                        "no foreground leaf colour in the live picture")

    def test_reference_path_is_distinct_and_restored(self):
        self.assertIn("reference", self.images)
        reference = self.images["reference"]
        mid = self.images["mid"]
        late = self.images["late"]
        # The reference frame uses different water/sky tones and a static
        # pre-rendered layout. It must not be a copy of the live scene.
        self.assertFalse(_near(reference.getpixel((250, 130)), SEA, 4),
                         "reference and live sea are indistinguishable")
        self.assertNotEqual(reference.tobytes(), mid.tobytes())
        self.assertNotEqual(reference.tobytes(), late.tobytes())
        # Late is the live path again.
        self.assertTrue(_near(late.getpixel((250, 130)), SEA, 12))

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

    def _probe(self):
        path = os.environ.get("LIBSATURN_PARALLAX_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)


def _load():
    dump = os.environ.get("LIBSATURN_PARALLAX_DUMP")
    map_path = os.environ.get("LIBSATURN_PARALLAX_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-parallax-world.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_parallax",
                        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
