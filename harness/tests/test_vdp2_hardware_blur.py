r"""Acceptance for the VDP2 hardware horizontal blur (examples/vdp2_hardware_blur).

    .\harness\run-vdp2-hardware-blur.ps1 -Bios .\bios\saturn_bios_us.bin

NBG0 is a one-pixel black/white checker (top image, opaque, ratio 16);
NBG1 is solid blue below it. A colour calculation window confines the blend
to the left half, so the right half is the control and must not change when
the blur toggles.

Frame 100 is CC mode: left = mix(checker, blue, 15:17).
Frame 160 is BLUR mode: left = mix(checker, blur(checker), 15:17), where the
blur is (2*current + previous1 + previous2) / 4 (manual p12_13). Frame 250 is
BLUR 32 (ratio 31): the second image is everything, so it shows that blur
alone. Every expected colour is recomputed here from the palette values; the
right half is compared byte-for-byte between the screenshots.

The struct read is g_blur. Registers and the probe's watch-write provide the
non-pixel evidence: CCCTL must carry BOKEN + BOKN 2 + N0CCEN, and the BLUR
switch must appear as a write of the BOKEN bit.
"""

import json
import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x424C5231  # "BLR1"
FIELDS = ("magic", "cram_mode_ok", "layers_ok", "cc_window_ok",
          "refused_bokn3", "refused_bokn7", "refused_cram1", "refused_line_color",
          "blur_accepts_after_line_color",
          "blur_accepts_after_extended", "extended_accepts_after_blur",
          "mode_ok", "mode", "ratio", "frames")
WHITE = (31 << 3,) * 3
BLACK = (1 << 3,) * 3
BLUE = (2 << 3, 2 << 3, 31 << 3)
RATIO = 16
SOLO_RATIO = 31
TOL = 5
# Checker columns: the tile paints even x with palette 1 (white), odd x with 2.
# Probe y=100 avoids the HUD (rows 8..48) and the left window edge (<160).
PX_A, PX_B, PY = 10, 11, 100
PX_RIGHT_A, PX_RIGHT_B = 200, 201


def _mix(top, below, ratio=RATIO):
    """Manual p12_12: top (31-r) : second (r+1), in 32nds."""
    return tuple((t * (31 - ratio) + b * (ratio + 1)) // 32
                 for t, b in zip(top, below))


def _top(x):
    return WHITE if (x & 1) == 0 else BLACK


def _blur(x):
    """Manual p12_13 2:1:1 horizontal average; exact here because all three
    inputs are multiples of 8 and the sums are even."""
    return tuple((2 * c + b + a) // 4
                 for a, b, c in zip(_top(x - 2), _top(x - 1), _top(x)))


def _near(a, b, tol=TOL):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class Vdp2HardwareBlurTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_blur not initialised: %r" % cls.r)
        from PIL import Image
        cls.shots = os.environ.get("LIBSATURN_HARDWAREBLUR_SHOTS", "")
        cls.plain = Image.open(os.path.join(cls.shots, "plain.png")).convert("RGB")
        cls.cc = Image.open(os.path.join(cls.shots, "cc.png")).convert("RGB")
        cls.blur = Image.open(os.path.join(cls.shots, "blur.png")).convert("RGB")
        cls.solo = Image.open(os.path.join(cls.shots, "solo.png")).convert("RGB")
        for image in (cls.plain, cls.cc, cls.blur, cls.solo):
            assert image.size == (320, 224), image.size
        cls.p_plain = cls.plain.load()
        cls.p_cc = cls.cc.load()
        cls.p_blur = cls.blur.load()
        cls.p_solo = cls.solo.load()
        cls.misaligned = os.path.join(cls.shots, "misaligned.txt") if cls.shots else ""

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
        for key in FIELDS[1:11]:
            self.assertEqual(r[key], 1, key)
        self.assertEqual(r["mode_ok"], 0xF, "every mode applied")
        self.assertEqual(r["mode"], 3, "final mode is BLUR 32")
        self.assertEqual(r["ratio"], SOLO_RATIO)
        self.assertGreater(r["frames"], 250)

    def test_plain_mode_shows_the_sharp_checker_on_both_halves(self):
        for x, want in ((PX_A, WHITE), (PX_B, BLACK),
                        (PX_RIGHT_A, WHITE), (PX_RIGHT_B, BLACK)):
            self.assertTrue(_near(self.p_plain[x, PY], want, 2),
                            "plain %d: %r want %r" % (x, self.p_plain[x, PY], want))

    def test_cc_mode_mixes_with_the_screen_below_on_the_left_only(self):
        want_a = _mix(WHITE, BLUE)
        want_b = _mix(BLACK, BLUE)
        self.assertTrue(_near(self.p_cc[PX_A, PY], want_a),
                        "CC white column: %r want %r" % (self.p_cc[PX_A, PY], want_a))
        self.assertTrue(_near(self.p_cc[PX_B, PY], want_b),
                        "CC black column: %r want %r" % (self.p_cc[PX_B, PY], want_b))
        # The window's outside is the untouched top image.
        self.assertTrue(_near(self.p_cc[PX_RIGHT_A, PY], WHITE, 2))
        self.assertTrue(_near(self.p_cc[PX_RIGHT_B, PY], BLACK, 2))

    def test_blur_mode_mixes_the_checker_with_its_own_2_1_1_average(self):
        want_a = _mix(WHITE, _blur(PX_A))
        want_b = _mix(BLACK, _blur(PX_B))
        self.assertTrue(_near(self.p_blur[PX_A, PY], want_a),
                        "blur white column: %r want %r" % (self.p_blur[PX_A, PY], want_a))
        self.assertTrue(_near(self.p_blur[PX_B, PY], want_b),
                        "blur black column: %r want %r" % (self.p_blur[PX_B, PY], want_b))
        # The blur is large: the two columns no longer look like the CC mix.
        self.assertFalse(_near(self.p_blur[PX_A, PY], self.p_cc[PX_A, PY], 4))
        self.assertFalse(_near(self.p_blur[PX_B, PY], self.p_cc[PX_B, PY], 4))

    def test_blur_only_mode_shows_the_2_1_1_average(self):
        """Ratio 31 makes the second image everything, so this frame is the
        blurred layer itself: (2*current + previous1 + previous2) / 4."""
        self.assertTrue(_near(self.p_solo[PX_A, PY], _blur(PX_A), 2),
                        "solo white column: %r want %r"
                        % (self.p_solo[PX_A, PY], _blur(PX_A)))
        self.assertTrue(_near(self.p_solo[PX_B, PY], _blur(PX_B), 2),
                        "solo black column: %r want %r"
                        % (self.p_solo[PX_B, PY], _blur(PX_B)))

    def test_the_window_outside_is_identical_with_blur_on_or_off(self):
        for x in (PX_RIGHT_A, PX_RIGHT_B, 250, 300):
            self.assertEqual(self.p_cc[x, PY], self.p_blur[x, PY],
                             "right-half pixel %d changed with the blur" % x)

    def test_probe_dump_mem_reads_the_struct(self):
        with open(os.path.join(self.shots, "struct.bin"), "rb") as stream:
            self.assertEqual(
                stream.read(),
                struct.pack(">%dI" % len(FIELDS), *[self.r[k] for k in FIELDS]))

    def test_probe_screenshot_when_fires_only_when_reached(self):
        self.assertTrue(os.path.exists(os.path.join(self.shots, "plain.png")))
        self.assertTrue(os.path.exists(os.path.join(self.shots, "cc.png")))
        self.assertTrue(os.path.exists(os.path.join(self.shots, "blur.png")))
        self.assertTrue(os.path.exists(os.path.join(self.shots, "solo.png")))
        self.assertFalse(os.path.exists(os.path.join(self.shots, "never.png")))

    def _probe(self):
        path = os.environ.get("LIBSATURN_HARDWAREBLUR_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)

    def test_ccctl_register_carries_boken_bokn_and_n0ccen(self):
        doc = self._probe()
        regs = doc["vdp2"]
        want = 0x8000 | (2 << 12) | 0x0001
        self.assertEqual(regs["ccctl"], want, "CCCTL: %04X" % regs["ccctl"])
        self.assertEqual(regs["ccrna"] & 0x1F, SOLO_RATIO, "CCRNA: %04X" % regs["ccrna"])
        self.assertEqual(regs["lnclen"], 0, "LNCLEN: %04X" % regs["lnclen"])
        self.assertEqual(regs["ccctl"] & 0x0400, 0, "EXCCEN must be clear under BOKEN")

    def test_watch_write_records_the_boken_switch(self):
        """The probe watches CCCTL from emulated frame 209, just before the
        BLUR switch at program frame 120; the write must carry BOKEN."""
        doc = self._probe()
        watch = doc.get("watch_writes")
        if not watch:
            self.skipTest("no --watch-write capture")
        hits = watch["master"]["hits"]
        self.assertGreater(watch["master"]["count"], 0)
        self.assertTrue(any(hit["value"] & 0x8000 for hit in hits),
                        "no BOKEN write in %r" % hits)


def _load():
    dump = os.environ.get("LIBSATURN_HARDWAREBLUR_DUMP")
    map_path = os.environ.get("LIBSATURN_HARDWAREBLUR_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-hardware-blur.ps1 first")
    raw = tel.load_dump(
        dump, map_path, "_g_blur",
        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
