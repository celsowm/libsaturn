"""test_rbg0_ground.py — assertions over harness/build/probe.json for the
vdp2_rbg0_ground example.

GPL-3.0 (see harness/LICENSE / harness/README.md for why this directory is
licensed separately from the rest of the repo).

Run after `run-harness.ps1 vdp2_rbg0_ground` has produced probe.json:

    python -m unittest discover harness/tests

Design bias, matching harness/README.md: assert on registers and VRAM
contents (values derived from the Sega hardware manuals in
docs/sega_saturn_hardware/), not on rendered pixels — Ymir's own rendering
is not treated as ground truth here.

Two ways to point this test at a probe.json:
  - LIBSATURN_PROBE_JSON env var (what run-harness.ps1 sets), or
  - --probe-json on the command line when run standalone.
Falls back to harness/build/probe.json.
"""

import json
import os
import sys
import unittest

RP_BASE_WORD = 0x10000
COEF_BASE_WORD = 0x12000
HORIZON = 96
SCREEN_HEIGHT = 224


def _default_probe_json_path():
    env = os.environ.get("LIBSATURN_PROBE_JSON")
    if env:
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "..", "build", "probe.json")


def load_probe(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def words_at(probe, base_word, count):
    """Looks up `count` consecutive words starting at `base_word` from
    probe["vdp2"]["vram_ranges"], which is a list of {base_word, words[]}
    exactly as requested via --dump-vram. Raises if the range wasn't dumped
    (run-harness.ps1 is responsible for requesting the ranges these tests need).
    """
    for r in probe["vdp2"]["vram_ranges"]:
        rb = r["base_word"]
        rw = r["words"]
        if rb <= base_word and base_word + count <= rb + len(rw):
            off = base_word - rb
            return rw[off:off + count]
    raise AssertionError(
        f"no dumped VRAM range covers word {hex(base_word)}..{hex(base_word + count)} — "
        f"was --dump-vram passed for this range?"
    )


class Rbg0GroundProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = _default_probe_json_path()
        if not os.path.exists(path):
            raise unittest.SkipTest(
                f"probe.json not found at {path} — run "
                f"'.\\harness\\run-harness.ps1 vdp2_rbg0_ground' first (requires a "
                f"Saturn BIOS/IPL image; see harness/README.md)."
            )
        cls.probe = load_probe(path)

        # Enforced here, not as an ordinary test method, so it can never be
        # skipped by test selection and always fails first: this harness
        # boots by injecting the built .bin directly into work RAM and
        # jumping to it (see harness/README.md — Ymir's CD block does not
        # currently complete a real BIOS disc boot for these images), rather
        # than by having the BIOS load the disc itself. Every other
        # assertion in this file is only meaningful if that injection
        # actually landed control in OUR code; otherwise every register read
        # below is just a BIOS default, and a green run would be a false pass.
        boot = cls.probe.get("boot")
        if boot is None or not boot.get("injected"):
            raise AssertionError("probe.json has no boot.injected=true — was probe.exe run with --bin?")
        load_addr = boot["load_addr"]
        bin_end = load_addr + boot["bin_bytes"]
        pc = boot["pc_after_run"]
        if not (load_addr <= pc < bin_end):
            raise AssertionError(
                f"pc_after_run=0x{pc:08X} is outside the injected program's range "
                f"[0x{load_addr:08X}, 0x{bin_end:08X}) — our code never ran; "
                f"every assertion below would be testing BIOS defaults, not our program. "
                f"Try a larger --boot-frames, or check the injection landed."
            )

    # --- The PR1 regressions -------------------------------------------

    def test_yst_differs_from_py(self):
        """The Yst==Py bug: collapses Ysp=E*(Yst-Py) to 0 on every scanline,
        so every row of the ground plane samples the same texture Y."""
        table = words_at(self.probe, RP_BASE_WORD, 48)
        yst = table[2]
        py = table[27]
        self.assertNotEqual(yst, py, "Yst == Py: the ground plane will be flat (PR1 regression)")

    def test_ewdr_is_transparent(self):
        """The opaque-erase bug: EWDR=0x8000 paints the whole VDP1 framebuffer
        opaque black every frame, hiding the VDP2 backdrop and RBG0 sky."""
        ewdr = self.probe["vdp1"]["ewdr"]
        self.assertEqual(ewdr, 0x0000, f"EWDR=0x{ewdr:04X}, expected 0x0000 (transparent erase)")

    def test_framebuffer_sample_matches_transparent_erase(self):
        """vdp2_rbg0_ground never issues any VDP1 sprite draws, so the whole
        VDP1 framebuffer is just the erase fill. With EWDR=0x0000 every
        sampled byte should read back as 0 — the direct pixel-level version
        of test_ewdr_is_transparent (register state vs. actual memory)."""
        sample = self.probe["vdp1"]["display_framebuffer_sample"]
        self.assertTrue(len(sample) > 0, "no framebuffer sample captured")
        self.assertTrue(
            all(b == 0 for b in sample),
            f"framebuffer sample is not all-zero: {sample[:16]}...",
        )

    # --- Register setup ---------------------------------------------------

    def test_ramctl_bank_assignment(self):
        """A0 = bitmap/character data (bits 1..0 = 11), A1 = coefficient table
        (bits 3..2 = 01)."""
        self.assertEqual(self.probe["vdp2"]["ramctl"], 0x1107)

    def test_ktctl_coefficient_enable(self):
        """A enable, 2-word, KMD=0 (coefficient drives both kx and ky)."""
        self.assertEqual(self.probe["vdp2"]["ktctl"], 0x0001)

    def test_bgon_r0on_and_r0tpon(self):
        """bit4 = R0ON (RBG0 enabled), bit12 = R0TPON (palette index 0 is
        real texel data, not the transparent code)."""
        bgon = self.probe["vdp2"]["bgon"]
        self.assertTrue(bgon & (1 << 4), f"BGON=0x{bgon:04X}: R0ON not set")
        self.assertTrue(bgon & (1 << 12), f"BGON=0x{bgon:04X}: R0TPON not set")

    def test_chctlb_bitmap_mode(self):
        """256-color, 512x256 bitmap, bitmap mode on."""
        self.assertEqual(self.probe["vdp2"]["chctlb"], 0x1200)

    def test_rpmd_parameter_a_only(self):
        self.assertEqual(self.probe["vdp2"]["rpmd"], 0x0000)

    # --- Coefficient table -------------------------------------------------

    def test_coefficient_table_transparency_boundary(self):
        """Rows y <= HORIZON are transparent (sky shows through); rows below
        are opaque coefficient data."""
        table = words_at(self.probe, COEF_BASE_WORD, SCREEN_HEIGHT * 2)
        for y in range(SCREEN_HEIGHT):
            w0 = table[y * 2]
            transparent = bool(w0 & 0x8000)
            if y <= HORIZON:
                self.assertTrue(transparent, f"row {y} (<=HORIZON) should be transparent")
            else:
                self.assertFalse(transparent, f"row {y} (>HORIZON) should not be transparent")

    def test_coefficient_k_monotonically_non_increasing(self):
        """k(y) = FOCAL / depth must never increase as y grows (depth grows),
        or the floor would appear to warp rather than recede smoothly."""
        table = words_at(self.probe, COEF_BASE_WORD, SCREEN_HEIGHT * 2)

        def k16(y):
            w0 = table[y * 2] & 0x007F
            w1 = table[y * 2 + 1]
            return (w0 << 16) | w1

        prev = None
        for y in range(HORIZON + 1, SCREEN_HEIGHT):
            cur = k16(y)
            if prev is not None:
                self.assertLessEqual(cur, prev, f"k(y={y}) increased vs k(y={y - 1})")
            prev = cur

    # --- Coefficient table addressing --------------------------------------

    def test_kast_encoding(self):
        """COEF_BASE_WORD 0x12000 (word) => byte 0x24000 => KAst = 0x9000,
        per Table 6.3 (LSB = 4H for 2-word coefficient data)."""
        table = words_at(self.probe, RP_BASE_WORD, 48)
        kast = table[42]
        expected = (COEF_BASE_WORD * 2) // 4
        self.assertEqual(kast, expected)

    def test_delta_kast_advances_one_row_per_scanline(self):
        table = words_at(self.probe, RP_BASE_WORD, 48)
        self.assertEqual(table[44], 0x0001)


if __name__ == "__main__":
    for i, arg in enumerate(sys.argv):
        if arg == "--probe-json" and i + 1 < len(sys.argv):
            os.environ["LIBSATURN_PROBE_JSON"] = sys.argv[i + 1]
            del sys.argv[i:i + 2]
            break
    unittest.main()
