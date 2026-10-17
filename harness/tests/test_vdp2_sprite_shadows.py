r"""Acceptance for VDP2 sprite shadows (examples/vdp2_sprite_shadows).

    .\harness\run-vdp2-sprite-shadows.ps1 -Bios .\bios\saturn_bios_us.bin

NBG0 is a scrolling vertical-stripe ground. Type-2 palette sprites carry a
normal-shadow word (0x47FE) or a transparent-MSB-shadow word (0x8000) through
a LUT4 texture; a palette sprite is also self-shadowed with VDP1 MSB-on. A
VDP1 RGB panel plus a VDP1 shadow sprite is the control.

The captures are 2 frames after the reported counter value; the scene clock
is deterministic, so the ground colour is recomputed from the scroll and the
shadow x from the frame counter:

  frame 44  NORMAL      0x47FE normal shadow over NBG0
  frame 104 MSB         0x8000 transparent MSB shadow + MSB-on self-shadow
  frame 164 VDP1        VDP1 RGB panel + VDP1 shadow
  frame 224 TARGET OFF  normal silhouette, SDCTL=0 -> ground unchanged
  frame 284 OFF         shadow disabled -> ground unchanged

No golden image: every expected pixel is derived from the 5-bit palette, the
manual's normal-shadow/MSB rules and the 8-bit VDP2 versus 5-bit VDP1 halving.
"""

import json
import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x53484431  # "SHD1"
FIELDS = ("magic", "layers_ok", "shadow_ok", "refused_palette",
          "refused_mode", "refused_target", "refused_priority",
          "refused_busy", "mode_ok", "mode", "scroll", "shadow_x", "frames")
# Counter value used by --screenshot-when; the image itself is 2 frames later.
CAPTURE = {"normal": 42, "msb": 102, "vdp1": 162, "blocked": 222, "off": 282}
DELAY = 2
GROUND5 = [
    (31, 0, 0), (0, 31, 0), (0, 0, 31), (31, 31, 0),
    (31, 0, 31), (0, 31, 31), (31, 16, 0), (24, 24, 24),
]
TOL = 2
SELF_X, SELF_Y = 248, 56
PANEL_BASE = (0, 248, 248)       # SAT_RGB555(0,31,31) expanded by >>3
PANEL_VDP1_SHADOW = (0, 120, 120)  # VDP1 halves 5-bit 31 -> 15 -> 120


def _eight(color5):
    """Ymir's 5-bit to 8-bit expansion: each channel << 3."""
    return tuple(v << 3 for v in color5)


def _vdp2_half(color5):
    """VDP2 halves the expanded 8-bit top image (Color888ShadowMasked)."""
    return tuple((v << 3) >> 1 for v in color5)


def _vdp1_half(color5):
    """VDP1 halves the RGB555 destination, then VDP2 expands it."""
    return tuple((v >> 1) << 3 for v in color5)


def _scene(frame):
    """(scroll, shadow_x, shadow_center_x) for an actual captured frame."""
    scroll = (2 * (frame - 1)) & 0x1FF
    shadow_x = 64 + (frame & 63)
    return scroll, shadow_x, shadow_x + 16


def _ground5(x, scroll):
    return GROUND5[(((x + scroll) >> 3) & 63) & 7]


def _near(a, b, tol=TOL):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class Vdp2SpriteShadowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_sprite_shadows not initialised: %r" % cls.r)
        from PIL import Image
        cls.shots = os.environ.get("LIBSATURN_SPRITESHADOWS_SHOTS", "")
        cls.images = {}
        for name in CAPTURE:
            image = Image.open(os.path.join(cls.shots, name + ".png")).convert("RGB")
            assert image.size == (320, 224), (name, image.size)
            cls.images[name] = image
        cls.misaligned = os.path.join(cls.shots, "misaligned.txt") if cls.shots else ""

    def assertPixel(self, image, x, y, want, label):
        got = image.getpixel((x, y))
        self.assertTrue(_near(got, want),
                        "%s (%d,%d): %r want %r" % (label, x, y, got, want))

    def test_telemetry_and_refusals(self):
        r = self.r
        for key in ("layers_ok", "shadow_ok", "refused_palette", "refused_mode",
                    "refused_target", "refused_priority", "refused_busy"):
            self.assertEqual(r[key], 1, key)
        self.assertEqual(r["mode_ok"], 0x1F, "every mode applied")
        self.assertEqual(r["mode"], 4, "final mode is OFF")
        self.assertGreater(r["frames"], 300)
        self.assertEqual(r["scroll"], (2 * (r["frames"] - 1)) & 0x1FF)
        self.assertEqual(r["shadow_x"], 64 + (r["frames"] & 63))

    def test_normal_shadow_halves_the_scrolled_ground(self):
        actual = CAPTURE["normal"] + DELAY
        scroll, shadow_x, center = _scene(actual)
        ground = _ground5(center, scroll)
        image = self.images["normal"]
        # Inside the 32x32 silhouette: VDP2 halves the top image's 8-bit colour.
        self.assertPixel(image, center, 96, _vdp2_half(ground), "normal shadow")
        # The same x one stripe row above is outside the silhouette and keeps
        # the full ground colour, even though the ground is scrolling.
        self.assertPixel(image, center, 64, _eight(ground), "normal reference")
        # The self-shadow sprite is not shadowed in NORMAL mode.
        self.assertPixel(image, SELF_X, SELF_Y, (248, 248, 248), "normal sprite")

    def test_transparent_msb_shadow_and_msb_on_self_shadow(self):
        actual = CAPTURE["msb"] + DELAY
        scroll, shadow_x, center = _scene(actual)
        ground = _ground5(center, scroll)
        image = self.images["msb"]
        self.assertPixel(image, center, 96, _vdp2_half(ground), "msb shadow")
        self.assertPixel(image, center, 64, _eight(ground), "msb reference")
        # VDP1 MSB-on preserved the palette dot's 15 bits; VDP2 fetches its
        # own colour (white) and halves it.
        self.assertPixel(image, SELF_X, SELF_Y, (124, 124, 124), "msb self shadow")

    def test_vdp1_shadow_is_the_five_bit_control(self):
        image = self.images["vdp1"]
        # The RGB panel is not a VDP2 layer, so the VDP2 shadow function is off
        # in this mode; the VDP1 shadow halves the RGB555 destination.
        self.assertPixel(image, 210, 100, PANEL_BASE, "vdp1 panel")
        self.assertPixel(image, 240, 120, PANEL_VDP1_SHADOW, "vdp1 shadow")
        # The same 31 -> 15 -> 120 path differs from VDP2's 248 -> 124.
        self.assertNotEqual(PANEL_VDP1_SHADOW[1], _vdp2_half((0, 31, 31))[1])

    def test_target_restriction_leaves_the_ground_unchanged(self):
        actual = CAPTURE["blocked"] + DELAY
        scroll, shadow_x, center = _scene(actual)
        ground = _ground5(center, scroll)
        image = self.images["blocked"]
        # SDCTL=0: the silhouette is still drawn but NBG0's SDEN is clear.
        self.assertPixel(image, center, 96, _eight(ground), "blocked shadow")
        self.assertPixel(image, center, 64, _eight(ground), "blocked reference")

    def test_disable_restores_the_full_ground(self):
        actual = CAPTURE["off"] + DELAY
        scroll, shadow_x, center = _scene(actual)
        ground = _ground5(center, scroll)
        image = self.images["off"]
        self.assertPixel(image, center, 96, _eight(ground), "off shadow")
        self.assertPixel(image, center, 64, _eight(ground), "off reference")

    def test_the_ground_really_scrolled(self):
        # Fixed x, no shadow: the stripe under it changes between captures.
        normal_scroll = _scene(CAPTURE["normal"] + DELAY)[0]
        msb_scroll = _scene(CAPTURE["msb"] + DELAY)[0]
        normal = _ground5(20, normal_scroll)
        msb = _ground5(20, msb_scroll)
        self.assertNotEqual(normal, msb)
        self.assertPixel(self.images["normal"], 20, 64, _eight(normal), "normal ground")
        self.assertPixel(self.images["msb"], 20, 64, _eight(msb), "msb ground")

    def test_no_misaligned_accesses(self):
        if not self.misaligned or not os.path.isfile(self.misaligned):
            self.skipTest("no misaligned report")
        with open(self.misaligned, "r", encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertTrue(
            lines and lines[0].endswith("(first access per PC; totals: master 0, slave 0)"),
            "\n".join(lines[:20]))

    def test_probe_dump_mem_reads_the_struct(self):
        with open(os.path.join(self.shots, "struct.bin"), "rb") as stream:
            self.assertEqual(
                stream.read(),
                struct.pack(">%dI" % len(FIELDS), *[self.r[k] for k in FIELDS]))

    def test_probe_screenshot_when_fires_only_when_reached(self):
        for name in CAPTURE:
            self.assertTrue(os.path.exists(os.path.join(self.shots, name + ".png")))
        self.assertFalse(os.path.exists(os.path.join(self.shots, "never.png")))

    def _probe(self):
        path = os.environ.get("LIBSATURN_SPRITESHADOWS_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)

    def test_final_sprite_registers_are_restored(self):
        regs = self._probe()["vdp2"]
        self.assertEqual(regs["sdctl"], 0x0000, "SDCTL: %04X" % regs["sdctl"])
        self.assertEqual(regs["spctl"], 0x0020, "SPCTL: %04X" % regs["spctl"])
        self.assertEqual(regs["prisa"], 0x0404, "PRISA: %04X" % regs["prisa"])
        self.assertEqual(regs["prisb"], 0, "PRISB: %04X" % regs["prisb"])
        self.assertEqual(regs["prisc"], 0, "PRISC: %04X" % regs["prisc"])
        self.assertEqual(regs["prisd"], 0, "PRISD: %04X" % regs["prisd"])
        self.assertEqual(regs["ccctl"], 0, "CCCTL: %04X" % regs["ccctl"])

    def test_watch_write_records_the_msb_sdctl(self):
        """The NORMAL -> MSB switch must write NBG0 SDEN + TPSDSL (0x0101)."""
        doc = self._probe()
        watch = doc.get("watch_writes")
        if not watch:
            self.skipTest("no --watch-write capture")
        self.assertEqual(watch["address"], 0x05F800E2)
        hits = watch["master"]["hits"]
        self.assertGreater(watch["master"]["count"], 0)
        self.assertTrue(any(hit["value"] == 0x0101 for hit in hits),
                        "no SDCTL=0x0101 write in %r" % hits[:20])


def _load():
    dump = os.environ.get("LIBSATURN_SPRITESHADOWS_DUMP")
    map_path = os.environ.get("LIBSATURN_SPRITESHADOWS_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-sprite-shadows.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_sprite_shadows",
                        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
