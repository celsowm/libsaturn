r"""Acceptance for VDP2 special colour: ratio, add and the extended cascade
(examples/vdp2_special_color).

    .\harness\run-vdp2-special-color.ps1 -Bios .\bios\saturn_bios_us.bin

Three palette layers - NBG0 yellow bars (top), NBG1 red stripes, NBG2 solid
green - with a colour calculation window confining every effect to the left
half. Screenshots: frame 100 is RATIO mode (15:17 at ratio 16), frame 150 is
ADD mode (CCMD saturation), frame 250 is EXT mode (EXCCEN:
second = (NBG1 + NBG2)/2 by table 12.2,
then NBG0 blends over it at ratio 8; NBG1's own ratio is 31). Every expected
colour is recomputed here from the 5-bit palette values and the manual's
weight tables; the struct read is g_special_color.
"""

import json
import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x53504331  # "SPC1"
Y5 = (31, 28, 3)
R5 = (31, 6, 6)
G5 = (6, 28, 8)
FIELDS = ("magic", "cram_mode_ok", "layers_ok", "cc_window_ok",
          "refused_ratio_range", "refused_screen_range", "refused_blur_screen",
          "refused_insert_behind_blur", "extended_clears_blur",
          "mode_ok", "mode", "ratio", "frames")
TOL = 12


def _eight(c):
    return tuple(v << 3 for v in c)


def _ratio_mix(top, below, r):
    """CCRTMD 0: top (31 - r) : second (r + 1), in 32nds (chapter 12)."""
    return tuple(((31 - r) * t + (r + 1) * b) // 32 for t, b in zip(top, below))


def _add(top, below):
    """CCMD: added as they are and saturated at 31 per channel."""
    return tuple(min(31, t + b) for t, b in zip(top, below))


def _near(a, b, tol=TOL):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def _load():
    dump = os.environ.get("LIBSATURN_SPECIALCOLOR_DUMP")
    map_path = os.environ.get("LIBSATURN_SPECIALCOLOR_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-special-color.ps1 first")
    raw = tel.load_dump(
        dump, map_path, "_g_special_color",
        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


class Vdp2SpecialColorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_special_color not initialised: %r" % cls.r)
        from PIL import Image
        shots = os.environ.get("LIBSATURN_SPECIALCOLOR_SHOTS", "")
        cls.shots = shots
        cls.img = Image.open(os.path.join(shots, "ratio.png")).convert("RGB")
        cls.pix = cls.img.load()
        cls.img2 = Image.open(os.path.join(shots, "ext.png")).convert("RGB")
        cls.pix2 = cls.img2.load()
        cls.img3 = Image.open(os.path.join(shots, "add.png")).convert("RGB")
        cls.pix3 = cls.img3.load()
        assert cls.img.size == (320, 224), cls.img.size
        assert cls.img3.size == (320, 224), cls.img3.size

        cls.misaligned = os.path.join(shots, "misaligned.txt") if shots else ""

    def test_no_misaligned_accesses(self):
        if not self.misaligned or not os.path.isfile(self.misaligned):
            self.skipTest("no misaligned report")
        with open(self.misaligned, "r", encoding="utf-8") as stream:
            lines = stream.read().splitlines()
        self.assertTrue(
            lines and lines[0].endswith("(first access per PC; totals: master 0, slave 0)"),
            "\n".join(lines[:20]))

    def test_calls_succeeded_and_refusals_were_recorded(self):
        r = self.r
        for key in FIELDS[1:9]:
            self.assertEqual(r[key], 1, key)
        self.assertEqual(r["mode_ok"], 0xF, "mode_ok")   # every mode applied
        self.assertEqual(r["mode"], 3, "final mode is EXT")
        self.assertEqual(r["ratio"], 8, "final ratio")
        self.assertGreater(r["frames"], 250)

    def test_ratio_mode_mixes_the_top_image_with_what_is_below(self):
        # Bar columns are cells x%5==0 (pixels 0..3, 40..43, ...); stripe rows
        # are cells y%4==0 (rows 0..3, 32..35, 64..67, ...).
        want_bar_stripe = _eight(_ratio_mix(Y5, R5, 16))
        want_bar_green = _eight(_ratio_mix(Y5, G5, 16))
        self.assertTrue(_near(self.pix[1, 65], want_bar_stripe),
                        "bar over stripe: %r want %r" % (self.pix[1, 65], want_bar_stripe))
        self.assertTrue(_near(self.pix[41, 65], want_bar_stripe),
                        "second bar column: %r" % (self.pix[41, 65],))
        self.assertTrue(_near(self.pix[1, 49], want_bar_green),
                        "bar over green: %r want %r" % (self.pix[1, 49], want_bar_green))
        self.assertFalse(_near(self.pix[1, 65], _eight(Y5), 4))
        # NBG1's own dots are not colour-calculated in this mode: pure red on
        # both halves, and the right half stays plain everywhere.
        self.assertTrue(_near(self.pix[9, 65], _eight(R5), 6))
        self.assertTrue(_near(self.pix[161, 65], _eight(Y5), 6))
        self.assertTrue(_near(self.pix[169, 65], _eight(R5), 6))
        self.assertTrue(_near(self.pix[169, 49], _eight(G5), 6))

    def test_add_mode_saturates_the_top_image_with_the_second(self):
        # CCMD: both images added as they are, clamped at 31 per channel.
        want_bar_stripe = _eight(_add(Y5, R5))
        want_bar_green = _eight(_add(Y5, G5))
        self.assertTrue(_near(self.pix3[1, 65], want_bar_stripe),
                        "bar over stripe: %r want %r" % (self.pix3[1, 65], want_bar_stripe))
        self.assertTrue(_near(self.pix3[41, 65], want_bar_stripe))
        self.assertTrue(_near(self.pix3[1, 49], want_bar_green),
                        "bar over green: %r want %r" % (self.pix3[1, 49], want_bar_green))
        self.assertFalse(_near(self.pix3[1, 65], _eight(Y5), 4))
        # Sprites are never colour-calculated here, and the right half stays
        # plain: no mixed pixels outside the left half.
        self.assertTrue(_near(self.pix3[161, 65], _eight(Y5), 6))
        self.assertTrue(_near(self.pix3[169, 65], _eight(R5), 6))
        self.assertTrue(_near(self.pix3[169, 49], _eight(G5), 6))

    def test_extended_cascade_mixes_the_pair_then_the_top_image(self):
        ext = tuple((a + b) // 2 for a, b in zip(R5, G5))       # table 12.2: 2:2:0
        want_bar_ext = _eight(_ratio_mix(Y5, ext, 8))
        want_bar_green = _eight(_ratio_mix(Y5, G5, 8))
        # Under a bar: NBG0 blends over the pre-mixed (NBG1 + NBG2)/2 pair.
        self.assertTrue(_near(self.pix2[1, 65], want_bar_ext),
                        "bar over extended pair: %r want %r" % (self.pix2[1, 65], want_bar_ext))
        self.assertTrue(_near(self.pix2[1, 49], want_bar_green),
                        "bar over green: %r want %r" % (self.pix2[1, 49], want_bar_green))
        # Outside a bar NBG1's ratio 31 shows only NBG2 on the effect side...
        self.assertTrue(_near(self.pix2[9, 65], _eight(G5), 8),
                        "stripe outside a bar not green: %r" % (self.pix2[9, 65],))
        # ...while the right half keeps the plain stack.
        self.assertTrue(_near(self.pix2[161, 65], _eight(Y5), 6))
        self.assertTrue(_near(self.pix2[169, 65], _eight(R5), 6))
        self.assertTrue(_near(self.pix2[169, 49], _eight(G5), 6))
        # The cascade visibly changes the effect side: not plain red, not the
        # ratio-mode weight either.
        self.assertFalse(_near(self.pix2[9, 65], _eight(R5), 6))
        self.assertFalse(_near(self.pix2[1, 65], _eight(_ratio_mix(Y5, R5, 8)), 6))

    def test_probe_dump_mem_reads_the_struct(self):
        with open(os.path.join(self.shots, "struct.bin"), "rb") as stream:
            self.assertEqual(
                stream.read(),
                struct.pack(">%dI" % len(FIELDS), *[self.r[k] for k in FIELDS]))

    def test_probe_screenshot_when_fires_only_when_reached(self):
        self.assertTrue(os.path.exists(os.path.join(self.shots, "ratio.png")))
        self.assertTrue(os.path.exists(os.path.join(self.shots, "add.png")))
        self.assertTrue(os.path.exists(os.path.join(self.shots, "ext.png")))
        self.assertFalse(os.path.exists(os.path.join(self.shots, "never.png")))

    def _probe(self):
        path = os.environ.get("LIBSATURN_SPECIALCOLOR_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)

    def test_ccctl_register_tracks_the_final_extended_mode(self):
        """The final EXT mode must leave N0CCEN, N1CCEN and EXCCEN set, NBG0's
        ratio at 8, and the temporary line-colour insert cleared."""
        doc = self._probe()
        regs = doc["vdp2"]
        self.assertEqual(regs["ccctl"] & 0x0403, 0x0403,
                         "CCCTL: %04X" % regs["ccctl"])
        self.assertEqual(regs["ccrna"] & 0x1F, 8, "CCRNA: %04X" % regs["ccrna"])
        self.assertEqual(regs["lnclen"], 0, "LNCLEN: %04X" % regs["lnclen"])

    def test_watch_write_records_the_ratio_mode_ccctl_commit(self):
        """The probe watches CCCTL from emulated frame 149, just before the
        RATIO switch at program frame 60; the write must carry N0CCEN (bit 0)."""
        doc = self._probe()
        watch = doc.get("watch_writes")
        if not watch:
            self.skipTest("no --watch-write capture")
        hits = watch["master"]["hits"]
        self.assertGreater(watch["master"]["count"], 0)
        self.assertTrue(any(hit["value"] & 0x0001 for hit in hits),
                        "no N0CCEN write in %r" % hits)


if __name__ == "__main__":
    unittest.main()
