r"""Acceptance for RBG1, the second rotation screen, beside RBG0
(examples/vdp2_dual_rotation).

Run through the wrapper, which builds the example, runs Ymir and dumps
Work RAM and screenshots:

    .\harness\run-vdp2-dual-rotation.ps1 -Bios .\bios\saturn_bios_jp.bin

The struct read here is g_dual_rotation (31 big-endian words). The scene
math is recomputed in this file from the same integer formulas the shipped
header documents - a failure means the parameter regions the VDP2 read
were wrong, not that the assertion drifted. The screenshots then prove what
the emulator shows per mode: the slate grid alone, the warm checker RBG0
reads through its own name banks in mode B, and - in DUAL - the same
checker showing through the grid's transparent cells via RBG1's fixed
VRAM-B1/B0 fetch, while the six illegal combinations are refused.
"""

import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x44524F54  # "DROT"
SAT_OK = 0
SAT_ERR_INVALID_ARG = -1
SAT_ERR_BUSY = -7

CAPTURE_FRAME = 300
SPIN_A_TURNS = 240
SPIN_B_TURNS = 180

FIELDS = ">31I"


def sine(turns):
    """The header's fixed-point series, mirrored (16.16 turns -> 16.16)."""
    quarter = (turns >> 14) & 3
    a = ((turns & 0x3FFF) * 102944) >> 14
    if quarter & 1:
        a = 102944 - a
    a2 = (a * a) >> 16
    a3 = (a2 * a) >> 16
    a5 = (a3 * a2) >> 16
    a7 = (a5 * a2) >> 16
    v = a - ((a3 * 10923) >> 16) + ((a5 * 546) >> 16) - ((a7 * 13) >> 16)
    return -v if quarter & 2 else v


def _load():
    dump = os.environ.get("LIBSATURN_DROT_DUMP")
    map_path = os.environ.get("LIBSATURN_DROT_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-dual-rotation.ps1 first")
    return tel.load_dump(dump, map_path, "_g_dual_rotation", FIELDS)


def _signed(v):
    return struct.unpack(">i", struct.pack(">I", v))[0]


def _pixels(path, y0, y1):
    """The playfield rows [y0, y1) as a flat tuple of RGB triples."""
    from PIL import Image
    image = Image.open(path).convert("RGB")
    rows = []
    for y in range(y0, min(y1, image.height)):
        rows.extend(image.crop((0, y, image.width, y + 1)).getdata())
    return rows


def _families(path):
    """Pixel counts per palette family, playfield rows only (the HUD lives
    in rows 0..32): warm and dark are the checker palette RBG1 draws, slate
    is RBG0's grid fill; the grid frame (near-white) and the backdrop
    (black) match no family."""
    warm = dark = slate = 0
    for r, g, b in _pixels(path, 40, 220):
        if r >= 160 and b <= 96:
            warm += 1
        elif 80 <= r <= 144 and g <= 64 and b <= 64:
            dark += 1
        elif b >= 120 and r <= 112:
            slate += 1
    return warm, dark, slate


class DualRotationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = _load()
        cls.r = {
            "magic": raw[0],
            "frames": raw[1],
            "mode": raw[2],
            "mode_ran": raw[3],
            "parameter_mode": raw[4],
            "rbg1_enabled": raw[5],
            "refused_configure_mode2": _signed(raw[6]),
            "refused_banks_in_vram_b": _signed(raw[7]),
            "refused_map_outside_b1": _signed(raw[8]),
            "refused_nbg_while_dual": _signed(raw[9]),
            "refused_mode_switch_while_rbg1": _signed(raw[10]),
            "refused_rbg1_enable_in_mode_b": _signed(raw[11]),
            "transition_result": [_signed(v) for v in raw[12:15]],
            "live_hide_result": _signed(raw[15]),
            "params_writes": raw[16],
            "region": raw[17],
            "capture_frame": raw[18],
            "capture_mode": raw[19],
            "capture_cos_a": _signed(raw[20]),
            "capture_sin_a": _signed(raw[21]),
            "capture_cos_b": _signed(raw[22]),
            "capture_sin_b": _signed(raw[23]),
            "capture_mx_a": _signed(raw[24]),
            "capture_my_a": _signed(raw[25]),
            "capture_mx_b": _signed(raw[26]),
            "capture_my_b": _signed(raw[27]),
            "capture_region": raw[28],
            "rbg0_priority": raw[29],
            "rbg1_priority": raw[30],
        }
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_dual_rotation not initialised: %r" % cls.r)
        cls.shots = os.environ.get("LIBSATURN_DROT_SHOTS", "")
        cls.misaligned = os.path.join(cls.shots, "misaligned.txt") if cls.shots else ""

    def test_every_mode_ran(self):
        self.assertEqual(self.r["mode_ran"], 7)
        self.assertGreaterEqual(self.r["frames"], 405)

    def test_live_paths_and_double_buffer(self):
        # The run ends one wrap past DUAL: A-ONLY again, RBG1 hidden again
        # through the live sat_vdp2_rotation_rbg1_set_enabled(0) path, RPMD
        # back to A.
        self.assertEqual(self.r["mode"], 0)
        self.assertEqual(self.r["parameter_mode"], 0)
        self.assertEqual(self.r["rbg1_enabled"], 0)
        self.assertEqual(self.r["transition_result"], [SAT_OK] * 3)
        self.assertEqual(self.r["live_hide_result"], SAT_OK)
        # Every frame writes one parameter region into the inactive half;
        # the counter matching the frame count means no write ever failed.
        self.assertGreaterEqual(self.r["params_writes"], 405)
        self.assertEqual(self.r["params_writes"], self.r["frames"])
        self.assertIn(self.r["region"], (0x10000, 0x10200))

    def test_illegal_combinations_are_refused(self):
        # RBG1 with a switching parameter mode ("Mode 0 must be set when
        # displaying RBG1", the RPMD page), RBG0's image banks inside VRAM-B
        # (the RBG1 separate-VRAM rule), parameter B's map outside VRAM-B1,
        # a parameter-mode switch away from A while RBG1 displays, and RBG1
        # enabled in mode B: all SAT_ERR_INVALID_ARG.
        self.assertEqual(self.r["refused_configure_mode2"], SAT_ERR_INVALID_ARG)
        self.assertEqual(self.r["refused_banks_in_vram_b"], SAT_ERR_INVALID_ARG)
        self.assertEqual(self.r["refused_map_outside_b1"], SAT_ERR_INVALID_ARG)
        self.assertEqual(self.r["refused_mode_switch_while_rbg1"], SAT_ERR_INVALID_ARG)
        self.assertEqual(self.r["refused_rbg1_enable_in_mode_b"], SAT_ERR_INVALID_ARG)
        # A normal scroll screen cannot display while RBG1 does (p06_20):
        # the NBG0 configure is refused with SAT_ERR_BUSY at the HAL.
        self.assertEqual(self.r["refused_nbg_while_dual"], SAT_ERR_BUSY)

    def test_capture_matches_the_reference_math(self):
        self.assertEqual(self.r["capture_frame"], CAPTURE_FRAME)
        self.assertEqual(self.r["capture_mode"], 2)  # DUAL
        turn_a = (CAPTURE_FRAME << 16) // SPIN_A_TURNS
        turn_b = (-((CAPTURE_FRAME << 16) // SPIN_B_TURNS)) & 0xFFFFFFFF
        self.assertEqual(self.r["capture_cos_a"], sine(turn_a + 0x4000))
        self.assertEqual(self.r["capture_sin_a"], sine(turn_a))
        self.assertEqual(self.r["capture_cos_b"], sine((turn_b + 0x4000) & 0xFFFFFFFF))
        self.assertEqual(self.r["capture_sin_b"], sine(turn_b))
        drift_a = (CAPTURE_FRAME << 16) // 384
        drift_b = (CAPTURE_FRAME << 16) // 320
        self.assertEqual(self.r["capture_mx_a"], (sine(drift_a) * 80) >> 16)
        self.assertEqual(self.r["capture_my_a"], (sine(drift_a + 0x4000) * 60) >> 16)
        self.assertEqual(self.r["capture_mx_b"], (sine((drift_b + 0x4000) & 0xFFFFFFFF) * 80) >> 16)
        self.assertEqual(self.r["capture_my_b"], (sine(drift_b) * 60) >> 16)
        self.assertEqual(self.r["capture_region"], 0x10200)  # frame 300 is even
        self.assertEqual(self.r["rbg0_priority"], 4)
        self.assertEqual(self.r["rbg1_priority"], 3)

    def _shot(self, name):
        path = os.path.join(self.shots, name)
        if not os.path.isfile(path):
            self.skipTest("missing capture %s" % path)
        return path

    def test_the_modes_draw_distinct_pictures(self):
        """What the emulator shows per mode, playfield rows only: A-ONLY is
        the slate grid over the black backdrop; B-on-RBG0 is the warm
        checker read through RBG0's own name banks; DUAL shows RBG1's
        checker through the grid's transparent cells, so both palette
        families are present at once."""
        f60 = self._shot("f60.png")     # A-ONLY
        f180 = self._shot("f180.png")   # B-on-RBG0
        f300 = self._shot("f300.png")   # DUAL (the capture frame)
        f380 = self._shot("f380.png")   # A-ONLY again after the wrap

        warm60, dark60, slate60 = _families(f60)
        warm180, dark180, slate180 = _families(f180)
        warm300, dark300, slate300 = _families(f300)
        warm380, dark380, slate380 = _families(f380)

        # A-ONLY: the slate grid only, no checker palette anywhere.
        self.assertGreater(slate60, 2000)
        self.assertEqual((warm60, dark60), (0, 0))
        self.assertGreater(slate380, 2000)
        self.assertEqual((warm380, dark380), (0, 0))

        # B-on-RBG0: the checker through RBG0's own banks, no slate.
        self.assertGreater(warm180, 2000)
        self.assertGreater(dark180, 2000)
        self.assertEqual(slate180, 0)

        # DUAL: both screens at once - RBG1's checker showing through the
        # grid's transparent cells under RBG0's higher priority.
        self.assertGreater(slate300, 2000)
        self.assertGreater(warm300, 2000)
        self.assertGreater(dark300, 2000)

    def test_no_misaligned_accesses(self):
        if not self.misaligned or not os.path.isfile(self.misaligned):
            self.skipTest("no misaligned report")
        text = open(self.misaligned).read()
        self.assertIn("master 0, slave 0", text)


if __name__ == "__main__":
    unittest.main()
