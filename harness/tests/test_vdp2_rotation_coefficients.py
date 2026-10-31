r"""Acceptance for RBG0 parameter sets A and B and the coefficient table
contract (examples/vdp2_rotation_coefficients).

Run through the wrapper, which builds the example, runs Ymir and dumps
Work RAM and screenshots:

    .\harness\run-vdp2-rotation-coefficients.ps1 -Bios .\bios\saturn_bios_us.bin

The struct read here is g_rotation_coefficients (36 big-endian words). The
scene math is recomputed in this file from the same integer formulas the
shipped header documents - a failure means the tables the VDP2 read were
wrong, not that the assertion drifted. The screenshots then prove what the
emulator actually shows per parameter mode: the sky visible through the
hidden lines of the A scenes, the yawed B image covering it, and the per-line
A/B slice of RPMD mode 2.
"""

import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x52434631  # "RCF1"
SAT_OK = 0
SAT_ERR_INVALID_ARG = -1

HORIZON = 96
FOCAL = 96
MIN_DEPTH = 8
YAW_COS = 56756

HIDE = 1
USE_B = 2

FIELDS = ">36I"


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


def floor_k(y):
    """k(y) = round(focal / depth), the header's exact integer rounding."""
    if y <= HORIZON:
        return 0
    d = (y - HORIZON) + MIN_DEPTH
    return ((FOCAL << 32) + (d << 15)) // (d << 16)


def mix_boundary(frame):
    sway = sine((frame << 16) // 96)
    return HORIZON + 32 + ((sway * 16) >> 16)


def encode_2word(value, flag):
    """Figure 6.7: sign and magnitude are separate; the flag is bit 15."""
    negative = value < 0
    mag = -value if negative else value
    if mag > 0x7FFFFF:
        mag = 0x7FFFFF
    return ((0x80000000 if flag else 0)
            | (0x00800000 if negative else 0)
            | (mag & 0x007F0000)
            | (mag & 0xFFFF))


def _load():
    dump = os.environ.get("LIBSATURN_ROTC_DUMP")
    map_path = os.environ.get("LIBSATURN_ROTC_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-rotation-coefficients.ps1 first")
    return tel.load_dump(dump, map_path, "_g_rotation_coefficients", FIELDS, 144)


def _signed(v):
    return struct.unpack(">i", struct.pack(">I", v))[0]


def _region(path, y0, y1):
    """The pixels of rows [y0, y1) as a flat tuple, for exact comparison."""
    from PIL import Image
    image = Image.open(path).convert("RGB")
    rows = []
    for y in range(y0, min(y1, image.height)):
        rows.extend(image.crop((0, y, image.width, y + 1)).getdata())
    return rows


class RotationCoefficientsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = _load()
        cls.r = {
            "magic": raw[0],
            "frames": raw[1],
            "state": raw[2],
            "state_ran": raw[3],
            "parameter_mode": raw[4],
            "coefficient_mode_a": raw[5],
            "refused_set_window": _signed(raw[6]),
            "refused_configure_window": _signed(raw[7]),
            "refused_region_bit7": _signed(raw[8]),
            "transition_result": [_signed(v) for v in raw[9:13]],
            "params_writes": raw[13],
            "coefficient_writes": raw[14],
            "region": raw[15],
            "capture_frame": raw[16],
            "capture_state": raw[17],
            "capture_boundary": raw[18],
            "capture_xp_base": _signed(raw[19]),
            "capture_coef_a": list(raw[20:25]),
            "capture_flags_a": list(raw[25:30]),
            "capture_coef_b": list(raw[30:35]),
            "capture_region": raw[35],
        }
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_rotation_coefficients not initialised: %r" % cls.r)
        cls.shots = os.environ.get("LIBSATURN_ROTC_SHOTS", "")
        cls.misaligned = os.path.join(cls.shots, "misaligned.txt") if cls.shots else ""

    def test_every_scene_ran(self):
        self.assertEqual(self.r["state_ran"], 0xF)
        self.assertGreaterEqual(self.r["frames"], 240)
        self.assertEqual(self.r["state"], 3)               # ended in MIX
        self.assertEqual(self.r["parameter_mode"], 2)      # RPMD per-line switch
        self.assertEqual(self.r["coefficient_mode_a"], 0)  # kx/ky
        self.assertEqual(self.r["transition_result"], [SAT_OK] * 4)

    def test_illegal_combinations_are_refused(self):
        # The rotation window mode (RPMD 3) needs a register this manager
        # does not model; a region with byte bit 7 set is not what RPTA
        # would read (its bit 6 is ignored and fixed, p06_32).
        self.assertEqual(self.r["refused_set_window"], SAT_ERR_INVALID_ARG)
        self.assertEqual(self.r["refused_configure_window"], SAT_ERR_INVALID_ARG)
        self.assertEqual(self.r["refused_region_bit7"], SAT_ERR_INVALID_ARG)

    def test_double_buffer_counters(self):
        # Every frame writes one parameter region and both coefficient
        # tables into the inactive half; the counters must show it.
        self.assertGreaterEqual(self.r["params_writes"], 240)
        self.assertEqual(self.r["coefficient_writes"], 2 * self.r["params_writes"])
        self.assertIn(self.r["region"], (0x40000, 0x40200))

    def test_capture_matches_the_reference_math(self):
        self.assertEqual(self.r["capture_frame"], 200)
        self.assertEqual(self.r["capture_state"], 3)
        self.assertEqual(self.r["capture_boundary"], mix_boundary(200))
        # Set B's rotated viewpoint base: (1 - cos(30 degrees)) * cx.
        self.assertEqual(self.r["capture_xp_base"], (65536 - YAW_COS) * 160)
        self.assertEqual(self.r["capture_region"], 0x40200)  # frame 200 is even

        rows = (16, 96, 112, 160, 200)
        for i, y in enumerate(rows):
            # RPMD mode 2 repurposes the flag bit: it selects the B image
            # for that line, so no row is hidden in the MIX scene (rows at
            # and above the horizon show B's k=0 solid lines instead).
            above_boundary = y < self.r["capture_boundary"]
            flag_a = USE_B if above_boundary else 0
            expected_a = encode_2word(floor_k(y), flag_a)
            self.assertEqual(self.r["capture_coef_a"][i], expected_a,
                             "A row %d" % y)
            self.assertEqual(self.r["capture_flags_a"][i], flag_a, "flags row %d" % y)
            self.assertEqual(self.r["capture_coef_b"][i], encode_2word(floor_k(y), 0),
                             "B row %d" % y)

    def _shot(self, name):
        path = os.path.join(self.shots, name)
        if not os.path.isfile(path):
            self.skipTest("missing capture %s" % path)
        return path

    def test_the_parameter_modes_draw_distinct_pictures(self):
        """What the emulator shows per mode, rows chosen off the HUD:
        the A scenes hide their sky lines so the sky bands show; the B
        scene covers everything with the yawed ocean; the MIX scene shows
        B above the animated boundary and A below it."""
        f50 = self._shot("f50.png")    # SWELL: set A, kx/ky
        f110 = self._shot("f110.png")  # WAVE: set A, Xp
        f170 = self._shot("f170.png")  # YAW B: set B everywhere
        f230 = self._shot("f230.png")  # MIX: per-line A/B

        # The sky shows identically through both A scenes' hidden lines.
        self.assertEqual(_region(f50, 40, 92), _region(f110, 40, 92))
        # The wave changes the floor: the shimmer state trades the per-line
        # depth factor for the per-line displacement (the manual's Xp mode).
        self.assertNotEqual(_region(f50, 120, 220), _region(f110, 120, 220))
        # The yawed B image covers the sky region entirely.
        self.assertNotEqual(_region(f50, 40, 92), _region(f170, 40, 92))
        # Above the MIX boundary both screens show the same B image, drawn
        # by the same tables; below it the flat A floor takes over.
        boundary = mix_boundary(230)
        self.assertGreater(boundary, 100)
        self.assertEqual(_region(f170, 40, boundary - 2),
                         _region(f230, 40, boundary - 2))
        self.assertNotEqual(_region(f170, 150, 220), _region(f230, 150, 220))

    def test_no_misaligned_accesses(self):
        if not self.misaligned or not os.path.isfile(self.misaligned):
            self.skipTest("no misaligned report")
        text = open(self.misaligned).read()
        self.assertIn("master 0, slave 0", text)


if __name__ == "__main__":
    unittest.main()
