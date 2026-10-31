r"""Acceptance for VDP2 special priority and special colour calculation
(examples/vdp2_special_priority).

    .\harness\run-vdp2-special-priority.ps1 -Bios .\bios\saturn_bios_us.bin

NBG0 is a 2-word pattern-name band over a solid NBG1, with a magenta VDP1 bar
at priority 4. Even cells carry PR=0/CC=0; odd cells below the HUD carry
PR=1/CC=1. The tile alternates dot code 1 (white, CRAM MSB clear) and dot
code 6 (orange, CRAM MSB set). SFCODE A bit 3 selects codes 6/7.

Screenshots:
  frame  40  SCREEN  SFPRMD/SFCCMD 0: priority and CC for the whole screen
  frame 100  CHAR    mode 1: the 2-word name's PR/CC bits
  frame 160  DOT     mode 2: PR/CC=1 and the dot code in SFCODE
  frame 240  MSB     SFPRMD 0, SFCCMD 3: the CRAM colour MSB

All expected colours are recomputed here from the palette values and the
15:17 ratio (CCRT 16). The pips at y=100 are inside the bar; y=60 is above it,
where the second image is NBG1 (blue). In CHAR mode the special cell rises
above the bar, so its second image is the magenta sprite itself.
"""

import json
import os
import struct
import unittest

from harness.scripts import telemetry as tel

MAGIC = 0x53505231  # "SPR1"
FIELDS = ("magic", "layers_ok", "cc_ok",
          "refused_priority_mode3", "refused_color_mode4", "refused_select2",
          "refused_layer", "refused_one_word_priority", "refused_one_word_color",
          "mode_ok", "mode", "ratio", "frames")
WHITE = (248, 248, 248)
ORANGE = (248, 128, 0)
BLUE = (0, 0, 128)
MAGENTA = (248, 0, 248)
RATIO = 16
TOL = 5
# Cell 0 (x 0..7) is normal; cell 1 (x 8..15) is special below the HUD rows.
PNORMAL, PSPECIAL = 2, 10
PY_BAR, PY_OPEN = 100, 60


def _mix(top, below, ratio=RATIO):
    """Manual p12_12: top (31-r) : second (r+1), in 32nds."""
    return tuple((t * (31 - ratio) + b * (ratio + 1)) // 32
                 for t, b in zip(top, below))


def _near(a, b, tol=TOL):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class Vdp2SpecialPriorityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_special_priority not initialised: %r" % cls.r)
        from PIL import Image
        cls.shots = os.environ.get("LIBSATURN_SPECIALPRIORITY_SHOTS", "")
        cls.screen = Image.open(os.path.join(cls.shots, "screen.png")).convert("RGB")
        cls.char = Image.open(os.path.join(cls.shots, "char.png")).convert("RGB")
        cls.dot = Image.open(os.path.join(cls.shots, "dot.png")).convert("RGB")
        cls.msb = Image.open(os.path.join(cls.shots, "msb.png")).convert("RGB")
        for image in (cls.screen, cls.char, cls.dot, cls.msb):
            assert image.size == (320, 224), image.size
        cls.p_screen = cls.screen.load()
        cls.p_char = cls.char.load()
        cls.p_dot = cls.dot.load()
        cls.p_msb = cls.msb.load()
        cls.misaligned = os.path.join(cls.shots, "misaligned.txt") if cls.shots else ""

    def assertPixel(self, image, x, y, want, label):
        got = image[x, y]
        self.assertTrue(_near(got, want),
                        "%s (%d,%d): %r want %r" % (label, x, y, got, want))

    def test_layers_and_refusals_were_recorded(self):
        r = self.r
        for key in FIELDS[1:9]:
            self.assertEqual(r[key], 1, key)
        self.assertEqual(r["mode_ok"], 0xF, "every mode applied")
        self.assertEqual(r["mode"], 3, "final mode is MSB")
        self.assertEqual(r["ratio"], RATIO)
        self.assertGreater(r["frames"], 240)

    def test_screen_mode_uses_priority_and_cc_for_the_whole_screen(self):
        # Priority 4 loses to the priority-4 sprite bar (sprites win ties).
        for x in (PNORMAL, PNORMAL + 1, PSPECIAL, PSPECIAL + 1):
            self.assertPixel(self.p_screen, x, PY_BAR, MAGENTA, "screen bar")
        # Above the bar every cell colour-calculates with NBG1.
        for x, top in ((PNORMAL, WHITE), (PNORMAL + 1, ORANGE),
                       (PSPECIAL, WHITE), (PSPECIAL + 1, ORANGE)):
            self.assertPixel(self.p_screen, x, PY_OPEN, _mix(top, BLUE), "screen open")

    def test_character_mode_uses_the_name_bits(self):
        # Normal cell: PR=0 -> priority 4, covered by the bar; CC=0.
        for x in (PNORMAL, PNORMAL + 1):
            self.assertPixel(self.p_char, x, PY_BAR, MAGENTA, "char normal bar")
            self.assertPixel(self.p_char, x, PY_OPEN,
                             WHITE if x == PNORMAL else ORANGE, "char normal open")
        # Special cell: PR=1 -> priority 5, above the bar; CC=1, so it mixes
        # with the image below, which now is the magenta sprite.
        self.assertPixel(self.p_char, PSPECIAL, PY_BAR, _mix(WHITE, MAGENTA), "char special white bar")
        self.assertPixel(self.p_char, PSPECIAL + 1, PY_BAR, _mix(ORANGE, MAGENTA), "char special orange bar")
        # Above the bar the special cell mixes with NBG1.
        self.assertPixel(self.p_char, PSPECIAL, PY_OPEN, _mix(WHITE, BLUE), "char special white open")
        self.assertPixel(self.p_char, PSPECIAL + 1, PY_OPEN, _mix(ORANGE, BLUE), "char special orange open")

    def test_dot_mode_uses_the_function_code(self):
        # Normal cell stays below the bar.
        for x in (PNORMAL, PNORMAL + 1):
            self.assertPixel(self.p_dot, x, PY_BAR, MAGENTA, "dot normal bar")
            self.assertPixel(self.p_dot, x, PY_OPEN,
                             WHITE if x == PNORMAL else ORANGE, "dot normal open")
        # Dot code 1 (white) is not in SFCODE A bit 3, so it keeps priority 4
        # and no CC; dot code 6 is, so it rises and blends with the bar.
        self.assertPixel(self.p_dot, PSPECIAL, PY_BAR, MAGENTA, "dot white bar")
        self.assertPixel(self.p_dot, PSPECIAL + 1, PY_BAR, _mix(ORANGE, MAGENTA), "dot orange bar")
        self.assertPixel(self.p_dot, PSPECIAL, PY_OPEN, WHITE, "dot white open")
        self.assertPixel(self.p_dot, PSPECIAL + 1, PY_OPEN, _mix(ORANGE, BLUE), "dot orange open")

    def test_msb_mode_uses_the_cram_colour_msb(self):
        # Priority mode 0: every cell is priority 4, so the bar covers them.
        for x in (PNORMAL, PNORMAL + 1, PSPECIAL, PSPECIAL + 1):
            self.assertPixel(self.p_msb, x, PY_BAR, MAGENTA, "msb bar")
        # White has CRAM bit 15 clear; orange has it set. The cell's CC bit is
        # ignored in mode 3.
        for x, top in ((PNORMAL, WHITE), (PNORMAL + 1, ORANGE),
                       (PSPECIAL, WHITE), (PSPECIAL + 1, ORANGE)):
            want = top if top == WHITE else _mix(ORANGE, BLUE)
            self.assertPixel(self.p_msb, x, PY_OPEN, want, "msb open")

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
        for name in ("screen.png", "char.png", "dot.png", "msb.png"):
            self.assertTrue(os.path.exists(os.path.join(self.shots, name)))
        self.assertFalse(os.path.exists(os.path.join(self.shots, "never.png")))

    def _probe(self):
        path = os.environ.get("LIBSATURN_SPECIALPRIORITY_PROBE")
        if not path or not os.path.isfile(path):
            self.skipTest("no probe.json")
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)

    def test_special_registers_match_the_final_mode(self):
        regs = self._probe()["vdp2"]
        self.assertEqual(regs["sfprmd"], 0x0000, "SFPRMD: %04X" % regs["sfprmd"])
        self.assertEqual(regs["sfccmd"], 0x0003, "SFCCMD: %04X" % regs["sfccmd"])
        self.assertEqual(regs["sfcode"], 0x0008, "SFCODE: %04X" % regs["sfcode"])
        self.assertEqual(regs["sfsel"], 0x0000, "SFSEL: %04X" % regs["sfsel"])
        self.assertEqual(regs["ccctl"], 0x0001, "CCCTL: %04X" % regs["ccctl"])
        self.assertEqual(regs["ccrna"] & 0x1F, RATIO, "CCRNA: %04X" % regs["ccrna"])
        self.assertEqual(regs["lnclen"], 0, "LNCLEN: %04X" % regs["lnclen"])

    def test_watch_write_records_the_msb_switch(self):
        """The probe watches SFCCMD from emulated frame 269, one frame before
        the MSB switch at program frame 180; the write must carry mode 3."""
        doc = self._probe()
        watch = doc.get("watch_writes")
        if not watch:
            self.skipTest("no --watch-write capture")
        hits = watch["master"]["hits"]
        self.assertGreater(watch["master"]["count"], 0)
        self.assertTrue(any(hit["value"] == 3 for hit in hits),
                        "no SFCCMD=3 write in %r" % hits)


def _load():
    dump = os.environ.get("LIBSATURN_SPECIALPRIORITY_DUMP")
    map_path = os.environ.get("LIBSATURN_SPECIALPRIORITY_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-vdp2-special-priority.ps1 first")
    raw = tel.load_dump(dump, map_path, "_g_special_priority",
                        ">%dI" % len(FIELDS), 4 * len(FIELDS))
    return dict(zip(FIELDS, raw))


if __name__ == "__main__":
    unittest.main()
